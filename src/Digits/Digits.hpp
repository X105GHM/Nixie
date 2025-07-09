#pragma once

#include <cstdint>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_task_wdt.h"

#include "Globals/Globals.hpp"
#include "TimeWeather/WeatherClient/WeatherClient.hpp"
#include "ACP/ACP.hpp"
#include "Logger/Logger.hpp"
#include <algorithm>
#include <ctime>
#include <cstring>
#include <driver/gpio.h>

/// Software-PWM für 400 Hz (Periode = 2 500 µs)
extern std::uint32_t PWM_PERIOD_US;

constexpr bool ADAPTIVE_BRIGHTNESS = true;

extern bool displayEnabled;
extern bool zipMaskingEnabled;
extern bool tempMaskingEnabled;
extern bool singleDigitACP;
extern std::uint8_t singleDigit; 
extern std::int32_t digits;
extern std::int32_t lastdigits;
extern std::uint32_t brightness;   // 0..100
extern const std::uint32_t symbolArray[10];

void displayDigitsTask(void* pvParameters) noexcept;
void displayTime() noexcept;
void displayDate() noexcept;
void displayWeather() noexcept;

static void delayMicrosYield(uint32_t usec) 
{
    int64_t start = esp_timer_get_time();
    while ((esp_timer_get_time() - start) < usec) 
    {
        taskYIELD(); 
    }
}