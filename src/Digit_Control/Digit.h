#ifndef DISPLAY_CONTROL_H
#define DISPLAY_CONTROL_H

#include <Arduino.h>
#include <SPI.h>

// Globale Variablen
extern bool displayEnabled;
extern bool Relay_State;
extern int32_t digits;
extern byte singleDigit;
extern uint32_t symbolArray[10];

// Funktionsdeklarationen
void displayDigits();
void updateDisplay();
void displayTime();
void displayDate();

#endif // DISPLAY_CONTROL_H
