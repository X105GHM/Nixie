#ifndef DISPLAY_CONTROL_H
#define DISPLAY_CONTROL_H

#include <Arduino.h>
#include <SPI.h>

constexpr uint8_t PIN_DIN = 13;
constexpr uint8_t PIN_CLK = 14;

extern bool displayEnabled;
extern bool Relay_State;
extern int32_t digits;
extern byte singleDigit;
extern uint32_t symbolArray[10];
extern int32_t previousDigits;

void displayDigits();
void displayCustomDigits(int hh, int ss);
void updateDisplay();
void updateIfChanged(int32_t newDigits);
void displayTime();
void displayDate();
void displayIP();

#endif // DISPLAY_CONTROL_H
