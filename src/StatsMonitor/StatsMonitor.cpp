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

void StatsMonitor::update()
{
    coreLoad0_ = cpu_load_get_core(0);
    coreLoad1_ = cpu_load_get_core(1);
    totalLoad_ = 0.5f * (coreLoad0_ + coreLoad1_);
}

void StatsMonitor::logLoad()
{
    auto &sm = instance();
    Logger::log(LoggerType::GENERAL, "Core0: %.1f%%  Core1: %.1f%%  Total: %.1f%%", sm.coreLoad0_, sm.coreLoad1_, sm.totalLoad_);
}

uint32_t StatsMonitor::diffU32_(uint32_t now, uint32_t prev)
{
    return (now >= prev) ? (now - prev) : (uint32_t)(now + (UINT32_MAX - prev) + 1u);
}

char StatsMonitor::stateChar_(eTaskState s)
{
    switch (s)
    {
        case eRunning:   return 'R';
        case eReady:     return 'Y';
        case eBlocked:   return 'B';
        case eSuspended: return 'S';
        case eDeleted:   return 'D';
        default:         return '?';
    }
}

bool StatsMonitor::isIdleName_(const char* name)
{
    if (!name) return false;
    return std::strncmp(name, "IDLE", 4) == 0;
}

bool StatsMonitor::isIdleTask_(const TaskStatus_t& t) const
{
    return isIdleName_(t.pcTaskName);
}

uint32_t StatsMonitor::findPrevRunTime_(TaskHandle_t handle) const
{
    for (const auto& p : prevTasks_)
    {
        if (p.handle == handle)
        {
            return p.runTime;
        }
    }
    return 0;
}

void StatsMonitor::refreshBaseline_(const std::vector<TaskStatus_t>& snap, uint32_t totalRunTime, int64_t nowUs)
{
    prevTasks_.clear();
    prevTasks_.reserve(snap.size());

    for (const auto& t : snap)
    {
        PrevTask p;
        p.handle = t.xHandle;
        p.runTime = t.ulRunTimeCounter;
        prevTasks_.push_back(p);
    }

    prevTotalRunTime_ = totalRunTime;
    prevSampleUs_ = nowUs;
}

String StatsMonitor::buildIsoTimestampUtc_() const
{
    time_t now = 0;
    time(&now);

    struct tm tmUtc{};
    gmtime_r(&now, &tmUtc);

    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmUtc);
    return String(buf);
}

void StatsMonitor::sampleTaskTimes(uint32_t window_ms, bool skipIdle)
{
    const int64_t nowUs = esp_timer_get_time();

    if (prevSampleUs_ != 0 && (nowUs - prevSampleUs_) < (int64_t)window_ms * 1000)
    {
        return;
    }

    UBaseType_t taskCount = uxTaskGetNumberOfTasks();
    std::vector<TaskStatus_t> snap(taskCount + 4);
    uint32_t totalRunTime = 0;

    UBaseType_t written = uxTaskGetSystemState(snap.data(), snap.size(), &totalRunTime);
    if (written == 0)
    {
        return;
    }
    snap.resize(written);

    if (prevSampleUs_ == 0 || prevTasks_.empty())
    {
        refreshBaseline_(snap, totalRunTime, nowUs);
        return;
    }

    const uint32_t totalDiff = diffU32_(totalRunTime, prevTotalRunTime_);
    if (totalDiff == 0)
    {
        refreshBaseline_(snap, totalRunTime, nowUs);
        return;
    }

    update();

    std::vector<TaskRow> rows;
    rows.reserve(snap.size());

    uint32_t coreTotal[2] = {0u, 0u};

    for (const auto& t : snap)
    {
        TaskRow row;
        row.name = t.pcTaskName ? String(t.pcTaskName) : String("?");
        row.core = (int)t.xCoreID;
        row.state = stateChar_(t.eCurrentState);
        row.priority = t.uxCurrentPriority;
        row.stackHwm = (uint32_t)t.usStackHighWaterMark;
        row.runtimeDeltaUs = diffU32_(t.ulRunTimeCounter, findPrevRunTime_(t.xHandle));
        row.pctTotal = 100.0f * (float)row.runtimeDeltaUs / (float)totalDiff;
        row.isIdle = isIdleTask_(t);

        if (row.core == 0 || row.core == 1)
        {
            coreTotal[row.core] += row.runtimeDeltaUs;
        }

        rows.push_back(row);
    }

    for (auto& row : rows)
    {
        if ((row.core == 0 || row.core == 1) && coreTotal[row.core] > 0)
        {
            row.pctCore = 100.0f * (float)row.runtimeDeltaUs / (float)coreTotal[row.core];
        }
    }

    std::sort(rows.begin(), rows.end(), [](const TaskRow& a, const TaskRow& b)
    {
        return a.runtimeDeltaUs > b.runtimeDeltaUs;
    });

    if (skipIdle)
    {
        rows.erase(std::remove_if(rows.begin(), rows.end(), [](const TaskRow& r){ return r.isIdle; }), rows.end());
    }

    lastRows_ = std::move(rows);
    lastWindowMs_ = window_ms;
    lastTimestampUs_ = nowUs;

    refreshBaseline_(snap, totalRunTime, nowUs);
}

String StatsMonitor::getTaskStatsJson(uint32_t top_n, bool skipIdle) const
{
    String json;
    json.reserve(4096);

    json += "{\n";
    json += "  \"timestamp\": \"" + buildIsoTimestampUtc_() + "\",\n";
    json += "  \"window_ms\": " + String(lastWindowMs_) + ",\n";
    json += "  \"cpu\": {\n";
    json += "    \"total_load_pct\": " + String(totalLoad_, 2) + ",\n";
    json += "    \"core0_load_pct\": " + String(coreLoad0_, 2) + ",\n";
    json += "    \"core1_load_pct\": " + String(coreLoad1_, 2) + "\n";
    json += "  },\n";
    json += "  \"tasks\": [\n";

    uint32_t written = 0;
    for (size_t i = 0; i < lastRows_.size(); ++i)
    {
        const auto& r = lastRows_[i];
        if (skipIdle && r.isIdle) continue;
        if (written >= top_n) break;

        if (written > 0) json += ",\n";

        json += "    {\n";
        json += "      \"name\": \"" + r.name + "\",\n";
        json += "      \"core\": " + String(r.core) + ",\n";
        json += "      \"state\": \"" + String(r.state) + "\",\n";
        json += "      \"priority\": " + String((uint32_t)r.priority) + ",\n";
        json += "      \"stack_hwm_words\": " + String(r.stackHwm) + ",\n";
        json += "      \"runtime_delta_us\": " + String(r.runtimeDeltaUs) + ",\n";
        json += "      \"pct_total\": " + String(r.pctTotal, 2) + ",\n";
        json += "      \"pct_core\": " + String(r.pctCore, 2) + "\n";
        json += "    }";

        ++written;
    }

    json += "\n  ]\n";
    json += "}";

    return json;
}