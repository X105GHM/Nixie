#include "Timer.hpp"

void Timer::start(uint32_t seconds, Callback cb)
{
    configured_ = seconds;
    remaining_ = seconds;
    callback_ = cb;
    running_ = true;
}

void Timer::stop()
{
    running_ = false;
    remaining_ = 0;
}

void Timer::update()
{
    if (!running_ || remaining_ == 0)
        return;

    --remaining_;
    if (remaining_ == 0 && callback_)
    {
        callback_();
        running_ = false;
    }
}