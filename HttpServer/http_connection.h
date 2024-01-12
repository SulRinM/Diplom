#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <regex>
#include <pqxx/pqxx>
#include <sstream>
#include <iomanip>
#include <locale>
#include <codecvt>

#include "../ConfigParser/config.h"

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
using namespace std;

class HttpConnection : public enable_shared_from_this<HttpConnection> {

private:
	vector<pair<string, int>> getSorted(unordered_map<string, int>&);
	vector<pair<string, int>> getDatafromDB(const string&);
	vector<string> parseQuery(const string&);
protected:
	void writeResponse();
	void checkDeadline();
	void readRequest();
	void processRequest();
	void createResponseGet();
	void createResponsePost();

	tcp::socket socket_;
	beast::flat_buffer buffer_{ 8192 };
	http::request<http::dynamic_body> request_;
	http::response<http::dynamic_body> response_;

	net::steady_timer deadline_{
		socket_.get_executor(), chrono::seconds(60) };

public:
	HttpConnection(tcp::socket socket);
	void start();
};

