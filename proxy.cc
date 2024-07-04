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
#include "regex"

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written = fwrite(ptr, size, nmemb, stream);
    return written;
}

size_t write_callback(void *ptr, size_t size, size_t nmemb, std::string *data) {
    data->append((char*)ptr, size * nmemb);
    return size * nmemb;
}

size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata) {
    std::string header(buffer, size * nitems);
    // printf("[header_callback] header is %s\n", header.c_str());
    std::regex mode_regex("File-Mode:\\s*(\\d+)");
    std::smatch match;
    if (std::regex_search(header, match, mode_regex) && match.size() > 1) {
        long *mode = static_cast<long*>(userdata);
        *mode = std::strtol(match[1].str().c_str(), nullptr, 8); // Convert string to octal number
    }
    return nitems * size;
}

int download_file(const std::string& host, const std::string& image, const char *path, const char* full_path) {
    std::cout << "[download_file] Try to download " << path << std::endl;
    CURL *curl;
    FILE *fp;
    CURLcode res;
    long http_code = 0;
    long fileMode = 0;  // This will hold the file mode
    std::string url = host + "/file" + std::string(path);
    std::string temp_full_path = std::string(full_path) + ".tmp";

    curl = curl_easy_init();
    if (curl) {
        fp = fopen(temp_full_path.c_str(), "wb");
        if (!fp) {
            perror("Failed to open file");
            curl_easy_cleanup(curl);
            return -1;
        }
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);

        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &fileMode);

        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Authorization: Basic YWRtaW46MTIzNDU2");
        headers = curl_slist_append(headers, "Content-Type: application/json");
        char header[100];
        sprintf(header, "Image: %s", image.c_str());
        headers = curl_slist_append(headers, header);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code); // 获取HTTP响应代码

        if (res == CURLE_OK) {
            fclose(fp);  // Close the file before checking the file mode or renaming
            if (http_code == 404) {
                printf("[download_file] file not found %s", path);
                remove(temp_full_path.c_str());  // Remove temp file
            }else if(http_code > 500 && http_code < 600) {
                printf("[download_file] server internal error for file %s", path);
                remove(temp_full_path.c_str());  // Remove temp file
            }else {
                if (fileMode) {// set file mode
                    chmod(temp_full_path.c_str(), fileMode);  // Set file permissions
                }
                struct stat buffer;
                if (stat(full_path, &buffer) != 0) { // Check if the original file does not exist
                    rename(temp_full_path.c_str(), full_path);  // Rename temp file to official file name
                } else {
                    remove(temp_full_path.c_str());  // Remove temp file, keep existing file
                }
            }
        } else {
            std::cerr << "[download_file] Curl failed with error: " << curl_easy_strerror(res) << std::endl;
            fclose(fp);  // Close the file on error
            remove(temp_full_path.c_str());  // Delete the temp file if the download failed
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(headers);
        return (res == CURLE_OK) ? 0 : -1;
    }
    std::cerr << "[download_file] Curl initialization failed." << std::endl;
    return -1;
}

// int download_file(const std::string& host, const std::string& image, const char *path, const char* full_path) {
//     std::cout << "[DEBUG] Try to download " << path << std::endl;
//     CURL *curl;
//     FILE *fp;
//     CURLcode res;
//     long fileMode = 0;
//     std::string url = host + "/file" + std::string(path);
    
//     curl = curl_easy_init();
//     if (curl) {
//         fp = fopen(full_path, "wb");
//         if (!fp) {
//             perror("Failed to open file");
//             curl_easy_cleanup(curl);
//             return -1;
//         }
//         curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
//         curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
//         curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
//         curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
//         curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);  

//         struct curl_slist* headers = NULL;
//         headers = curl_slist_append(headers, "Authorization: Basic YWRtaW46MTIzNDU2");
//         headers = curl_slist_append(headers, "Content-Type: application/json");
//         char header[100];
//         sprintf(header, "Image: %s", image.c_str());
//         headers = curl_slist_append(headers, header);
//         curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

//         res = curl_easy_perform(curl);
//         if (res == CURLE_OK) {
//             curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &fileMode);
//             if (fileMode) {
//                 std::cout << "[INFO] file mode" << fileMode << std::endl;    
//                 // chmod(full_path, fileMode); // Set file permissions
//             }
//             std::cout << "[INFO] Download successful, permissions set." << std::endl;
//         } else {
//             std::cerr << "[ERROR] Curl failed with error: " << curl_easy_strerror(res) << std::endl;
//             remove(full_path);  // Delete the file if the download failed
//         }

//         curl_easy_cleanup(curl);
//         fclose(fp);
//         return (res == CURLE_OK) ? 0 : -1;
//     }
//     std::cerr << "[ERROR] Curl initialization failed." << std::endl;
//     return -1;
// }

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
    return download_file(host, image, path.c_str(), full_path.c_str());
}