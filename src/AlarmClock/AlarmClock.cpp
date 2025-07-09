#include "AlarmClock.hpp"

void AlarmClock::setAlarm(uint8_t hour, uint8_t minute, Callback cb)
{
    time_ = {hour, minute};
    callback_ = cb;
    alarmSet_ = true;
    triggeredToday_ = false;
}

void AlarmClock::removeAlarm()
{
    alarmSet_ = false;
    callback_ = nullptr;
}

void AlarmClock::update(uint8_t currentHour, uint8_t currentMinute)
{
    if (!alarmSet_)
        return;

    if (time_.hour == currentHour && time_.minute == currentMinute)
    {
        if (!triggeredToday_ && callback_)
        {
            callback_();
            triggeredToday_ = true;
        }
    }
    else
    {
        triggeredToday_ = false;
    }
}

bool AlarmClock::isAlarmConfigured() const noexcept
{
    return alarmSet_;
}

AlarmTime AlarmClock::getAlarmTime() const noexcept
{
    return time_;
}
