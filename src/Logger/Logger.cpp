#include "Logger/Logger.h"

static const __FlashStringHelper* getLoggerPrefix(LoggerType type) {
    switch(type) {
        case LoggerType::HTTP:
            return F("HTTP-Logger");
        case LoggerType::TIME:
            return F("Time-Logger");
        case LoggerType::HSS:
            return F("HSS-Logger");
        case LoggerType::GENERAL:
        default:
            return F("General-Logger");
    }
}

void Logger::log(LoggerType type, const __FlashStringHelper* message) {
    Serial.println();
    Serial.println(F("========================================"));
    Serial.println(getLoggerPrefix(type));
    Serial.println(F("----------------------------------------"));                                                    
    Serial.println();
    Serial.println(message);
    Serial.println(); 
    Serial.println(F("========================================"));
    Serial.println();
}
