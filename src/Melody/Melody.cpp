#include "Melody/Melody.h"

MelodyType currentMelody = GONG;

int wholenote = (60000 * 4) / tempo;

int marioMelody[] = {
  // Bar 1:
  NOTE_E5, 8,
  NOTE_E5, 8,
  NOTE_E5, 8,
  NOTE_C5, 8,
  NOTE_E5, 4,
  // Bar 2:
  NOTE_G5, 4,
  NOTE_G4, 4,
  // Bar 3:
  NOTE_C5, 4,
  NOTE_G4, 4,
  NOTE_E4, 4,
  // Bar 4:
  NOTE_A4, 4,
  NOTE_B4, 4,
  NOTE_AS4, 8,
  NOTE_A4, 4,
  // Bar 5:
  NOTE_G4, 6, 
  NOTE_E5, 6,  
  NOTE_G5, 6,
  NOTE_A5, 4,
  NOTE_F5, 8,
  NOTE_G5, 8,
  // Bar 6:
  NOTE_E5, 4,
  NOTE_C5, 8,
  NOTE_D5, 8,
  NOTE_B4, 4,
  // Bar 3-2:
  NOTE_C5, 4,
  NOTE_G4, 4,
  NOTE_E4, 4,
  // Bar 4-2:
  NOTE_A4, 4,
  NOTE_B4, 4,
  NOTE_AS4, 8,
  NOTE_A4, 4,
  // Bar 5-2:
  NOTE_G4, 6,
  NOTE_E5, 6,
  NOTE_G5, 6,
  NOTE_A5, 4,
  NOTE_F5, 8,
  NOTE_G5, 8,
  // Bar 6-2:
  NOTE_E5, 4,
  NOTE_C5, 8,
  NOTE_D5, 8,
  NOTE_B4, 4,
  // Bar 7:
  NOTE_G5, 8,
  NOTE_FS5, 8,
  NOTE_F5, 8,
  NOTE_DS5, 4,
  NOTE_E5, 8,
  // Bar 8:
  NOTE_GS4, 8,
  NOTE_A4, 8,
  NOTE_C5, 8,
  NOTE_A4, 8,
  NOTE_C5, 8,
  NOTE_D5, 8,
  // Bar 9:
  NOTE_G5, 8,
  NOTE_FS5, 8,
  NOTE_F5, 8,
  NOTE_DS5, 4,
  NOTE_E5, 8,
  // Bar 10:
  NOTE_C6, 4,
  NOTE_C6, 8,
  NOTE_C6, 4,
  // Bar 11:
  NOTE_G5, 8,
  NOTE_FS5, 8,
  NOTE_F5, 8,
  NOTE_DS5, 4,
  NOTE_E5, 8,
  // Bar 12:
  NOTE_GS4, 8,
  NOTE_A4, 8,
  NOTE_C5, 8,
  NOTE_A4, 8,
  NOTE_C5, 8,
  NOTE_D5, 8,
  // Bar 13:
  NOTE_DS5, 4,
  NOTE_D5, 4,
  // Bar 14:
  NOTE_C5, 4,
  // Bar 7-2:
  NOTE_G5, 8,
  NOTE_FS5, 8,
  NOTE_F5, 8,
  NOTE_DS5, 4,
  NOTE_E5, 8,
  // Bar 8-2:
  NOTE_GS4, 8,
  NOTE_A4, 8,
  NOTE_C5, 8,
  NOTE_A4, 8,
  NOTE_C5, 8,
  NOTE_D5, 8,
  // Bar 9-2:
  NOTE_G5, 8,
  NOTE_FS5, 8,
  NOTE_F5, 8,
  NOTE_DS5, 4,
  NOTE_E5, 8,
  // Bar 10-2:
  NOTE_C6, 4,
  NOTE_C6, 8,
  NOTE_C6, 4,
  // Bar 11-2:
  NOTE_G5, 8,
  NOTE_FS5, 8,
  NOTE_F5, 8,
  NOTE_DS5, 4,
  NOTE_E5, 8,
  // Bar 12-2:
  NOTE_GS4, 8,
  NOTE_A4, 8,
  NOTE_C5, 8,
  NOTE_A4, 8,
  NOTE_C5, 8,
  NOTE_D5, 8,
  // Bar 13-2:
  NOTE_DS5, 4,
  NOTE_D5, 4,
  // Bar 14-2:
  NOTE_C5, 4,
  // Bar 15:
  NOTE_C5, 8,
  NOTE_C5, 8,
  NOTE_C5, 8,
  NOTE_C5, 8,
  NOTE_D5, 4,
  // Bar 16:
  NOTE_E5, 8,
  NOTE_C5, 8,
  NOTE_A4, 8,
  NOTE_G4, 2,
  // Bar 17:
  NOTE_C5, 8,
  NOTE_C5, 8,
  NOTE_C5, 8,
  NOTE_C5, 8,
  NOTE_D5, 8,
  NOTE_E5, 8
};

int marioNotes = sizeof(marioMelody) / sizeof(marioMelody[0]) / 2;

int gongMelody[] = {
    NOTE_E5, 8,
    NOTE_D5, 8,
    NOTE_FS4, 4,
    NOTE_GS4, 4,
    NOTE_CS5, 8,
    NOTE_B4, 8,
    NOTE_D4, 4,
    NOTE_E4, 4,
    NOTE_B4, 8,
    NOTE_A4, 8,
    NOTE_CS4, 4,
    NOTE_E4, 4,
    NOTE_A4, 2,
};

int gongNotes = sizeof(gongMelody) / sizeof(gongMelody[0]) / 2;

void playMelody(int *melody, int notes) {
  for (int thisNote = 0; thisNote < notes * 2; thisNote += 2) {
    int divider = melody[thisNote + 1];
    int noteDuration = (divider > 0) ? (wholenote / divider)
                                     : ((wholenote / abs(divider)) * 1.5);
    tone(PIN_BUZZER, melody[thisNote], noteDuration * 0.9);
    delay(noteDuration);
    noTone(PIN_BUZZER);
  }
}

void playSelectedMelody(MelodyType type) {
  switch (type) {
    case MARIO: {
      int originalWholenote = wholenote;   
      wholenote = originalWholenote * 1.6f;         
      playMelody(marioMelody, marioNotes);
      wholenote = originalWholenote;           
      break;
    }
    case GONG:
      playMelody(gongMelody, gongNotes);
      break;
  }
}
