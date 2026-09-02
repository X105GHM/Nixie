#pragma once

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include "esp_log.h"

enum class LoggerType
{
    HTTP,
    TIME,
    HSS,
    DIGIT,
    Webserver,
    OTA,
    BUTTON,
    GENERAL,
    COUNT,
    WiFi
};

class Logger
{
public:
    static void begin() noexcept;

    static std::atomic_bool HTTPEnabled;
    static std::atomic_bool TIMEEnabled;
    static std::atomic_bool HSSEnabled;
    static std::atomic_bool DIGITEnabled;
    static std::atomic_bool WebserverEnabled;
    static std::atomic_bool OTAEnabled;
    static std::atomic_bool BUTTONEnabled;
    static std::atomic_bool GENERALEnabled;
    static std::atomic_bool WiFiEnabled;

    static void log(LoggerType type, const char *format, ...) noexcept;

private:
    static bool isEnabled(LoggerType type) noexcept;
    static const char *typeName(LoggerType type) noexcept;
};
