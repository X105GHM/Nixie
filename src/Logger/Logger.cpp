#include "Logger.hpp"

std::atomic_bool Logger::HTTPEnabled{true};
std::atomic_bool Logger::TIMEEnabled{true};
std::atomic_bool Logger::HSSEnabled{true};
std::atomic_bool Logger::DIGITEnabled{true};
std::atomic_bool Logger::WebserverEnabled{true};
std::atomic_bool Logger::OTAEnabled{true};
std::atomic_bool Logger::BUTTONEnabled{true};
std::atomic_bool Logger::GENERALEnabled{true};
std::atomic_bool Logger::WiFiEnabled{true};

void Logger::begin() noexcept
{
    static constexpr const char *tags[] = {
        "HTTP", "TIME", "HSS", "DIGIT", "Web",
        "OTA", "BUTTON", "GEN", "WiFi", "EWM"
    };

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
    default:
        return false;
    }
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
