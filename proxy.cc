#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <iostream>

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

int download_file(const char *path, std::string full_path) {
    std::cout << "[DEBUG] Try to download " << path << std::endl;
    CURL *curl;
    FILE *fp;
    CURLcode res;
    std::string url = "http://127.0.0.1:8099" + std::string(path);
    // std::string full_path = std::string(dir_path) + path;
    
    std::cout << "[DEBUG] url: " << url << std::endl << "full_path:" << full_path << std::endl;

    curl = curl_easy_init();
    if (curl) {
        fp = fopen(full_path.c_str(), "wb");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);
        return 0;
    }
    return -1;
}