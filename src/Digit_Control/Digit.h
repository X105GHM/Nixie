#ifndef DISPLAY_CONTROL_H
#define DISPLAY_CONTROL_H

#include <Arduino.h>
#include <SPI.h>

// Konstanten
constexpr uint16_t ON_TIME_US = 1000;
constexpr uint8_t ADAPTIVE_BRIGHTNESS = 1;

// Globale Variablen
extern bool displayEnabled;
extern bool Relay_State;
extern int32_t digits;
extern byte singleDigit;
extern uint32_t brightness;
extern uint32_t symbolArray[10];

// Funktionsdeklarationen
void IRAM_ATTR displayDigits(void *pvParameters);
void displayTime();
void displayDate();

#endif // DISPLAY_CONTROL_H
