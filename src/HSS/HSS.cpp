#include "HSS/HSS.h"
#include "ACP/ACP.h"
#include "Digit_Control/Digit.h"
#include <driver/adc.h>

bool HSS_LED = false;
bool HSS_CUTOFF = false;
bool samplesFilled = false;
float HSS_V;
u_int16_t samples[NUM_SAMPLES];
u_int8_t sampleIndex = 0;


void readHSS() {
  int rawADC = adc1_get_raw(ADC1_CHANNEL_4);
  samples[sampleIndex] = rawADC;
  sampleIndex = (sampleIndex + 1) % NUM_SAMPLES;

  if (sampleIndex == 0) {
    samplesFilled = true;
  }

  if (samplesFilled) {
    long sum = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
      sum += samples[i];
    }
    float averageADC = sum / (float)NUM_SAMPLES;
    HSS_V = (averageADC / 4095.0) * 270.0;
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