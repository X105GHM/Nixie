#include "NTPClient.hpp"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace
{
    bool readLocalTime(struct tm &timeInfo, uint32_t timeoutMs) noexcept
    {
        const int64_t startUs = esp_timer_get_time();
        const int64_t timeoutUs = static_cast<int64_t>(timeoutMs) * 1000;

        do
        {
            time_t now = 0;
            time(&now);
            localtime_r(&now, &timeInfo);
            if (timeInfo.tm_year > (2016 - 1900))
            {
                return true;
            }

            if (esp_timer_get_time() - startUs >= timeoutUs)
            {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        while (true);

        return false;
    }
}

NTPClient::NTPClient() noexcept {}

bool NTPClient::initTime(const std::string &timezone, uint32_t timeoutMs) noexcept
{
    applyTimeZone(timezone);

    if (esp_sntp_enabled())
    {
        esp_sntp_stop();
    }
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_setservername(2, "time.cloudflare.com");
    esp_sntp_init();

    struct tm timeInfo {};
    if (!readLocalTime(timeInfo, timeoutMs))
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
    return readLocalTime(timeInfo, 5000);
}

bool NTPClient::isWithinTimeLimit(struct tm &timeInfo) const noexcept
{
    int nowSec = timeInfo.tm_hour * 3600 + timeInfo.tm_min * 60 + timeInfo.tm_sec;
    const auto config = Globals::getTextConfig();

    int fromH, fromM, fromS;
    int toH, toM, toS;

    if (sscanf(config.timeLimitFrom.c_str(), "%d:%d:%d", &fromH, &fromM, &fromS) != 3 ||
        sscanf(config.timeLimitTo.c_str(), "%d:%d:%d", &toH, &toM, &toS) != 3)
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
