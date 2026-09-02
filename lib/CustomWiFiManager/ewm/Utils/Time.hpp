#pragma once

#include <cstdint>
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ewm::utils
{
    inline uint64_t monotonicMillis() noexcept
    {
        const int64_t microseconds = esp_timer_get_time();
        return microseconds > 0 ? static_cast<uint64_t>(microseconds / 1000) : 0;
    }

    inline void delayMilliseconds(uint32_t milliseconds) noexcept
    {
        TickType_t ticks = pdMS_TO_TICKS(milliseconds);
        if (milliseconds > 0 && ticks == 0)
        {
            ticks = 1;
        }
        vTaskDelay(ticks);
    }
}
