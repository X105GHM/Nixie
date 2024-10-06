#include "ACP/ACP.h"
#include "HSS/HSS.h"
#include "Digit_Control/Digit.h"

bool runningACP1 = false;
bool runningACP2 = false;
bool runningManualACP = false;
int32_t previousDigits = -1;

void updateIfChanged(int32_t newDigits)
{
  if (newDigits != previousDigits)
  {
    digits = newDigits;
    updateDisplay();
    previousDigits = newDigits;
  }
}

void ACP()
{
  for (int number = 0; number <= 9; number++)
  {
    updateIfChanged(111111 * number);
    delay(500);
  }

  delay(200);
  runningACP1 = true;

  for (int number = 0; number <= 9; number++)
  {
    updateIfChanged(101010 * number);
    delay(100);
  }

  for (int i = 0; i <= 8; i++)
  {
    for (int number = 0; number <= 9; number++)
    {
      updateIfChanged(101010 * number);
      delay(20);
    }
  }

  runningACP1 = false;
  runningACP2 = true;

  for (int i = 0; i <= 4; i++)
  {
    for (int number = 0; number <= 9; number++)
    {
      updateIfChanged(10101 * number);
      delay(50);
    }
  }

  for (int number = 0; number <= 9; number++)
  {
    updateIfChanged(90909 - (10101 * number));
    delay(100);
  }

  runningACP2 = false;

  for (int number = 9; number >= 0; number--)
  {
    updateIfChanged(111111 * number);
    delay(500);
  }

  runningACP1 = true;

  int32_t displaySequence[] = {306060, 407070, 508080, 609090, 706060, 807070, 908080, 309090};
  for (int i = 0; i < 8; i++)
  {
    for (int number = 0; number <= 9; number++)
    {
      updateIfChanged(111111 * number);
      delay(50);
    }

  	for (int number = 8; number >= 0; number--)
    {
      updateIfChanged(111111 * number);
      delay(50);
    }

    updateIfChanged(displaySequence[i]);
    delay(4900);
  }
  digitalWrite(PIN_HSS_CUTOFF, LOW);
  runningACP1 = false;
}
