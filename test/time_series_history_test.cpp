#include "History/TimeSeriesPyramid.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

namespace
{
    using chart_history::LEVEL_CAPACITY;
    using chart_history::Point;
    using chart_history::RING_POINT_COUNT;
    using chart_history::SERIES_COUNT;
    using chart_history::TimeSeriesPyramid;

    constexpr int64_t BASE_TIME = chart_history::MINIMUM_VALID_EPOCH_SECONDS + 1000;

    void expect(bool condition, const char* message)
    {
        if (!condition)
        {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    bool closeEnough(float actual, double expected, double tolerance = 0.001)
    {
        return std::fabs(static_cast<double>(actual) - expected) <= tolerance;
    }

    TimeSeriesPyramid::SampleValues values(float value)
    {
        TimeSeriesPyramid::SampleValues sample{};
        sample.fill(value);
        return sample;
    }

    struct Fixture
    {
        std::vector<Point> rings{RING_POINT_COUNT};
        std::vector<Point> output{LEVEL_CAPACITY};
        TimeSeriesPyramid pyramid{};

        Fixture()
        {
            expect(pyramid.initialize(rings.data(), rings.size()), "initialize fixture");
        }
    };

    void testEmptyAndFirstSample()
    {
        Fixture fixture;
        expect(fixture.pyramid.copyRange(0, BASE_TIME, BASE_TIME + 10,
                                         fixture.output.data(), fixture.output.size()) == 0,
               "empty ring after boot");
        expect(fixture.pyramid.add(BASE_TIME, values(42.0f)), "first sample accepted");
        expect(fixture.pyramid.levelSize(0) == 1, "first sample stored in level 0");
        const Point* point = fixture.pyramid.levelPoint(0, 0);
        expect(point && point->sampleCount == 1, "raw sample count");
        expect(point && closeEnough(point->values[0].average, 42.0), "raw average");
        expect(point && closeEnough(point->values[0].minimum, 42.0), "raw minimum");
        expect(point && closeEnough(point->values[0].maximum, 42.0), "raw maximum");
    }

    void testLevelZeroWrapAndChronology()
    {
        Fixture fixture;
        for (size_t index = 0; index < LEVEL_CAPACITY + 5; ++index)
        {
            expect(fixture.pyramid.add(BASE_TIME + static_cast<int64_t>(index), values(static_cast<float>(index))), "level 0 fill");
        }
        expect(fixture.pyramid.levelSize(0) == LEVEL_CAPACITY, "level 0 fixed capacity");
        const size_t copied = fixture.pyramid.copyRange(
            0, BASE_TIME, BASE_TIME + LEVEL_CAPACITY + 10,
            fixture.output.data(), fixture.output.size());
        expect(copied == LEVEL_CAPACITY, "copy full wrapped level 0");
        expect(fixture.output.front().timestampSeconds == BASE_TIME + 5, "oldest wrapped point");
        expect(fixture.output.back().timestampSeconds == BASE_TIME + 1004, "newest wrapped point");
        for (size_t index = 1; index < copied; ++index)
        {
            expect(fixture.output[index - 1].timestampSeconds < fixture.output[index].timestampSeconds,
                   "chronological output after wrap");
        }
    }

    void testLevelOneAggregation()
    {
        Fixture fixture;
        for (int index = 0; index < 4; ++index)
            expect(fixture.pyramid.add(BASE_TIME + index, values(static_cast<float>(index + 1))), "level 1 source sample");
        expect(fixture.pyramid.levelSize(1) == 1, "one level 1 point after four samples");
        const Point* point = fixture.pyramid.levelPoint(1, 0);
        expect(point && point->sampleCount == 4, "level 1 sample count");
        expect(point && closeEnough(point->values[0].average, 2.5), "level 1 average");
        expect(point && closeEnough(point->values[0].minimum, 1.0), "level 1 minimum");
        expect(point && closeEnough(point->values[0].maximum, 4.0), "level 1 maximum");
    }

    void testCascadeAndWeightedAggregation()
    {
        Fixture fixture;
        for (int index = 0; index < 1024; ++index)
            expect(fixture.pyramid.add(BASE_TIME + index, values(static_cast<float>(index + 1))), "cascade source sample");

        expect(fixture.pyramid.levelSize(1) == 256, "level 1 cascade count");
        expect(fixture.pyramid.levelSize(2) == 32, "level 2 cascade count");
        expect(fixture.pyramid.levelSize(3) == 16, "level 3 cascade count");
        expect(fixture.pyramid.levelSize(4) == 8, "level 4 cascade count");
        expect(fixture.pyramid.levelSize(5) == 1, "level 5 cascade count");

        const Point* point = fixture.pyramid.levelPoint(5, 0);
        expect(point && point->sampleCount == 1024, "level 5 original sample count");
        expect(point && closeEnough(point->values[0].average, 512.5), "weighted level 5 average");
        expect(point && closeEnough(point->values[0].minimum, 1.0), "level 5 minimum");
        expect(point && closeEnough(point->values[0].maximum, 1024.0), "level 5 maximum peak");
    }

    void testUnequalPendingWeightsAndPeak()
    {
        Fixture fixture;
        for (int index = 0; index < 128; ++index)
            expect(fixture.pyramid.add(BASE_TIME + index, values(static_cast<float>(index + 1))),
                   "pending weighted source");
        expect(fixture.pyramid.add(BASE_TIME + 128, values(1000.0f)), "pending peak source");

        const size_t copied = fixture.pyramid.copyRange(
            5, BASE_TIME, BASE_TIME + 200, fixture.output.data(), fixture.output.size());
        expect(copied == 1, "incomplete hierarchy exposed as one exact tail point");
        const Point& tail = fixture.output[0];
        expect(tail.sampleCount == 129, "pending tail sample count");
        expect(closeEnough(tail.values[0].average, 9256.0 / 129.0, 0.01),
               "pending tail uses weighted mean, not mean of means");
        expect(closeEnough(tail.values[0].minimum, 1.0), "pending tail minimum");
        expect(closeEnough(tail.values[0].maximum, 1000.0), "pending tail preserves peak");
    }

    void testRangesAndTimeFiltering()
    {
        const struct ExpectedRange
        {
            const char* name;
            uint32_t seconds;
            size_t level;
            uint32_t resolution;
        } expected[] = {
            {"10m", 600, 0, 1}, {"1h", 3600, 1, 4}, {"6h", 21600, 2, 32},
            {"12h", 43200, 3, 64}, {"24h", 86400, 4, 128}, {"7d", 604800, 5, 1024}};
        for (const auto& item : expected)
        {
            const auto* range = chart_history::findRange(item.name);
            expect(range && range->windowSeconds == item.seconds, "range duration mapping");
            expect(range && range->level == item.level, "range level mapping");
            expect(chart_history::LEVELS[item.level].resolutionSeconds == item.resolution,
                   "range resolution mapping");
        }
        expect(chart_history::findRange("bad") == nullptr, "invalid range rejected");

        Fixture fixture;
        for (int index = 0; index < 4096; ++index)
            expect(fixture.pyramid.add(BASE_TIME + index, values(static_cast<float>(index))),
                   "range source sample");
        const int64_t now = BASE_TIME + 4095;
        for (const auto& item : expected)
        {
            const size_t copied = fixture.pyramid.copyRange(
                item.level, now - item.seconds, now, fixture.output.data(), fixture.output.size());
            expect(copied > 0 && copied <= LEVEL_CAPACITY, "range query point bounds");
            for (size_t index = 0; index < copied; ++index)
            {
                expect(fixture.output[index].timestampSeconds >= now - item.seconds,
                       "range query lower time bound");
                expect(fixture.output[index].timestampSeconds <= now, "range query upper time bound");
            }
        }
    }

    void testFailureAndUnsynchronisedTime()
    {
        TimeSeriesPyramid pyramid;
        expect(!pyramid.initialize(nullptr, 0), "allocation/init failure is handled");

        Fixture fixture;
        expect(!fixture.pyramid.add(0, values(1.0f)), "Unix epoch is not treated as valid time");
        expect(!chart_history::validEpoch(chart_history::MINIMUM_VALID_EPOCH_SECONDS - 1),
               "pre-sync timestamp rejected");
        expect(chart_history::validEpoch(chart_history::MINIMUM_VALID_EPOCH_SECONDS),
               "valid timestamp accepted");
    }

    void testConcurrentWriterAndSnapshotPattern()
    {
        Fixture fixture;
        std::mutex mutex;
        std::atomic_bool finished{false};
        std::thread writer([&]()
        {
            for (int index = 0; index < 5000; ++index)
            {
                std::lock_guard<std::mutex> lock(mutex);
                expect(fixture.pyramid.add(BASE_TIME + index, values(static_cast<float>(index))),
                       "concurrent writer sample");
            }
            finished.store(true);
        });

        std::vector<Point> localSnapshot(LEVEL_CAPACITY);
        while (!finished.load())
        {
            size_t copied = 0;
            {
                std::lock_guard<std::mutex> lock(mutex);
                copied = fixture.pyramid.copyRange(
                    0, BASE_TIME, BASE_TIME + 6000, fixture.output.data(), fixture.output.size());
                std::copy_n(fixture.output.data(), copied, localSnapshot.data());
            }
            for (size_t index = 1; index < copied; ++index)
            {
                expect(localSnapshot[index - 1].timestampSeconds < localSnapshot[index].timestampSeconds,
                       "consistent copied snapshot during writes");
            }
        }
        writer.join();
    }
}

int main()
{
    testEmptyAndFirstSample();
    testLevelZeroWrapAndChronology();
    testLevelOneAggregation();
    testCascadeAndWeightedAggregation();
    testUnequalPendingWeightsAndPeak();
    testRangesAndTimeFiltering();
    testFailureAndUnsynchronisedTime();
    testConcurrentWriterAndSnapshotPattern();
    std::cout << "PASS time-series pyramid unit tests\n";
    return 0;
}
