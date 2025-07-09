#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>

#include "Memory/Memory.hpp"
#include "SupplyWatch/SupplyWatch.hpp"
#include "HSS/HSS.hpp"
#include "TimeWeather/NTPClient/NTPClient.hpp"
#include "ClockControl/ClockControl.hpp"
#include "EnergyMonitor/EnergyMonitor.hpp"
#include "HTTP/HTTP.hpp"
#include "Brownout/Brownout.hpp"
#include "Button/Button.hpp"
#include "WiFiConnector/WiFiConnector.hpp"
#include "Acoustics/Buzzer/Buzzer.hpp"
#include "Timer/Timer.hpp"
#include "AlarmClock/AlarmClock.hpp"

extern TaskHandle_t displayTaskHandle;
extern TaskHandle_t clockTaskHandle;
extern TaskHandle_t buttonTaskHandle;
extern TaskHandle_t brownoutTaskHandle;
extern TaskHandle_t httpTaskHandle;
extern TaskHandle_t energyMonitorTaskHandle;

extern Memory::PersistentStorage storage;
extern SupplyWatch       supplyWatch;
extern HSS               hssController;
extern ButtonPoll        buttonPoll;
extern NTPClient         ntpClient;
extern ClockControl      clockControl;
extern EnergyMonitor     energyMonitor;
extern WiFiConnector     wifiConnector;
extern HTTPHandler       httpHandler;
extern Brownout*         brownoutInstance;
extern Relay             relay;
extern OTAManager        otaManager;
extern NtcThermistor     temperatureSensor;
extern Timer             timer;
extern AlarmClock        alarmClock;
extern Buzzer            buzzer;