#ifndef HSS_CONTROL_H
#define HSS_CONTROL_H

#include <Arduino.h>

// Konstanten
constexpr uint8_t PIN_OE = 27;
constexpr uint8_t PIN_HSS_V = 34;
constexpr uint8_t PIN_HSS_CUTOFF = 33;
constexpr uint8_t PIN_JFET = 25;
constexpr uint8_t PIN_HV_LED = 2;
constexpr uint8_t Operating_Voltage_Day = 80; //50 170V // ? Frage: Geht auch kleiner 170V? ----> Lebenszeit ?
constexpr uint8_t Operating_Voltage_Noon = 100; //75 170V // ? Frage: Geht auch kleiner 170V? ----> Lebenszeit ?
constexpr uint8_t Operating_Voltage_Night = 120; //95 170V // ? Frage: Geht auch kleiner 170V? ----> Lebenszeit ?
constexpr uint8_t ACP_Voltage = 35; // 185V       // ! Achtung: Z-Diode ~200V, > 200V = Nixie TOT !
constexpr uint16_t  NUM_SAMPLES = 200;

// Globale Variablen
extern bool HSS_LED;
extern bool HSS_CUTOFF;
extern bool samplesFilled;
extern float HSS_V;
extern u_int16_t samples[NUM_SAMPLES];
extern u_int8_t sampleIndex;

// Funktionsdeklarationen
void readHSS();
void updateHSS();
void loadCheck();

#endif // HSS_CONTROL_H