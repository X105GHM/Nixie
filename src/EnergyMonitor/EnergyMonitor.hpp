#pragma once

#include "SupplyWatch/SupplyWatch.hpp"
#include <string>

class EnergyMonitor
{
public:
    explicit EnergyMonitor(SupplyWatch &sw, float systemVoltage) noexcept;

    void update() noexcept;

    float getInstantPowerW() const noexcept;

    float getTotalEnergyWh() const noexcept;

    float getTotalEnergykWh() const noexcept;
    std::string getTotalEnergyString() const noexcept;

private:
    SupplyWatch &sw_;
    float systemVoltage_;
    float instantPowerW_;
    float totalEnergyWh_;

    static constexpr float dtHours_ = 1.0f / 3600.0f;
};
