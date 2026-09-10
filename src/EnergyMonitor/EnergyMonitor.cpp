#include "EnergyMonitor.hpp"
#include "Logger/Logger.hpp"
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
    const float currentA = sw_.readCurrent();
    const float instantPowerW = systemVoltage_ * currentA;

    taskENTER_CRITICAL(&mux_);
    instantPowerW_ = instantPowerW;
    totalEnergyWh_ += instantPowerW * dtHours_;
    const float totalEnergyWh = totalEnergyWh_;
    taskEXIT_CRITICAL(&mux_);

    Logger::log(LoggerType::POWER, "Instant Power: %.3f W | Total Energy: %.3f Wh", instantPowerW, totalEnergyWh);
}

float EnergyMonitor::getInstantPowerW() const noexcept
{
    return getSnapshot().instantPowerW;
}

float EnergyMonitor::getTotalEnergyWh() const noexcept
{
    return getSnapshot().totalEnergyWh;
}

float EnergyMonitor::getTotalEnergykWh() const noexcept
{
    return getSnapshot().totalEnergyWh / 1000.0f;
}

std::string EnergyMonitor::getTotalEnergyString() const noexcept
{
    const float totalEnergyWh = getSnapshot().totalEnergyWh;
    char buf[32];
    if (totalEnergyWh < 1000.0f)
    {
        std::snprintf(buf, sizeof(buf), "%.2f Wh", totalEnergyWh);
    }
    else
    {
        std::snprintf(buf, sizeof(buf), "%.4f kWh", totalEnergyWh / 1000.0f);
    }
    return std::string(buf);
}

EnergyMonitor::Snapshot EnergyMonitor::getSnapshot() const noexcept
{
    taskENTER_CRITICAL(&mux_);
    const Snapshot result{instantPowerW_, totalEnergyWh_};
    taskEXIT_CRITICAL(&mux_);
    return result;
}
