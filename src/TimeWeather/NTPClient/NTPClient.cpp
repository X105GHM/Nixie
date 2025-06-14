#include "NTPClient.hpp"

NTPClient::NTPClient() noexcept {}

void NTPClient::initTime(const std::string &timezone) noexcept
{
    if (WiFi.status() != WL_CONNECTED)
    {
        return;
    }
    configTime(0, 0, "pool.ntp.org");
    struct tm timeInfo;
    if (!getLocalTime(&timeInfo))
    {
        return;
    }
    setenv("TZ", timezone.c_str(), 1);
    tzset();
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
        Logger::log(LoggerType::TIME, "Ungültiges Zeitformat in Globals");
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
