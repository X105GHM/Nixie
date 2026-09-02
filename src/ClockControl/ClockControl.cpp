#include "ClockControl.hpp"
#include "Config/Secrets.hpp"

ClockControl::ClockControl(NTPClient &ntp, HSS &hss) noexcept
    : ntpClient(ntp),
      hssCtrl(hss),
      prevTime(0)
{
}

void ClockControl::clockTask(void *pvParameters) noexcept
{
    auto *instance = static_cast<ClockControl *>(pvParameters);
    instance->timeCycle();
    vTaskDelete(nullptr);
}

void ClockControl::timeCycle() noexcept
{
    struct tm timeInfo{};

    for (;;)
    {
        if (!ntpClient.getTime(timeInfo))
        {
            Logger::log(LoggerType::TIME, "Fehler: Zeit nicht verfügbar");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        std::time_t nowTime = std::mktime(&timeInfo);
        if (nowTime != prevTime)
        {
            prevTime = nowTime;
            const auto textConfig = Globals::getTextConfig();

            static int currentHour = -1;
            static int cricketTriggerMinute1 = -1;
            static int cricketTriggerMinute2 = -1;
            static int lastCricketMinute = -1;

            static int lastUpdateCheckYear  = -1;
            static int lastUpdateCheckMonth = -1;
            static int lastUpdateCheckDay   = -1;

            if (timeInfo.tm_hour != currentHour && Globals::cricketSoundEnabled) 
            {
                currentHour = timeInfo.tm_hour;

                cricketTriggerMinute1 = esp_random() % 60;
                cricketTriggerMinute2 = (esp_random() % 2 == 0) ? esp_random() % 60 : -1;
            }

            if (timeInfo.tm_hour == 0 && timeInfo.tm_min == 0 && timeInfo.tm_sec >= 3 && timeInfo.tm_sec <= 8
                && !(lastUpdateCheckYear  == timeInfo.tm_year && lastUpdateCheckMonth == timeInfo.tm_mon && lastUpdateCheckDay == timeInfo.tm_mday))
            {
                lastUpdateCheckYear  = timeInfo.tm_year;
                lastUpdateCheckMonth = timeInfo.tm_mon;
                lastUpdateCheckDay   = timeInfo.tm_mday;

                Logger::log(LoggerType::OTA, "Daily OTA manifest check triggered");

                const std::string baseUrl = Globals::getFirmwareUrl(Globals::currentFirmwareTarget);
                Globals::updateAvailable = OTAManager::instance().checkForUpdateAvailable(baseUrl);

                Logger::log(LoggerType::OTA,"Daily OTA check finished, updateAvailable=%s", Globals::updateAvailable ? "true" : "false");
            }

            static bool lastState160 = false;

            static bool inTime = true; 

            bool currentState160 = displayEnabled && Globals::loadDetected && inTime;
            if (currentState160 != lastState160)
            {
                if (currentState160)
                {
                    (void)hssCtrl.enable160();
                }
                else
                {
                    (void)hssCtrl.disable160();
                }
                lastState160 = currentState160;
            }

            if (Globals::cricketSoundEnabled && (timeInfo.tm_min == cricketTriggerMinute1 || 
                timeInfo.tm_min == cricketTriggerMinute2) && timeInfo.tm_sec == 0 && timeInfo.tm_min != lastCricketMinute)
            {
                lastCricketMinute = timeInfo.tm_min;
                Logger::log(LoggerType::TIME, "Cricket chirping triggered");
                buzzer.startCricketInTask();
            }

            if (Globals::timeLimitEnabled)
            {
                time_t now = time(nullptr);
                struct tm timeInfo;
                localtime_r(&now, &timeInfo);

                if (!ntpClient.isWithinTimeLimit(timeInfo))
                {
                    Logger::log(LoggerType::TIME, "Time outside allowed range (%s - %s)",
                                textConfig.timeLimitFrom.c_str(), textConfig.timeLimitTo.c_str());
                    vTaskDelay(pdMS_TO_TICKS(200));
                    inTime = false;
                    continue;
                }

                inTime = true;
            }
            else
            {
                inTime = true;
            }

            if (Globals::tickerEnabled && displayEnabled && Globals::loadDetected)
            {
                relay.toggle();
            }

            if (!Globals::manualBrightnessEnabled)
            {
                const uint8_t hour = static_cast<uint8_t>(timeInfo.tm_hour);

                const bool isNight = (hour < Globals::brightnessNightEndHour) || (hour >= Globals::brightnessNightStartHour);

                const bool isDim = (hour < Globals::brightnessDimEndHour) || (hour >= Globals::brightnessDimStartHour);

                if (isNight)
                {
                    brightness = Globals::brightnessNightValue;
                }
                else if (isDim)
                {
                    brightness = Globals::brightnessDimValue;
                }
                else
                {
                    brightness = Globals::brightnessDayValue;
                }
            }

            const bool isNightTime = (timeInfo.tm_hour < Globals::brightnessNightEndHour) || (timeInfo.tm_hour >= Globals::brightnessNightStartHour);

            if (timeInfo.tm_min % 10 == 9 && timeInfo.tm_sec >= 50 && timeInfo.tm_sec < 55 && displayEnabled && Globals::loadDetected)
            {
                Logger::log(LoggerType::TIME, "Displaying Date");
                displayDate();
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
            else if (((timeInfo.tm_min == 57 && timeInfo.tm_sec == 15) || (timeInfo.tm_min == 27 && timeInfo.tm_sec == 15)) && 
                        displayEnabled && Globals::loadDetected && !(Globals::noACPatNight && isNightTime))
            {
                Logger::log(LoggerType::TIME, "Running ACP");
                digits = 0;
                (void)hssCtrl.enable190();
                vTaskDelay(pdMS_TO_TICKS(10));
                (void)hssCtrl.enableResistorReduction();
                vTaskDelay(pdMS_TO_TICKS(10));
                ACP();
                (void)hssCtrl.disableResistorReduction();
                (void)hssCtrl.disable190();
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            else if ((timeInfo.tm_min % 10 == 9 && timeInfo.tm_sec >= 0 && timeInfo.tm_sec < 5) && displayEnabled && 
                        Globals::WeatherUpdateEnabled && Globals::loadDetected)
            {
                WeatherClient weather(std::string(OPENWEATHER_API_KEY));

                zipMaskingEnabled = true;
                digits = textConfig.zipCode.empty() ? 0 : std::stoi(textConfig.zipCode) * 10;
                vTaskDelay(pdMS_TO_TICKS(4000));
                displayWeather();
                tempMaskingEnabled = true;
                zipMaskingEnabled = false;
                vTaskDelay(pdMS_TO_TICKS(6000));
                tempMaskingEnabled = false;
            }
            else
            {
                displayTime();
            }

            alarmClock.update(timeInfo.tm_hour, timeInfo.tm_min);

            timer.update();

            buzzer.update();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
