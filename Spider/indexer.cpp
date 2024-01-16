#include "indexer.h"

void Indexer::clear_data(const std::vector<std::string>&& regular_expressions, std::string& data) {
	for (const auto& regex_pattern : regular_expressions) {
		std::regex regex(regex_pattern);
		data = std::regex_replace(data, regex, " ");
	}
}

void Indexer::index_words(const std::string&& regex, const std::string& sentence) {
	std::regex word_regex(regex);

	for (auto itr = std::sregex_iterator(sentence.begin(), sentence.end(), word_regex); itr != std::sregex_iterator(); ++itr) {
		std::string word = (*itr).str();
		std::unordered_map<std::string, unsigned long>::iterator word_value_itr = words.find(word);
		if (word_value_itr == words.end())
			words.emplace(word, 1);
		else
			++word_value_itr->second;
	}
}

bool Indexer::find_protocol(const std::string& ref, const Link& link, ProtocolType& protocol) {
	std::regex http_regex("http:\\s*//");
	std::regex https_regex("https:\\s*//");
	std::regex href_regex("href\\s*=\\s*/");
	std::smatch match;

	if (std::regex_search(ref, match, http_regex)) {
		protocol = ProtocolType::HTTP;

		return true;
	}
	else if (std::regex_search(ref, match, https_regex)) {
		protocol = ProtocolType::HTTPS;

		return true;
	}
	else if (std::regex_search(ref, match, href_regex)) {
		protocol = link.protocol;

		return true;
	}
	else
		return false;
}

bool Indexer::find_domain(const std::string& ref, const Link& link, std::string& domain) {
	std::regex domain_regex("//(.)+?[^/]/");
	std::regex href_regex("href\\s*=\\s*/");
	std::smatch match;

	if (std::regex_search(ref, match, domain_regex)) {
		std::regex slash_regex("/");
		std::string tmp = match[0];
		domain = std::regex_replace(tmp, slash_regex, "");

		return true;
	}
	else if (std::regex_search(ref, match, href_regex)) {
		domain = link.hostName;

		return true;
	}
	else
		return false;
}

bool Indexer::find_query(const std::string& ref, std::string& query) {
	std::regex domain_regex("//(.)+?[^/]/");
	std::regex href_regex("href\\s*=\\s*/");
	std::smatch match;

	if (std::regex_search(ref, match, domain_regex) || std::regex_search(ref, match, href_regex)) {
		query = "/";
		std::string tmp = match.suffix();
		if (!tmp.empty())
			query.append(tmp);

		return true;
	}
	else
		return false;
}

void Indexer::parse_words(const std::string& raw_data) {
	std::string sub_html = raw_data;

	static int debug = 1;
	if (debug) {
		std::ofstream f("debug1.txt");
		f << sub_html;
		f.close();
	}

	Indexer::clear_data
	(
		{
			"<^(script|style|noscript|!--)([\\w\\W]+?)<^/(script|style|noscript|--)>",
			"<^([\\w\\W]*?)>",
			"[^A-Za-z]"
		},
		sub_html
	);

	if (debug) {
		std::ofstream f("debug2.txt");
		f << sub_html;
		f.close();
	}


	std::transform(sub_html.begin(), sub_html.end(), sub_html.begin(), [](unsigned char ch) { return std::tolower(ch); });

	Indexer::index_words
	(
		"[a-z]{3,32}",
		sub_html
	);
}

