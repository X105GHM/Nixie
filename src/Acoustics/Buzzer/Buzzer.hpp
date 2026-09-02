#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include <atomic>
#include <cstdint>

#include "Digits/Digits.hpp"

class Buzzer
{
public:
    Buzzer() noexcept;

    bool isOn() const noexcept { return state_.load(std::memory_order_relaxed); }

    void startAlarm(uint8_t repeatCount = 5) noexcept;

    void stopAlarm() noexcept;

    void update() noexcept;

    void startCricketInTask() noexcept;

    void playCricketSound() noexcept;

    void setLevel(bool level) noexcept;

    void Silence() noexcept;

private:
    static constexpr gpio_num_t BUZZER_PIN = GPIO_NUM_40;
    static constexpr uint32_t CRICKET_TASK_STACK_BYTES = 8192;

    std::atomic_bool cricketRunning_{false};
    std::atomic_bool state_{false};
    bool alarmRunning_ = false;
    bool toneState_ = false; 
    uint32_t lastToggleTime_ = 0;
    uint8_t remainingToggles_ = 0;
};
