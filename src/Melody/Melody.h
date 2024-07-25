/*
#ifndef MELODY_H
#define MELODY_H

#include <Arduino.h>

// Definiere die Frequenzen der Noten
constexpr u_int16_t NOTE_B0 = 31;
constexpr u_int16_t NOTE_C1 = 33;
constexpr u_int16_t NOTE_CS1 = 35;
constexpr u_int16_t NOTE_D1 = 37;
constexpr u_int16_t NOTE_DS1 = 39;
constexpr u_int16_t NOTE_E1 = 41;
constexpr u_int16_t NOTE_F1 = 44;
constexpr u_int16_t NOTE_FS1 = 46;
constexpr u_int16_t NOTE_G1 = 49;
constexpr u_int16_t NOTE_GS1 = 52;
constexpr u_int16_t NOTE_A1 = 55;
constexpr u_int16_t NOTE_AS1 = 58;
constexpr u_int16_t NOTE_B1 = 62;
constexpr u_int16_t NOTE_C2 = 65;
constexpr u_int16_t NOTE_CS2 = 69;
constexpr u_int16_t NOTE_D2 = 73;
constexpr u_int16_t NOTE_DS2 = 78;
constexpr u_int16_t NOTE_E2 = 82;
constexpr u_int16_t NOTE_F2 = 87;
constexpr u_int16_t NOTE_FS2 = 93;
constexpr u_int16_t NOTE_G2 = 98;
constexpr u_int16_t NOTE_GS2 = 104;
constexpr u_int16_t NOTE_A2 = 110;
constexpr u_int16_t NOTE_AS2 = 117;
constexpr u_int16_t NOTE_B2 = 123;
constexpr u_int16_t NOTE_C3 = 131;
constexpr u_int16_t NOTE_CS3 = 139;
constexpr u_int16_t NOTE_D3 = 147;
constexpr u_int16_t NOTE_DS3 = 156;
constexpr u_int16_t NOTE_E3 = 165;
constexpr u_int16_t NOTE_F3 = 175;
constexpr u_int16_t NOTE_FS3 = 185;
constexpr u_int16_t NOTE_G3 = 196;
constexpr u_int16_t NOTE_GS3 = 208;
constexpr u_int16_t NOTE_A3 = 220;
constexpr u_int16_t NOTE_AS3 = 233;
constexpr u_int16_t NOTE_B3 = 247;
constexpr u_int16_t NOTE_C4 = 262;
constexpr u_int16_t NOTE_CS4 = 277;
constexpr u_int16_t NOTE_D4 = 294;
constexpr u_int16_t NOTE_DS4 = 311;
constexpr u_int16_t NOTE_E4 = 330;
constexpr u_int16_t NOTE_F4 = 349;
constexpr u_int16_t NOTE_FS4 = 370;
constexpr u_int16_t NOTE_G4 = 392;
constexpr u_int16_t NOTE_GS4 = 415;
constexpr u_int16_t NOTE_A4 = 440;
constexpr u_int16_t NOTE_AS4 = 466;
constexpr u_int16_t NOTE_B4 = 494;
constexpr u_int16_t NOTE_C5 = 523;
constexpr u_int16_t NOTE_CS5 = 554;
constexpr u_int16_t NOTE_D5 = 587;
constexpr u_int16_t NOTE_DS5 = 622;
constexpr u_int16_t NOTE_E5 = 659;
constexpr u_int16_t NOTE_F5 = 698;
constexpr u_int16_t NOTE_FS5 = 740;
constexpr u_int16_t NOTE_G5 = 784;
constexpr u_int16_t NOTE_GS5 = 831;
constexpr u_int16_t NOTE_A5 = 880;
constexpr u_int16_t NOTE_AS5 = 932;
constexpr u_int16_t NOTE_B5 = 988;
constexpr u_int16_t NOTE_C6 = 1047;
constexpr u_int16_t NOTE_CS6 = 1109;
constexpr u_int16_t NOTE_D6 = 1175;
constexpr u_int16_t NOTE_DS6 = 1245;
constexpr u_int16_t NOTE_E6 = 1319;
constexpr u_int16_t NOTE_F6 = 1397;
constexpr u_int16_t NOTE_FS6 = 1480;
constexpr u_int16_t NOTE_G6 = 1568;
constexpr u_int16_t NOTE_GS6 = 1661;
constexpr u_int16_t NOTE_A6 = 1760;
constexpr u_int16_t NOTE_AS6 = 1865;
constexpr u_int16_t NOTE_B6 = 1976;
constexpr u_int16_t NOTE_C7 = 2093;
constexpr u_int16_t NOTE_CS7 = 2217;
constexpr u_int16_t NOTE_D7 = 2349;
constexpr u_int16_t NOTE_DS7 = 2489;
constexpr u_int16_t NOTE_E7 = 2637;
constexpr u_int16_t NOTE_F7 = 2794;
constexpr u_int16_t NOTE_FS7 = 2960;
constexpr u_int16_t NOTE_G7 = 3136;
constexpr u_int16_t NOTE_GS7 = 3322;
constexpr u_int16_t NOTE_A7 = 3520;
constexpr u_int16_t NOTE_AS7 = 3729;
constexpr u_int16_t NOTE_B7 = 3951;
constexpr u_int16_t NOTE_C8 = 4186;
constexpr u_int16_t NOTE_CS8 = 4435;
constexpr u_int16_t NOTE_D8 = 4699;
constexpr u_int16_t NOTE_DS8 = 4978;

void playJingleBells();

#endif // MELODY_H
*/