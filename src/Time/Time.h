#ifndef TIME_CONTROL_H
#define TIME_CONTROL_H

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

// Konstanten
constexpr uint8_t PIN_LED = 26;

// Globale Variablen
extern struct tm timeInfo;
extern time_t prevTime;
extern uint32_t lastRefresh;

// Funktionsdeklarationen
void setTimezone(String timezone);
void initTime(String timezone);
void blinkError();
void timeCycle();

#endif // TIME_CONTROL_H
