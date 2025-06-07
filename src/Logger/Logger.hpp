#pragma once

#include <Arduino.h>
#include <cstdarg>
#include <cstdio>
#include <ctime>

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
    static void begin(Stream &output);

    static bool HTTPEnabled;
    static bool TIMEEnabled;
    static bool HSSEnabled;
    static bool DIGITEnabled;
    static bool WebserverEnabled;
    static bool OTAEnabled;
    static bool BUTTONEnabled;
    static bool GENERALEnabled;
    static bool WiFiEnabled;

    static void log(LoggerType type, const __FlashStringHelper *message) noexcept;
    static void log(LoggerType type, const char *format, ...) noexcept;

private:
    static Stream *out_;
    static const __FlashStringHelper *typeName(LoggerType type) noexcept;
    static void printTimestamp() noexcept;
};
