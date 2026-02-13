#pragma once
#include <Arduino.h>

#ifndef EWM_LOG
#define EWM_LOG(...) do { Serial.printf("[EWM] " __VA_ARGS__); Serial.printf("\n"); } while (0)
#endif
