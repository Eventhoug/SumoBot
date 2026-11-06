#include <Arduino.h>
#include "sang.h"   // Laves med: node tools/midi2melodi.js <fil.mid> <spor>

// Afspiller til sang fra en MIDI-fil på buzzer / lille højttaler
// Her sættes pin (samme som lyd_test)
const int LYD_PIN = 12;

void spilSang() {
  for (uint16_t i = 0; i < SANG_ANTAL; i++) {
    // Her hentes tone og længde fra flash-hukommelsen
    uint16_t hz = pgm_read_word(&SANG[i * 2]);
    uint16_t ms = pgm_read_word(&SANG[i * 2 + 1]);

    if (hz > 0) {
      // Her laves et kort hul (10 ms) til sidst, så ens toner i træk kan høres hver for sig.
      // Et fast hul i stedet for 10 % gør, at hurtige noder ikke bliver hakkede.
      uint16_t lyd = (ms > 40) ? ms - 10 : ms;
      tone(LYD_PIN, hz);
      delay(lyd);
      noTone(LYD_PIN);
      delay(ms - lyd);
    } else {
      delay(ms);
    }
  }
  noTone(LYD_PIN);
}

void setup() {
  Serial.begin(9600);
  pinMode(LYD_PIN, OUTPUT);
  Serial.print("Sang med ");
  Serial.print(SANG_ANTAL);
  Serial.println(" noder");
}

void loop() {
  Serial.println("Spiller sang");
  spilSang();
  delay(3000);
}
