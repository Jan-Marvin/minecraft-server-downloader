#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <chrono>

#include <curl/curl.h>
#include <openssl/sha.h>
#include <openssl/crypto.h>

#include "minecraft-server-downloader.h"

std::chrono::system_clock::time_point start_timer;

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
	if (start_timer.time_since_epoch().count() == 0) {
		start_timer = std::chrono::system_clock::now();
	}
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
	std::chrono::duration<float, std::milli> diff = std::chrono::system_clock::now() - start_timer;
	float speed = (dlnow / 1e+6) / (diff.count() / 1000);
	printf("\r[%s]%s%.2f/%.2f MB   %.2f MB/s     ", bar.c_str(), space.c_str(), ((float)dlnow / 1e+6), ((float)dltotal / 1e+6), speed);
	return CURLE_OK;
}

//write jar
void get_file(std::string url, std::string file_name, bool verbose_logging) {
	CURL* curl;
	FILE* p_file;
	CURLcode res;
	curl = curl_easy_init();
	if (curl) {
		p_file = fopen(file_name.c_str(), "wb");
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_file_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, p_file);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		if (verbose_logging) {
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
std::string get_data(std::string url, bool verbose_logging) {
	std::string result;
	CURL* curl;
	CURLcode res;
	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data_string_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
		if (verbose_logging) {
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
void in_use(std::string file_name) {
	if (std::filesystem::exists(file_name)) {
		int result = rename(file_name.c_str(), file_name.c_str());
		if (result != 0) {
			std::cerr << "ERROR: Cannot write to " + file_name + " is it in use?\n";
			if (win) {
				system("pause");
			}
			std::exit(1);
		}
	}
}

//calculate sha1
std::string calc_hash(std::string file_name) {
	//open file
	std::ifstream infile(file_name, std::ifstream::binary);

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
void compare_hash(std::string sha_str, bool first, std::string file_name) {
	if (first) {
		if (sha_str == calc_hash(file_name)) {
			std::cout << file_name + " is up to date\n";
			if (win) {
				system("pause");
			}
			std::exit(0);
		}
	}
	else {
		if (sha_str != calc_hash(file_name)) {
			std::cout << "ERROR: Wrong checksum, please try again\n";
			if (win) {
				system("pause");
			}
			std::exit(1);
		}
	}
}

std::string between(std::string begin, std::string end, std::string data, bool use_rfind_begin, bool use_rfind_end) {
	std::size_t found;
	if (use_rfind_begin) {
		found = data.rfind(begin);
	}
	else {
		found = data.find(begin);
	}
	data.erase(0, found + begin.size());

	if (use_rfind_end) {
		found = data.rfind(end);
	}
	else {
		found = data.find(end);
	}
	data.erase(found);

	return data;
}

int main(int argc, char* argv[]) {
	std::string version = "2.40";
	std::cout << "Minecraft server downloader, 2nd Edition Ver. " + version + "\n\n";
	bool curl_verbose_logging = false;
	std::string file_name = "server.jar";

	if (argc > 1) {
		for (int i = 0; i < argc; i++) {
			if (argv[i] == std::string("-h") || argv[i] == std::string("--help")) {
				std::cout << "Usage: minecraft-server-downloader [-h] [-l] [-o <name>] [-v] \n\n"
					<< "Options:\n"
					<< "-h, --help 		Print this help\n"
					<< "-l			Enable curl verbose information\n"
					<< "-o			Set output name\n"
					<< "-v, -V, --version	Print version\n";
				return 1;
			}
			if (argv[i] == std::string("-v") || argv[i] == std::string("-V") || argv[i] == std::string("--version")) {
				std::cout << "minecraft-server-downloader/" + version + "\n"
					<< "Build with:\n"
					<< curl_version() << ", "
					<< OPENSSL_VERSION_TEXT;
				return 1;
			}
			if (argv[i] == std::string("-l")) {
				curl_verbose_logging = true;
				continue;
			}
			if (argv[i] == std::string("-o")) {
				file_name = argv[i + 1];
				continue;
			}
		}
	}

	in_use(file_name);

	std::string json_url = between("\"type\": \"release\", \"url\": \"", "\", \"time\":", get_data("https://launchermeta.mojang.com/mc/game/version_manifest.json", curl_verbose_logging), 0, 0);

	std::string mc_version = between("/", ".json", json_url, 1, 0);
	std::cout << "Version: " + mc_version << "\n";

	std::string sha1_server = between("\"server\": {\"sha1\": \"", "\",", get_data(json_url, curl_verbose_logging), 0, 0);
	compare_hash(sha1_server, 1, file_name);

	std::string server_url = between("\"server\": {\"sha1\": \"", "server_mappings", get_data(json_url, curl_verbose_logging), 0, 0);
	server_url = between("\"url\": \"", "\"}", server_url, 0, 0);
	std::cout << "URL: " + server_url << "\n";

	std::cout << "Downloading...\n";
	get_file(server_url, file_name, curl_verbose_logging);
	compare_hash(sha1_server, 0, file_name);

	return 0;
}
