#include "http_utils.h"

bool is_text(const boost::beast::multi_buffer::const_buffers_type& b)
{
	for (auto itr = b.begin(); itr != b.end(); itr++)
	{
		for (int i = 0; i < (*itr).size(); i++)
		{
			if (*((const char*)(*itr).data() + i) == 0)
			{
				return false;
			}
		}
	}

	return true;
}

string get_html_content(const Link& link)
{

	string result;

	try
	{
		string host = link.hostName;
		string query = link.query;

		net::io_context ioc; // The io_context class provides the core I/O functionality for users of the asynchronous I/O objects, including:
							// boost::asio::ip::tcp::socket
							// boost::asio::ip::tcp::acceptor
							// boost::asio::ip::udp::socket
							// deadline_timer
							// The io_context class also includes facilities intended for developers of custom asynchronous services.

		if (link.protocol == ProtocolType::HTTPS)
		{

			ssl::context ctx(ssl::context::tlsv13_client); // SSL Context is a collection of ciphers, protocol versions, trusted certificates, TLS options, TLS extensions etc.
			ctx.set_default_verify_paths(); // Configures the context to use the default directories for finding certification authority certificates.

			beast::ssl_stream<beast::tcp_stream> stream(ioc, ctx); // Creates ssl_stream. ssl_stream provides stream-oriented functionality using OpenSSL.
			stream.set_verify_mode(ssl::verify_none); // Set the peer verification mode.

			stream.set_verify_callback([](bool preverified, ssl::verify_context& ctx) {
				return true; // Accept any certificate
				});


			if (!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str())) 
			{
				beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
				throw beast::system_error{ec};
			}

			ip::tcp::resolver resolver(ioc); // The ip::basic_resolver class template provides the ability to resolve a query to a list of endpoints.
			get_lowest_layer(stream).connect(resolver.resolve({ host, "https" }));
			get_lowest_layer(stream).expires_after(chrono::seconds(30));

			http::request<http::empty_body> req{http::verb::get, query, 11}; // Set up an HTTP GET request message.
			req.set(http::field::host, host);
			req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

			stream.handshake(ssl::stream_base::client); //Send the request to the remote host.
			http::write(stream, req);

			beast::flat_buffer buffer; // Buffer is used for reading and must be persisted.
			http::response<http::dynamic_body> res; // Declare a container to hold the response.
			http::read(stream, buffer, res); // Receive the HTTP response.

			if (is_text(res.body().data()))
			{
				result = buffers_to_string(res.body().data());
			}
			else
			{
				cout << "This is not a text link, bailing out..." << endl;
			}

			beast::error_code ec;
			stream.shutdown(ec); // Shut down SSL on the stream.
			if (ec == net::error::eof) 
			{
				ec = {};
			}
			if (ec) 
			{
				throw beast::system_error{ec};
			}
		}
		else
		{
			tcp::resolver resolver(ioc);
			beast::tcp_stream stream(ioc);

			auto const results = resolver.resolve(host, "http"); // Look up the domain name.

			stream.connect(results); // Make the connection on the IP address we get from a lookup.

			http::request<http::string_body> req{http::verb::get, query, 11}; // Set up an HTTP GET request message.
			req.set(http::field::host, host);
			req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

			http::write(stream, req); // Send the HTTP request to the remote host.

			beast::flat_buffer buffer; // Buffer is used for reading and must be persisted.

			http::response<http::dynamic_body> res; // Receive the HTTP response.

			http::read(stream, buffer, res); // Receive the HTTP response.

			if (is_text(res.body().data()))
			{
				result = buffers_to_string(res.body().data());
			}
			else
			{
				cout << "This is not a text link, bailing out..." << endl;
			}

			beast::error_code ec;
			stream.socket().shutdown(tcp::socket::shutdown_both, ec); // Close the socket.

			if (ec && ec != beast::errc::not_connected)
				throw beast::system_error{ec};
		}
	}
	catch (const exception& e)
	{
		cout << e.what() << endl;
	}

	return result;
}

