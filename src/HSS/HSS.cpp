#include "HSS/HSS.h"
#include "ACP/ACP.h"
#include "Digit_Control/Digit.h"
#include <driver/adc.h>

bool HSS_LED = false;
bool HSS_CUTOFF = false;
bool samplesFilled = false;
bool HSS_load = false;
float HSS_V;

void readHSS()
{
  int debuggspannung = analogReadMilliVolts(34);
  HSS_V = (debuggspannung / 1000.0) * 57;
}

void updateHSS()
{
  if (displayEnabled || runningACP1 || runningACP2 || runningManualACP)
  {
    // Wenn die Anzeige aktiv ist oder spezielle Modi laufen, HSS aktivieren
    digitalWrite(PIN_HSS_CUTOFF, LOW);
  }
  else
  {
    // Sonst HSS abschalten
    digitalWrite(PIN_HSS_CUTOFF, HIGH);
  }
}

void loadCheck()
{
    brightness = 100;
    digits = 999999;
    displayEnabled = true;
    while (!HSS_load)
    {
        Serial.println("HSS ON");
        digitalWrite(PIN_HSS_CUTOFF, LOW);
        delay(400);
        readHSS();
        delay(100);
        Serial.print("Spannung:");
        Serial.println(HSS_V);
        if (HSS_V > 110.0)
        {
            digitalWrite(PIN_HSS_CUTOFF, HIGH);
            Serial.println("Wartezeit von 30 Sekunden");
            HSS_load = false;
            delay(30000); // Wartezeit von 30 Sekunden
        }
        else
        {
            displayEnabled = false;
            digitalWrite(PIN_HSS_CUTOFF, HIGH);
            Serial.println("HSS OFF");
            HSS_load = true;
        }
    }
}