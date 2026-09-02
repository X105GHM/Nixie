#include "TimeSeriesPyramid.hpp"

#include <algorithm>
#include <cmath>

namespace chart_history
{
    const RangeSpec* findRange(std::string_view name) noexcept
    {
        const auto match = std::find_if(RANGES.begin(), RANGES.end(), [name](const RangeSpec& range)
        {
            return range.name == name;
        });
        return match == RANGES.end() ? nullptr : &*match;
    }

    bool validEpoch(int64_t epochSeconds) noexcept
    {
        return epochSeconds >= MINIMUM_VALID_EPOCH_SECONDS;
    }

    bool TimeSeriesPyramid::initialize(Point* ringStorage, size_t pointCapacity) noexcept
    {
        if (!ringStorage || pointCapacity < RING_POINT_COUNT)
        {
            initialized_ = false;
            return false;
        }

        for (size_t level = 0; level < LEVEL_COUNT; ++level)
        {
            rings_[level].points = ringStorage + level * LEVEL_CAPACITY;
        }
        initialized_ = true;
        reset();
        return true;
    }

    void TimeSeriesPyramid::reset() noexcept
    {
        for (Ring& ring : rings_)
        {
            ring.writeIndex = 0;
            ring.count = 0;
        }
        clearPending();
        lastTimestampSeconds_ = 0;
    }

    bool TimeSeriesPyramid::add(int64_t timestampSeconds, const SampleValues& values) noexcept
    {
        if (!initialized_ || !validEpoch(timestampSeconds)) return false;
        if (lastTimestampSeconds_ != 0 && timestampSeconds <= lastTimestampSeconds_) return false;

        for (float value : values)
        {
            if (!std::isfinite(value)) return false;
        }

        if (lastTimestampSeconds_ != 0 && timestampSeconds - lastTimestampSeconds_ > 2)
        {
            // Never aggregate across an acquisition/SNTP gap.
            clearPending();
        }

        Point point{};
        point.timestampSeconds = timestampSeconds;
        point.sampleCount = 1;
        for (size_t series = 0; series < SERIES_COUNT; ++series)
        {
            point.values[series] = {values[series], values[series], values[series]};
        }

        lastTimestampSeconds_ = timestampSeconds;
        push(0, point);
        return true;
    }

    void TimeSeriesPyramid::merge(Point& target, const Point& source) noexcept
    {
        if (source.sampleCount == 0) return;
        if (target.sampleCount == 0)
        {
            target = source;
            return;
        }

        const uint32_t previousCount = target.sampleCount;
        const uint32_t combinedCount = previousCount + source.sampleCount;
        for (size_t series = 0; series < SERIES_COUNT; ++series)
        {
            const double weightedSum =
                static_cast<double>(target.values[series].average) * previousCount +
                static_cast<double>(source.values[series].average) * source.sampleCount;
            target.values[series].average = static_cast<float>(weightedSum / combinedCount);
            target.values[series].minimum = std::min(target.values[series].minimum, source.values[series].minimum);
            target.values[series].maximum = std::max(target.values[series].maximum, source.values[series].maximum);
        }
        target.sampleCount = combinedCount;
        target.timestampSeconds = std::max(target.timestampSeconds, source.timestampSeconds);
    }

    void TimeSeriesPyramid::push(size_t level, const Point& point) noexcept
    {
        Ring& ring = rings_[level];
        ring.points[ring.writeIndex] = point;
        ring.writeIndex = (ring.writeIndex + 1) % LEVEL_CAPACITY;
        if (ring.count < LEVEL_CAPACITY) ++ring.count;

        if (level + 1 < LEVEL_COUNT) feedNextLevel(level, point);
    }

    void TimeSeriesPyramid::feedNextLevel(size_t sourceLevel, const Point& point) noexcept
    {
        PendingAggregate& aggregate = pending_[sourceLevel];
        merge(aggregate.point, point);
        ++aggregate.received;

        const uint8_t factor = LEVELS[sourceLevel + 1].factorFromPrevious;
        if (aggregate.received < factor) return;

        const Point complete = aggregate.point;
        aggregate = {};
        push(sourceLevel + 1, complete);
    }

    void TimeSeriesPyramid::clearPending() noexcept
    {
        for (PendingAggregate& aggregate : pending_) aggregate = {};
    }

    bool TimeSeriesPyramid::pendingTail(size_t level, Point& tail) const noexcept
    {
        if (level == 0 || level >= LEVEL_COUNT) return false;
        tail = {};

        // Higher pending levels are older; lower levels contain the newest samples.
        for (size_t transition = level; transition-- > 0;)
        {
            if (pending_[transition].received != 0) merge(tail, pending_[transition].point);
        }
        return tail.sampleCount != 0;
    }

    size_t TimeSeriesPyramid::copyRange
    (
        size_t level,
        int64_t firstTimestampSeconds,
        int64_t lastTimestampSeconds,
        Point* destination,
        size_t destinationCapacity
    ) const noexcept
    {
        if (!initialized_ || level >= LEVEL_COUNT || !destination || destinationCapacity == 0) return 0;

        const Ring& ring = rings_[level];
        const size_t oldest = (ring.writeIndex + LEVEL_CAPACITY - ring.count) % LEVEL_CAPACITY;
        size_t copied = 0;
        for (size_t offset = 0; offset < ring.count && copied < destinationCapacity; ++offset)
        {
            const Point& point = ring.points[(oldest + offset) % LEVEL_CAPACITY];
            if (point.timestampSeconds < firstTimestampSeconds || point.timestampSeconds > lastTimestampSeconds) continue;
            destination[copied++] = point;
        }

        Point tail{};
        if (copied < destinationCapacity && pendingTail(level, tail) &&
            tail.timestampSeconds >= firstTimestampSeconds && tail.timestampSeconds <= lastTimestampSeconds &&
            (copied == 0 || tail.timestampSeconds > destination[copied - 1].timestampSeconds))
        {
            destination[copied++] = tail;
        }
        return copied;
    }

    size_t TimeSeriesPyramid::levelSize(size_t level) const noexcept
    {
        return initialized_ && level < LEVEL_COUNT ? rings_[level].count : 0;
    }

    const Point* TimeSeriesPyramid::levelPoint(size_t level, size_t chronologicalIndex) const noexcept
    {
        if (!initialized_ || level >= LEVEL_COUNT) return nullptr;
        const Ring& ring = rings_[level];
        if (chronologicalIndex >= ring.count) return nullptr;
        const size_t oldest = (ring.writeIndex + LEVEL_CAPACITY - ring.count) % LEVEL_CAPACITY;
        return &ring.points[(oldest + chronologicalIndex) % LEVEL_CAPACITY];
    }
}
