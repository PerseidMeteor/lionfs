#pragma once

#include <stdio.h>
#include <iostream>

int download_file(const char *path, std::string full_path);

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream);