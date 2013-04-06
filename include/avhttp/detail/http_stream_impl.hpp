//
// http_stream.hpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2012 -2013 Jack (jack dot wgm at gmail dot com)
// Copyright (c) 2013 microcai ( microcaicai at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef __AVHTTP_DETAIL_HTTP_STREAM_IMPL_HPP_
#define __AVHTTP_DETAIL_HTTP_STREAM_IMPL_HPP_

#include <boost/bind.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/variant/static_visitor.hpp>

#include "../settings.hpp"
#include "parsers.hpp"
#include "error_codec.hpp"

#include "coro/coro.hpp"

namespace avhttp{
namespace detail{

#include "coro/yield.hpp"

// ----------------------------
// 关于这个类的设计缘由，参考
// 	http://microcai.org/2013/04/05/template-as-polymiorphism.html
// http_request_impl 实现传输层无关的 HTTP 协议.
// http_request_impl 负责格式化 HTTP 头，并将头发送出去
// 然后解析 服务器应答。http_request_impl 只处理到 header 部分，剩余部分由
// http_stream 继续使用 boost::variant 提供 read/write
template<class AsioStream>
class http_stream_impl : boost::noncopyable{
	AsioStream& m_stream;

	boost::asio::streambuf m_response_buf;
	boost::asio::streambuf m_request_buf;
	bool m_has_expect_continue;                                // 是否向服务器发送了 EXPECT: 100 contine
	response_opts m_response_opts;                             // http服务器返回的http头信息.

	std::string m_content_type;                                // 数据类型.
	std::size_t m_content_length;                              // 数据内容长度.
	std::string m_location;                                    // 重定向的地址.

private:
	// 从 request_opts 构建一个 HTTP REQUEST HEAD
	void build_header(request_opts opts)
	{
		std::ostream header(&m_request_buf);

		// 从 opts 立即格式化好头部数据.
		m_has_expect_continue = boost::to_lower_copy(opts.find("Expect")) == "100-cotinue";

		// 为了实现“漂亮” 的HTTP 头部， 遵从约定：
		// 		Host 要放在第二行， 而 Connection: close 放到最后一行
		// 第一行
		if (opts.find(http_options::request_method).empty())
			header <<  "GET ";
		else
			header << opts.find(http_options::request_method) <<  " " ;
		header << opts.find(http_options::url) <<  " " ;
		if (opts.find(http_options::http_version).empty())
			header <<  "HTTP/1.1";
		else
			header <<  opts.find(http_options::http_version);
		header <<  "\r\n";
		// 第一行结束，删除使用过的选项.
		opts.remove(http_options::request_method);
		opts.remove(http_options::url);
		opts.remove(http_options::http_version);
		// 第二行， Host
		std::string host;
		opts.find(http_options::host, host);
		opts.remove(http_options::host);

		if (!host.empty())
			header <<  "Host: " <<  host <<  "\r\n";

		// 得到Accept信息.
		std::string accept = "*/*";
		if (opts.find(http_options::accept, accept))
			opts.remove(http_options::accept);	// 删除处理过的选项.
		header <<  "Accept: " << accept <<  "\r\n";
		// 获取 body
		std::string request_body;
		opts.find(http_options::request_body, request_body);
		opts.remove(http_options::request_body);

		std::string connection = "close"; // 默认 close
		opts.find(http_options::connection, connection);
		opts.remove(http_options::connection);

		header <<  opts.header_string();

		header <<  "connection: " <<  connection <<  "\r\n";
		header << "\r\n";
		// 头部完成

		// 身体接上
		header <<  request_body;
		// 搞定！
	}
	// helper ,  用于投递消息.
	template<class Handler>
	void post(BOOST_ASIO_MOVE_ARG(Handler) handler)
	{
		m_stream.get_io_service().post(handler);
	}
public:
	// ----------------------
	// 构造一个 http_request_impl, 用于后续的访问.
	http_stream_impl(AsioStream & stream)
		: m_stream(stream)
	{
		// 实现 HTTP 协议.
	}

	// -----------------------
	// 执行一次异步请求
	template<class Handler>
	void async_request(const request_opts & opts, Handler handler)
	{
		BOOST_ASIO_CONNECT_HANDLER_CHECK(Handler, handler) type_check;

		m_response_opts.clear();
		build_header(opts);
		// 执行异步HTTP请求.
		// 发送构造好的 HTTP 头，然后读取服务器应答.
		boost::asio::async_write(m_stream, m_request_buf,
				boost::bind(&http_stream_impl::handle_request_write<Handler>, this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred,
						handler
				)
		);
	}
	// ----------------------
	// 执行一次同步请求
	void request(const request_opts & opts, boost::system::error_code & ec)
	{
		std::size_t bytes_transferred;
		m_response_opts.clear();
		build_header(opts);
		do{
			boost::asio::write(m_stream, m_request_buf, ec);
			if (ec)	break;

			bytes_transferred = boost::asio::read_until(m_stream, m_response_buf,"\r\n", ec);
			if (ec)	break;
			// 解析服务器应答.
			if ( !parse_response_status( ec, bytes_transferred ) )
				break;

			if ( ec == errc::continue_request )
			{
				if ( !m_has_expect_continue )
				{
					// 没有使用 Expect: 100-contine 的情况下服务器发送这个， 就是错误的.
					// 否则用户就可以自己发送 body 了.
					ec = errc::make_error_code( errc::malformed_response_headers );
				}
				break;
			}
			bytes_transferred = boost::asio::read_until(m_stream, m_response_buf, "\r\n\r\n", ec);
			if (ec)
			{
				ec = errc::make_error_code( errc::malformed_response_headers );
			}
			// 解析
			parse_response_header(ec, bytes_transferred);
		}while (0);
	}

