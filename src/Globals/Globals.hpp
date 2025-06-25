#ifndef SOFTWARE_VERSION
  #define SOFTWARE_VERSION "0.0.0"
#endif

#pragma once

#include <cstdint>
#include <string>
#include "Logger/Logger.hpp"

namespace Globals
{
    enum LogBits : uint32_t
    {
        LOG_HTTP       = 1 << 0,  ///< Bit 0  → Logger::HTTPEnabled
        LOG_TIME       = 1 << 1,  ///< Bit 1  → Logger::TIMEEnabled
        LOG_HSS        = 1 << 2,  ///< Bit 2  → Logger::HSSEnabled
        LOG_DIGIT      = 1 << 3,  ///< Bit 3  → Logger::DIGITEnabled
        LOG_WEBSERVER  = 1 << 4,  ///< Bit 4  → Logger::WebserverEnabled
        LOG_OTA        = 1 << 5,  ///< Bit 5  → Logger::OTAEnabled
        LOG_BUTTON     = 1 << 6,  ///< Bit 6  → Logger::BUTTONEnabled
        LOG_GENERAL    = 1 << 7,  ///< Bit 7  → Logger::GENERALEnabled
        LOG_WIFI       = 1 << 8,  ///< Bit 8  → Logger::WiFiEnabled
    };

    extern uint32_t logConfig;

    extern bool tickerEnabled;

    extern bool timeLimitEnabled;

    extern bool SilentModeEnabled;

    extern bool manualBrightnessEnabled;

    extern bool WeatherUpdateEnabled;

    extern bool PWM_disabled;

    extern bool loadDetected;

    extern std::string zipCode;

    extern std::string HardwareVersion;

    extern std::string SoftwareVersion;

    extern std::string timeLimitFrom; 

    extern std::string timeLimitTo;


    void applyLogConfig();

    enum class FirmwareTarget : uint8_t
    {
        NixieV6_std,
        NixieV6_dev,
        NixieV6_BOS,
        COUNT
    };

    extern FirmwareTarget currentFirmwareTarget;

    std::string getFirmwareUrl(FirmwareTarget target);

    inline void enableAllLogging()
    {
        logConfig = 
            LOG_HTTP |
            LOG_TIME |
            LOG_HSS |
            LOG_DIGIT |
            LOG_WEBSERVER |
            LOG_OTA |
            LOG_BUTTON |
            LOG_GENERAL |
            LOG_WIFI;
        applyLogConfig();
    }

    inline void disableAllLogging()
    {
        logConfig = 0;
        applyLogConfig();
    }
}
