#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>
#include "esp_timer.h"

#include "Globals/Globals.hpp"
#include "Digits/Digits.hpp"
#include "cpu_load.h"
#include "Logger/Logger.hpp"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}

class StatsMonitor
{
public:
    float coreLoad0_ = 0.0f;
    float coreLoad1_ = 0.0f;
    float totalLoad_ = 0.0f;

    static StatsMonitor &instance();

    float getCoreLoad(uint8_t coreId) const;
    float getTotalLoad() const;

    void update();
    void logLoad();

    void sampleTaskTimes(uint32_t window_ms = 1000, bool skipIdle = true);
    String getTaskStatsJson(uint32_t top_n = 12, bool skipIdle = true) const;

private:
    StatsMonitor() = default;
    ~StatsMonitor() = default;
    StatsMonitor(const StatsMonitor&) = delete;
    StatsMonitor& operator=(const StatsMonitor&) = delete;

    struct PrevTask
    {
        TaskHandle_t handle = nullptr;
        uint32_t runTime = 0;
    };

    struct TaskRow
    {
        String name;
        int core = -1;
        char state = '?';
        UBaseType_t priority = 0;
        uint32_t stackHwm = 0;
        uint32_t runtimeDeltaUs = 0;
        float pctTotal = 0.0f;
        float pctCore = 0.0f;
        bool isIdle = false;
    };

    static uint32_t diffU32_(uint32_t now, uint32_t prev);
    static char stateChar_(eTaskState s);
    static bool isIdleName_(const char* name);
    bool isIdleTask_(const TaskStatus_t& t) const;

    uint32_t findPrevRunTime_(TaskHandle_t handle) const;
    void refreshBaseline_(const std::vector<TaskStatus_t>& snap, uint32_t totalRunTime, int64_t nowUs);
    String buildIsoTimestampUtc_() const;

    std::vector<PrevTask> prevTasks_;
    uint32_t prevTotalRunTime_ = 0;
    int64_t prevSampleUs_ = 0;

    std::vector<TaskRow> lastRows_;
    uint32_t lastWindowMs_ = 0;
    int64_t lastTimestampUs_ = 0;
};