#include "Logger.hpp"

Stream *Logger::out_ = &Serial;

bool Logger::HTTPEnabled = true;
bool Logger::TIMEEnabled = true;
bool Logger::HSSEnabled = true;
bool Logger::DIGITEnabled = true;
bool Logger::WebserverEnabled = true;
bool Logger::OTAEnabled = true;
bool Logger::BUTTONEnabled = true;
bool Logger::GENERALEnabled = true;
bool Logger::WiFiEnabled = true;

void Logger::begin(Stream &output)
{
    out_ = &output;
}

const __FlashStringHelper *Logger::typeName(LoggerType type) noexcept
{
    switch (type)
    {
    case LoggerType::HTTP:
        return F("HTTP");
    case LoggerType::TIME:
        return F("TIME");
    case LoggerType::HSS:
        return F("HSS");
    case LoggerType::DIGIT:
        return F("DIGIT");
    case LoggerType::Webserver:
        return F("Web");
    case LoggerType::OTA:
        return F("OTA");
    case LoggerType::BUTTON:
        return F("BUTTON");
    case LoggerType::GENERAL:
        return F("GEN");
    case LoggerType::WiFi:
        return F("WiFi");
    default:
        return F("UNK");
    }
}

void Logger::printTimestamp() noexcept
{
    std::time_t t = std::time(nullptr);
    struct tm tmInfo;
    if (localtime_r(&t, &tmInfo))
    {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tmInfo.tm_hour, tmInfo.tm_min, tmInfo.tm_sec);
        out_->print(buf);
    }
    else
    {
        unsigned long sec = millis() / 1000;
        out_->print(sec);
        out_->print(F("s"));
    }
    out_->print(F(" "));
}

void Logger::log(LoggerType type, const __FlashStringHelper *message) noexcept
{
    bool enabled = false;
    switch (type)
    {
    case LoggerType::HTTP:
        enabled = HTTPEnabled;
        break;
    case LoggerType::TIME:
        enabled = TIMEEnabled;
        break;
    case LoggerType::HSS:
        enabled = HSSEnabled;
        break;
    case LoggerType::DIGIT:
        enabled = DIGITEnabled;
        break;
    case LoggerType::Webserver:
        enabled = WebserverEnabled;
        break;
    case LoggerType::OTA:
        enabled = OTAEnabled;
        break;
    case LoggerType::BUTTON:
        enabled = BUTTONEnabled;
        break;
    case LoggerType::GENERAL:
        enabled = GENERALEnabled;
        break;
    case LoggerType::WiFi:
        enabled = WiFiEnabled;
        break;
    default:
        enabled = false;
        break;
    }
    if (!enabled)
        return;

    printTimestamp();
    out_->print(F("["));
    out_->print(typeName(type));
    out_->print(F("] "));
    out_->println(message);
}

void Logger::log(LoggerType type, const char *format, ...) noexcept
{
    bool enabled = false;
    switch (type)
    {
    case LoggerType::HTTP:
        enabled = HTTPEnabled;
        break;
    case LoggerType::TIME:
        enabled = TIMEEnabled;
        break;
    case LoggerType::HSS:
        enabled = HSSEnabled;
        break;
    case LoggerType::DIGIT:
        enabled = DIGITEnabled;
        break;
    case LoggerType::Webserver:
        enabled = WebserverEnabled;
        break;
    case LoggerType::OTA:
        enabled = OTAEnabled;
        break;
    case LoggerType::BUTTON:
        enabled = BUTTONEnabled;
        break;
    case LoggerType::GENERAL:
        enabled = GENERALEnabled;
        break;
    case LoggerType::WiFi:
        enabled = WiFiEnabled;
        break;
    default:
        enabled = false;
        break;
    }
    if (!enabled)
        return;

    char buffer[128];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    printTimestamp();
    out_->print(F("["));
    out_->print(typeName(type));
    out_->print(F("] "));
    out_->println(buffer);
}