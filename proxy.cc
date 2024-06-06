#include "proxy.h"
#include "include/rapidjson/document.h"
#include <cerrno>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <curl/curl.h>
#include <sys/stat.h>
#include <iostream>

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

size_t write_callback(void *ptr, size_t size, size_t nmemb, std::string *data) {
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
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

int get_stat_from_server(const std::string& host, const std::string& image, const char *path, struct stat *stbuf){
    CURL *curl;
    CURLcode res;
    std::string url = host + "/st" + std::string(path);
    std::string readBuffer;
    
    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
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
        long http_code = 0;

        if (res != CURLE_OK) {
            // printf("[DEBUG] Curl failed with error: %s ", curl_easy_strerror(res));
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            // printf("http code %ld", http_code);
            if (http_code == 404) {
                return -ENOENT;
            }
            else if (http_code == 500) {
                return -EIO;
            }
            return -1;
        } else {
            
            // printf("[DEBUG] fetch stat info successfully %s", readBuffer.c_str());
            // status ok
            rapidjson::Document document;
            document.Parse(readBuffer.c_str());
            if (document.HasParseError()) {
                return -EIO;
            } else {
                stbuf->st_mode = document["st_mode"].GetInt();
                stbuf->st_nlink = document["st_nlink"].GetInt();
                stbuf->st_size = document["st_size"].GetInt();
                stbuf->st_uid = document["st_uid"].GetInt();
                stbuf->st_gid = document["st_gid"].GetInt();
                stbuf->st_atime = document["st_atime"].GetInt();
                stbuf->st_mtime = document["st_mtime"].GetInt();
                stbuf->st_ctime = document["st_ctime"].GetInt();
                stbuf->st_blocks = document["st_blocks"].GetInt();
                stbuf->st_blksize = document["st_blksize"].GetInt();
                stbuf->st_dev = document["st_dev"].GetInt64();
                stbuf->st_ino = document["st_ino"].GetInt64();
            }
            return 0;
        }
        return 0;
    }
    // std::cerr << "[ERROR] Curl initialization failed." << std::endl;
    return -EIO;
}

int download_file(const std::string& host, const std::string& image, const std::string& path, const std::string& full_path){
    std::cout << host << image << path << full_path;
    return download_file(host, image, path.c_str(), full_path.c_str());
}