void Indexer::parse_links(const std::string& raw_data, const Link& parrent_link) {
	std::string sub_html = raw_data;

	std::regex a_tag_regex("<a\\b([^>]+)>(.*?)</a>");

	std::unordered_map<std::string, char> stored_links;

	for (auto itr = std::sregex_iterator(sub_html.begin(), sub_html.end(), a_tag_regex); itr != std::sregex_iterator(); ++itr) {
		std::string a_tag = (*itr).str();

		std::regex href_regex("href.+?[^\"]\"");
		std::smatch match;
		if (std::regex_search(a_tag, match, href_regex)) {
			std::string ref = match[0];

			std::regex quotes_regex("\"");
			ref = std::regex_replace(ref, quotes_regex, "");

			ProtocolType protocol;
			std::string domain;
			std::string query;

			if (Indexer::find_protocol(ref, parrent_link, protocol) && Indexer::find_domain(ref, parrent_link, domain) && Indexer::find_query(ref, query)) {
				std::string link_key = domain + query;

				auto itr_check = stored_links.find(link_key);
				if (itr_check == stored_links.end()) {
					stored_links.insert({ link_key,'y' });
					links.push_back({ protocol, domain, query });
				}
			}
		}
	}
}

void Indexer::push_data_to_db(const std::string& host, const std::string& port, const std::string& dbname, const std::string& user, const std::string& pass, const Link& link) {
	try {
		std::string con_str
		(
			"host=" + host + " "
			"port=" + port + " "
			"dbname=" + dbname + " "
			"user=" + user + " "
			"password=" + pass
		);

		pqxx::connection connection(con_str);
		pqxx::work work{ connection };

		work.exec
		(
			"CREATE TABLE IF NOT EXISTS documents("
			"id SERIAL PRIMARY KEY, "
			"url VARCHAR(2500) NOT NULL UNIQUE);"

			"CREATE TABLE IF NOT EXISTS words("
			"id SERIAL PRIMARY KEY, "
			"word VARCHAR(32) NOT NULL UNIQUE);"

			"CREATE TABLE IF NOT EXISTS documents_words("
			"document_id INTEGER REFERENCES documents(id), "
			"word_id INTEGER REFERENCES words(id), "
			"CONSTRAINT pk PRIMARY KEY(document_id, word_id), "
			"quantity INTEGER NOT NULL);"
		);

		std::string url_str = (static_cast<int>(link.protocol) == 0 ? "http" : "https");
		url_str += "://" + link.hostName + link.query;

		pqxx::result link_id_res = work.exec
		(
			"SELECT id FROM documents "
			"WHERE url = '" + url_str + "'"
		);

		if (link_id_res.size()) {
			std::cout << "DB contains current link " << "\"" << " ID = " << link_id_res[0][0] << std::endl;
			connection.close();

			return;
		}
		else {
			work.exec
			(
				"INSERT INTO documents(url) "
				"VALUES('" + url_str + "')"
			);

			std::cout << "Link \"" << url_str << "\" has been pushed to DB" << std::endl;

			for (std::unordered_map<std::string, unsigned long>::const_iterator con_itr = words.cbegin(); con_itr != words.end(); ++con_itr) {
				std::string word_str = con_itr->first;
				std::string quantity = std::to_string(con_itr->second);

				pqxx::result word_id_res = work.exec
				(
					"SELECT id FROM words "
					"WHERE word = '" + word_str + "'"
				);

				if (word_id_res.size()) {
					std::cout << "DB contains current word " << "\"" << word_str << "\"" << " ID = " << word_id_res[0][0] << std::endl;
				}
				else {
					work.exec
					(
						"INSERT INTO words(word) "
						"VALUES('" + word_str + "');"
					);

					std::cout << "Word " << "\"" << word_str << "\"" << " has been pushed to DB" << std::endl;
				}
				work.exec
				(
					"INSERT INTO documents_words(document_id, word_id, quantity) "
					"VALUES((SELECT id from documents WHERE url = '" + url_str + "'), (SELECT id from words WHERE word = '" + word_str + "'), " + quantity + ");"
				);
			}
		}

		work.commit();
		connection.close();
	}
	catch (pqxx::sql_error& e) {
		std::cout << e.what() << std::endl;
	}
	catch (std::runtime_error& e) {
		std::cout << e.what() << std::endl;
	}
}

std::unordered_map<std::string, unsigned long> Indexer::get_words() {
	return words;
}

std::vector<Link> Indexer::get_links() {
	return links;
}