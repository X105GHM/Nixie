#include "Timer.hpp"

#include <utility>

Timer::Timer() noexcept : mutex_(xSemaphoreCreateMutexStatic(&mutexStorage_)){}

void Timer::start(uint32_t seconds, Callback cb)
{
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return;
    configured_ = seconds;
    remaining_ = seconds;
    callback_ = std::move(cb);
    running_ = true;
    xSemaphoreGive(mutex_);
}

void Timer::stop()
{
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return;
    running_ = false;
    remaining_ = 0;
    xSemaphoreGive(mutex_);
}

void Timer::update()
{
    Callback callback;
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return;
    if (!running_ || remaining_ == 0)
    {
        xSemaphoreGive(mutex_);
        return;
    }

    --remaining_;
    if (remaining_ == 0 && callback_)
    {
        running_ = false;
        callback = callback_;
    }
    xSemaphoreGive(mutex_);
    if (callback) callback();
}

bool Timer::isRunning() const noexcept
{
    return getSnapshot().running;
}

uint32_t Timer::getConfiguredSeconds() const noexcept
{
    return getSnapshot().configuredSeconds;
}

Timer::Snapshot Timer::getSnapshot() const noexcept
{
    if (!mutex_ || xSemaphoreTake(mutex_, portMAX_DELAY) != pdTRUE) return {};
    const Snapshot result{running_, configured_};
    xSemaphoreGive(mutex_);
    return result;
}
