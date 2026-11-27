#include <Arduino.h>
#include <Servo.h>
#include "sang.h"   // Laves med: node tools/midi2melodi.js <fil.mid> <spor>

// Samler alle testprogrammer, så de kører SAMTIDIG:
// ultralyd, stepmotor, 2 DC-motorer, servo og lyd (melodi + MIDI-sang skiftes på samme buzzer).
//
// Tricket: ingen delay() noget sted. Hver del har sin egen "opdater"-funktion, som loop()
// kalder hele tiden. Funktionen kigger på millis()/micros() og gør kun noget, når det er tid.
//
// Timere på Uno: tone() bruger Timer2 (pin 3 og 11 kan ikke lave PWM), Servo bruger Timer1
// (pin 9 og 10 kan ikke lave PWM). Det går fint, fordi 3, 9 og 10 kun bruges som tænd/sluk,
// og farten på DC-motorerne styres med pin 5 og 6 (Timer0).

// ===================== Pins =====================
// Ultralyd (HC-SR04)
const int TRIG_PIN = 7;
const int ECHO_PIN = 8;            // Skal være pin 8, fordi interruptet herunder lytter på den
const int LED_PIN = LED_BUILTIN;   // LED på pin 13 lyser, når noget er tæt på

// Stepmotor (28BYJ-48 via ULN2003)
const int S_IN1 = A0;
const int S_IN2 = A1;
const int S_IN3 = A2;
const int S_IN4 = A3;

// DC-motorer (L298N)
const int ENA = 5;   // Fart venstre (PWM)
const int IN1 = 4;
const int IN2 = 3;
const int ENB = 6;   // Fart højre (PWM)
const int IN3 = 9;
const int IN4 = 10;

// Servo og lyd
const int SERVO_PIN = 11;
const int LYD_PIN = 12;

// ===================== Ultralyd =====================
// pulseIn() venter op til 25 ms, og så ville stepmotoren hakke. I stedet måles ekkoet med et
// pin-change-interrupt på pin 8: det husker tiden, når ECHO går høj, og regner længden ud, når den går lav.
const int MODSTANDER_CM = 30;
const unsigned long MAALE_INTERVAL_MS = 60;   // Min. 60 ms mellem målinger, så gamle ekkoer er væk
const unsigned long PRINT_INTERVAL_MS = 500;

volatile unsigned long ekkoStart = 0;
volatile unsigned long ekkoLaengde = 0;
volatile bool ekkoKlar = false;

long afstandCm = -1;                 // Seneste måling, -1 = intet ekko
unsigned long sidsteMaaling = 0;
unsigned long sidstePrint = 0;

ISR(PCINT0_vect) {
  if (PINB & _BV(PB0)) {             // Pin 8 = PB0 gik høj
    ekkoStart = micros();
  } else {                           // Pin 8 gik lav igen
    ekkoLaengde = micros() - ekkoStart;
    ekkoKlar = true;
  }
}

void ultralydSetup() {
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  PCICR |= _BV(PCIE0);     // Her slås pin-change-interrupt til for pin 8-13
  PCMSK0 |= _BV(PCINT0);   // ... men kun pin 8
}

void ultralydOpdater(unsigned long nu) {
  if (nu - sidsteMaaling >= MAALE_INTERVAL_MS) {
    sidsteMaaling = nu;

    // Her hentes resultatet af sidste måling (kom der intet ekko, eller over ca. 4 m, er det -1)
    noInterrupts();
    bool klar = ekkoKlar;
    unsigned long tid = ekkoLaengde;
    ekkoKlar = false;
    interrupts();
    // Under 116 µs (2 cm) kan sensoren ikke måle, så det er støj (fx hvis ECHO ikke er forbundet)
    afstandCm = (klar && tid >= 116UL && tid < 25000UL) ? (long)(tid / 58) : -1;

    digitalWrite(LED_PIN, (afstandCm >= 0 && afstandCm <= MODSTANDER_CM) ? HIGH : LOW);

    // Her sendes en ny 10 µs puls på TRIG (så kort, at det ikke forstyrrer de andre)
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
  }

  if (nu - sidstePrint >= PRINT_INTERVAL_MS) {
    sidstePrint = nu;
    if (afstandCm < 0) {
      Serial.println(F("[Ultralyd] Intet ekko"));
    } else {
      Serial.print(F("[Ultralyd] "));
      Serial.print(afstandCm);
      Serial.println(afstandCm <= MODSTANDER_CM ? F(" cm  <-- MODSTANDER!") : F(" cm"));
    }
  }
}

