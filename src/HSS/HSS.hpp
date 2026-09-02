#pragma once

#include <atomic>
#include "Logger/Logger.hpp"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <functional>

class HSS
{
public:
    HSS() noexcept;

    esp_err_t enable160() const noexcept;
    esp_err_t disable160() const noexcept;
    esp_err_t enable190() const noexcept;
    esp_err_t disable190() const noexcept;
    esp_err_t enableResistorReduction() const noexcept;
    esp_err_t disableResistorReduction() const noexcept;

    bool testLoad(const std::function<float()>& readVoltage, float thresholdV, uint32_t discrimMs, uint32_t maxWaitMs) const noexcept;

    mutable std::atomic_bool enableResistor{false};
    mutable std::atomic_bool enable190V{false};
    mutable std::atomic_bool enable160V{false};

private:
    static constexpr gpio_num_t PIN_160V = GPIO_NUM_15;
    static constexpr gpio_num_t PIN_190V = GPIO_NUM_7;
    static constexpr gpio_num_t PIN_REDUCE = GPIO_NUM_19;
    static constexpr gpio_num_t PIN_HSS_LED = GPIO_NUM_42;
};
