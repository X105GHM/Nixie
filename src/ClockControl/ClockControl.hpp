#pragma once

#include <time.h>
#include "TimeWeather/NTPClient/NTPClient.hpp"
#include "Logger/Logger.hpp"
#include "Digits/Digits.hpp"
#include "ACP/ACP.hpp"
#include "Acoustics/Relay/Relay.hpp"
#include "Hss/HSS.hpp"
#include "Globals/Globals.hpp"
#include "TimeWeather/WeatherClient/WeatherClient.hpp"
#include "Timer/Timer.hpp"
#include "AlarmClock/AlarmClock.hpp"
#include "Acoustics/Buzzer/Buzzer.hpp"
#include "OTA/OTA.hpp"
#include <ctime>

extern Relay relay;
extern Timer timer;
extern AlarmClock alarmClock;
extern Buzzer buzzer;

class NTPClient;
class HSS;

class ClockControl
{
public:

    explicit ClockControl(NTPClient& ntp, HSS& hss) noexcept;

    static void clockTask(void* pvParameters) noexcept;

private:

    NTPClient& ntpClient;
    HSS& hssCtrl;
    std::time_t prevTime{0};
    void timeCycle() noexcept;
};
