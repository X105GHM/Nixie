#include "ClockControl.hpp"

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
            Logger::log(LoggerType::TIME, F("Fehler: Zeit nicht verfügbar"));
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        std::time_t nowTime = std::mktime(&timeInfo);
        if (nowTime != prevTime)
        {
            prevTime = nowTime;

            StatsMonitor &sm = StatsMonitor::instance();

            sm.update();

            static bool lastState160 = false;

            static bool inTime = true; 

            bool currentState160 = displayEnabled && Globals::loadDetected && inTime;
            if (currentState160 != lastState160)
            {
                if (currentState160)
                {
                    hssCtrl.enable160();
                }
                else
                {
                    hssCtrl.disable160();
                }
                lastState160 = currentState160;
            }

            if (Globals::timeLimitEnabled)
            {
                time_t now = time(nullptr);
                struct tm timeInfo;
                localtime_r(&now, &timeInfo);

                if (!ntpClient.isWithinTimeLimit(timeInfo))
                {
                    Logger::log(LoggerType::TIME, "Time outside allowed range (%s - %s)", Globals::timeLimitFrom.c_str(), Globals::timeLimitTo.c_str());
                    vTaskDelay(pdMS_TO_TICKS(200));
                    inTime = false;
                    continue;
                }

                inTime = true;
            }

            if (Globals::tickerEnabled)
            {
                relay.toggle();
            }

            if ((timeInfo.tm_hour < 6 || timeInfo.tm_hour >= 22) && !Globals::manualBrightnessEnabled)
            {
                brightness = 10;
            }
            else if ((timeInfo.tm_hour < 8 || timeInfo.tm_hour >= 20) && !Globals::manualBrightnessEnabled)
            {
                brightness = 75;
            }
            else if (!Globals::manualBrightnessEnabled)
            {
                brightness = 100;
            }

            if (timeInfo.tm_min % 10 == 9 && timeInfo.tm_sec >= 50 && timeInfo.tm_sec < 55 && displayEnabled && Globals::loadDetected)
            {
                Logger::log(LoggerType::TIME, F("Displaying Date"));
                displayDate();
                vTaskDelay(pdMS_TO_TICKS(5000));
            }
            else if (((timeInfo.tm_min == 57 && timeInfo.tm_sec == 15) || (timeInfo.tm_min == 27 && timeInfo.tm_sec == 15)) && displayEnabled && Globals::loadDetected)
            {
                Logger::log(LoggerType::TIME, F("Running ACP"));
                hssCtrl.enable190();
                vTaskDelay(pdMS_TO_TICKS(10));
                hssCtrl.enableResistorReduction();
                vTaskDelay(pdMS_TO_TICKS(10));
                ACP();
                hssCtrl.disableResistorReduction();
                hssCtrl.disable190();
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            else if ((timeInfo.tm_min % 10 == 9 && timeInfo.tm_sec >= 0 && timeInfo.tm_sec < 5) && displayEnabled && Globals::WeatherUpdateEnabled)
            {
                WeatherClient weather(std::string(OPENWEATHER_API_KEY));

                zipMaskingEnabled = true;
                digits = Globals::zipCode.empty() ? 0 : std::stoi(Globals::zipCode)*10;
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
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}