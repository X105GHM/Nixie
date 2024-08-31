#include "Digit_Control/Digit.h"
#include "HSS/HSS.h"
#include "ACP/ACP.h"
#include "Time/Time.h"

bool displayEnabled = false;
bool Relay_State = false;

int32_t digits = 0; 
/*Zahl, die angezeigt werden soll, negativ bedeutet, dass nur 4 Ziffern angezeigt werden (zweite Positionen sind leer).
  0-9 ist die erste Röhre links, 10-19 die zweite Röhre usw.*/

byte singleDigit = 0;
uint32_t brightness = 100; // * 0 - 100 Prozent, 32Bit wegen "min" Funktion, Augen zu und durch!
uint32_t symbolArray[10] = {512, 1, 2, 4, 8, 16, 32, 64, 128, 256}; // 0 to 9

void IRAM_ATTR displayDigits(void *pvParameters)
{
  Serial.print("Task1 running on core ");
  Serial.println(xPortGetCoreID());
  for (;;)
  {
    delayMicroseconds(ON_TIME_US);
    if (!displayEnabled)
    {
      continue;
    }

    if (runningManualACP)
    {
      digitalWrite(PIN_OE, LOW); // Dateneingabe zulassen (transparenter Modus, alle Ausgänge sind LOW)
      unsigned long var32 = 0;   // 32 Bit alle auf 0 initialisieren
      if (singleDigit < 30)
      {
        SPI.transfer(var32 >> 24);
        SPI.transfer(var32 >> 16);
        SPI.transfer(var32 >> 8);
        SPI.transfer(var32);
        var32 |= symbolArray[singleDigit % 10] << singleDigit - (singleDigit % 10);
        SPI.transfer(var32 >> 24);
        SPI.transfer(var32 >> 16);
        SPI.transfer(var32 >> 8);
        SPI.transfer(var32);
      }
      else
      {
        var32 |= symbolArray[singleDigit % 10] << singleDigit - (singleDigit % 10) - 30;
        SPI.transfer(var32 >> 24);
        SPI.transfer(var32 >> 16);
        SPI.transfer(var32 >> 8);
        SPI.transfer(var32);
        var32 = 0;
        SPI.transfer(var32 >> 24);
        SPI.transfer(var32 >> 16);
        SPI.transfer(var32 >> 8);
        SPI.transfer(var32);
      }
      digitalWrite(PIN_OE, HIGH); // Daten zwischenspeichern (aktiviert HV-Ausgänge entsprechend den Registern)
      delay(200);                 // Längere Einschaltdauer erzwingen, wenn die ACP-Routine ausgeführt wird
      continue;
    }

    long digitsCopy;
    memcpy(&digitsCopy, &digits, sizeof(long));

    digitalWrite(PIN_OE, LOW); // Dateneingabe zulassen (transparenter Modus, alle Ausgänge sind LOW)
    unsigned long var32 = 0;

    //---------------------------------- REG 1 -----------------------------------------------
    var32 = 0; // 32 Bit alle auf 0 initialisieren

    // 00 0000000000 0000000000 0000000000 00 0000000000 0000000000 0000000000
    //        s2         s1         m2            m1         h2         h1
    // -- 0987654321 0987654321 0987654321 -- 0987654321 0987654321 0987654321

    if (!runningACP1)
    {
      var32 |= (unsigned long)(symbolArray[digitsCopy % 10]) << 20; // s2
    }
    digitsCopy /= 10;

    if (!runningACP2)
    {
      var32 |= (unsigned long)(symbolArray[digitsCopy % 10]) << 10; // s1
    }
    digitsCopy /= 10;

    if (!runningACP1)
    {
      var32 |= (unsigned long)(symbolArray[digitsCopy % 10]); // m2
    }
    digitsCopy /= 10;

    SPI.transfer(var32 >> 24);
    SPI.transfer(var32 >> 16);
    SPI.transfer(var32 >> 8);
    SPI.transfer(var32);

    //---------------------------------- REG 0 -----------------------------------------------
    var32 = 0;

    if (!runningACP2)
    {
      var32 |= (unsigned long)(symbolArray[digitsCopy % 10]) << 20; // m1
    }
    digitsCopy /= 10;

    if (!runningACP1)
    {
      var32 |= (unsigned long)(symbolArray[digitsCopy % 10]) << 10; // h2
    }
    digitsCopy /= 10;

    if (!runningACP2)
    {
      var32 |= (unsigned long)(symbolArray[digitsCopy % 10]); // h1
    }
    digitsCopy /= 10;

    SPI.transfer(var32 >> 24);
    SPI.transfer(var32 >> 16);
    SPI.transfer(var32 >> 8);
    SPI.transfer(var32);

    if (ADAPTIVE_BRIGHTNESS && !runningACP1 || !runningACP2)
    {
      // Helligkeit dimmen, indem eine längere Ausschaltzeit erzwungen wird
      delayMicroseconds((ON_TIME_US * 100 - ON_TIME_US * min(brightness, (uint)100)) / min(brightness, (uint)100));
    }
    digitalWrite(PIN_OE, HIGH);  // Daten zwischenspeichern (aktiviert HV-Ausgänge entsprechend den Registern)
    if (runningACP1 || runningACP2 || runningManualACP)
    {
      delay(100);  // Längere Einschaltdauer erzwingen, wenn die ACP-Routine ausgeführt wird
    }
  }
}

void displayTime()
{
  digits = 0;
  digits += timeInfo.tm_hour * 10000;
  digits += timeInfo.tm_min * 100;
  digits += timeInfo.tm_sec;
}

void displayDate()
{
  digits = 0;

  digits += timeInfo.tm_mday * 10000;
  digits += (timeInfo.tm_mon + 1) * 100;  // Monat ist 0-11, zum Anzeigen +1
  digits += (timeInfo.tm_year + 1900) % 100;
}


