#pragma once

#include <stdio.h>
#include <iostream>
#include <sys/stat.h>

// @brief download file from remote server
// @param path file path, like "a.txt"
// @param full_path real path for file storage, like "/home/yq/a.txt"
int download_file(const std::string& host, const std::string& image, const char *path, const char* full_path);

int download_file(const std::string& host, const std::string& image, const std::string& path, const std::string& full_path);

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream);

int get_stat_from_server(const std::string& host, const std::string& image, const char *path, struct stat *stbuf);