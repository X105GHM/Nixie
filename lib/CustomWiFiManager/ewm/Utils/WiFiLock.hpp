#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace ewm::utils
{
    struct WiFiLock
    {
        explicit WiFiLock(SemaphoreHandle_t m) : m_(m)
        {
            if (m_) xSemaphoreTakeRecursive(m_, portMAX_DELAY);
        }
        ~WiFiLock()
        {
            if (m_) xSemaphoreGiveRecursive(m_);
        }
        SemaphoreHandle_t m_;
    };
}
