#ifndef MELODY_H
#define MELODY_H

#include <Arduino.h>

// Definiert Pin und Tempo
constexpr uint8_t PIN_BUZZER = 22;
constexpr uint8_t tempo = 180;

// Definiert die Noten
constexpr uint16_t NOTE_CS4 = 277;
constexpr uint16_t NOTE_D4  = 294;
constexpr uint16_t NOTE_E4  = 330;
constexpr uint16_t NOTE_A4  = 440;
constexpr uint16_t NOTE_B4  = 494;
constexpr uint16_t NOTE_CS5 = 554;
constexpr uint16_t NOTE_D5  = 587;
constexpr uint16_t NOTE_E5  = 659;
constexpr uint16_t NOTE_FS4 = 370;
constexpr uint16_t NOTE_GS4 = 415;

// Melodie-Array
extern int melody[];

// Funktion
void playGongTone();

#endif // MELODY_H