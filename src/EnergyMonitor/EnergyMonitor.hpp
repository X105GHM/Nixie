#pragma once

#include "SupplyWatch/SupplyWatch.hpp"
#include "freertos/FreeRTOS.h"
#include <string>

class EnergyMonitor
{
public:
    struct Snapshot
    {
        float instantPowerW{0.0f};
        float totalEnergyWh{0.0f};
    };

    explicit EnergyMonitor(SupplyWatch &sw, float systemVoltage) noexcept;

    void update() noexcept;

    float getInstantPowerW() const noexcept;

    float getTotalEnergyWh() const noexcept;

    float getTotalEnergykWh() const noexcept;
    std::string getTotalEnergyString() const noexcept;
    Snapshot getSnapshot() const noexcept;

private:
    SupplyWatch &sw_;
    float systemVoltage_;
    float instantPowerW_;
    float totalEnergyWh_;
    mutable portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;

    static constexpr float dtHours_ = 1.0f / 3600.0f;
};
