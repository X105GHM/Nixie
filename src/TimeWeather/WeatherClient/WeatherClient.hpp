#pragma once

#include "Logger/Logger.hpp"
#include <string>
#include "esp_http_client.h"
#include "cJSON.h"
#include <cstdlib>
#include <cstring>
#include <limits>


class WeatherClient
{
public:
    WeatherClient(const std::string &apiKey);
    float getTemperatureByZip(const std::string &zip) const;

private:
    std::string apiKey_;
};

