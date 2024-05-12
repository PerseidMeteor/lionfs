#include "config.h"
#include "string"
Config::Config(std::string protocol, std::string address, std::string user_name, std::string password,
               std::string rwdir, std::string image) : protocol_(std::move(protocol)), address_(std::move(address)),
                                                       user_name_(std::move(user_name)), password_(std::move(password)), rw_dir_(std::move(rwdir)),
                                                       image_(std::move(image))
{
}