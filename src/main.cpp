#include <WiFi.h>
#include <WiFiManager.h>
#include "ACP/ACP.h"
#include "Button/Button.h"
#include "Digit_Control/Digit.h"
#include "Melody/Melody.h"
#include "HSS/HSS.h"
#include "Time/Time.h"
#include <driver/adc.h>
#include "HTTP-Server/HTTP-Server.h"

//* Der DAC-Wert muss für jede Uhr individuell angepasst werden.
//* 160V bis 170V bei allen Helligkeitsstufen, 180V bis 190V bei ACP.
//  Erinnerung: Z-Diode 200V!!!

int32_t currentDigits = 0; // Standardpreis: 2.50 als ganze Zahl (250)
bool isDisplayActive = false;

// Konstanten
constexpr uint8_t PIN_DIN = 13;
constexpr uint8_t PIN_CLK = 14;

TaskHandle_t mainTask;

HTTP_Server httpServer;

void setup()
{
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(FUNC_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PIN_HSS_CUTOFF, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_OE, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_JFET, OUTPUT);
  pinMode(PIN_HV_LED, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);  //* Kein Ticker ? ---> Jumper X2 Ziehen          Wenn nicht Tickt aber Jumper steckt ---> U1 (F2)
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_0);
  digitalWrite(PIN_OE, LOW);  // erzwingt alle Röhren aus // * Hier entsteht das Leuchten beim einstecken!
  digitalWrite(PIN_HSS_CUTOFF, HIGH);
  dacWrite(PIN_JFET, Operating_Voltage);

  Serial.begin(115200);

  SPI.begin(PIN_CLK, -1, PIN_DIN, -1); // Wir nutzen nur clock und MOSI
  SPI.setDataMode(SPI_MODE2);
  SPI.setClockDivider(SPI_CLOCK_DIV8); // SCK = 16MHz/8 = 2MHz

  httpServer.begin();

  /***wifiManager.autoConnect("Nixie Clock");

  initTime("CET-1CEST,M3.5.0,M10.5.0/3");

  //loadCheck();

  if (WiFi.status() != WL_CONNECTED)
  {
    digits = 123456; // Demo Modus
    displayEnabled = true;
  }
  ***/
}

void loop()
{
    httpServer.handleClient();

    int32_t newDigits = httpServer.getDigits();
    if (httpServer.isDisplayEnabled() && (newDigits != currentDigits || !displayEnabled))
    {
        currentDigits = newDigits;
        digits = currentDigits;

        if (!displayEnabled)
        {
            displayEnabled = true; 
            isDisplayActive = true;
        }

        updateHSS();    
        updateDisplay(); 
    }
    else if (!httpServer.isDisplayEnabled() && isDisplayActive)
    {
        displayEnabled = false;
        isDisplayActive = false;
        updateHSS();
    }

    if (httpServer.isBuzzerActive())
    {
        playGongTone();
    }

    readHSS();
    updateHSS();

    if (HSS_V > 60.0 && HSS_LED == false) {
        digitalWrite(PIN_HV_LED, HIGH);
        HSS_LED = true;
    } else if (HSS_V < 60.0 && HSS_LED == true) {
        digitalWrite(PIN_HV_LED, LOW);
        HSS_LED = false;
    }

    // buttonRoutine();
    // timeCycle();
}