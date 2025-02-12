#ifndef BUTTON_HANDLING_H
#define BUTTON_HANDLING_H

#include <Arduino.h>
#include <WiFiManager.h>

constexpr uint8_t BUTTON_PIN = 0;
constexpr uint8_t FUNC_BUTTON_PIN = 23;
constexpr uint8_t PIN_RELAY = 12;

class ButtonHandler {
public:
    ButtonHandler();
    void onButtonPress();
    void onButtonLongPress();
    void buttonRoutine();
    void eraseWiFiCredentialsAndRestart();
};

extern uint32_t buttonPressedTime;
extern bool buttonPreviouslyPressed;

extern uint32_t funcButtonPressedTime;
extern bool funcButtonPreviouslyPressed;

extern WiFiManager wifiManager;

#endif // BUTTON_HANDLING_H