	response_opts &response_options(){
		return m_response_opts;
	}

	template <typename MutableBufferSequence>
	std::size_t read_some( const MutableBufferSequence &buffers,
						   boost::system::error_code &ec )
	{
		// 如果还有数据在m_response中, 先读取m_response中的数据.
		if ( m_response_buf.size() > 0 ) {
			std::size_t bytes_transferred = 0;
			typename MutableBufferSequence::const_iterator iter = buffers.begin();
			typename MutableBufferSequence::const_iterator end = buffers.end();
			for ( ; iter != end && m_response_buf.size() > 0; ++iter ) {
				boost::asio::mutable_buffer buffer( *iter );
				size_t length = boost::asio::buffer_size( buffer );
				if ( length > 0 ) {
					bytes_transferred += m_response_buf.sgetn(
											 boost::asio::buffer_cast<char*>( buffer ), length );
				}
			}
			ec = boost::system::error_code();
			return bytes_transferred;
		}

		// 再从socket中读取数据.
		std::size_t bytes_transferred = m_stream.read_some(buffers, ec);
		if ( ec == boost::asio::error::shut_down )
			ec = boost::asio::error::eof;
		return bytes_transferred;
	}
	///从这个http_stream异步读取一些数据.
	// @param buffers一个或多个用于读取数据的缓冲区, 这个类型必须满足MutableBufferSequence,
	//  MutableBufferSequence的定义在boost.asio文档中.
	// http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/MutableBufferSequence.html
	// @param handler在读取操作完成或出现错误时, 将被回调, 它满足以下条件:
	// @begin code
	//  void handler(
	//    const boost::system::error_code &ec	// 用于返回操作状态.
	//    int bytes_transferred,				// 返回读取的数据字节数.
	//  );
	// @end code
	// @begin example
	//   void handler(const boost::system::error_code &ec, int bytes_transferred)
	//   {
	//		// 处理异步回调.
	//   }
	//   http_stream h(io_service);
	//   ...
	//   boost::array<char, 1024> buffer;
	//   boost::asio::async_read(h, boost::asio::buffer(buffer), handler);
	//   ...
	// @end example
	// 关于示例中的boost::asio::buffer用法可以参考boost中的文档. 它可以接受一个
	// boost.array或std.vector作为数据容器.
	template <typename MutableBufferSequence, typename Handler>
	void async_read_some(const MutableBufferSequence &buffers, Handler handler)
	{
		BOOST_ASIO_READ_HANDLER_CHECK(Handler, handler) type_check;

		boost::system::error_code ec;
		if (m_response_buf.size() > 0)
		{
			std::size_t bytes_transferred = read_some(buffers, ec);
			post(boost::asio::detail::bind_handler(handler, ec, bytes_transferred));
			return;
		}
		// 当缓冲区数据不够, 直接从socket中异步读取.
		m_stream.async_read_some(buffers, handler);
	}

	///向这个http_stream中发送一些数据.
	// @param buffers是一个或多个用于发送数据缓冲. 这个类型必须满足ConstBufferSequence, 参考文档:
	// http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/ConstBufferSequence.html
	// @返回实现发送的数据大小.
	// @备注: 该函数将会阻塞到一直等待数据被发送或发生错误时才返回.
	// write_some不保证发送完所有数据, 用户需要根据返回值来确定已经发送的数据大小.
	// @begin example
	//  boost::system::error_code ec;
	//  std::size bytes_transferred = s.write_some(boost::asio::buffer(data, size), ec);
	//  ...
	// @end example
	// 关于示例中的boost::asio::buffer用法可以参考boost中的文档. 它可以接受一个
	// boost.array或std.vector作为数据容器.
	template <typename ConstBufferSequence>
	std::size_t write_some(const ConstBufferSequence &buffers,
		boost::system::error_code &ec)
	{
		std::size_t bytes_transferred = m_stream.write_some(buffers, ec);
		if (ec == boost::asio::error::shut_down)
			ec = boost::asio::error::eof;
		return bytes_transferred;
	}

