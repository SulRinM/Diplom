#include "config.h"


void Config::parse(const string& filename) {
	ifstream file(filename);

	if (!file.is_open()) {
		throw runtime_error("File open error.");
	}

	string tmp;
	while (!file.eof()) {
		getline(file, tmp);

		size_t eqPos = tmp.find("=");
		if (eqPos != string::npos) {
			string key = tmp.substr(0, eqPos);
			string value = tmp.substr(eqPos + 1, string::npos);

			if (key == "url") {
				size_t pos = value.find("/");

				if (pos == string::npos)
					throw runtime_error("Wrong URL.");

				Config::configs.insert(make_pair<string, string>("domain", value.substr(0, pos)));
				Config::configs.insert(make_pair<string, string>("query", value.substr(pos, string::npos)));
			}
			else
				Config::configs.insert(make_pair<string, string>(move(key), move(value)));
		}
	}

	file.close();
}

string Config::getConfig(const string& key) const {
	if (Config::configs.empty()) {
		throw runtime_error("Error. Empty config file.");
	}

	return Config::configs.at(key);
}

Config::Config(const string& filename) {
	Config::parse(filename);
}

