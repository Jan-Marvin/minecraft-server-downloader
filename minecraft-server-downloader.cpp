#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <curl/curl.h>
#include <filesystem>

int lenght = INT_MAX;

size_t write_data(void* ptr, size_t size, size_t nmemb, FILE* stream) {
	size_t written = fwrite(ptr, size, nmemb, stream);
	return written;
}

static size_t my_write(void* buffer, size_t size, size_t nmemb, void* param) {
	std::string& text = *static_cast<std::string*>(param);
	size_t totalsize = size * nmemb;
	text.append(static_cast<char*>(buffer), totalsize);
	return totalsize;
}

static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
	std::string header = buffer;
	std::size_t found = header.find("Content-Length: ");
	if (found != std::string::npos) {
		header.erase(0, found + 16);
		lenght = stoi(header);
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

int get_file(std::string url, boolean verbose) {
	CURL* curl;
	FILE* fp;
	CURLcode res;
	char outfilename[FILENAME_MAX] = "server.jar";
	curl = curl_easy_init();
	if (curl) {
		fp = fopen(outfilename, "wb");
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
		curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress_callback);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		if (verbose) {
			FILE* filep = fopen("curl.log", "wb");
			curl_easy_setopt(curl, CURLOPT_STDERR, filep);
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		}
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		fclose(fp);
		if (res != 0) {
			std::cerr << std::endl << "[get_file]CURL ERROR: " << res << std::endl;
			system("pause");
			abort();
		}
	}
	curl_global_cleanup();
	return 0;
}

std::string get_data(std::string url, boolean verbose) {
	std::string result;
	CURL* curl;
	CURLcode res;
	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();
	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, my_write);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
		if (verbose) {
			FILE* filep = fopen("curl.log", "wb");
			curl_easy_setopt(curl, CURLOPT_STDERR, filep);
			curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		}
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
		if (res != 0) {
			std::cerr << "[get_data]CURL ERROR: " << res << std::endl;
			system("pause");
			abort();
		}
	}
	curl_global_cleanup();
	return result;
}

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

int main(int argc, char* argv[]) {
	std::cout << "Minecraft server downloader 2nd Edition Ver. 2.34" << std::endl;
	boolean verbose = false;
	//verbose
	if (argc > 1) {
		if (argv[1] == std::string("-v")) {
			verbose = true;
		}
	}

	if (in_use()) {
		std::cerr << "ERROR: Cannot write to server.jar is it in use?" << std::endl;
		system("pause");
		return 1;
	}

	//jar url
	std::string data = get_data("https://www.minecraft.net/en-us/download/server", verbose);
	std::string data_url = data;
	std::size_t found = data_url.find(".jar");
	data_url.erase(found + 4);
	found = data_url.find("<a href=\"https://");
	data_url.erase(0, found + 9);
	std::cout << "Server jar url: " + data_url << std::endl;

	//version
	std::string data_version = data;
	found = data_version.find(".jar</a>");
	data_version.erase(found);
	found = data_version.find("minecraft_server");
	data_version.erase(0, found + 17);

	std::cout << "Version: " + data_version << std::endl;
	std::cout << "Download..." << std::endl;
	get_file(data_url, verbose);

	if (std::filesystem::file_size("server.jar") != lenght) {
		std::cerr << "ERROR: Wrong file size" << std::endl;
		system("pause");
		return 1;
	}
	else if (lenght == INT_MAX) {
		std::cerr << "ERROR: Could not get remote server.jar file size" << std::endl;
		system("pause");
		return 1;
	}
	return 0;
}
