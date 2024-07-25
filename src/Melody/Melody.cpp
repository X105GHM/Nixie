/*
#include "Melody/Melody.h"
#include "Button/Button.h"

// Definiere die Melodie und die Notendauern
constexpr int melody[] = {
  NOTE_E4, NOTE_E4, NOTE_E4, // Jingle bells, jingle bells
  NOTE_E4, NOTE_E4, NOTE_E4, // Jingle all the way
  NOTE_E4, NOTE_G4, NOTE_C4, NOTE_D4, // Oh what fun it is to ride
  NOTE_E4, NOTE_F4, NOTE_F4, NOTE_F4, NOTE_F4, NOTE_F4, NOTE_E4, // In a one horse open sleigh
  NOTE_E4, NOTE_E4, NOTE_E4, NOTE_E4, // Jingle bells, jingle bells
  NOTE_E4, NOTE_E4, NOTE_E4, NOTE_G4, // Jingle all the way
  NOTE_G4, NOTE_G4, NOTE_G4, NOTE_F4, NOTE_D4, NOTE_C4 // Oh what fun it is to ride
};

constexpr u_int8_t noteDurations[] = {
  8, 8, 8, 8, 8, 8, 8, 8, 8, 4, 4, 8, 8, 8, 8, 8, 8, 4, 4, 8, 8, 8, 8, 8, 8, 4, 4, 4, 4, 8, 8, 8, 8
};

void playJingleBells() {
  for (int thisNote = 0; thisNote < sizeof(melody)/sizeof(melody[0]); thisNote++) {
    int noteDuration = 1000 / noteDurations[thisNote];
    tone(PIN_BUZZER, melody[thisNote], noteDuration);

    int pauseBetweenNotes = noteDuration * 1.30;
    delay(pauseBetweenNotes);
    noTone(PIN_BUZZER);
  }
}
*///