#include "StatsMonitor.hpp"

StatsMonitor &StatsMonitor::instance()
{
    static StatsMonitor inst;
    return inst;
}

float StatsMonitor::getCoreLoad(uint8_t coreId) const
{
    return cpu_load_get_core(coreId);
}

float StatsMonitor::getTotalLoad() const
{
    return cpu_load_get_total();
}

void StatsMonitor::logLoad()
{
    auto &sm = instance();
    float c0 = sm.getCoreLoad(0);
    float c1 = sm.getCoreLoad(1);
    float tot = sm.getTotalLoad();
    Logger::log(LoggerType::GENERAL,"Core0: %.1f%%  Core1: %.1f%%  Total: %.1f%%",c0, c1, tot);
}

void StatsMonitor::update() 
{
    coreLoad0_ = cpu_load_get_core(0);

    if(displayEnabled && !ACP_enabled && !Globals::PWM_disabled)
    {
        coreLoad1_ = 99.0f;
    }
    else
    {
        coreLoad1_ = cpu_load_get_core(1);
    }

    totalLoad_ = 0.5f * (coreLoad0_ + coreLoad1_);
}