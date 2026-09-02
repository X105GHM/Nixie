#include "ChartHistory.hpp"

#include <ctime>

#include "esp_heap_caps.h"
#include "esp_log.h"

namespace
{
    constexpr const char* TAG = "ChartHistory";
}

ChartHistory& ChartHistory::instance() noexcept
{
    static ChartHistory history;
    return history;
}

ChartHistory::ChartHistory() noexcept
    : mutex_(xSemaphoreCreateMutexStatic(&mutexStorage_))
{
}

bool ChartHistory::initialize() noexcept
{
    if (available()) return true;
    if (!mutex_)
    {
        state_.store(State::MutexUnavailable, std::memory_order_release);
        ESP_LOGE(TAG, "History mutex creation failed; history disabled");
        return false;
    }

    allocatedBytes_ = TOTAL_POINT_COUNT * sizeof(Point);
    allocation_ = static_cast<Point*>(heap_caps_malloc(allocatedBytes_, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!allocation_)
    {
        state_.store(State::PsramAllocationFailed, std::memory_order_release);
        ESP_LOGE(TAG, "Could not reserve %u bytes in PSRAM; charts fall back to live data", static_cast<unsigned>(allocatedBytes_));
        allocatedBytes_ = 0;
        return false;
    }

    snapshotBuffer_ = allocation_ + chart_history::RING_POINT_COUNT;
    if (!pyramid_.initialize(allocation_, chart_history::RING_POINT_COUNT))
    {
        heap_caps_free(allocation_);
        allocation_ = nullptr;
        snapshotBuffer_ = nullptr;
        allocatedBytes_ = 0;
        state_.store(State::RingInitializationFailed, std::memory_order_release);
        ESP_LOGE(TAG, "History ring initialization failed; history disabled");
        return false;
    }

    freePsramAfterAllocation_ = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    state_.store(State::Ready, std::memory_order_release);
    available_.store(true, std::memory_order_release);
    ESP_LOGI(TAG, "Reserved %u bytes PSRAM for 6x1000 points plus HTTP snapshot; free PSRAM=%u",
             static_cast<unsigned>(allocatedBytes_),
             static_cast<unsigned>(freePsramAfterAllocation_));
    return true;
}

const char* ChartHistory::stateName(State state) noexcept
{
    switch (state)
    {
        case State::Ready: return "ready";
        case State::MutexUnavailable: return "mutex_unavailable";
        case State::PsramAllocationFailed: return "psram_allocation_failed";
        case State::RingInitializationFailed: return "ring_initialization_failed";
        case State::NotInitialized:
        default: return "not_initialized";
    }
}

bool ChartHistory::record(int64_t timestampSeconds, const SampleValues& values) noexcept
{
    if (!available() || !mutex_) return false;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return false;
    const bool stored = pyramid_.add(timestampSeconds, values);
    xSemaphoreGive(mutex_);
    return stored;
}

ChartHistory::SnapshotResult ChartHistory::acquireSnapshot(
    std::string_view rangeName, Snapshot& snapshot) noexcept
{
    snapshot = {};
    const chart_history::RangeSpec* range = chart_history::findRange(rangeName);
    if (!range) return SnapshotResult::InvalidRange;
    if (!available() || !mutex_ || !snapshotBuffer_) return SnapshotResult::Unavailable;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(1000)) != pdTRUE) return SnapshotResult::Busy;
    if (snapshotInUse_)
    {
        xSemaphoreGive(mutex_);
        return SnapshotResult::Busy;
    }

    const int64_t now = static_cast<int64_t>(std::time(nullptr));
    const int64_t first = chart_history::validEpoch(now)
        ? now - static_cast<int64_t>(range->windowSeconds)
        : chart_history::MINIMUM_VALID_EPOCH_SECONDS;
    const size_t count = chart_history::validEpoch(now)
        ? pyramid_.copyRange(range->level, first, now, snapshotBuffer_, SNAPSHOT_CAPACITY)
        : 0;

    snapshotInUse_ = true;
    snapshot.points = snapshotBuffer_;
    snapshot.count = count;
    snapshot.range = range;
    snapshot.resolutionSeconds = chart_history::LEVELS[range->level].resolutionSeconds;
    if (count != 0)
    {
        snapshot.startTimestampSeconds = snapshotBuffer_[0].timestampSeconds;
        snapshot.endTimestampSeconds = snapshotBuffer_[count - 1].timestampSeconds;
    }
    xSemaphoreGive(mutex_);
    return SnapshotResult::Ready;
}

void ChartHistory::releaseSnapshot() noexcept
{
    if (!mutex_) return;
    if (xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return;
    snapshotInUse_ = false;
    xSemaphoreGive(mutex_);
}
