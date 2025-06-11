#include "Globals.hpp"

namespace Globals
{   
    bool tickerEnabled              = false; //Mem
    bool timeLimitEnabled           = false; //Mem
    bool SilentlModeEnabled         = false; //Mem
    bool manualBrightnessEnabled    = false; //Mem
    bool WeatherUpdateEnabled       = false; //Mem

    bool loadDetected               = false;

    std::string zipCode = "88457";           //Mem

    std::string HardwareVersion = "V6.0.1";  //Mem

    std::string SoftwareVersion = SOFTWARE_VERSION;

    uint32_t logConfig =                     //Mem
        LOG_HTTP      |
        LOG_TIME      |
        LOG_HSS       |
        LOG_DIGIT     |
        LOG_WEBSERVER |
        LOG_OTA       |
        LOG_BUTTON    |
        LOG_GENERAL   |
        LOG_WIFI;

    void applyLogConfig()
    {
        Logger::HTTPEnabled      = (logConfig & LOG_HTTP)      != 0;
   
        Logger::TIMEEnabled      = (logConfig & LOG_TIME)      != 0;
  
        Logger::HSSEnabled       = (logConfig & LOG_HSS)       != 0;
 
        Logger::DIGITEnabled     = (logConfig & LOG_DIGIT)     != 0;

        Logger::WebserverEnabled = (logConfig & LOG_WEBSERVER) != 0;

        Logger::OTAEnabled       = (logConfig & LOG_OTA)       != 0;

        Logger::BUTTONEnabled    = (logConfig & LOG_BUTTON)    != 0;

        Logger::GENERALEnabled   = (logConfig & LOG_GENERAL)   != 0;

        Logger::WiFiEnabled      = (logConfig & LOG_WIFI)      != 0;
    }

    FirmwareTarget currentFirmwareTarget = FirmwareTarget::NixieV6_std; //Mem

    static constexpr const char* firmwareUrlTable[] = {
        /* NixieV6_std */ "https://update.server.com/nixiev6/standard/firmware.bin",
        /* NixieV6_dev */ "https://update.server.com/nixiev6/development/firmware.bin",
        /* NixieV6_BOS */ "https://update.server.com/nixiev6/branch/BOS/firmware.bin"

    };

    static_assert(
        static_cast<size_t>(Globals::FirmwareTarget::COUNT) == (sizeof(firmwareUrlTable) / sizeof(firmwareUrlTable[0])),
        "Enum FirmwareTarget und firmwareUrlTable müssen dieselbe Länge haben!"
    );

    std::string getFirmwareUrl(FirmwareTarget target)
    {
        size_t idx = static_cast<size_t>(target);
        if (idx >= static_cast<size_t>(FirmwareTarget::COUNT)) {
            return std::string();
        }
        return std::string(firmwareUrlTable[idx]);
    }

    static constexpr const char* manifestUrlTable[] = {
        /* NixieV6_std */ "https://update.server.com/nixiev6/standard/manifest.json",
        /* NixieV6_dev */ "https://update.server.com/nixiev6/development/manifest.json",
        /* NixieV6_BOS */ "https://update.server.com/nixiev6/branch/BOS/manifest.json"
    };

    static_assert(
        static_cast<size_t>(Globals::FirmwareTarget::COUNT) == (sizeof(manifestUrlTable) / sizeof(manifestUrlTable[0])),
        "Enum FirmwareTarget und manifestUrlTable müssen dieselbe Länge haben!"
    );

    std::string getManifestUrl(FirmwareTarget target)
    {
        size_t idx = static_cast<size_t>(target);
        if (idx >= static_cast<size_t>(FirmwareTarget::COUNT)) {
            return std::string();
        }
        return std::string(manifestUrlTable[idx]);
    }
}