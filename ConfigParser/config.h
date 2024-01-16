#pragma once

#include <string>
#include <unordered_map>
#include <fstream>
#include <exception>
#include <utility>

using namespace std;

extern const string STR_CONFIG;

class Config {
public:
	Config() {};
	Config(const string&);
	string getConfig(const string&) const;
	virtual ~Config() = default;
private:
	unordered_map<std::string, string> configs;
	void parse(const string&);
};