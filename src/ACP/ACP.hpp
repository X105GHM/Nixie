#pragma once

#include "HSS/HSS.hpp"
#include "Digits/Digits.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdint>

extern bool runningACP1;
extern bool runningACP2;
extern bool runningManualACP;

void ACP() noexcept;
