#include "Melody/Melody.h"
#include "Button/Button.h"

// Melodie-Array
int melody[] = {
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

// Die Anzahl der Noten berechnen
int notes = sizeof(melody) / sizeof(melody[0]) / 2;

// Die Dauer einer ganzen Note berechnen
int wholenote = (60000 * 4) / tempo;

void playGongTone()
{
  for (int thisNote = 0; thisNote < notes * 2; thisNote += 2)
  {
    int divider = melody[thisNote + 1];
    int noteDuration = (divider > 0) ? (wholenote / divider) : (wholenote / abs(divider)) * 1.5;
    tone(PIN_BUZZER, melody[thisNote], noteDuration * 0.9);
    vTaskDelay(noteDuration);
    noTone(PIN_BUZZER);
  }
}