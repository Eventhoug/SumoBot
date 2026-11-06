#include <Arduino.h>

// Testprogram til 2 DC-motorer (TT-gearmotorer) med L298N H-bro-modul (rødt board)
// Her sættes pins. EN-pins skal være PWM-pins (~), så vi kan styre farten
const int ENA = 5;   // ENA (fart venstre motor, OUT1/OUT2) - jumperen skal af
const int IN1 = 4;   // IN1
const int IN2 = 3;   // IN2
const int ENB = 6;   // ENB (fart højre motor, OUT3/OUT4) - jumperen skal af
const int IN3 = 9;   // IN3
const int IN4 = 10;  // IN4

// Her styres én motor. fart: -255 (fuld bak) til 255 (fuld frem), 0 = stop
void koerMotor(int en, int inA, int inB, int fart) {
  if (fart > 0) {
    digitalWrite(inA, HIGH);
    digitalWrite(inB, LOW);
  } else if (fart < 0) {
    digitalWrite(inA, LOW);
    digitalWrite(inB, HIGH);
  } else {
    digitalWrite(inA, LOW);
    digitalWrite(inB, LOW);
  }
  analogWrite(en, abs(fart));
}

void venstre(int fart) { koerMotor(ENA, IN1, IN2, fart); }
void hoejre(int fart)  { koerMotor(ENB, IN3, IN4, fart); }

void stop() {
  venstre(0);
  hoejre(0);
}

// Her køres et trin i testen i et antal millisekunder, og så stoppes der
void trin(const char *tekst, int v, int h, int tid) {
  Serial.println(tekst);
  venstre(v);
  hoejre(h);
  delay(tid);
  stop();
  delay(500);   // Kort pause, så motorerne ikke skifter retning på fuld fart
}

void setup() {
  Serial.begin(9600);
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  stop();
  Serial.println("Motortest starter om 3 sekunder - loft robotten op!");
  delay(3000);
}

void loop() {
  trin("Venstre motor frem",  200,    0, 1500);
  trin("Venstre motor bak",  -200,    0, 1500);
  trin("Hoejre motor frem",     0,  200, 1500);
  trin("Hoejre motor bak",      0, -200, 1500);
  trin("Begge frem",          200,  200, 1500);
  trin("Begge bak",          -200, -200, 1500);
  trin("Drej paa stedet",     200, -200, 1500);

  Serial.println("Fart op fra 0 til 255");
  for (int fart = 0; fart <= 255; fart += 5) {
    venstre(fart);
    hoejre(fart);
    Serial.println(fart);
    delay(80);
  }
  stop();

  Serial.println("--- Testen starter forfra om 3 sekunder ---");
  delay(3000);
}
