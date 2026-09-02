#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "History/TimeSeriesPyramid.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class ChartHistory final
{
public:
    using Point = chart_history::Point;
    using SampleValues = chart_history::TimeSeriesPyramid::SampleValues;

    enum class State : uint8_t
    {
        NotInitialized,
        Ready,
        MutexUnavailable,
        PsramAllocationFailed,
        RingInitializationFailed
    };

    enum class SnapshotResult : uint8_t
    {
        Ready,
        InvalidRange,
        Unavailable,
        Busy
    };

    struct Snapshot
    {
        const Point* points{nullptr};
        size_t count{0};
        const chart_history::RangeSpec* range{nullptr};
        uint32_t resolutionSeconds{0};
        int64_t startTimestampSeconds{0};
        int64_t endTimestampSeconds{0};
    };

    static ChartHistory& instance() noexcept;

    bool initialize() noexcept;
    bool record(int64_t timestampSeconds, const SampleValues& values) noexcept;
    SnapshotResult acquireSnapshot(std::string_view rangeName, Snapshot& snapshot) noexcept;
    void releaseSnapshot() noexcept;

    bool available() const noexcept { return available_.load(std::memory_order_acquire); }
    State state() const noexcept { return state_.load(std::memory_order_acquire); }
    static const char* stateName(State state) noexcept;
    size_t allocatedBytes() const noexcept { return allocatedBytes_; }
    size_t freePsramAfterAllocation() const noexcept { return freePsramAfterAllocation_; }

    ChartHistory(const ChartHistory&) = delete;
    ChartHistory& operator=(const ChartHistory&) = delete;

private:
    static constexpr size_t SNAPSHOT_CAPACITY = chart_history::LEVEL_CAPACITY;
    static constexpr size_t TOTAL_POINT_COUNT =
        chart_history::RING_POINT_COUNT + SNAPSHOT_CAPACITY;

    ChartHistory() noexcept;

    StaticSemaphore_t mutexStorage_{};
    SemaphoreHandle_t mutex_{nullptr};
    chart_history::TimeSeriesPyramid pyramid_{};
    Point* allocation_{nullptr};
    Point* snapshotBuffer_{nullptr};
    bool snapshotInUse_{false};
    std::atomic_bool available_{false};
    std::atomic<State> state_{State::NotInitialized};
    size_t allocatedBytes_{0};
    size_t freePsramAfterAllocation_{0};
};
