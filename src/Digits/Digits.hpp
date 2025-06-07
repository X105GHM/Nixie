#pragma once

#include <cstdint>
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ACP/ACP.hpp"
#include <algorithm>
#include <ctime>
#include <cstring>
#include <driver/gpio.h>

constexpr std::uint32_t ON_TIME_US = 1000;
constexpr bool ADAPTIVE_BRIGHTNESS = true;

extern bool displayEnabled;
extern std::int32_t digits; 
extern std::uint8_t singleDigit;
extern std::uint32_t brightness;   // 0..100
extern const std::uint32_t symbolArray[10];

void displayDigitsTask(void* pvParameters) noexcept;
void displayTime() noexcept;
void displayDate() noexcept;
