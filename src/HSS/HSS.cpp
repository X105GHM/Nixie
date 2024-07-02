#include "HSS/HSS.h"
#include "ACP/ACP.h"
#include "Digit_Control/Digit.h"

bool HSS_LED = false;
bool HSS_CUTOFF = false;

void readHSS()
{
  float HSS_V = (analogRead(PIN_HSS_V) / 4095) * 270;

  if (HSS_V > 210)
  {
    digitalWrite(PIN_HSS_CUTOFF, HIGH);
    HSS_CUTOFF = true;
  }
  else if (HSS_V < 100 && HSS_CUTOFF == true)
  {
    digitalWrite(PIN_HSS_CUTOFF, LOW);
    HSS_CUTOFF = false;
  }

  if (HSS_V > 30 && HSS_LED == false)
  {
    digitalWrite(PIN_HV_LED, HIGH);
    HSS_LED = true;
  }
  else if (HSS_V < 30 && HSS_LED == true)
  {
    digitalWrite(PIN_HV_LED, LOW);
    HSS_LED = false;
  }
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