#ifndef DISPLAY_CONTROL_H
#define DISPLAY_CONTROL_H

#include <Arduino.h>
#include <SPI.h>

// Konstanten
constexpr uint8_t PIN_DIN = 13;
constexpr uint8_t PIN_CLK = 14;

// Globale Variablen
extern bool displayEnabled;
extern bool Relay_State;
extern int32_t digits;
extern byte singleDigit;
extern uint32_t symbolArray[10];
extern int32_t previousDigits;

// Funktionsdeklarationen
void displayDigits();
void updateDisplay();
void updateIfChanged(int32_t newDigits);
void displayTime();
void displayDate();
void displayIP();

#endif // DISPLAY_CONTROL_H
