#ifndef BUTTON_HANDLING_H
#define BUTTON_HANDLING_H

#include <Arduino.h>
#include <WiFiManager.h>

// Konstanten
constexpr uint8_t BUTTON_PIN = 0;
constexpr uint8_t FUNC_BUTTON_PIN = 23;
constexpr uint8_t PIN_BUZZER = 22;
constexpr uint8_t PIN_RELAY = 12;

// Globale Variablen
extern uint32_t buttonPressedTime;
extern bool buttonPreviouslyPressed;

extern uint32_t funcButtonPressedTime;
extern bool funcButtonPreviouslyPressed;

extern WiFiManager wifiManager;

// Funktionsdeklarationen
void onButtonPress();
void onButtonLongPress();
void buttonRoutine();
void eraseWiFiCredentialsAndRestart();

#endif // BUTTON_HANDLING_H
