#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include <cstdint>

#include "Digits/Digits.hpp"

class Buzzer
{
public:
    Buzzer() noexcept;

    bool isOn() const noexcept { return state_; }

    void startAlarm(uint8_t repeatCount = 5) noexcept;

    void stopAlarm() noexcept;

    void update() noexcept;

    void startCricketInTask() noexcept;

    void playCricketSound() noexcept;

private:
    static constexpr gpio_num_t BUZZER_PIN = GPIO_NUM_40;

    bool cricketRunning_;
    bool state_;
    bool alarmRunning_ = false;
    bool toneState_ = false; 
    uint32_t lastToggleTime_ = 0;
    uint8_t remainingToggles_ = 0;

    void setLevel(bool level) noexcept;
};
