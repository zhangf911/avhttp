#ifndef __HTTP_STREAM_IMPL_VISITORS_HPP__
#define __HTTP_STREAM_IMPL_VISITORS_HPP__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/variant.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>

namespace avhttp{
namespace detail{

namespace visitor{

	struct response_options :boost::static_visitor<response_opts>
	{
 		template<class T>
		response_opts & operator()(boost::shared_ptr<T> p) const
		{
			return p->response_options();
		}
	};

	struct request: boost::static_visitor<>
	{
		const avhttp::request_opts & opt_;
		boost::system::error_code & ec_;
		request(const avhttp::request_opts &_opt, boost::system::error_code &_ec)
			: opt_(_opt), ec_(_ec)
		{}

		template<class T>
		void operator()(boost::shared_ptr<T> p) const
		{
			p->request(opt_, ec_);
		}
	};

	struct async_request : boost::static_visitor<>
	{
		avhttp::request_opts opt_;
		boost::function<void (boost::system::error_code ec)> handler_;

		template<class Handler>
		async_request(const avhttp::request_opts &_opt, Handler _handler)
			: opt_(_opt), handler_(_handler)
		{}

		template<class T>
		void operator()(boost::shared_ptr<T> p) const
		{
 			p->async_request(opt_, handler_);
		}
	};

	template <typename MutableBufferSequence>
	struct read_some : boost::static_visitor<std::size_t>
	{
		const MutableBufferSequence &buffers_;
		boost::system::error_code &ec_;
		read_some(const MutableBufferSequence &buffers, boost::system::error_code &ec)
		  : buffers_(buffers), ec_(ec)
		{
		}

		template<class T>
		std::size_t operator()(boost::shared_ptr<T> p) const
		{
 			return p->read_some(buffers_, ec_);
		}
	};

	template <typename MutableBufferSequence>
	struct async_read_some : boost::static_visitor<>
	{
		const MutableBufferSequence &buffers_;
		boost::function<void (boost::system::error_code ec, std::size_t) > handler_;

		template<class Handler>
		async_read_some(const MutableBufferSequence &buffers, Handler handler)
		  : buffers_(buffers), handler_(handler)
		{}

		template<class T>
		void operator()(boost::shared_ptr<T> p) const
		{
  			p->async_read_some(buffers_, handler_);
		}
	};

	template <typename ConstBufferSequence>
	struct write_some : boost::static_visitor<std::size_t>
	{
		const ConstBufferSequence &buffers_;
		boost::system::error_code &ec_;
		write_some(const ConstBufferSequence &buffers, boost::system::error_code &ec)
		  : buffers_(buffers), ec_(ec)
		{
		}

		template<class T>
		std::size_t operator()(boost::shared_ptr<T> p) const
		{
 			return p->write_some(buffers_, ec_);
		}
	};

	template <typename ConstBufferSequence>
	struct async_write_some : boost::static_visitor<>
	{
		const ConstBufferSequence &buffers_;
		boost::function<void (boost::system::error_code ec, std::size_t) > handler_;

		template<class Handler>
		async_write_some(const ConstBufferSequence &buffers, Handler handler)
		  : buffers_(buffers), handler_(handler)
		{}

		template<class T>
		void operator()(boost::shared_ptr<T> p) const
		{
  			p->async_write_some(buffers_, handler_);
		}
	};


} // namespace visitor
} // namespace detail
} // namespace avhttp

#endif
