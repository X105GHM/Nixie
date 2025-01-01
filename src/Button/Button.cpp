#include "Button/Button.h"
#include "HSS/HSS.h"
#include "ACP/ACP.h"
#include "Digit_Control/Digit.h"
#include "Time/Time.h"
#include "Melody/Melody.h"

uint32_t buttonPressedTime = 0;
bool buttonPreviouslyPressed = false;

uint32_t funcButtonPressedTime = 0;
bool funcButtonPreviouslyPressed = false;

WiFiManager wifiManager;

void onButtonPress()
{
  if (!runningManualACP)
  {
    runningManualACP = true;
    return;
  }
  singleDigit = (singleDigit + 1) % 60;
}

void onButtonLongPress()
{
  if (runningManualACP)
  {
    runningManualACP = false;
    singleDigit = 0;
  }
  displayDate();
  delay(5000);
  displayIP();
  digitalWrite(PIN_RELAY, LOW);
  dacWrite(PIN_JFET, ACP_Voltage); // 190V
  ACP(); // Blockierend
  dacWrite(PIN_JFET, Operating_Voltage); // 160V
}

void buttonRoutine()
{
bool buttonPressed = digitalRead(BUTTON_PIN) == LOW; 

  bool funcButtonPressed = digitalRead(FUNC_BUTTON_PIN) == LOW;

  if (buttonPressed && !buttonPreviouslyPressed)
  {
    buttonPressedTime = millis();
    buttonPreviouslyPressed = true;
  }
  else if (!buttonPressed && buttonPreviouslyPressed)
  {
    if (millis() - buttonPressedTime > 2000)
    { // Langer Tastendruck
      onButtonLongPress();
    }
    else
    { // Kurzer Tastendruck
      onButtonPress();
    }
    buttonPreviouslyPressed = false;
  }

  if (funcButtonPressed && !funcButtonPreviouslyPressed)
  {
    funcButtonPressedTime = millis();
    funcButtonPreviouslyPressed = true;
  }
  else if (!buttonPressed && buttonPreviouslyPressed)
  {
    if (millis() - funcButtonPressedTime > 2000)
    { // Langer Tastendruck
      eraseWiFiCredentialsAndRestart();
    }
    else
    { // Kurzer Tastendruck
      ESP.restart();
    }
    buttonPreviouslyPressed = false;
  }
}

void eraseWiFiCredentialsAndRestart()
{
  Serial.println("Lösche gespeicherte WLAN-Daten...");
  wifiManager.resetSettings();

  delay(1000);

  Serial.println("Starte ESP32 neu...");
  ESP.restart();
}