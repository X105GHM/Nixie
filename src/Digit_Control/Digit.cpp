#include "Digit_Control/Digit.h"
#include "HSS/HSS.h"
#include "ACP/ACP.h"
#include "Time/Time.h"
#include <WiFi.h>
#include <cstring>

extern TimeControl timeControl;
extern bool runningACP1;
extern bool runningACP2;

bool displayEnabled = false;
bool Relay_State = false;
int32_t previousDigits = -1;

int32_t digits = 0;
/* Zahl, die angezeigt werden soll, negativ bedeutet, dass nur 4 Ziffern angezeigt werden (zweite Positionen sind leer).
   0-9 ist die erste Röhre links, 10-19 die zweite Röhre usw. */

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
    var32 = 0; // 32 Bit alle auf 0 initialisieren

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

    digitalWrite(PIN_OE, HIGH); // Daten zwischenspeichern (aktiviert HV-Ausgänge entsprechend den Registern)
}

void displayCustomDigits(int hh, int ss) 
{
    int h1 = hh / 10;
    int h2 = hh % 10;

    int s1 = ss / 10;     
    int s2 = ss % 10;     

    uint32_t blank = 0;


    unsigned long reg1 = 0;
    reg1 |= (unsigned long)(symbolArray[s2]) << 20; 
    reg1 |= (unsigned long)(symbolArray[s1]) << 10;  
    reg1 |= blank;                                   
    
    SPI.transfer(reg1 >> 24);
    SPI.transfer(reg1 >> 16);
    SPI.transfer(reg1 >> 8);
    SPI.transfer(reg1);
    
    unsigned long reg0 = 0;
    reg0 |= (unsigned long)(blank) << 20;           
    reg0 |= (unsigned long)(symbolArray[h2]) << 10;  
    reg0 |= (unsigned long)(symbolArray[h1]);         
    
    SPI.transfer(reg0 >> 24);
    SPI.transfer(reg0 >> 16);
    SPI.transfer(reg0 >> 8);
    SPI.transfer(reg0);
    
    digitalWrite(PIN_OE, HIGH);
}

void updateDisplay()
{
    displayDigits();
}

void displayTime()
{
    digits = 0;
    digits += timeControl.timeInfo.tm_hour * 10000;
    digits += timeControl.timeInfo.tm_min * 100;
    digits += timeControl.timeInfo.tm_sec;
    updateDisplay();
}

void displayDate()
{
    digits = 0;
    digits += timeControl.timeInfo.tm_mday * 10000;
    digits += (timeControl.timeInfo.tm_mon + 1) * 100; // Monat ist 0-11, zum Anzeigen +1
    digits += (timeControl.timeInfo.tm_year + 1900) % 100;
    updateDisplay();
}

void updateIfChanged(int32_t newDigits)
{
    if (newDigits != previousDigits)
    {
        digits = newDigits;
        updateDisplay();
        previousDigits = newDigits;
    }
}

void displayIP()
{
    bool runningACP2 = true;

    IPAddress ip = WiFi.localIP();
    String ipStr = ip.toString();

    String formattedBlock = "00000";
    int blockIndex = 0;

    for (size_t i = 0; i < ipStr.length(); i++)
    {
        if (ipStr[i] != '.')
        {
            formattedBlock[blockIndex * 2] = ipStr[i];
            blockIndex ++;
        }

        if (ipStr[i] == '.' || i == ipStr.length() - 1)
        {
            int32_t currentDigits = formattedBlock.toInt();
            updateIfChanged(currentDigits);

            delay(5000);

            formattedBlock = "00000";
            blockIndex = 0;
        }
    }
    runningACP2 = false;
}