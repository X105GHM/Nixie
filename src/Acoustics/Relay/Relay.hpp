#pragma once

#include "driver/gpio.h"

class Relay
{
public:
    Relay() noexcept;

    void toggle() noexcept;

    bool isOn() const noexcept { return state_; }

private:
    static constexpr gpio_num_t RELAY_PIN = GPIO_NUM_16;
    bool state_;

    void setLevel(bool level) noexcept;
};
