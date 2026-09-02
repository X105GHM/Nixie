#pragma once

#include <string>

class WeatherClient
{
public:
    WeatherClient(const std::string &apiKey);
    float getTemperatureByZip(const std::string &zip) const;

private:
    std::string apiKey_;
};

