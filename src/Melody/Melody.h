#ifndef MELODY_H
#define MELODY_H

#include <Arduino.h>

// Hardware und Tempo
constexpr uint8_t PIN_BUZZER = 22;
constexpr uint8_t tempo = 180;

// Noten
constexpr uint16_t NOTE_CS4 = 277;
constexpr uint16_t NOTE_D4  = 294;
constexpr uint16_t NOTE_E4  = 330;
constexpr uint16_t NOTE_F4  = 349;
constexpr uint16_t NOTE_FS4 = 370;
constexpr uint16_t NOTE_G4  = 392;
constexpr uint16_t NOTE_GS4 = 415;
constexpr uint16_t NOTE_A4  = 440;
constexpr uint16_t NOTE_AS4 = 466;  // A#4
constexpr uint16_t NOTE_B4  = 494;
constexpr uint16_t NOTE_C5  = 523;
constexpr uint16_t NOTE_CS5 = 554;
constexpr uint16_t NOTE_D5  = 587;
constexpr uint16_t NOTE_E5  = 659;
constexpr uint16_t NOTE_F5  = 698;
constexpr uint16_t NOTE_FS5 = 740;  // F#5
constexpr uint16_t NOTE_G5  = 784;
constexpr uint16_t NOTE_A5  = 880;
constexpr uint16_t NOTE_DS5 = 622;  // D#5
constexpr uint16_t NOTE_C6  = 1047;

extern int wholenote;
extern int marioMelody[];
extern int marioNotes;
extern int gongMelody[];
extern int gongNotes;

enum MelodyType {
  MARIO,
  GONG
};

void playMelody(MelodyType type);
void playSelectedMelody(MelodyType type);

extern MelodyType currentMelody;

#endif // MELODY_H
