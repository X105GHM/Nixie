#pragma once

#include "Logger/Logger.hpp"
#include "driver/gpio.h"
#include <functional>

extern bool enable160V;
extern bool enable190V;
extern bool enableResistor;

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

    mutable bool enableResistor = false;
    mutable bool enable190V = false;
    mutable bool enable160V = false;

private:
    static constexpr gpio_num_t PIN_160V = GPIO_NUM_15;
    static constexpr gpio_num_t PIN_190V = GPIO_NUM_7;
    static constexpr gpio_num_t PIN_REDUCE = GPIO_NUM_19;
    static constexpr gpio_num_t PIN_HSS_LED = GPIO_NUM_42;
};
