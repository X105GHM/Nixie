#pragma once

#include <cstdint>
#include <functional>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

class HttpCommandQueue
{
public:
    enum class Result : uint8_t
    {
        Queued,
        NotStarted,
        QueueFull,
        OutOfMemory,
        Unsupported,
        ExecutionFailed
    };

    bool start() noexcept;
    Result enqueue(std::function<void()> action) noexcept;
    Result run(std::function<void()> action) noexcept;

private:
    struct Command
    {
        std::function<void()>* action;
        SemaphoreHandle_t completion;
        bool* executionSucceeded;
    };

    static void taskEntry(void* context);
    void taskLoop();

    QueueHandle_t queue_{nullptr};
    TaskHandle_t task_{nullptr};
};
