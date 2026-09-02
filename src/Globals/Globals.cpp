#include "Globals.hpp"

#include <utility>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace
{
struct TextConfigStorage
{
    TextConfigStorage() noexcept
        : mutex(xSemaphoreCreateMutexStatic(&mutexStorage))
    {
    }

    StaticSemaphore_t mutexStorage{};
    SemaphoreHandle_t mutex{nullptr};
    Globals::TextConfigSnapshot value{
        "88457",
        "V6.0.1",
        SOFTWARE_VERSION,
        "06:00:00",
        "00:00:00"
    };
};

TextConfigStorage& textConfigStorage() noexcept
{
    static TextConfigStorage storage;
    return storage;
}
}

namespace Globals
{
    std::atomic_bool tickerEnabled{false}; // Mem
    std::atomic_bool timeLimitEnabled{false}; // Mem
    std::atomic_bool SilentModeEnabled{false}; // Mem
    std::atomic_bool manualBrightnessEnabled{false}; // Mem wenn aktiv auch brightness merken
    std::atomic_bool WeatherUpdateEnabled{false}; // Mem
    std::atomic_bool PWM_disabled{false}; // Mem
    std::atomic_bool cricketSoundEnabled{false};
    std::atomic_bool updateAvailable{false};
    std::atomic_bool noACPatNight{false}; // Mem

    std::atomic_bool loadDetected{false};

    TextConfigSnapshot getTextConfig()
    {
        auto& storage = textConfigStorage();
        if (!storage.mutex || xSemaphoreTake(storage.mutex, portMAX_DELAY) != pdTRUE) return {};
        TextConfigSnapshot snapshot = storage.value;
        xSemaphoreGive(storage.mutex);
        return snapshot;
    }

    void setTextConfig(TextConfigSnapshot config)
    {
        auto& storage = textConfigStorage();
        if (!storage.mutex || xSemaphoreTake(storage.mutex, portMAX_DELAY) != pdTRUE) return;
        storage.value = std::move(config);
        xSemaphoreGive(storage.mutex);
    }

    std::atomic_uint8_t brightnessNightStartHour{22};  // Mem
    std::atomic_uint8_t brightnessNightEndHour{6};   // Mem
    std::atomic_uint8_t brightnessDimStartHour{20};  // Mem
    std::atomic_uint8_t brightnessDimEndHour{8};   // Mem

    std::atomic_uint8_t brightnessNightValue{15};  // Mem
    std::atomic_uint8_t brightnessDimValue{75};  // Mem
    std::atomic_uint8_t brightnessDayValue{100}; // Mem

    std::atomic_uint32_t logConfig{ // Mem
        LOG_HTTP        |
        LOG_TIME        |
        LOG_HSS         |
        LOG_DIGIT       |
        LOG_WEBSERVER   |
        LOG_OTA         |
        LOG_BUTTON      |
        LOG_GENERAL     |
        LOG_WIFI};

    void applyLogConfig()
    {
        const uint32_t config = logConfig.load(std::memory_order_relaxed);
        Logger::HTTPEnabled = (config & LOG_HTTP) != 0;

        Logger::TIMEEnabled = (config & LOG_TIME) != 0;

        Logger::HSSEnabled = (config & LOG_HSS) != 0;

        Logger::DIGITEnabled = (config & LOG_DIGIT) != 0;

        Logger::WebserverEnabled = (config & LOG_WEBSERVER) != 0;

        Logger::OTAEnabled = (config & LOG_OTA) != 0;

        Logger::BUTTONEnabled = (config & LOG_BUTTON) != 0;

        Logger::GENERALEnabled = (config & LOG_GENERAL) != 0;

        Logger::WiFiEnabled = (config & LOG_WIFI) != 0;
    }

    std::atomic<FirmwareTarget> currentFirmwareTarget{FirmwareTarget::NixieV6_std}; // Mem

    static constexpr const char *firmwareUrlTable[] = {
        /* NixieV6_std */ "https://raw.githubusercontent.com/X105GHM/Nixie/V.6/bin", 
        /* NixieV6_dev */ "https://raw.githubusercontent.com/X105GHM/Nixie/V.6_dev/bin",
        /* NixieV6_BOS */ "https://update.server.com/nixiev6/branch/V.6_BOS/bin"

    };

    static_assert(
        static_cast<size_t>(Globals::FirmwareTarget::COUNT) == (sizeof(firmwareUrlTable) / sizeof(firmwareUrlTable[0])),
        "Enum FirmwareTarget und firmwareUrlTable müssen dieselbe Länge haben!");

    std::string getFirmwareUrl(FirmwareTarget target)
    {
        size_t idx = static_cast<size_t>(target);
        if (idx >= static_cast<size_t>(FirmwareTarget::COUNT))
        {
            return std::string();
        }
        return std::string(firmwareUrlTable[idx]);
    }

    std::atomic<TimeZone> currentTimeZone{TimeZone::CET};
}
