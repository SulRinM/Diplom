#include "http_connection.h"

static string url_decode(const string& encoded);
static string convert_to_utf8(const string& str);


vector<string> HttpConnection::parseQuery(const string& searchStr) {
	string requestStr = searchStr.substr(6, searchStr.length());
	vector<string> res(requestStr.length() / 3);
	regex word_regex("[a-zA-Z]{3,32}");
	for (auto itr = sregex_iterator(requestStr.begin(), requestStr.end(), word_regex); itr != sregex_iterator(); ++itr) {
		string word = (*itr).str();
		transform(word.begin(), word.end(), word.begin(), [](unsigned char ch) { return tolower(ch); });
		res.push_back(word);
	}
	return res;
}

vector<pair<string, int>> HttpConnection::getSorted(unordered_map<string, int>& results) {
	vector<pair<string, int>> res;
	for (auto pair : results)
		res.push_back(pair);
	sort(res.begin(), res.end(), []
	(pair<string, int>& a, pair<string, int>& b) { return a.second > b.second; });
	return res;
}

vector<pair<string, int>> HttpConnection::getDatafromDB(const string& searchStr) {
	vector<pair<string, int>> res;
	try {
		Config config("../Config/config.ini");

		string host = config.getConfig("db_host");
		string port = config.getConfig("db_port");
		string dbname = config.getConfig("db_name");
		string user = config.getConfig("db_username");
		string pass = config.getConfig("db_user_password");

		string con_str
		(
			"host=" + host + " "
			"port=" + port + " "
			"dbname=" + dbname + " "
			"user=" + user + " "
			"password=" + pass
		);

		pqxx::connection connection(con_str);
		pqxx::work work{ connection };

		vector<string> queryWords = HttpConnection::parseQuery(searchStr);
		unordered_map<string, int> resultsNotSorted;

		for (const auto& word : queryWords) {
			for (auto [url, quantity] : work.query<string, int>(
				"select url, quantity from documents "
				"join documents_words on documents.id = documents_words.document_id "
				"where word_id = (select id from words where word = '" + word + "');")) {
				unordered_map<string, int>::iterator itrUrl = resultsNotSorted.find(url);
				if (itrUrl == resultsNotSorted.end())
					resultsNotSorted.emplace(url, quantity);
				else
					itrUrl->second += quantity;
			}
		}
		connection.close();
		res = HttpConnection::getSorted(resultsNotSorted);
	}
	catch (const pqxx::sql_error& e) {
		cout << e.what() << endl;
	}
	catch (const runtime_error& e) {
		cout << e.what() << endl;
	}
	catch (...) {
		cout << "General error" << endl;
	}
	return res;
}

void HttpConnection::readRequest() {
	auto self = shared_from_this();

	http::async_read(
		socket_,
		buffer_,
		request_,
		[self](beast::error_code ec,
			size_t bytes_transferred) {
				boost::ignore_unused(bytes_transferred);
				if (!ec)
					self->processRequest();
		});
}

void HttpConnection::processRequest() {
	response_.version(request_.version());
	response_.keep_alive(false);

	switch (request_.method()) {
	case http::verb::get:
		response_.result(http::status::ok);
		response_.set(http::field::server, "Beast");
		createResponseGet();
		break;
	case http::verb::post:
		response_.result(http::status::ok);
		response_.set(http::field::server, "Beast");
		createResponsePost();
		break;

	default:
		response_.result(http::status::bad_request);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body())
			<< "Invalid request-method '"
			<< string(request_.method_string())
			<< "'";
		break;
	}
	writeResponse();
}

HttpConnection::HttpConnection(tcp::socket socket)
	: socket_(move(socket)) {
}

void HttpConnection::start() {
	readRequest();
	checkDeadline();
}


void HttpConnection::createResponseGet() {
	if (request_.target() == "/") {
		response_.set(http::field::content_type, "text/html");
		beast::ostream(response_.body())
			<< "<html>\n"
			<< "<head><meta charset=\"UTF-8\"><title>Search Engine</title></head>\n"
			<< "<body>\n"
			<< "<h1>Search Engine</h1>\n"
			<< "<p>Welcome!<p>\n"
			<< "<form action=\"/\" method=\"post\">\n"
			<< "    <label for=\"search\">Search:</label><br>\n"
			<< "    <input type=\"text\" id=\"search\" name=\"search\"><br>\n"
			<< "    <input type=\"submit\" value=\"Search\">\n"
			<< "</form>\n"
			<< "</body>\n"
			<< "</html>\n";
	}
	else {
		response_.result(http::status::not_found);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body()) << "File not found\r\n";
	}
}

void HttpConnection::createResponsePost() {
	if (request_.target() == "/") {
		string s = buffers_to_string(request_.body().data());

		cout << "POST data: " << s << endl;

		size_t pos = s.find('=');
		if (pos == string::npos) {
			response_.result(http::status::not_found);
			response_.set(http::field::content_type, "text/plain");
			beast::ostream(response_.body()) << "File not found\r\n";
			return;
		}

		string key = s.substr(0, pos);
		string value = s.substr(pos + 1);

		string utf8value = convert_to_utf8(value);

		if (key != "search") {
			response_.result(http::status::not_found);
			response_.set(http::field::content_type, "text/plain");
			beast::ostream(response_.body()) << "File not found\r\n";
			return;
		}

		vector<pair<string, int>> searchResult = HttpConnection::getDatafromDB(s);

		response_.set(http::field::content_type, "text/html");
		beast::ostream(response_.body())
			<< "<html>\n"
			<< "<head><meta charset=\"UTF-8\"><title>Search Engine</title></head>\n"
			<< "<body>\n"
			<< "<h1>Search Engine</h1>\n"
			<< "<p>Response:<p>\n"
			<< "<ul>\n";

		if (searchResult.size() == 0) {
			beast::ostream(response_.body())
				<< "<tt>"
				<< "Words not found! Change request!"
				<< "</tt>";
		}
		else {
			for (const auto& url : searchResult) {

				beast::ostream(response_.body())
					<< "<li><a href=\""
					<< url.first << "\">"
					<< url.first << "</a></li>";
			}
		}

		beast::ostream(response_.body())
			<< "</ul>\n"
			<< "</body>\n"
			<< "</html>\n";
	}
	else {
		response_.result(http::status::not_found);
		response_.set(http::field::content_type, "text/plain");
		beast::ostream(response_.body()) << "File not found\r\n";
	}
}

void HttpConnection::writeResponse() {
	auto self = shared_from_this();

	response_.content_length(response_.body().size());

	http::async_write(
		socket_,
		response_,
		[self](beast::error_code ec, size_t) {
			self->socket_.shutdown(tcp::socket::shutdown_send, ec);
			self->deadline_.cancel();
		});
}

void HttpConnection::checkDeadline() {
	auto self = shared_from_this();

	deadline_.async_wait(
		[self](beast::error_code ec) {
			if (!ec) {
				self->socket_.close(ec);
			}
		});
}

static string url_decode(const string& encoded) {
	string res;
	istringstream iss(encoded);
	char ch;

	while (iss.get(ch)) {
		if (ch == '%') {
			int h;
			iss >> std::hex >> h;
			res += static_cast<char>(h);
		}
		else {
			res += ch;
		}
	}
	return res;
}

static string convert_to_utf8(const string& str) {
	string url_decoded = url_decode(str);
	return url_decoded;
}