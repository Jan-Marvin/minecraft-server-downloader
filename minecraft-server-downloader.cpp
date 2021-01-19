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

//space betwenn bar and number
int getspace(int dltotal, int dlnow) {
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
void get_file(std::string url, bool verbose) {
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
		std::cout << "\n";
		if (res != 0) {
			std::cerr << "[get_file]CURL ERROR: " << res << "\n";
			if (win) {
				system("pause");
			}
			std::exit(1);
		}
	}
	curl_global_cleanup();
}

//get data
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
			std::cerr << "[get_data]CURL ERROR: " << res << "\n";
			if (win) {
				system("pause");
			}
			std::exit(1);
		}
	}
	return result;
}

//is jar writable
void in_use() {
	if (std::filesystem::exists("server.jar")) {
		int result = rename("server.jar", "server.jar");
		if (result != 0) {
			std::cerr << "ERROR: Cannot write to server.jar is it in use?\n";
			if (win) {
				system("pause");
			}
			std::exit(1);
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
		std::string file_sha1;
		unsigned char hash[SHA_DIGEST_LENGTH];
		SHA1((unsigned char*)buffer, length, hash);
		file_sha1.assign((char*)&hash[0], sizeof(hash));
		delete[] buffer;

		//to hex
		static const char hex_digits[] = "0123456789abcdef";
		std::string file_sha1_fnal;
		file_sha1_fnal.reserve(file_sha1.length() * 2);
		for (unsigned char c : file_sha1) {
			file_sha1_fnal.push_back(hex_digits[c >> 4]);
			file_sha1_fnal.push_back(hex_digits[c & 15]);
		};
		return file_sha1_fnal;
	}
	else {
		return "";
	}
}

//compare sha1 
void compare_hash(std::string sha_str, bool first) {
	if (first) {
		if (sha_str == calc_hash()) {
			std::cout << "server.jar is up to date\n";
			if (win) {
				system("pause");
			}
			std::exit(1);
		}
	}
	else {
		if (sha_str != calc_hash()) {
			std::cout << "ERROR: Wrong checksum, please try again\n";
			if (win) {
				system("pause");
			}
			std::exit(1);
		}
	}
}

int main(int argc, char* argv[]) {
	std::cout << "Minecraft server downloader 2nd Edition Ver. 2.39\n\n";

	bool verbose = false;
	if (argc > 1) {
		//verbose
		if (argv[1] == std::string("-l")) {
			verbose = true;
		}
		//version 
		else if (argv[1] == std::string("-v")) {
			std::cout << curl_version() << "\n";
			std::cout << SSLeay_version(SSLEAY_VERSION);;
			return 1;
		}
	}

	in_use();

	std::string data = get_data("https://launchermeta.mojang.com/mc/game/version_manifest.json", verbose);
	std::size_t found = data.find("\"type\": \"release\", \"url\": ");
	if (found == std::string::npos) {
		std::cout << "ERROR: With version_manifest.json\n";
		if (win) {
			system("pause");
		}
		return 1;
	}
	data.erase(0, found + 27);
	found = data.find(".json");
	data.erase(found + 5);

	std::string mc_version = data;
	found = mc_version.rfind("/");
	mc_version = mc_version.erase(0, found + 1).erase(mc_version.size() - 5);
	std::cout << "Version: " + mc_version << "\n";

	data = get_data(data, verbose);
	found = data.find("\"server\":");
	data.erase(0, found);

	std::string sha1_server = data;
	found = sha1_server.find("sha1");
	sha1_server = sha1_server.erase(0, found + 8).erase(40);
	compare_hash(sha1_server, 1);

	found = data.find("url");
	data.erase(0, found + 7);
	found = data.find(".jar");
	data.erase(found + 4);
	std::cout << "URL: " + data << "\n";

	std::cout << "Download...\n";
	get_file(data, verbose);
	compare_hash(sha1_server, 0);

	return 0;

}
