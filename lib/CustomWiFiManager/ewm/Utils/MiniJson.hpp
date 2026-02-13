#pragma once
#include <Arduino.h>
#include <vector>

namespace ewm::utils
{
    bool json_get_string(const String& b, const char* key, String& out);
    bool json_get_int(const String& b, const char* key, int& out);
    bool json_get_order_array(const String& b, std::vector<String>& order);
}
