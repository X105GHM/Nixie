#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>


enum class LoggerType {
    HTTP,
    TIME,
    HSS,
    DIGIT,
    BUTTON,
    GENERAL
};

class Logger {
public:
    static void log(LoggerType type, const __FlashStringHelper* message);
};

#endif // LOGGER_H
