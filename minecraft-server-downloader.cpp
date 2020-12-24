#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <curl/curl.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>
#include "minecraft-server-downloader.h"

//write data to file
size_t write_data_file_callback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
	size_t written = fwrite(ptr, size, nmemb, stream);
	return written;
}

//write data to string
static size_t write_data_string_callback(void* buffer, size_t size, size_t nmemb, std::string* param) {
	std::string& text = *static_cast<std::string*>(param);
	size_t totalsize = size * nmemb;
	text.append(static_cast<char*>(buffer), totalsize);
	return totalsize;
}

//header data
static size_t header_callback(char* buffer, size_t size, size_t nitems, int* p_lenght) {
	std::string header = buffer;
	std::size_t found = header.find("Content-Length: ");
	if (found != std::string::npos) {
		header.erase(0, found + 16);
		*p_lenght = stoi(header);
	}
	return nitems * size;
}

//space betwenn bar and number
int getspace(long long dltotal, long long dlnow) {
	int nr1 = 0;
	int nr2 = 0;
	bool lock = false;
	while (dltotal != 0) {
		dltotal = dltotal / 10;
		nr1++;
		if (dltotal == 0 && !lock) {
			lock = true;
			dltotal = dlnow;
			nr2 = nr1;
			nr1 = 0;
		}
	}
	if (nr2 - nr1 == 8) {
		return nr2 - nr1 - 4;
	}
	return nr2 - nr1;
}

//progress bar
int progress_callback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
	std::string bar;
	int nrspace = getspace(dltotal, dlnow);
	for (int i = 0; i < (10.0 / dltotal) * dlnow; i++) {
		bar = bar + "#";
	}
	bar.append((10 - bar.length()), '-');
	std::string space;
	if (nrspace == 0) {
		space = "  ";
	}
	else {
		for (int i = 0; i < nrspace + 2; i++) {
			space.append(" ");
		}
	}
	printf("\r[%s]%s%lld/%lld", bar.c_str(), space.c_str(), dlnow / 1000, dltotal / 1000);
	return CURLE_OK;
}

//write jar
int get_file(std::string url, bool verbose, int* p_lenght) {
	CURL* curl;
	FILE* p_file;
	CURLcode res;
	char outfilename[FILENAME_MAX] = "server.jar";
	curl = curl_easy_init();
	if (curl) {
		p_file = fopen(outfilename, "wb");
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_file_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, p_file);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
		curl_easy_setopt(curl, CURLOPT_HEADERDATA, p_lenght);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		if (verbose) {
			FILE* p_log = fopen("curl-get_file.log", "wb");
			curl_easy_setopt(curl, CURLOPT_STDERR, p_log);
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		}
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		fclose(p_file);
		std::cout << std::endl;
		if (res != 0) {
			std::cerr << "[get_file]CURL ERROR: " << res << std::endl;
			if (win) {
				system("pause");
			}
		}
	}
	curl_global_cleanup();
	return res;
}

//get website
std::string get_data(std::string url, bool verbose) {
	std::string result;
	CURL* curl;
	CURLcode res;
	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_string_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
		if (verbose) {
			FILE* p_log = fopen("curl-get_data.log", "wb");
			curl_easy_setopt(curl, CURLOPT_STDERR, p_log);
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		}
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		if (res != 0) {
			std::cerr << "[get_data]CURL ERROR: " << res << std::endl;
			if (win) {
				system("pause");
			}
		}
	}
	return result;
}

//is jar writable
bool in_use() {
	if (!std::filesystem::exists("server.jar")) {
		return 0;
	}
	else {
		int result = rename("server.jar", "server.jar");
		if (result == 0) {
			return 0;
		}
		else {
			return 1;
		}
	}
}

