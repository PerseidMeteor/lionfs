#pragma once

#include <stdio.h>
#include <iostream>

// @brief download file from remote server
// @param path file path, like "a.txt"
// @param full_path real path for file storage, like "/home/yq/a.txt"
int download_file(const char *path, const char* full_path);

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream);