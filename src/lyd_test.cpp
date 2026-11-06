#include <Arduino.h>

// Testprogram til buzzer / lille højttaler med 2 ledninger
// Her sættes pin. D12 er fri (motorer, ultralyd og stepper bruger de andre)
// Højttaler: sæt en 100 Ω modstand i serie. Buzzer: kan sættes direkte i.
const int LYD_PIN = 12;

// Her er tonerne (frekvens i Hz)
#define PAUSE 0
#define C4 262
#define D4 294
#define DS4 311
#define F4 349
#define G4 392
#define GS4 415
#define AS4 466
#define C5 523
#define D5 587
#define DS5 622
#define F5 698
#define G5 784

const int TEMPO = 140;                       // Slag pr. minut
const int SLAG_MS = 60000 / TEMPO;           // Længde af én fjerdedelsnode i ms

// Egen melodi i "trap"-stil (C-mol). To tal pr. node: tone, længde
// Længde: 4 = fjerdedel, 8 = ottendedel, 16 = sekstendedel, 2 = halvnode
const int melodi[] = {
  // Hook
  C5, 8,  C5, 16, C5, 16, DS5, 8, C5, 8,   AS4, 8, G4, 8,  PAUSE, 4,
  C5, 8,  C5, 16, C5, 16, DS5, 8, F5, 8,   DS5, 8, D5, 8,  PAUSE, 4,
  // Svar
  G5, 8,  F5, 8,  DS5, 8, D5, 8,  C5, 8,   AS4, 8, C5, 4,
  GS4, 8, AS4, 8, C5, 8,  PAUSE, 8, G4, 2,
  // Bas-agtigt "808"-fald
  C4, 8,  PAUSE, 8, C4, 16, C4, 16, DS4, 8, F4, 8, G4, 8, F4, 8, DS4, 4,
};
const int ANTAL_NODER = sizeof(melodi) / sizeof(melodi[0]) / 2;

void spilMelodi() {
  for (int i = 0; i < ANTAL_NODER; i++) {
    int tone_hz = melodi[i * 2];
    int laengde = (SLAG_MS * 4) / melodi[i * 2 + 1];

    if (tone_hz != PAUSE) {
      tone(LYD_PIN, tone_hz, laengde * 0.9);  // 90 % lyd, 10 % stilhed, så noderne kan skelnes
    }
    delay(laengde);
  }
  noTone(LYD_PIN);
}

void setup() {
  Serial.begin(9600);
  pinMode(LYD_PIN, OUTPUT);
  Serial.println("Lydtest: bip");
  tone(LYD_PIN, 1000, 300);   // Kort bip, så vi ved den virker
  delay(1000);
}

void loop() {
  Serial.println("Spiller melodi");
  spilMelodi();
  delay(3000);
}
