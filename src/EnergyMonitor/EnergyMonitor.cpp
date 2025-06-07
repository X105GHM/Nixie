#include "EnergyMonitor.hpp"
#include "esp_log.h"
#include <cstdio>

static const char *TAG = "EnergyMonitor";

EnergyMonitor::EnergyMonitor(SupplyWatch &sw, float systemVoltage) noexcept
    : sw_(sw),
      systemVoltage_(systemVoltage),
      instantPowerW_(0.0f),
      totalEnergyWh_(0.0f)
{
}

void EnergyMonitor::update() noexcept
{
    float currentA = sw_.readCurrent();

    instantPowerW_ = systemVoltage_ * currentA;

    totalEnergyWh_ += instantPowerW_ * dtHours_;

    ESP_LOGI(TAG,
             "Instant Power: %.3f W | Total Energy: %.3f Wh",
             instantPowerW_,
             totalEnergyWh_);
}

float EnergyMonitor::getInstantPowerW() const noexcept
{
    return instantPowerW_;
}

float EnergyMonitor::getTotalEnergyWh() const noexcept
{
    return totalEnergyWh_;
}

float EnergyMonitor::getTotalEnergykWh() const noexcept
{
    return totalEnergyWh_ / 1000.0f;
}

std::string EnergyMonitor::getTotalEnergyString() const noexcept
{
    char buf[32];
    if (totalEnergyWh_ < 1000.0f)
    {
        std::snprintf(buf, sizeof(buf), "%.2f Wh", totalEnergyWh_);
    }
    else
    {
        std::snprintf(buf, sizeof(buf), "%.4f kWh", totalEnergyWh_ / 1000.0f);
    }
    return std::string(buf);
}
