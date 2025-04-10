#include <WiFi.h>
#include <WiFiManager.h>
#include "ACP/ACP.h"
#include "Button/Button.h"
#include "Digit_Control/Digit.h"
#include "Melody/Melody.h"
#include "HSS/HSS.h"
#include "Time/Time.h"
#include "HTTP/HTTP.h"
#include "Logger/Logger.h"
#include <driver/adc.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <ESPmDNS.h>

//* Der DAC-Wert muss für jede Uhr individuell angepasst werden.
//* 160V bis 170V bei allen Helligkeitsstufen, 180V bis 190V bei ACP.
//  Erinnerung: Z-Diode 200V!!!

TaskHandle_t hssTaskHandle;
TaskHandle_t timeTaskHandle;
TaskHandle_t gongTaskHandle;
TaskHandle_t httpTaskHandle;

TimeControl timeControl;
HSSControl hssControl;
ButtonHandler buttonHandler;
HTTPHandler httpHandler(80); // Port 80

// Task für HSS und Button-Routinen
void hssTask(void *parameter)
{
  while (true)
  {
    hssControl.readHSS();
    hssControl.updateHSS();
    buttonHandler.buttonRoutine();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// Task für Zeitzyklen
void timeTask(void *parameter)
{
  while (true)
  {
    timeControl.timeCycle();
    vTaskDelay(pdMS_TO_TICKS(10)); 
  }
}

// Task für das Abspielen des Gong-Tons
void gongTask(void *parameter)
{
  while (true)
  {
    if (xSemaphoreTake(timeControl.getGongSemaphore(), portMAX_DELAY) == pdTRUE)
    {
      playSelectedMelody(currentMelody);
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
  pinMode(PIN_RELAY, OUTPUT);
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_4, ADC_ATTEN_DB_0);
  digitalWrite(PIN_OE, LOW); // erzwingt alle Röhren aus // * Hier entsteht das Leuchten beim einstecken!
  digitalWrite(PIN_HSS_CUTOFF, HIGH);
  dacWrite(PIN_JFET, Operating_Voltage);

  Serial.begin(115200);

  SPI.begin(PIN_CLK, -1, PIN_DIN, -1); // Wir nutzen nur clock und MOSI
  SPI.setDataMode(SPI_MODE2);
  SPI.setClockDivider(SPI_CLOCK_DIV8); // SCK = 16MHz/8 = 2MHz

  WiFiManager wifiManager;
  wifiManager.autoConnect("Nixie Clock");

  if (!MDNS.begin("nixieclock")) 
  {
    Serial.println("Fehler beim Starten des mDNS responders");

  } 
  else 
  {
    Serial.println("mDNS responder gestartet");
    MDNS.addService("http", "tcp", 80);
  }

  timeControl.initTime("CET-1CEST,M3.5.0,M10.5.0/3");

  //loadCheck();

  if (WiFi.status() != WL_CONNECTED)
  {
    digits = 123456; // Demo Modus
    displayEnabled = true;
  }

  // FreeRTOS-Tasks
  xTaskCreatePinnedToCore(hssTask , "HSSTask" , 4096, NULL, 1, &hssTaskHandle, 1); // Core 1
  xTaskCreatePinnedToCore(timeTask, "TimeTask", 4096, NULL, 1, &timeTaskHandle, 0); // Core 0
  xTaskCreatePinnedToCore(gongTask, "GongTask", 4096, NULL, 1, &gongTaskHandle, 1); // Core 1
  xTaskCreatePinnedToCore(httpTask, "HTTPTask", 4096, NULL, 2, &httpTaskHandle, 0); // Core 0
}

void loop()
{
  if(WiFi.status() == WL_CONNECTED)
  {
    httpHandler.begin();
    Serial.print("Local IP: ");
    Serial.println(WiFi.localIP().toString());
    
    if (timeControl.timeInfo.tm_hour > 16 || timeControl.timeInfo.tm_hour < 6)
    {
      displayEnabled = false;
      Logger::log(LoggerType::GENERAL, F("Display disabled"));
    }
    else
    {
      displayEnabled = true;
      Logger::log(LoggerType::GENERAL, F("Display enabled"));
    }
    
    vTaskDelete(NULL);
  }
}
