#include "Logger.hpp"

#include <cstdint>

std::atomic_bool Logger::HTTPEnabled{true};
std::atomic_bool Logger::TIMEEnabled{true};
std::atomic_bool Logger::HSSEnabled{true};
std::atomic_bool Logger::DIGITEnabled{true};
std::atomic_bool Logger::WebserverEnabled{true};
std::atomic_bool Logger::OTAEnabled{true};
std::atomic_bool Logger::BUTTONEnabled{true};
std::atomic_bool Logger::GENERALEnabled{true};
std::atomic_bool Logger::WiFiEnabled{true};
std::atomic_bool Logger::STORAGEEnabled{true};
std::atomic_bool Logger::SENSOREnabled{true};
std::atomic_bool Logger::POWEREnabled{true};
std::atomic_bool Logger::HISTORYEnabled{true};
std::atomic_bool Logger::WEATHEREnabled{true};
std::atomic_bool Logger::SYSTEMEnabled{true};
std::atomic_bool Logger::AUDIOEnabled{true};

void Logger::begin() noexcept
{
    static constexpr const char *tags[] = {
        "HTTP", "TIME", "HSS", "DIGIT", "Web",
        "OTA", "BUTTON", "GEN", "WiFi", "Storage", "Sensor",
        "Power", "History", "Weather", "System", "Audio", "EWM"
    };

    // Start with the legacy-safe baseline until persisted settings are loaded.
    esp_log_level_set("*", ESP_LOG_ERROR);
    for (const char *tag : tags)
    {
        esp_log_level_set(tag, ESP_LOG_INFO);
    }
}

const char *Logger::typeName(LoggerType type) noexcept
{
    switch (type)
    {
    case LoggerType::HTTP:
        return "HTTP";
    case LoggerType::TIME:
        return "TIME";
    case LoggerType::HSS:
        return "HSS";
    case LoggerType::DIGIT:
        return "DIGIT";
    case LoggerType::Webserver:
        return "Web";
    case LoggerType::OTA:
        return "OTA";
    case LoggerType::BUTTON:
        return "BUTTON";
    case LoggerType::GENERAL:
        return "GEN";
    case LoggerType::WiFi:
        return "WiFi";
    case LoggerType::STORAGE:
        return "Storage";
    case LoggerType::SENSOR:
        return "Sensor";
    case LoggerType::POWER:
        return "Power";
    case LoggerType::HISTORY:
        return "History";
    case LoggerType::WEATHER:
        return "Weather";
    case LoggerType::SYSTEM:
        return "System";
    case LoggerType::AUDIO:
        return "Audio";
    default:
        return "UNK";
    }
}

bool Logger::isEnabled(LoggerType type) noexcept
{
    switch (type)
    {
    case LoggerType::HTTP:
        return HTTPEnabled;
    case LoggerType::TIME:
        return TIMEEnabled;
    case LoggerType::HSS:
        return HSSEnabled;
    case LoggerType::DIGIT:
        return DIGITEnabled;
    case LoggerType::Webserver:
        return WebserverEnabled;
    case LoggerType::OTA:
        return OTAEnabled;
    case LoggerType::BUTTON:
        return BUTTONEnabled;
    case LoggerType::GENERAL:
        return GENERALEnabled;
    case LoggerType::WiFi:
        return WiFiEnabled;
    case LoggerType::STORAGE:
        return STORAGEEnabled;
    case LoggerType::SENSOR:
        return SENSOREnabled;
    case LoggerType::POWER:
        return POWEREnabled;
    case LoggerType::HISTORY:
        return HISTORYEnabled;
    case LoggerType::WEATHER:
        return WEATHEREnabled;
    case LoggerType::SYSTEM:
        return SYSTEMEnabled;
    case LoggerType::AUDIO:
        return AUDIOEnabled;
    default:
        return false;
    }
}

void Logger::applyPlatformLevels(uint32_t config) noexcept
{
    // No category selected means no application or component log output.
    // Category tags are explicitly re-enabled below when selected.
    esp_log_level_set("*", ESP_LOG_NONE);

    const auto enable = [config](uint32_t bit, const char *tag)
    {
        if ((config & bit) != 0U)
            esp_log_level_set(tag, ESP_LOG_INFO);
    };

    enable(1U << 0, "HTTP");
    enable(1U << 1, "TIME");
    enable(1U << 2, "HSS");
    enable(1U << 3, "DIGIT");
    enable(1U << 4, "Web");
    enable(1U << 5, "OTA");
    enable(1U << 6, "BUTTON");
    enable(1U << 7, "GEN");
    enable(1U << 8, "WiFi");
    enable(1U << 9, "Storage");
    enable(1U << 10, "Sensor");
    enable(1U << 11, "Power");
    enable(1U << 12, "History");
    enable(1U << 13, "Weather");
    enable(1U << 14, "System");
    enable(1U << 15, "Audio");

    // These tags are used by shared ESP-IDF clients.  They are only visible
    // when the corresponding application category is enabled.
    if ((config & ((1U << 0) | (1U << 5) | (1U << 13))) != 0U)
        esp_log_level_set("HTTP_CLIENT", ESP_LOG_WARN);
    if ((config & (1U << 8)) != 0U)
        esp_log_level_set("EWM", ESP_LOG_INFO);
}

void Logger::log(LoggerType type, const char *format, ...) noexcept
{
    if (!format || !isEnabled(type))
    {
        return;
    }

    char buffer[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    ESP_LOGI(typeName(type), "%s", buffer);
}
