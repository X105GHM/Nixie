#ifndef ACP_H
#define ACP_H

#include <Arduino.h>

// Globale Variablen
extern bool runningACP1;
extern bool runningACP2;
extern bool runningManualACP;
extern int32_t previousDigits; 

// Funktionsdeklaration
void ACP();

#endif // ACP_H