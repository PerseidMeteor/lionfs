#include <cstdio>
#include <string>
#include <cstring>

std::string remove_prefix(const char* input, const std::string& prefix) {
    std::string strInput = input; // 将 char* 转换为 std::string
    // 检查前缀是否存在，并在位置0开始
    if (strInput.find(prefix) == 0) {
        // 移除前缀
        return strInput.substr(prefix.length());
    }
    return strInput; // 如果没有前缀，返回原始字符串
}

bool has_prefix(const char* str, const char* prefix) {
    // 查找最后一个 '/' 字符
    const char* last_slash = strrchr(str, '/');
    if (last_slash == NULL) {
        last_slash = str;  // 如果没有找到 '/', 假设整个字符串就是文件名
    } else {
        last_slash++;  // 移动到 '/' 后面的字符
    }

    // 比较前缀
    return strncmp(last_slash, prefix, strlen(prefix)) == 0;
}

bool has_prefix2(const char* str, const char* prefix) {
    // 直接比较整个字符串的前缀
    return strncmp(str, prefix, strlen(prefix)) == 0;
}