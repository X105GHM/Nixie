#pragma once
#include <cstdint>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

struct AlarmTime
{
    uint8_t hour;
    uint8_t minute;
};

class AlarmClock
{
public:
    using Callback = std::function<void()>;

    struct Snapshot
    {
        bool configured{false};
        AlarmTime time{0, 0};
    };

    AlarmClock() noexcept;

    void setAlarm(uint8_t hour, uint8_t minute, Callback cb);
    void removeAlarm();
    void update(uint8_t currentHour, uint8_t currentMinute);

    bool isAlarmConfigured() const noexcept;
    AlarmTime getAlarmTime() const noexcept;
    Snapshot getSnapshot() const noexcept;

private:
    AlarmTime time_{0, 0};
    Callback callback_ = nullptr;
    bool alarmSet_ = false;
    bool triggeredToday_ = false;
    mutable StaticSemaphore_t mutexStorage_{};
    mutable SemaphoreHandle_t mutex_{nullptr};
};
