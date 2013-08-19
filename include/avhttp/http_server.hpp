//
// http_server.hpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2013 Jack (jack dot wgm at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// path LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef __HTTP_SERVER_HPP__
#define __HTTP_SERVER_HPP__

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio.hpp>
#include <string>
#include <boost/array.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>

namespace avhttp {

using boost::asio::ip::tcp;

class http_server : boost::asio::coroutine
{

public:
	AVHTTP_DECL explicit http_server(boost::asio::io_service& io)
		: m_ioservice(io)
	{
	}

	AVHTTP_DECL virtual ~http_server()
	{
	}

private:

	// boost::asio::io_service引用.
	boost::asio::io_service& m_ioservice;

	// 用于接受进来的连接.
	boost::shared_ptr<tcp::acceptor> m_accetpor;

	// 一个客户端的当前连接.
	boost::shared_ptr<tcp::socket> m_socket;

};

} // namespace avhttp

#endif // __HTTP_SERVER_HPP__