// ===================== Stepmotor =====================
// Stepper-biblioteket blokerer i step(), så her tages ét trin ad gangen.
const int TRIN_PR_OMGANG = 2048;
const int STEP_RPM = 10;
const unsigned long STEP_INTERVAL_US = 60000000UL / ((unsigned long)TRIN_PR_OMGANG * STEP_RPM);  // ≈ 2930 µs

// Her er spolerækkefølgen (to spoler tændt ad gangen) - den samme som Stepper-biblioteket brugte
const byte SPOLER[4][4] = {
  {HIGH, HIGH, LOW,  LOW },
  {LOW,  HIGH, HIGH, LOW },
  {LOW,  LOW,  HIGH, HIGH},
  {HIGH, LOW,  LOW,  HIGH},
};

struct StepBevaegelse {
  int trin;                // + med uret, - mod uret
  unsigned int pauseMs;    // Pause bagefter
  bool sluk;               // Sluk spolerne i pausen
  const char *tekst;
};

const StepBevaegelse STEP_PROGRAM[] = {
  { TRIN_PR_OMGANG,     1000, true,  "En omgang med uret" },
  {-TRIN_PR_OMGANG,     1000, true,  "En omgang mod uret" },
  { TRIN_PR_OMGANG / 4,  500, false, "Kvart omgang frem (1/4)" },
  { TRIN_PR_OMGANG / 4,  500, false, "Kvart omgang frem (2/4)" },
  { TRIN_PR_OMGANG / 4,  500, false, "Kvart omgang frem (3/4)" },
  { TRIN_PR_OMGANG / 4, 2500, true,  "Kvart omgang frem (4/4)" },
};
const int STEP_ANTAL = sizeof(STEP_PROGRAM) / sizeof(STEP_PROGRAM[0]);

int stepNr = -1;               // Hvilken bevægelse vi er i gang med (-1 = ikke startet)
int stepTilbage = 0;           // Trin tilbage i bevægelsen
int stepFase = 0;              // Hvor i SPOLER-tabellen vi er
unsigned long stepSidsteUs = 0;
unsigned long stepPauseSlut = 0;

void skrivSpoler(byte a, byte b, byte c, byte d) {
  digitalWrite(S_IN1, a);
  digitalWrite(S_IN2, b);
  digitalWrite(S_IN3, c);
  digitalWrite(S_IN4, d);
}

void startStepBevaegelse(int nr) {
  stepNr = nr;
  stepTilbage = abs(STEP_PROGRAM[nr].trin);
  Serial.print(F("[Stepper] "));
  Serial.println(STEP_PROGRAM[nr].tekst);
}

void stepperSetup() {
  pinMode(S_IN1, OUTPUT);
  pinMode(S_IN2, OUTPUT);
  pinMode(S_IN3, OUTPUT);
  pinMode(S_IN4, OUTPUT);
  skrivSpoler(LOW, LOW, LOW, LOW);
  startStepBevaegelse(0);
}

void stepperOpdater(unsigned long nu, unsigned long nuUs) {
  if (stepTilbage > 0) {
    if (nuUs - stepSidsteUs >= STEP_INTERVAL_US) {
      stepSidsteUs = nuUs;
      stepFase = (STEP_PROGRAM[stepNr].trin > 0) ? (stepFase + 1) % 4 : (stepFase + 3) % 4;
      skrivSpoler(SPOLER[stepFase][0], SPOLER[stepFase][1], SPOLER[stepFase][2], SPOLER[stepFase][3]);
      stepTilbage--;

      if (stepTilbage == 0) {   // Bevægelsen er færdig, så starter pausen
        if (STEP_PROGRAM[stepNr].sluk) {
          skrivSpoler(LOW, LOW, LOW, LOW);   // Så bliver motor og driver ikke varme
        }
        stepPauseSlut = nu + STEP_PROGRAM[stepNr].pauseMs;
      }
    }
  } else if ((long)(nu - stepPauseSlut) >= 0) {
    startStepBevaegelse((stepNr + 1) % STEP_ANTAL);
  }
}

// ===================== DC-motorer =====================
struct DcTrin {
  const char *tekst;
  int v, h;          // Fart -255..255
  unsigned int tid;  // ms
};

const DcTrin DC_PROGRAM[] = {
  {"Venstre motor frem",  200,    0, 1500},
  {"Venstre motor bak",  -200,    0, 1500},
  {"Hoejre motor frem",     0,  200, 1500},
  {"Hoejre motor bak",      0, -200, 1500},
  {"Begge frem",          200,  200, 1500},
  {"Begge bak",          -200, -200, 1500},
  {"Drej paa stedet",     200, -200, 1500},
};
const int DC_ANTAL = sizeof(DC_PROGRAM) / sizeof(DC_PROGRAM[0]);

