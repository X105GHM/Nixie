#include "Globals/SharedObjects.hpp"

TaskHandle_t displayTaskHandle       = nullptr;
TaskHandle_t clockTaskHandle         = nullptr;
TaskHandle_t buttonTaskHandle        = nullptr;
TaskHandle_t brownoutTaskHandle      = nullptr;
TaskHandle_t httpTaskHandle          = nullptr;
TaskHandle_t energyMonitorTaskHandle = nullptr;

Memory::PersistentStorage   storage;
SupplyWatch         supplyWatch;
HSS                 hssController;
ButtonPoll          buttonPoll(BUTTON_PIN);
NTPClient           ntpClient;
ClockControl        clockControl(ntpClient, hssController);
EnergyMonitor       energyMonitor(supplyWatch, 12.0f);
WiFiConnector       wifiConnector;
HTTPHandler         httpHandler(80);
Brownout*           brownoutInstance = nullptr;
Relay               relay;
OTAManager          otaManager;
NtcThermistor       temperatureSensor;

