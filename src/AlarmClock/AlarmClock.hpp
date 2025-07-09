#pragma once
#include <cstdint>
#include <functional>

struct AlarmTime
{
    uint8_t hour;
    uint8_t minute;
};

class AlarmClock
{
public:
    using Callback = std::function<void()>;

    void setAlarm(uint8_t hour, uint8_t minute, Callback cb);
    void removeAlarm();
    void update(uint8_t currentHour, uint8_t currentMinute);

    bool isAlarmConfigured() const noexcept;
    AlarmTime getAlarmTime() const noexcept;

private:
    AlarmTime time_{0, 0};
    Callback callback_ = nullptr;
    bool alarmSet_ = false;
    bool triggeredToday_ = false;
};
