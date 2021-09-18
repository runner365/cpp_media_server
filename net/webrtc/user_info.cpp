#include "user_info.hpp"


user_info::user_info(const std::string& uid, const std::string& roomId, protoo_request_interface* fb):uid_(uid)
    , roomId_(roomId)
    , feedback_(fb)
{
}

user_info::~user_info()
{
}