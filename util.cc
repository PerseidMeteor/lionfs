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
    return strncmp(str, prefix, strlen(prefix)) == 0;
}
