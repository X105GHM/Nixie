#include "Digit_Control/Digit.h"
#include "HSS/HSS.h"
#include "ACP/ACP.h"
#include "Time/Time.h"

bool displayEnabled = true;
bool Relay_State = false;

int32_t digits = 0;
/*Zahl, die angezeigt werden soll, negativ bedeutet, dass nur 4 Ziffern angezeigt werden (zweite Positionen sind leer).
  0-9 ist die erste Röhre links, 10-19 die zweite Röhre usw.*/

byte singleDigit = 0;
uint32_t symbolArray[10] = {512, 1, 2, 4, 8, 16, 32, 64, 128, 256}; // 0 bis 9

void displayDigits()
{
    if (!displayEnabled)
    {
        return;
    }

    long digitsCopy;
    memcpy(&digitsCopy, &digits, sizeof(long));

    digitalWrite(PIN_OE, LOW); // Dateneingabe zulassen (transparenter Modus, alle Ausgänge sind LOW)
    unsigned long var32 = 0;

    //---------------------------------- REG 1 -----------------------------------------------
    var32 = 0; // 32 Bit alle auf 0 initialisieren

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

    digitalWrite(PIN_OE, HIGH);  // Daten zwischenspeichern (aktiviert HV-Ausgänge entsprechend den Registern)
}

void updateDisplay()
{
    displayDigits();
}

void displayTime()
{
  digits = 0;
  digits += timeInfo.tm_hour * 10000;
  digits += timeInfo.tm_min * 100;
  digits += timeInfo.tm_sec;
  updateDisplay();
}

void displayDate()
{
    digits = 0;
    digits += timeInfo.tm_mday * 10000;
    digits += (timeInfo.tm_mon + 1) * 100;  // Monat ist 0-11, zum Anzeigen +1
    digits += (timeInfo.tm_year + 1900) % 100;
    updateDisplay();
}
