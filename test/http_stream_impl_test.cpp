#include <iostream>
#include <boost/array.hpp>
#include "avhttp.hpp"
#include "avhttp/detail/http_stream_impl.hpp"

static void handle_request(boost::system::error_code ec)
{
	std::cout <<  ec.message() <<  std::endl;
}

int main(int argc, char* argv[])
{
	try {
		boost::asio::io_service io;
		boost::asio::ssl::context ctx(boost::asio::ssl::context_base::sslv23_client);
		boost::asio::ssl::stream<boost::asio::ip::tcp::socket> s(io, ctx);
		boost::asio::ip::tcp::resolver::query query("www.google.com", "80");
		boost::asio::ip::tcp::resolver resolver(io);
		resolver.resolve(query);
		
// 		s.lowest_layer().connect(*resolver.resolve(query));
// 		s.handshake(boost::asio::ssl::stream_base::client);
		boost::asio::ip::tcp::socket ss(io);
		ss.connect(*resolver.resolve(query));
//  		avhttp::detail::http_stream_impl<boost::asio::ssl::stream<boost::asio::ip::tcp::socket> > h(s.lowest_layer());
		avhttp::detail::http_stream_impl<boost::asio::ip::tcp::socket > h(ss);

		avhttp::request_opts opts;
		opts(avhttp::http_options::url, "/help")
			(avhttp::http_options::host, "www.google.com");

		boost::system::error_code ec;
		h.request(opts, ec);
		std::cout <<  ec.message() << std::endl;

		io.run();
	}
	catch (boost::system::system_error &e)
	{
		std::cerr << e.what() << std::endl;
		return -1;
	}

	return 0;
}
