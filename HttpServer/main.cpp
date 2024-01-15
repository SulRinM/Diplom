#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <iostream>
#include "http_connection.h"
#include <Windows.h>
#include "../ConfigParser/config.h"


static void httpServer(tcp::acceptor& acceptor, tcp::socket& socket) {
	acceptor.async_accept(socket,
		[&](beast::error_code ec) {
			if (!ec)
				make_shared<HttpConnection>(move(socket))->start();
			httpServer(acceptor, socket);
		});
}


int main(int argc, char** argv) {
	try {
		SetConsoleCP(CP_UTF8);
		SetConsoleOutputCP(CP_UTF8);

		Config config(STR_CONFIG);
		
		auto const address = net::ip::make_address("127.0.0.1");
		unsigned short port = static_cast<unsigned short>(stoi(config.getConfig("http_server_port")));

		net::io_context ioc{ 1 };

		tcp::acceptor acceptor{ ioc, { address, port } };
		tcp::socket socket{ ioc };
		httpServer(acceptor, socket);

		cout << "Open browser and connect to http://127.0.0.1:" << port << " to see the web server operating" << endl;

		ioc.run();

	}
	catch (const exception& e) {
		cerr << "Error: " << e.what() << endl;

		return EXIT_FAILURE;
	}
	catch (...) {
		cout << "General error" << endl;
	}

	return 0;
}