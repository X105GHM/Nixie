#pragma once

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_task_wdt.h"
#include <atomic>

#include "Globals/Globals.hpp"
#include "TimeWeather/WeatherClient/WeatherClient.hpp"
#include "ACP/ACP.hpp"
#include "Logger/Logger.hpp"
#include <algorithm>
#include <ctime>
#include <cstring>
#include <driver/gpio.h>

/// Software-PWM 
extern std::atomic_uint32_t PWM_PERIOD_US;

constexpr bool ADAPTIVE_BRIGHTNESS = true;

extern std::atomic_bool displayEnabled;
extern std::atomic_bool zipMaskingEnabled;
extern std::atomic_bool tempMaskingEnabled;
extern std::atomic_bool singleDigitACP;
extern std::atomic<bool> mode_running;
extern std::atomic_uint8_t singleDigit;
extern std::atomic_int32_t digits;
extern std::atomic_uint32_t brightness;   // 0..100

void displayDigitsTask(void* pvParameters) noexcept;
void displayTime() noexcept;
void displayDate() noexcept;
void displayWeather() noexcept;

[[maybe_unused]] static void delayMicrosYield(uint32_t usec) 
{
    int64_t start = esp_timer_get_time();
    while ((esp_timer_get_time() - start) < usec) 
    {
        taskYIELD(); 
    }
}
