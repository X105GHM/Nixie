#pragma once

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
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
    WiFi,
    STORAGE,
    SENSOR,
    POWER,
    HISTORY,
    WEATHER,
    SYSTEM,
    AUDIO,
    COUNT
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
    static std::atomic_bool STORAGEEnabled;
    static std::atomic_bool SENSOREnabled;
    static std::atomic_bool POWEREnabled;
    static std::atomic_bool HISTORYEnabled;
    static std::atomic_bool WEATHEREnabled;
    static std::atomic_bool SYSTEMEnabled;
    static std::atomic_bool AUDIOEnabled;

    static void log(LoggerType type, const char *format, ...) noexcept;

    // Synchronizes ESP-IDF tag levels with the application log mask.  Keeping
    // the baseline at NONE means that setting every application category off
    // also silences unclassified component output.
    static void applyPlatformLevels(uint32_t config) noexcept;

private:
    static bool isEnabled(LoggerType type) noexcept;
    static const char *typeName(LoggerType type) noexcept;
};
