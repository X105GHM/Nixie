#include "HSS/HSS.h"
#include "ACP/ACP.h"
#include "Digit_Control/Digit.h"
#include "Button/Button.h"
#include "Melody/Melody.h"
#include "Logger/Logger.h"
#include "time.h"
#include <stdlib.h>

TimeControl::TimeControl() : prevTime(0), lastRefresh(0) {
    gongSemaphore = xSemaphoreCreateBinary();
}

SemaphoreHandle_t TimeControl::getGongSemaphore() {
    return gongSemaphore;
}

void TimeControl::setTimezone(String timezone)
{
  Serial.printf("  Setting Timezone to %s\n", timezone.c_str());
  setenv("TZ", timezone.c_str(), 1); // * Passe die TZ an.  Die Uhreinstellungen werden angepasst, um die neue Ortszeit anzuzeigen
  tzset();
}

void TimeControl::initTime(String timezone)
{
  Serial.println("Setting up time");
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("No internet, skipping NTP setup");
    return;
  }
  configTime(0, 0, "pool.ntp.org"); // Zuerst eine Verbindung zum NTP-Server herstellen, mit einem TZ-Offset von 0
  if (!getLocalTime(&timeInfo))
  {
    Serial.println("  Failed to obtain time");
    blinkError();
    return;
  }
  Serial.println("  Got the time from NTP");
  setTimezone(timezone);
}

void TimeControl::blinkError()
{

  Logger::log(LoggerType::GENERAL, F("!!!ERROR!!!"));

  for (int i = 0; i < 10; i++)
  {
    digitalWrite(PIN_LED, HIGH);
    vTaskDelay(100);
    digitalWrite(PIN_LED, LOW);
    vTaskDelay(100);
  }
}

void TimeControl::timeCycle()
{
  if (!getLocalTime(&timeInfo))
  {
    Serial.println("Failed to obtain time");
    blinkError();
    digits = (digits + random(111111, 999999)) % 1000000;
    return;
  }
  time_t nowTime = mktime(&timeInfo);
  if (nowTime != prevTime)
  { // Aktualisiert die Zeit jede Sekunde
    prevTime = nowTime;

    Relay_State = !Relay_State;

    digitalWrite(PIN_RELAY, Relay_State);

    // HV LED
    if (HSS_V > 60.0 && HSS_LED == false)
    {
      digitalWrite(PIN_HV_LED, HIGH);
      HSS_LED = true;
    }
    else if (HSS_V < 60.0 && HSS_LED == true)
    {
      digitalWrite(PIN_HV_LED, LOW);
      HSS_LED = false;
    }

    if (timeInfo.tm_min % 10 == 9 && timeInfo.tm_sec >= 50 && timeInfo.tm_sec < 55 && displayEnabled)
    {
      Logger::log(LoggerType::TIME, F("Displaying Date"));
      displayDate();
    }
    else if ((timeInfo.tm_min == 57 && timeInfo.tm_sec == 10) || (timeInfo.tm_min == 27 && timeInfo.tm_sec == 10) && displayEnabled) // at xx:57:10 and at xx:27:10
    {
      Logger::log(LoggerType::TIME, F("Running ACP"));
      digitalWrite(PIN_RELAY, LOW);
      dacWrite(PIN_JFET, ACP_Voltage);
      ACP(); // Blockierend
      dacWrite(PIN_JFET, Operating_Voltage);
      digitalWrite(PIN_HSS_CUTOFF, LOW);
    }
    else
    {
      displayTime();
    }

    for (int i = 0; i < numGongTimes; i++)
    {
      if (timeInfo.tm_hour == gongHours[i] && timeInfo.tm_min == gongMinutes[i] && timeInfo.tm_sec == gongSeconds[i])
      {
        currentMelody = GONG;
        Logger::log(LoggerType::TIME, F("Playing Gong"));
        xSemaphoreGive(gongSemaphore); // Gong-Ton abspielen
      }
    }

    if (timeInfo.tm_hour == 17 && timeInfo.tm_min == 00 && timeInfo.tm_sec == 00)
    {
      displayEnabled = false;
    }
    else if (timeInfo.tm_hour == 7 && timeInfo.tm_min == 00 && timeInfo.tm_sec == 00)
    {
      displayEnabled = true;
    }
  }
}
