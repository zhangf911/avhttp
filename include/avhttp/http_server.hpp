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

class http_server : boost::asio::coroutine
{
public:
	AVHTTP_DECL explicit http_server()
	{
	}

	AVHTTP_DECL virtual ~http_server()
	{
	}

private:
};

} // namespace avhttp

#endif // __HTTP_SERVER_HPP__
