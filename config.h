#pragma once

#include <string>

class Config
{
public:
    std::string protocol_;
    std::string address_;
    std::string user_name_;
    std::string password_;
    std::string rw_dir_;
    std::string image_;

public:
    Config(std::string protocol, std::string address, std::string user_name, std::string password,
           std::string rwdir, std::string image);

    Config() = delete;
    Config(const Config &c) = delete;
};