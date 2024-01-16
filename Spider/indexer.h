#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <unordered_map>
#include <cctype>
#include <algorithm>
#include "link.h"
#include <pqxx/pqxx>


class Indexer
{
public:
	Indexer() = default;
	
	virtual ~Indexer() = default;

private:
	void clear_data(const std::vector<std::string>&&, std::string&);
	
	void index_words(const std::string&&, const std::string&);
	
	bool find_protocol(const std::string&, const Link&, ProtocolType&);
	
	bool find_domain(const std::string&, const Link&, std::string&);
	
	bool find_query(const std::string&, std::string&);
	
	std::unordered_map<std::string, unsigned long> words;
	std::vector<Link> links;

public:
	void parse_words(const std::string&);
	
	void parse_links(const std::string&, const Link&);
	
	void push_data_to_db(const std::string&, const std::string&, const std::string&, const std::string&, const std::string&, const Link&);
	
	std::unordered_map<std::string, unsigned long> get_words();
	
	std::vector<Link> get_links();
};