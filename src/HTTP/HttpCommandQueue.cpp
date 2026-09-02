#include "HttpCommandQueue.hpp"

#include <new>
#include <utility>

#include "AppState/AppState.hpp"
#include "esp_log.h"

namespace { constexpr const char* TAG = "HttpCommands"; }

bool HttpCommandQueue::start() noexcept
{
    if (task_) return true;
    queue_ = xQueueCreate(8, sizeof(Command));
    if (!queue_) return false;
    if (xTaskCreatePinnedToCore(taskEntry, "HttpCommands", 8192, this, 2, &task_, 0) != pdPASS)
    {
        vQueueDelete(queue_);
        queue_ = nullptr;
        return false;
    }
    return true;
}

HttpCommandQueue::Result HttpCommandQueue::enqueue(std::function<void()> action) noexcept
{
    if (!action) return Result::Unsupported;
    if (!queue_) return Result::NotStarted;

    auto* ownedAction = new (std::nothrow) std::function<void()>(std::move(action));
    if (!ownedAction) return Result::OutOfMemory;

    Command command{ownedAction, nullptr, nullptr};
    if (xQueueSend(queue_, &command, 0) != pdTRUE)
    {
        delete ownedAction;
        return Result::QueueFull;
    }
    return Result::Queued;
}

HttpCommandQueue::Result HttpCommandQueue::run(std::function<void()> action) noexcept
{
    if (!action) return Result::Unsupported;
    if (!queue_) return Result::NotStarted;

    auto* ownedAction = new (std::nothrow) std::function<void()>(std::move(action));
    if (!ownedAction) return Result::OutOfMemory;

    StaticSemaphore_t completionStorage{};
    SemaphoreHandle_t completion = xSemaphoreCreateBinaryStatic(&completionStorage);
    if (!completion)
    {
        delete ownedAction;
        return Result::OutOfMemory;
    }

    bool executionSucceeded = false;
    Command command{ownedAction, completion, &executionSucceeded};
    if (xQueueSend(queue_, &command, 0) != pdTRUE)
    {
        delete ownedAction;
        return Result::QueueFull;
    }
    if (xSemaphoreTake(completion, portMAX_DELAY) != pdTRUE)
    {
        return Result::ExecutionFailed;
    }
    return executionSucceeded ? Result::Queued : Result::ExecutionFailed;
}

void HttpCommandQueue::taskEntry(void* context)
{
    static_cast<HttpCommandQueue*>(context)->taskLoop();
}

void HttpCommandQueue::taskLoop()
{
    Command command{};
    for (;;)
    {
        if (xQueueReceive(queue_, &command, portMAX_DELAY) != pdTRUE) continue;
        if (command.action)
        {
            const bool succeeded = AppState::instance().executeMutation(*command.action);
            delete command.action;
            command.action = nullptr;
            if (!succeeded) ESP_LOGE(TAG, "Command execution failed");
            if (command.executionSucceeded) *command.executionSucceeded = succeeded;
            if (command.completion) xSemaphoreGive(command.completion);
        }
        else if (command.completion)
        {
            if (command.executionSucceeded) *command.executionSucceeded = false;
            xSemaphoreGive(command.completion);
        }
    }
}