enum DcTilstand { DC_START, DC_KOER, DC_PAUSE, DC_RAMPE, DC_SLUT };
DcTilstand dcTilstand = DC_START;
int dcNr = 0;
int dcRampeFart = 0;
unsigned long dcNaeste = 0;   // Tidspunkt for næste skift

void koerMotor(int en, int inA, int inB, int fart) {
  digitalWrite(inA, fart > 0 ? HIGH : LOW);
  digitalWrite(inB, fart < 0 ? HIGH : LOW);
  analogWrite(en, abs(fart));
}

void venstre(int fart) { koerMotor(ENA, IN1, IN2, fart); }
void hoejre(int fart)  { koerMotor(ENB, IN3, IN4, fart); }
void dcStop() { venstre(0); hoejre(0); }

void dcSetup() {
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  dcStop();
  Serial.println(F("[DC] Motortest starter om 3 sekunder - loft robotten op!"));
  dcNaeste = millis() + 3000;
}

void dcOpdater(unsigned long nu) {
  if ((long)(nu - dcNaeste) < 0) {
    return;   // Ikke tid endnu
  }

  switch (dcTilstand) {
    case DC_START:
    case DC_PAUSE:
      if (dcTilstand == DC_PAUSE) {
        dcNr++;
      }
      if (dcNr < DC_ANTAL) {
        Serial.print(F("[DC] "));
        Serial.println(DC_PROGRAM[dcNr].tekst);
        venstre(DC_PROGRAM[dcNr].v);
        hoejre(DC_PROGRAM[dcNr].h);
        dcNaeste = nu + DC_PROGRAM[dcNr].tid;
        dcTilstand = DC_KOER;
      } else {
        Serial.println(F("[DC] Fart op fra 0 til 255"));
        dcRampeFart = 0;
        dcNaeste = nu;
        dcTilstand = DC_RAMPE;
      }
      break;

    case DC_KOER:
      dcStop();
      dcNaeste = nu + 500;   // Kort pause, så motorerne ikke skifter retning på fuld fart
      dcTilstand = DC_PAUSE;
      break;

    case DC_RAMPE:
      if (dcRampeFart <= 255) {
        venstre(dcRampeFart);
        hoejre(dcRampeFart);
        dcRampeFart += 5;
        dcNaeste = nu + 80;
      } else {
        dcStop();
        Serial.println(F("[DC] --- Testen starter forfra om 3 sekunder ---"));
        dcNaeste = nu + 3000;
        dcTilstand = DC_SLUT;
      }
      break;

    case DC_SLUT:
      dcNr = 0;
      dcTilstand = DC_START;
      dcNaeste = nu;
      break;
  }
}

// ===================== Servo =====================
Servo servo;

struct ServoPos {
  int vinkel;
  unsigned int vent;
};

// -1 betyder "langsom sving 0 -> 180 -> 0" på det sted i programmet
const ServoPos SERVO_PROGRAM[] = {
  {0, 1000}, {90, 1000}, {180, 1000}, {90, 1000}, {-1, 0}, {90, 2000},
};
const int SERVO_ANTAL = sizeof(SERVO_PROGRAM) / sizeof(SERVO_PROGRAM[0]);

int servoNr = 0;
int servoSving = -1;          // -1 = ikke i gang med sving, 0..360 = hvor langt i svinget
unsigned long servoNaeste = 0;

void servoSetup() {
  servo.attach(SERVO_PIN);
  servo.write(90);   // Her starter servoen i midten
  Serial.println(F("[Servo] Servotest startet"));
  servoNaeste = millis() + 1000;
}

void servoOpdater(unsigned long nu) {
  if ((long)(nu - servoNaeste) < 0) {
    return;
  }

  if (servoSving >= 0) {
    // 0..180 er op, 181..360 er ned igen. 1 grad hvert 15. ms
    servo.write(servoSving <= 180 ? servoSving : 360 - servoSving);
    servoSving++;
    if (servoSving > 360) {
      servoSving = -1;
      servoNr = (servoNr + 1) % SERVO_ANTAL;
    }
    servoNaeste = nu + 15;
    return;
  }

  const ServoPos &p = SERVO_PROGRAM[servoNr];
  if (p.vinkel < 0) {
    Serial.println(F("[Servo] Langsom sving"));
    servoSving = 0;
    servoNaeste = nu;
  } else {
    Serial.print(F("[Servo] Vinkel: "));
    Serial.println(p.vinkel);
    servo.write(p.vinkel);
    servoNaeste = nu + p.vent;
    servoNr = (servoNr + 1) % SERVO_ANTAL;
  }
}

