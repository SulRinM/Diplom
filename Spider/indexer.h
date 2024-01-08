#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <regex>
#include <unordered_map>
#include <cctype>
#include <algorithm>
#include "link.h"
#include <pqxx/pqxx>

using namespace std;

class Indexer {
public:
	Indexer() = default;
	virtual ~Indexer() = default;
	void parse_words(const string&);
	void parse_links(const string&, const Link&);
	void push_data_db(const string&, const string&, const string&, const string&, const string&, const Link&);
	unordered_map<string, unsigned long> get_words();
	vector<Link> get_links();

private:
	void idx_words(const string&&, const string&);
	bool find_protocol(const string&, const Link&, ProtocolType&);
	bool find_domain(const string&, const Link&, string&);
	bool find_query(const string&, string&);
	void clr_data(const vector<string>&&, string&);
	unordered_map<string, unsigned long> words;
	vector<Link> links;
};