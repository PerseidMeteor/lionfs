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

int download_file(const std::string& host, const std::string& image, const char *path, const char* full_path) {
    std::cout << "[DEBUG] Try to download " << path << std::endl;
    CURL *curl;
    FILE *fp;
    CURLcode res;
    std::string url = host + "/file" + std::string(path);
    
    std::cout << "[DEBUG] url: " << url << std::endl << "full_path:" << full_path << std::endl;

    curl = curl_easy_init();
    if (curl) {
        fp = fopen(full_path, "wb");
        if (!fp) {
            perror("Failed to open file");
            curl_easy_cleanup(curl);
            return -1;
        }
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);  // 设置10秒超时
        // 让libcurl在HTTP >= 400时报错,
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L); 

        // 添加镜像名称的header
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Authorization: Basic YWRtaW46MTIzNDU2");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        char header[100];
        sprintf(header, "Image: %s", image.c_str());
        headers = curl_slist_append(headers, header);

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        fclose(fp);
        if (res != CURLE_OK) {
            std::cerr << "[ERROR] Curl failed with error: " << curl_easy_strerror(res) << std::endl;
            remove(full_path);  // Delete the file if the download failed
            return -1; // Download failure
        }
        std::cout << "[INFO] Download successful." << std::endl;
        return 0;
    }
    std::cerr << "[ERROR] Curl initialization failed." << std::endl;
    return -1;
}