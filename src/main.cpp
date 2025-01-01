#include <WiFi.h>
#include <WiFiManager.h>
#include "ACP/ACP.h"
#include "Button/Button.h"
#include "Digit_Control/Digit.h"
#include "Melody/Melody.h"
#include "HSS/HSS.h"
#include "Time/Time.h"
#include "HTTP/HTTP.h"
#include <driver/adc.h>

//* Der DAC-Wert muss für jede Uhr individuell angepasst werden.
//* 160V bis 170V bei allen Helligkeitsstufen, 180V bis 190V bei ACP.
//  Erinnerung: Z-Diode 200V!!!

TaskHandle_t hssTaskHandle;
TaskHandle_t timeTaskHandle;
TaskHandle_t gongTaskHandle;
TaskHandle_t httpTaskHandle;
HTTPHandler httpHandler;

// Task für HSS und Button-Routinen
void hssTask(void *parameter)
{
  while (true)
  {
    readHSS();
    updateHSS();
    buttonRoutine();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Task für Zeitzyklen
void timeTask(void *parameter)
{
  while (true)
  {
    timeCycle();
    vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}

// Task für das Abspielen des Gong-Tons
void gongTask(void *parameter)
{
  while (true)
  {
    if (xSemaphoreTake(gongSemaphore, portMAX_DELAY) == pdTRUE)
    {
      playGongTone();
    }
  }
}

// Task für HTTP-Client
void httpTask(void *parameter)
{
  while (true)
  {
    httpHandler.handleClient();
    vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}

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
  pinMode(PIN_RELAY, OUTPUT); //* Kein Ticker ? ---> Jumper X2 Ziehen          Wenn nicht Tickt aber Jumper steckt ---> U1 (F2)
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_0);
  digitalWrite(PIN_OE, LOW); // erzwingt alle Röhren aus // * Hier entsteht das Leuchten beim einstecken!
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


  // FreeRTOS-Tasks
  xTaskCreatePinnedToCore(hssTask , "HSSTask" , 4096, NULL, 1, & hssTaskHandle, 1); // Core 1
  xTaskCreatePinnedToCore(timeTask, "TimeTask", 4096, NULL, 1, &timeTaskHandle, 0); // Core 0
  xTaskCreatePinnedToCore(gongTask, "GongTask", 4096, NULL, 1, &gongTaskHandle, 1); // Core 1
  xTaskCreatePinnedToCore(timeTask, "HTTPTask", 4096, NULL, 2, &httpTaskHandle, 0); // Core 0

  httpHandler.begin();

  Serial.print("Local IP: ");
  Serial.println(WiFi.localIP().toString());

  vTaskDelete(NULL);
}