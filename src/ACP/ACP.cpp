#include "ACP/ACP.h"
#include "HSS/HSS.h"
#include "Digit_Control/Digit.h"

bool runningACP1 = false;
bool runningACP2 = false;
bool runningManualACP = false;

void ACP()
{
// Ziffern durchlaufen, um eine Kathodenvergiftung zu vermeiden

  brightness = 100;

  for (int number = 0; number <= 9; number++)
  {
    digits = 111111 * number;

    delay(500);
  }

  delay(200);

  runningACP1 = true;

  for (int number = 0; number <= 9; number++)
  {
    digits = 101010 * number;

    delay(100);
  }

  for (int i = 0; i <= 4; i++)
  {
    for (int number = 0; number <= 9; number++)
    {
      digits = 101010 * number;

      delay(100);
    }
  }

  runningACP1 = false;
  runningManualACP = true;

  for (int number = 0; number <= 240; number++)
  {
    singleDigit = (singleDigit + 1) % 60;

    delay(200);
  }

  singleDigit = 0;
  runningManualACP = false;
  runningACP2 = true;

  for (int i = 0; i <= 4; i++)
  {
    for (int number = 0; number <= 9; number++)
    {
      digits = 10101 * number;

      delay(50);
    }
  }

  for (int number = 0; number <= 9; number++)
  {
    digits = (90909 - (10101 * number));

    delay(100);
  }

  runningACP2 = false;

  runningACP1 = true;

  digits = 306060;
  delay(4000);
  digits = 407070;
  delay(4000);
  digits = 508080;
  delay(4000);
  digits = 609090;
  delay(4000);
  digits = 706060;
  delay(4000);
  digits = 807070;
  delay(4000);
  digits = 908080;
  delay(4000);
  digits = 309090;
  delay(4000);
  runningACP1 = false;
}