#include "StatsMonitor.hpp"
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
    class SemaphoreGuard
    {
    public:
        explicit SemaphoreGuard(SemaphoreHandle_t mutex) noexcept : mutex_(mutex)
        {
            locked_ = mutex_ && xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE;
        }

        ~SemaphoreGuard()
        {
            if (locked_) xSemaphoreGive(mutex_);
        }

        explicit operator bool() const noexcept { return locked_; }

    private:
        SemaphoreHandle_t mutex_{nullptr};
        bool locked_{false};
    };

    void appendFormatted(std::string &target, const char *format, ...) noexcept
    {
        char buffer[128];
        va_list args;
        va_start(args, format);
        const int length = std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        if (length > 0)
        {
            target.append(buffer, static_cast<size_t>(std::min(length, static_cast<int>(sizeof(buffer) - 1))));
        }
    }
}

StatsMonitor::StatsMonitor() : mutex_(xSemaphoreCreateMutexStatic(&mutexStorage_)){}

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

StatsMonitor::LoadSnapshot StatsMonitor::getLoadSnapshot() const noexcept
{
    SemaphoreGuard guard(mutex_);
    if (!guard) return {};
    return {coreLoad0_, coreLoad1_, totalLoad_};
}

void StatsMonitor::update()
{
    const float core0 = cpu_load_get_core(0);
    const float core1 = cpu_load_get_core(1);
    SemaphoreGuard guard(mutex_);
    if (!guard) return;
    coreLoad0_ = core0;
    coreLoad1_ = core1;
    totalLoad_ = 0.5f * (core0 + core1);
}

void StatsMonitor::logLoad()
{
    const auto load = instance().getLoadSnapshot();
    Logger::log(LoggerType::GENERAL, "Core0: %.1f%%  Core1: %.1f%%  Total: %.1f%%", load.core0, load.core1, load.total);
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

std::string StatsMonitor::buildIsoTimestampUtc_() const
{
    time_t now = 0;
    time(&now);

    struct tm tmUtc{};
    gmtime_r(&now, &tmUtc);

    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tmUtc);
    return buf;
}

void StatsMonitor::sampleTaskTimes(uint32_t window_ms, bool skipIdle)
{
    SemaphoreGuard guard(mutex_);
    if (!guard) return;

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

    coreLoad0_ = cpu_load_get_core(0);
    coreLoad1_ = cpu_load_get_core(1);
    totalLoad_ = 0.5f * (coreLoad0_ + coreLoad1_);

    std::vector<TaskRow> rows;
    rows.reserve(snap.size());

    uint32_t coreTotal[2] = {0u, 0u};

    for (const auto& t : snap)
    {
        TaskRow row;
        row.name = t.pcTaskName ? t.pcTaskName : "?";
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

std::string StatsMonitor::getTaskStatsJson(uint32_t top_n, bool skipIdle) const
{
    SemaphoreGuard guard(mutex_);
    if (!guard) return "{\"error\":\"stats unavailable\"}";

    std::string json;
    json.reserve(4096);

    json += "{\n";
    json += "  \"timestamp\": \"" + buildIsoTimestampUtc_() + "\",\n";
    appendFormatted(json, "  \"window_ms\": %u,\n", static_cast<unsigned>(lastWindowMs_));
    json += "  \"cpu\": {\n";
    appendFormatted(json, "    \"total_load_pct\": %.2f,\n", totalLoad_);
    appendFormatted(json, "    \"core0_load_pct\": %.2f,\n", coreLoad0_);
    appendFormatted(json, "    \"core1_load_pct\": %.2f\n", coreLoad1_);
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
        appendFormatted(json, "      \"core\": %d,\n", r.core);
        appendFormatted(json, "      \"state\": \"%c\",\n", r.state);
        appendFormatted(json, "      \"priority\": %u,\n", static_cast<unsigned>(r.priority));
        appendFormatted(json, "      \"stack_hwm_words\": %u,\n", static_cast<unsigned>(r.stackHwm));
        appendFormatted(json, "      \"runtime_delta_us\": %u,\n", static_cast<unsigned>(r.runtimeDeltaUs));
        appendFormatted(json, "      \"pct_total\": %.2f,\n", r.pctTotal);
        appendFormatted(json, "      \"pct_core\": %.2f\n", r.pctCore);
        json += "    }";

        ++written;
    }

    json += "\n  ]\n";
    json += "}";

    return json;
}
