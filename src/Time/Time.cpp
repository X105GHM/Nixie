#include "HSS/HSS.h"
#include "ACP/ACP.h"
#include "Digit_Control/Digit.h"
#include "Button/Button.h"
#include "time.h"

struct tm timeInfo;
time_t prevTime = 0;
uint32_t lastRefresh = 0;

void setTimezone(String timezone)
{
  Serial.printf("  Setting Timezone to %s\n", timezone.c_str());
  setenv("TZ", timezone.c_str(), 1);    // * Passen Sie nun die TZ an.  Die Uhreinstellungen werden angepasst, um die neue Ortszeit anzuzeigen
  tzset();
}

void initTime(String timezone)
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
  // Jetzt können wir die echte Zeitzone festlegen
  setTimezone(timezone);
}

void blinkError()
{
  for (int i = 0; i < 10; i++)
  {
    digitalWrite(PIN_LED, HIGH);
    delay(100);
    digitalWrite(PIN_LED, LOW);
    delay(100);
  }
}

void timeCycle() 
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
  { // Aktualisien die Zeit jede Sekunde
    prevTime = nowTime;

    Relay_State = !Relay_State;

    digitalWrite(PIN_RELAY, Relay_State);

    // Helligkeitskontrolle
    if (timeInfo.tm_hour < 6 || timeInfo.tm_hour >= 22)
    {
      brightness = 10;
    }
    else if (timeInfo.tm_hour < 8 || timeInfo.tm_hour >= 20)
    {
      brightness = 75;
    }
    else
    {
      brightness = 100;
    }

    if (timeInfo.tm_min % 10 == 9 && timeInfo.tm_sec >= 50 && timeInfo.tm_sec < 55)
    {
      displayDate(); // legt die anzuzeigenden Ziffern fest
    }
    else if ((timeInfo.tm_min == 57 && timeInfo.tm_sec == 10) || (timeInfo.tm_min == 27 && timeInfo.tm_sec == 10)) // at xx:57:10 and at xx:27:10
    {
      digitalWrite(PIN_RELAY, LOW);
      dacWrite(PIN_JFET, 35);
      ACP(); // Blockierend
      dacWrite(PIN_JFET, 50);
    }
    else
    {
      displayTime(); // Legt die anzuzeigenden Ziffern fest
    }
  }
  if (!displayEnabled)
  {
    displayEnabled = true;
  }
}