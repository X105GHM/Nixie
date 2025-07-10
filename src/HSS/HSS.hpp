#pragma once

#include "Logger/Logger.hpp"
#include "driver/gpio.h"
#include <functional>

class HSS
{
public:
    HSS() noexcept;

    void enable160() const noexcept;

    void disable160() const noexcept;

    void enable190() const noexcept;

    void disable190() const noexcept;

    void enableResistorReduction() const noexcept;

    void disableResistorReduction() const noexcept;

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
