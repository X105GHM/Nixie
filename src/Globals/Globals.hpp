#ifndef SOFTWARE_VERSION
  #define SOFTWARE_VERSION "6.6.9"
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
    
    extern bool cricketSoundEnabled;

    extern bool updateAvailable; 

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
            LOG_HTTP        |
            LOG_TIME        |
            LOG_HSS         |
            LOG_DIGIT       |
            LOG_WEBSERVER   |
            LOG_OTA         |
            LOG_BUTTON      |
            LOG_GENERAL     |
            LOG_WIFI;
        applyLogConfig();
    }

    inline void disableAllLogging()
    {
        logConfig = 0;
        applyLogConfig();
    }

    enum class TimeZone : uint8_t
    {
        CET,   // Mitteleuropa
        EET,   // Osteuropa
        WET,   // Westeuropa
        UTC,   // UTC ohne Sommerzeit
        EST,   // USA Ostküste
        CST,   // USA Mittelwesten
        MST,   // USA Bergland
        PST,   // USA Westküste
        HST,   // Hawaii
        JST,   // Japan
        IST,   // Indien
        AEST,  // Australien Ost
        AWST   // Australien West
    };

    inline const char* getPosixTZ(TimeZone tz) 
    {
        switch (tz) 
        {
            case TimeZone::CET:  return "CET-1CEST,M3.5.0/2,M10.5.0/3";
            case TimeZone::EET:  return "EET-2EEST,M3.5.0/3,M10.5.0/4";
            case TimeZone::WET:  return "WET0WEST,M3.5.0/1,M10.5.0/2";
            case TimeZone::UTC:  return "UTC0";
            case TimeZone::EST:  return "EST5EDT,M3.2.0/2,M11.1.0/2";
            case TimeZone::CST:  return "CST6CDT,M3.2.0/2,M11.1.0/2";
            case TimeZone::MST:  return "MST7MDT,M3.2.0/2,M11.1.0/2";
            case TimeZone::PST:  return "PST8PDT,M3.2.0/2,M11.1.0/2";
            case TimeZone::HST:  return "HST10";
            case TimeZone::JST:  return "JST-9";
            case TimeZone::IST:  return "IST-5:30";
            case TimeZone::AEST: return "AEST-10AEDT,M10.1.0/2,M4.1.0/3";
            case TimeZone::AWST: return "AWST-8";
            default:             return "UTC0";
        }
    }

    extern TimeZone currentTimeZone;
}
