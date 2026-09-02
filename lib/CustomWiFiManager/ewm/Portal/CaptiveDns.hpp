#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ewm
{
    class CaptiveDns
    {
    public:
        bool start(uint32_t accessPointAddress);
        void stop();
        bool running() const noexcept { return task_.load(std::memory_order_acquire) != nullptr; }

    private:
        static void taskEntry(void* arg);
        void run();
        size_t createResponse(const uint8_t* request, size_t requestLength, uint8_t* response, size_t capacity) const;

        std::atomic<bool> stopRequested_{false};
        std::atomic<TaskHandle_t> task_{nullptr};
        int socketFd_{-1};
        uint32_t accessPointAddress_{0};
    };
}
