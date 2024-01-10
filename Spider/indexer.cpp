#include "indexer.h"


void Indexer::idx_words(const string&& reg,
	const string& sentence) {
	regex word_regex(reg);

	for (auto itr =
		sregex_iterator(sentence.begin(), sentence.end(), word_regex);
		itr != sregex_iterator(); ++itr) {
		string word = (*itr).str();
		unordered_map<string, unsigned long>::iterator word_value_itr =
			words.find(word); 
		if (word_value_itr == words.end())
			words.emplace(word, 1);
		else
			++word_value_itr->second;
	}
}

bool Indexer::find_protocol(const string& ref, const Link& link,
	ProtocolType& protocol) {
	regex http_regex("http:\\s*//");
	regex https_regex("https:\\s*//");
	regex href_regex("href\\s*=\\s*/");
	smatch match;

	if (regex_search(ref, match, http_regex)) {
		protocol = ProtocolType::HTTP;

		return true;
	}
	else if (regex_search(ref, match, https_regex)) {
		protocol = ProtocolType::HTTPS;

		return true;
	}
	else if (regex_search(ref, match, href_regex)) {
		protocol = link.protocol;

		return true;
	}
	else
		return false;
}

bool Indexer::find_domain(const string& ref, const Link& link,
	string& domain) {
	regex domain_regex("//(.)+?[^/]/");
	regex href_regex("href\\s*=\\s*/");
	smatch match;

	if (regex_search(ref, match, domain_regex)) {
		regex slash_regex("/");
		string tmp = match[0];
		domain = regex_replace(tmp, slash_regex, "");

		return true;
	}
	else if (regex_search(ref, match, href_regex)) {
		domain = link.hostName;

		return true;
	}
	else
		return false;
}

bool Indexer::find_query(const string& ref, string& query) {
	regex domain_regex("//(.)+?[^/]/");
	regex href_regex("href\\s*=\\s*/");
	smatch match;

	if (regex_search(ref, match, domain_regex) ||
		regex_search(ref, match, href_regex)) {
		query = "/";
		string tmp = match.suffix();
		if (!tmp.empty())
			query.append(tmp);

		return true;
	}
	else
		return false;
}

void Indexer::parse_words(const string& raw_data) {
	string sub_html = raw_data;

	Indexer::clr_data(
		{
			"<(script|style|noscript|!--)([\\w\\W]+?)</"
			"(script|style|noscript|--)>",
			"<([\\w\\W]*?)>",
			"[^A-Za-z]"
		},
		sub_html);

	transform(sub_html.begin(), sub_html.end(), sub_html.begin(),
		[](unsigned char ch) { return tolower(ch); });

	Indexer::idx_words(
		"[a-z]{3,32}",
		sub_html);
}

void Indexer::parse_links(const string& raw_data,
	const Link& parrent_link) {
	string sub_html = raw_data;

	regex a_tag_regex("<a\\b([^>]+)>(.*?)</a>");

	unordered_map<string, char>
		stored_links;

	for (auto itr =
		sregex_iterator(sub_html.begin(), sub_html.end(), a_tag_regex);
		itr != sregex_iterator(); ++itr) {
		string a_tag = (*itr).str();

		regex href_regex(
			"href.+?[^\"]\"");
		smatch match;
		if (regex_search(a_tag, match, href_regex)) {
			string ref = match[0];

			regex quotes_regex("\"");
			ref = regex_replace(ref, quotes_regex, "");

			ProtocolType protocol;
			string domain;
			string query;

			if (Indexer::find_protocol(ref, parrent_link, protocol) &&
				Indexer::find_domain(ref, parrent_link, domain) &&
				Indexer::find_query(ref, query)) {
				string link_key = domain + query;

				auto itr_check = stored_links.find(link_key);
				if (itr_check == stored_links.end()) {
					stored_links.insert({ link_key, 'y' });
					links.push_back({ protocol, domain, query });
				}
			}
		}
	}
}

void Indexer::push_data_db(const string& host, const string& port,
	const string& dbname,
	const string& user, const string& pass,
	const Link& link) {
	try {
		string con_str("host=" + host +
			" "
			"port=" +
			port +
			" "
			"dbname=" +
			dbname +
			" "
			"user=" +
			user +
			" "
			"password=" +
			pass);

		pqxx::connection connection(con_str);
		pqxx::work work{ connection };

		work.exec("CREATE TABLE IF NOT EXISTS documents("
			"id SERIAL PRIMARY KEY, "
			"url VARCHAR(2500) NOT NULL UNIQUE);"

			"CREATE TABLE IF NOT EXISTS words("
			"id SERIAL PRIMARY KEY, "
			"word VARCHAR(32) NOT NULL UNIQUE);"

			"CREATE TABLE IF NOT EXISTS documents_words("
			"document_id INTEGER REFERENCES documents(id), "
			"word_id INTEGER REFERENCES words(id), "
			"CONSTRAINT pk PRIMARY KEY(document_id, word_id), "
			"quantity INTEGER NOT NULL);");

		string url_str =
			(static_cast<int>(link.protocol) == 0 ? "http" : "https");
		url_str += "://" + link.hostName + link.query;

		pqxx::result link_id_res = work.exec("SELECT id FROM documents "
			"WHERE url = '" +
			url_str + "'");

		if (link_id_res.size()) {
			cout << "DB contains current link "
				<< "\""
				<< " ID = " << link_id_res[0][0] << endl;
			connection.close();

			return;
		}
		else {
			work.exec("INSERT INTO documents(url) "
				"VALUES('" +
				url_str + "')");

			cout << "Link \"" << url_str << "\" has been pushed to DB"
				<< endl;

			for (unordered_map<string, unsigned long>::const_iterator
				con_itr = words.cbegin();
				con_itr != words.end(); ++con_itr) {
				string word_str = con_itr->first;
				string quantity = to_string(con_itr->second);

				pqxx::result word_id_res = work.exec("SELECT id FROM words "
					"WHERE word = '" +
					word_str + "'");

				if (word_id_res.size()) {
					cout << "DB contains current word "
						<< "\"" << word_str << "\""
						<< " ID = " << word_id_res[0][0] << endl;
				}
				else {
					work.exec("INSERT INTO words(word) "
						"VALUES('" +
						word_str + "');");

					cout << "Word "
						<< "\"" << word_str << "\""
						<< " has been pushed to DB" << endl;
				}
				work.exec("INSERT INTO documents_words(document_id, word_id, quantity) "
					"VALUES((SELECT id from documents WHERE url = '" +
					url_str + "'), (SELECT id from words WHERE word = '" +
					word_str + "'), " + quantity + ");");
			}
		}
		work.commit();
		connection.close();
	}
	catch (pqxx::sql_error& e) {
		cout << e.what() << endl;
	}
	catch (runtime_error& e) {
		cout << e.what() << endl;
	}
}

void Indexer::clr_data(const vector<string>&& regular_expressions,
	string& data) {
	for (const auto& regex_pattern : regular_expressions) {
		regex regex(regex_pattern);
		data = regex_replace(data, regex, " ");
	}
}

unordered_map<string, unsigned long> Indexer::get_words() {
	return words;
}

vector<Link> Indexer::get_links() { return links; }