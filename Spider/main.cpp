#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include "http_utils.h"
#include "indexer.h"
#include "../ConfigParser/config.h"
#include <pqxx/pqxx>
#include <Windows.h>

using namespace std;


bool exitThreadPool = false;
mutex mtx;
condition_variable cv;
queue<function<void()>> tasks;


void threadPoolWorker() {
	unique_lock<mutex> lock(mtx);

	while (!exitThreadPool || !tasks.empty()) {
		if (tasks.empty()) {
			cv.wait(lock);
		}
		else {
			auto task = tasks.front();
			tasks.pop();
			lock.unlock();
			task();
			lock.lock();
		}
	}
}

void parse_link(const Link& link, int depth, const Config& config) {
	try {
		this_thread::sleep_for(chrono::milliseconds(500));
		string html = get_html_content(link);

		if (html.size() == 0) {
			cout << "Failed to get HTML Content" << endl;
			return;
		}
		else {
			Indexer idx;
			idx.parse_words(html);
			idx.parse_links(html, link);
			idx.push_data_db(
				config.getConfig("db_host"),
				config.getConfig("db_port"),
				config.getConfig("db_name"),
				config.getConfig("db_username"),
				config.getConfig("db_user_password"),
				link);

			if (depth > 1) {
				vector<Link> links = idx.get_links();
				lock_guard<mutex> lock(mtx);
				for (const auto& subLink : links) {
					tasks.push([subLink, depth, config]() { parse_link(subLink, depth - 1, config); });
				}
				cv.notify_one();
			}
		}
	}
	catch (const exception& e) {
		cout << e.what() << endl;
	}
}


int main(int argc, char** argv) {
	try {
		SetConsoleCP(CP_UTF8);
		SetConsoleOutputCP(CP_UTF8);

		int numThreads = thread::hardware_concurrency();
		vector<thread> threadPool;
		for (int i = 0; i < numThreads; ++i) {
			threadPool.emplace_back(threadPoolWorker);
		}

		Config config("../Config/config.ini");
		int searching_depth = stoi(config.getConfig("searching_depth"));

		Link link{ ProtocolType::HTTPS, config.getConfig("domain"), config.getConfig("query") };
		{
			lock_guard<mutex> lock(mtx);
			tasks.push([link, searching_depth, config]() { parse_link(link, searching_depth, config); });
			cv.notify_one();
		}

		this_thread::sleep_for(chrono::seconds(2));

		{
			lock_guard<mutex> lock(mtx);
			exitThreadPool = true;
			cv.notify_all();
		}

		for (auto& t : threadPool) {
			t.join();
		}
	}

	catch (const pqxx::sql_error& e) {
    	cout << e.what() << endl;
	}
	catch (const exception& e) {
    	cout << e.what() << endl;
	}
	catch (...) {
    	cout << "General error" << endl;
	}

	return 0;
}
