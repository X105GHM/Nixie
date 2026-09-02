#pragma once
#include <cstdint>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class Timer
{
public:
    using Callback = std::function<void()>;

    struct Snapshot
    {
        bool running{false};
        uint32_t configuredSeconds{0};
    };

    Timer() noexcept;

    void start(uint32_t seconds, Callback cb);
    void stop();
    void update();

    bool isRunning() const noexcept;
    uint32_t getConfiguredSeconds() const noexcept;
    Snapshot getSnapshot() const noexcept;

private:
    uint32_t remaining_ = 0;
    uint32_t configured_ = 0;
    Callback callback_;
    bool running_ = false;
    mutable StaticSemaphore_t mutexStorage_{};
    mutable SemaphoreHandle_t mutex_{nullptr};
};
