#ifndef TIME_CONTROL_H
#define TIME_CONTROL_H

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

// Konstanten
constexpr uint8_t PIN_LED = 26;

// Gong Zeit
constexpr int numGongTimes = 15;

constexpr int gongHours[numGongTimes]  =  {8,  8,  9,  9, 10, 11, 11, 12, 13, 13, 14, 15, 16, 16, 16};
constexpr int gongMinutes[numGongTimes] = {0, 45, 30, 40, 25, 10, 30, 15,  0, 45, 30, 15, 0,  5, 50};
constexpr int gongSeconds[numGongTimes] = {0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 0};

class TimeControl {
public:

    TimeControl();
    void setTimezone(String timezone);
    void initTime(String timezone);
    void blinkError();
    void timeCycle();
    SemaphoreHandle_t getGongSemaphore();

    struct tm timeInfo;

private:

    time_t prevTime;
    uint32_t lastRefresh;
    SemaphoreHandle_t gongSemaphore;
};

#endif // TIME_CONTROL_H
