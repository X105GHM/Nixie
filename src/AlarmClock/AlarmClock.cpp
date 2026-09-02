#include "AlarmClock.hpp"

#include <utility>

AlarmClock::AlarmClock() noexcept : mutex_(xSemaphoreCreateMutexStatic(&mutexStorage_)) {}

void AlarmClock::setAlarm(uint8_t hour, uint8_t minute, Callback cb)
{
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return;
    time_ = {hour, minute};
    callback_ = std::move(cb);
    alarmSet_ = true;
    triggeredToday_ = false;
    xSemaphoreGive(mutex_);
}

void AlarmClock::removeAlarm()
{
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return;
    alarmSet_ = false;
    callback_ = nullptr;
    xSemaphoreGive(mutex_);
}

void AlarmClock::update(uint8_t currentHour, uint8_t currentMinute)
{
    Callback callback;
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return;
    if (!alarmSet_)
    {
        xSemaphoreGive(mutex_);
        return;
    }

    if (time_.hour == currentHour && time_.minute == currentMinute)
    {
        if (!triggeredToday_ && callback_)
        {
            callback = callback_;
            triggeredToday_ = true;
        }
    }
    else
    {
        triggeredToday_ = false;
    }
    xSemaphoreGive(mutex_);
    if (callback) callback();
}

bool AlarmClock::isAlarmConfigured() const noexcept
{
    return getSnapshot().configured;
}

AlarmTime AlarmClock::getAlarmTime() const noexcept
{
    return getSnapshot().time;
}

AlarmClock::Snapshot AlarmClock::getSnapshot() const noexcept
{
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return {};
    const Snapshot result{alarmSet_, time_};
    xSemaphoreGive(mutex_);
    return result;
}