//calculate sha1
std::string calc_hash() {
	//open file
	std::ifstream infile("server.jar", std::ifstream::binary);

	if (infile) {
		//get length of file:
		infile.seekg(0, infile.end);
		int length = infile.tellg();
		infile.seekg(0, infile.beg);
		char* buffer = new char[length];

		//read data as a block:
		infile.read(buffer, length);
		infile.close();

		//sha1
		std::string s1;
		unsigned char hash[SHA_DIGEST_LENGTH];
		SHA1((unsigned char*)buffer, length, hash);
		s1.assign((char*)&hash[0], sizeof(hash));
		delete[] buffer;

		//to hex
		static const char hex_digits[] = "0123456789ABCDEF";
		std::string s2;
		s2.reserve(s1.length() * 2);
		for (unsigned char c : s1) {
			s2.push_back(hex_digits[c >> 4]);
			s2.push_back(hex_digits[c & 15]);
		};
		return s2;
	}
	else {
		return "";
	}
}

//compare sha1 
//1=ok 0=nok
bool compare_hash(std::string strsha) {
	//hash to uppercase
	std::for_each(strsha.begin(), strsha.end(), [](char& c) {
		c = ::toupper(c);
		});
	//compare hash
	if (strsha == calc_hash()) {
		return 1;
	}
	else {
		return 0;
	}
}

int main(int argc, char* argv[]) {
	std::cout << "Minecraft server downloader 2nd Edition Ver. 2.38\n" << std::endl;
	int lenght = INT_MAX;
	bool verbose = false;
	if (argc > 1) {
		//verbose
		if (argv[1] == std::string("-l")) {
			verbose = true;
		}
		//version 
		else if (argv[1] == std::string("-v")) {
			std::cout << curl_version() << std::endl;
			std::cout << SSLeay_version(SSLEAY_VERSION);;
			return 1;
		}
	}

	if (in_use()) {
		std::cerr << "ERROR: Cannot write to server.jar is it in use?" << std::endl;
		if (win) {
			system("pause");
		}
		return 1;
	}

	//jar url
	std::string data = get_data("https://www.minecraft.net/en-us/download/server", verbose);
	std::string data_url = data;
	std::size_t found = data_url.find(".jar");
	if (found == std::string::npos) {
		return 1;
	}
	data_url.erase(found + 4);
	found = data_url.find("<a href=\"https://launcher.mojang.com");
	data_url.erase(0, found + 9);

	//filter check
	if (data_url.length() != 90 && data_url.substr(data_url.length() - 4) == ".jar") {
		std::cout << "ERROR: Wrong Server jar url, filter needs to be updated" << std::endl;
		if (win) {
			system("pause");
		}
		return 1;
	}

	std::cout << "server.jar url: " + data_url << std::endl;

	//hash
	found = data_url.find("objects/");
	std::string sHash = data_url.substr(found + 8, 40);

	if (compare_hash(sHash)) {
		std::cout << "server.jar is up to date" << std::endl;
		if (win) {
			system("pause");
		}
		return 1;
	}

	//minecraft version
	std::string data_version = data;
	found = data_version.find(".jar</a>");
	data_version.erase(found);
	found = data_version.find("minecraft_server");
	data_version.erase(0, found + 17);

	std::cout << "Version: " + data_version << std::endl;
	std::cout << "Download..." << std::endl;

	if (get_file(data_url, verbose, &lenght) != 0) {
		return 1;
	}

	//size check
	if (std::filesystem::file_size("server.jar") != lenght) {
		std::cerr << "ERROR: Wrong file size" << std::endl;
		if (win) {
			system("pause");
		}
		return 1;
	}
	else if (lenght == INT_MAX) {
		std::cerr << "ERROR: Could not get remote server.jar file size" << std::endl;
		if (win) {
			system("pause");
		}
		return 1;
	}
	//hash check
	else if (!compare_hash(sHash)) {
		std::cerr << "ERROR: Wrong checksum" << std::endl;
		if (win) {
			system("pause");
		}
		return 1;
	}
	return 0;
}
