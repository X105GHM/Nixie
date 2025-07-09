#pragma once
#include <cstdint>
#include <functional>

class Timer
{
public:
    using Callback = std::function<void()>;

    void start(uint32_t seconds, Callback cb);
    void stop();
    void update();

    bool isRunning() const noexcept { return running_; }
    uint32_t getConfiguredSeconds() const noexcept { return configured_; }

private:
    uint32_t remaining_ = 0;
    uint32_t configured_ = 0;
    Callback callback_;
    bool running_ = false;
};
