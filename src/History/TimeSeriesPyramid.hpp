#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace chart_history
{
    constexpr size_t SERIES_COUNT = 13;
    constexpr size_t LEVEL_COUNT = 6;
    constexpr size_t LEVEL_CAPACITY = 1000;
    constexpr size_t RING_POINT_COUNT = LEVEL_COUNT * LEVEL_CAPACITY;
    constexpr int64_t MINIMUM_VALID_EPOCH_SECONDS = 1704067200; // 2024-01-01 UTC

    enum class Series : size_t
    {
        Voltage12V,
        Voltage5V,
        Voltage3V3,
        Voltage18V,
        VoltageUhss,
        CurrentMa,
        PowerW,
        EnergyWh,
        TemperatureC,
        FreeHeap,
        TotalLoad,
        CoreLoad0,
        CoreLoad1
    };

    inline constexpr std::array<std::string_view, SERIES_COUNT> SERIES_NAMES = {
        "voltage_12V", "voltage_5V", "voltage_3V3", "voltage_18V", "voltage_UHSS",
        "current_mA", "power_W", "energy_Wh", "temperature", "freeHeap",
        "totalLoad", "coreLoad0", "coreLoad1"};

    struct AggregateValue
    {
        float average{0.0f};
        float minimum{0.0f};
        float maximum{0.0f};
    };

    struct Point
    {
        int64_t timestampSeconds{0};
        uint32_t sampleCount{0};
        std::array<AggregateValue, SERIES_COUNT> values{};
    };

    static_assert(sizeof(Point) == 168, "Unexpected history point padding");

    struct LevelDefinition
    {
        uint32_t resolutionSeconds;
        uint8_t factorFromPrevious;
    };

    inline constexpr std::array<LevelDefinition, LEVEL_COUNT> LEVELS = 
    {{
        {1, 1},
        {4, 4},
        {32, 8},
        {64, 2},
        {128, 2},
        {1024, 8},
    }};

    struct RangeSpec
    {
        std::string_view name;
        uint32_t windowSeconds;
        size_t level;
    };

    inline constexpr std::array<RangeSpec, LEVEL_COUNT> RANGES = 
    {{
        {"10m", 10U * 60U, 0},
        {"1h", 60U * 60U, 1},
        {"6h", 6U * 60U * 60U, 2},
        {"12h", 12U * 60U * 60U, 3},
        {"24h", 24U * 60U * 60U, 4},
        {"7d", 7U * 24U * 60U * 60U, 5},
    }};

    const RangeSpec* findRange(std::string_view name) noexcept;
    bool validEpoch(int64_t epochSeconds) noexcept;

    class TimeSeriesPyramid final
    {
    public:
        using SampleValues = std::array<float, SERIES_COUNT>;

        bool initialize(Point* ringStorage, size_t pointCapacity) noexcept;
        void reset() noexcept;
        bool add(int64_t timestampSeconds, const SampleValues& values) noexcept;

        size_t copyRange(
            size_t level,
            int64_t firstTimestampSeconds,
            int64_t lastTimestampSeconds,
            Point* destination,
            size_t destinationCapacity) const noexcept;

        size_t levelSize(size_t level) const noexcept;
        const Point* levelPoint(size_t level, size_t chronologicalIndex) const noexcept;

    private:
        struct Ring
        {
            Point* points{nullptr};
            size_t writeIndex{0};
            size_t count{0};
        };

        struct PendingAggregate
        {
            Point point{};
            uint8_t received{0};
        };

        static void merge(Point& target, const Point& source) noexcept;
        void push(size_t level, const Point& point) noexcept;
        void feedNextLevel(size_t sourceLevel, const Point& point) noexcept;
        void clearPending() noexcept;
        bool pendingTail(size_t level, Point& tail) const noexcept;

        std::array<Ring, LEVEL_COUNT> rings_{};
        std::array<PendingAggregate, LEVEL_COUNT - 1> pending_{};
        int64_t lastTimestampSeconds_{0};
        bool initialized_{false};
    };
}
