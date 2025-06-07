#pragma once

#include "driver/gpio.h"
#include "esp_err.h"
#include "Logger/Logger.hpp"

static constexpr gpio_num_t BUTTON_PIN = GPIO_NUM_41;

class ButtonPoll
{
public:
    explicit ButtonPoll(gpio_num_t pin = GPIO_NUM_41) noexcept;

    esp_err_t init() noexcept;

    void poll() noexcept;

private:
    gpio_num_t pin_;
    bool lastState_;
    static void buttonCallback() noexcept;
};
