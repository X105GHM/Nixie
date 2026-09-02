#pragma once
#include <string>
#include <vector>

namespace ewm::utils
{
    bool json_get_string(const std::string& b, const char* key, std::string& out);
    bool json_get_int(const std::string& b, const char* key, int& out);
    bool json_get_order_array(const std::string& b, std::vector<std::string>& order);
    std::string json_escape(const std::string& in);
}
