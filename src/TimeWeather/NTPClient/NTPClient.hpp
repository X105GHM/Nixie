#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <string>

#include "Globals/Globals.hpp"

class NTPClient
{
public:
    explicit NTPClient() noexcept;

    void applyTimeZone(const std::string &timezone) noexcept;
    bool initTime(const std::string &timezone, uint32_t timeoutMs = 15000) noexcept;

    bool getTime(struct tm &timeInfo) const noexcept;
    bool isWithinTimeLimit(struct tm &timeInfo) const noexcept;
};