	///从这个http_stream异步发送一些数据.
	// @param buffers一个或多个用于读取数据的缓冲区, 这个类型必须满足ConstBufferSequence,
	//  ConstBufferSequence的定义在boost.asio文档中.
	// http://www.boost.org/doc/libs/1_53_0/doc/html/boost_asio/reference/ConstBufferSequence.html
	// @param handler在发送操作完成或出现错误时, 将被回调, 它满足以下条件:
	// @begin code
	//  void handler(
	//    const boost::system::error_code &ec	// 用于返回操作状态.
	//    int bytes_transferred,				// 返回发送的数据字节数.
	//  );
	// @end code
	// @begin example
	//   void handler(const boost::system::error_code &ec, int bytes_transferred)
	//   {
	//		// 处理异步回调.
	//   }
	//   http_stream h(io_service);
	//   ...
	//   h.async_write_some(boost::asio::buffer(data, size), handler);
	//   ...
	// @end example
	// 关于示例中的boost::asio::buffer用法可以参考boost中的文档. 它可以接受一个
	// boost.array或std.vector作为数据容器.
	template <typename ConstBufferSequence, typename Handler>
	void async_write_some(const ConstBufferSequence &buffers, Handler handler)
	{
		BOOST_ASIO_WAIT_HANDLER_CHECK(Handler, handler) type_check;

		m_stream.async_write_some(buffers, handler);
	}

private:
	template<class Handler>
	void handle_request_write(const boost::system::error_code & ec,
								std::size_t bytes_transferred, Handler handler)
	{
		if (ec){
			// 返回错误.
			post(boost::asio::detail::bind_handler(handler, ec));
		}else{
			// 读取服务器应答.
			boost::asio::async_read_until(m_stream, m_response_buf, "\r\n", 
				boost::bind(&http_stream_impl::handle_response_read<Handler>, this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred,
						handler,
						boost::coro::coroutine()
				)
			);
		}
	}

	// 以每次读取一行来读取 HTTP RESPONSE HEAD
	template<class Handler>
	void handle_response_read(boost::system::error_code ec, std::size_t bytes_transferred,
								Handler handler, boost::coro::coroutine coro)
	{
		reenter(&coro)
		{
			if (ec)
			{
				// 返回错误.
				post(boost::asio::detail::bind_handler(handler, ec));
			}
			else
			{
                // 解析服务器应答.
                if ( !parse_response_status( ec, bytes_transferred ) )
				{
					post( boost::asio::detail::bind_handler( handler, ec ) );
				}

				if ( ec == errc::continue_request )
				{
					if ( m_has_expect_continue )
					{
						// 向用户报告，让用户自己继续发送 body
						post( boost::asio::detail::bind_handler( handler, ec ) );
					}
					else
					{	// 没有使用 Expect: 100-contine 的情况下服务器发送这个， 就是错误的.
						post( boost::asio::detail::bind_handler( handler,
								errc::make_error_code( errc::malformed_response_headers ) ) );
					}
					return;
				}

				// 继续读取服务器应答直到结束 (遇到 \r\n\r\n).
				_yield	boost::asio::async_read_until(m_stream, m_response_buf, "\r\n\r\n", 
							boost::bind(&http_stream_impl::handle_response_read<Handler>, this,
									boost::asio::placeholders::error,
									boost::asio::placeholders::bytes_transferred,
									handler,
									coro
							)
					);
				if (ec)
				{
					post( boost::asio::detail::bind_handler( handler,
							errc::make_error_code( errc::malformed_response_headers ) ) );
					return ;
				}
				// 解析
				parse_response_header(ec, bytes_transferred);
				post( boost::asio::detail::bind_handler( handler, ec) );
			}
		}

	}

	bool parse_response_status(boost::system::error_code & ec, std::size_t bytes_transferred)
	{
		std::string statusline;
		statusline.resize(bytes_transferred);
		m_response_buf.sgetn(&statusline[0], bytes_transferred);

		int  version_major, version_minor, status;

		if (!parse_http_status_line(statusline.begin(), statusline.end(), 
									version_major, version_minor, status))
		{
			ec = errc::make_error_code(errc::malformed_status_line);
			return false;
		}
		m_response_opts.insert(avhttp::http_options::status_code, boost::lexical_cast<std::string>(status));
		ec = make_error_code(static_cast<avhttp::errc::errc_t>(status));
		return true;
	}

	bool parse_response_header(boost::system::error_code & ec, std::size_t bytes_transferred)
	{
		std::string header_string;
		header_string.resize(bytes_transferred);
		m_response_buf.sgetn(&header_string[0], bytes_transferred);

		// 解析Http Header.
		if (!parse_http_headers(header_string.begin(), header_string.end(),
			m_content_type, m_content_length, m_location, m_response_opts.option_all()))
		{
			ec = errc::make_error_code(avhttp::errc::malformed_response_headers);
			return false;
		}
		return true;
	}
};

#include "coro/unyield.hpp"

} // namespace detail
} // namespace avhttp

#endif // __AVHTTP_DETAIL_HTTP_STREAM_IMPL_HPP_
