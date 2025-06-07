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