// ===================== Lyd =====================
// Melodi og MIDI-sang deler én buzzer, så de spilles på skift: melodi, pause, sang, pause, ...
// tone(pin, hz, ms) stopper selv tonen igen, så her skal vi kun huske, hvornår næste node starter.
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

const int TEMPO = 140;
const int SLAG_MS = 60000 / TEMPO;

const int MELODI[] PROGMEM = {
  C5, 8,  C5, 16, C5, 16, DS5, 8, C5, 8,   AS4, 8, G4, 8,  PAUSE, 4,
  C5, 8,  C5, 16, C5, 16, DS5, 8, F5, 8,   DS5, 8, D5, 8,  PAUSE, 4,
  G5, 8,  F5, 8,  DS5, 8, D5, 8,  C5, 8,   AS4, 8, C5, 4,
  GS4, 8, AS4, 8, C5, 8,  PAUSE, 8, G4, 2,
  C4, 8,  PAUSE, 8, C4, 16, C4, 16, DS4, 8, F4, 8, G4, 8, F4, 8, DS4, 4,
};
const int MELODI_ANTAL = sizeof(MELODI) / sizeof(MELODI[0]) / 2;

enum LydTilstand { LYD_START, LYD_MELODI, LYD_SANG };
LydTilstand lydTilstand = LYD_START;
bool naesteErSang = false;
uint16_t lydNode = 0;
unsigned long lydNaeste = 0;

void lydSetup() {
  pinMode(LYD_PIN, OUTPUT);
  Serial.println(F("[Lyd] Bip"));
  tone(LYD_PIN, 1000, 300);   // Kort bip, så vi ved den virker
  lydNaeste = millis() + 1000;
}

void lydOpdater(unsigned long nu) {
  if ((long)(nu - lydNaeste) < 0) {
    return;
  }

  switch (lydTilstand) {
    case LYD_START:
      lydNode = 0;
      if (naesteErSang) {
        Serial.print(F("[Lyd] Spiller sang ("));
        Serial.print(SANG_ANTAL);
        Serial.println(F(" noder)"));
        lydTilstand = LYD_SANG;
      } else {
        Serial.println(F("[Lyd] Spiller melodi"));
        lydTilstand = LYD_MELODI;
      }
      naesteErSang = !naesteErSang;
      lydNaeste = nu;
      break;

    case LYD_MELODI:
      if (lydNode < MELODI_ANTAL) {
        int hz = pgm_read_word(&MELODI[lydNode * 2]);
        int laengde = (SLAG_MS * 4) / (int)pgm_read_word(&MELODI[lydNode * 2 + 1]);
        if (hz != PAUSE) {
          tone(LYD_PIN, hz, laengde * 9 / 10);   // 90 % lyd, 10 % stilhed
        }
        lydNaeste = nu + laengde;
        lydNode++;
      } else {
        noTone(LYD_PIN);
        lydNaeste = nu + 3000;
        lydTilstand = LYD_START;
      }
      break;

    case LYD_SANG:
      if (lydNode < SANG_ANTAL) {
        uint16_t hz = pgm_read_word(&SANG[lydNode * 2]);
        uint16_t ms = pgm_read_word(&SANG[lydNode * 2 + 1]);
        if (hz > 0) {
          tone(LYD_PIN, hz, (ms > 40) ? ms - 10 : ms);   // 10 ms hul, så ens toner i træk kan høres
        }
        lydNaeste = nu + ms;
        lydNode++;
      } else {
        noTone(LYD_PIN);
        lydNaeste = nu + 3000;
        lydTilstand = LYD_START;
      }
      break;
  }
}

// ===================== Hovedprogram =====================
void setup() {
  Serial.begin(115200);   // Hurtigere end 9600, så Serial.print ikke bremser de andre dele
  Serial.println(F("Alle tests koerer samtidig"));
  ultralydSetup();
  stepperSetup();
  dcSetup();
  servoSetup();
  lydSetup();
}

void loop() {
  unsigned long nu = millis();
  unsigned long nuUs = micros();

  ultralydOpdater(nu);
  stepperOpdater(nu, nuUs);
  dcOpdater(nu);
  servoOpdater(nu);
  lydOpdater(nu);
}
