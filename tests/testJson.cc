#include "../include/rapidjson/document.h"
#include "../include/rapidjson/writer.h"
#include "../include/rapidjson/stringbuffer.h"
#include "../include/rapidjson/filereadstream.h"

void test_rapid_json()
{
    // 从镜像的JSON元信息中读取文件信息，并放至内存中
    FILE* fp = fopen("../files.json", "r"); // 非 Windows 平台使用 "r"
 
    char readBuffer[65536];
    rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));
    
    rapidjson::Document d;
    d.ParseStream(is);

    // const rapidjson::Value& a = d["a"];
    assert(d.IsArray());
    for (rapidjson::SizeType i = 0; i < d.Size(); i++) {
        if (d[i].HasMember("path") && d[i]["path"].IsString()) {
            printf("a[%u] = %s\n", i, d[i]["path"].GetString());
        }
    }
}

int main() {
    test_rapid_json();

    return 0;
}