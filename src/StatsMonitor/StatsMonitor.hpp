#pragma once
#include <cstdint>
#include "Globals/Globals.hpp"
#include "Digits/Digits.hpp"
#include "cpu_load.h"
#include "Logger/Logger.hpp"

class StatsMonitor
{
public:

    float coreLoad0_  = 0.0f;
    float coreLoad1_  = 0.0f;
    float totalLoad_  = 0.0f;

    static StatsMonitor &instance();
    float getCoreLoad(uint8_t coreId) const;
    float getTotalLoad() const;
    void logLoad();
    void update();

private:
    StatsMonitor() = default;
    ~StatsMonitor() = default;
    StatsMonitor(const StatsMonitor&) = delete;
    StatsMonitor& operator=(const StatsMonitor&) = delete;
};