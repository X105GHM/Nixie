#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <string>

class NTPClient
{
public:
    explicit NTPClient() noexcept;
    void initTime(const std::string &timezone) noexcept;
    bool getTime(struct tm &timeInfo) const noexcept;
};
