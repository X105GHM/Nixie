#include "NTPClient.hpp"

NTPClient::NTPClient() noexcept {}

bool NTPClient::initTime(const std::string &timezone, uint32_t timeoutMs) noexcept
{
    applyTimeZone(timezone);

    if (WiFi.status() != WL_CONNECTED)
    {
        Logger::log(LoggerType::TIME, "NTP skipped: WiFi not connected, TZ is still applied");
        return false;
    }

    configTzTime(
        timezone.c_str(),
        "pool.ntp.org",
        "time.google.com",
        "time.cloudflare.com"
    );

    struct tm timeInfo {};
    if (!getLocalTime(&timeInfo, timeoutMs))
    {
        Logger::log(LoggerType::TIME, "NTP sync failed / timeout");
        return false;
    }

    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &timeInfo);
    Logger::log(LoggerType::TIME, "NTP synced: %s", buf);

    return true;
}

void NTPClient::applyTimeZone(const std::string &timezone) noexcept
{
    const char* tz = timezone.empty() ? "UTC0" : timezone.c_str();

    setenv("TZ", tz, 1);
    tzset();

    Logger::log(LoggerType::TIME, "Timezone applied: %s", tz);
}

bool NTPClient::getTime(struct tm &timeInfo) const noexcept
{
    return getLocalTime(&timeInfo);
}

bool NTPClient::isWithinTimeLimit(struct tm &timeInfo) const noexcept
{
    int nowSec = timeInfo.tm_hour * 3600 + timeInfo.tm_min * 60 + timeInfo.tm_sec;

    int fromH, fromM, fromS;
    int toH, toM, toS;

    if (sscanf(Globals::timeLimitFrom.c_str(), "%d:%d:%d", &fromH, &fromM, &fromS) != 3 ||
        sscanf(Globals::timeLimitTo.c_str(), "%d:%d:%d", &toH, &toM, &toS) != 3)
    {
        Logger::log(LoggerType::TIME, "Invalid time format in Globals");
        return true;
    }

    int fromSec = fromH * 3600 + fromM * 60 + fromS;
    int toSec   = toH * 3600 + toM * 60 + toS;

    if (fromSec <= toSec)
    {
        return nowSec >= fromSec && nowSec <= toSec;
    }
    else
    {
        return nowSec >= fromSec || nowSec <= toSec;
    }
}
