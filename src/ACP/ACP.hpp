#pragma once

#include "HSS/HSS.hpp"
#include "Digits/Digits.hpp"
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdint>

extern std::atomic_bool runningACP1;
extern std::atomic_bool runningACP2;
extern std::atomic_bool ACP_enabled;
extern std::atomic<bool> mode_running;

void ACP() noexcept;
