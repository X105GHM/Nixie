#pragma once

#include "HSS/HSS.hpp"
#include "Digits/Digits.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdint>

extern bool runningACP1;
extern bool runningACP2;
extern bool ACP_enabled;
extern std::atomic<bool> mode_running;

void ACP() noexcept;
