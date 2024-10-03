#include <WiFi.h>
#include <WiFiManager.h>
#include "ACP/ACP.h"
#include "Button/Button.h"
#include "Digit_Control/Digit.h"
#include "HSS/HSS.h"
#include "Time/Time.h"
#include <driver/adc.h>

//* Der DAC-Wert muss für jede Uhr individuell angepasst werden.
//* 160V bis 170V bei allen Helligkeitsstufen, 180V bis 190V bei ACP.
//  Erinnerung: Z-Diode 200V!!!


// Konstanten
constexpr uint8_t PIN_DIN = 13;
constexpr uint8_t PIN_CLK = 14;

TaskHandle_t mainTask;

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

  wifiManager.autoConnect("Nixie Clock");

  initTime("CET-1CEST,M3.5.0,M10.5.0/3");

  //loadCheck();

  if (WiFi.status() != WL_CONNECTED)
  {
    digits = 123456; // Demo Modus
    displayEnabled = true;
  }
}

void loop()
{
  readHSS();

  updateHSS();

  buttonRoutine();

  timeCycle();
}