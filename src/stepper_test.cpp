#include <Arduino.h>
#include <Stepper.h>

// Testprogram til 28BYJ-48 stepmotor med ULN2003-driverboard
// Her sættes pins (A0-A3 bruges som digitale udgange, så 7/8 er fri til ultralyd og PWM til motorerne)
const int IN1 = A0;
const int IN2 = A1;
const int IN3 = A2;
const int IN4 = A3;

// 28BYJ-48: 32 trin pr. omgang inde i motoren × gear 1:64 ≈ 2048 trin pr. omgang på akslen
const int TRIN_PR_OMGANG = 2048;

// OBS: rækkefølgen er IN1, IN3, IN2, IN4 – ellers summer motoren bare uden at dreje
Stepper motor(TRIN_PR_OMGANG, IN1, IN3, IN2, IN4);

// Her slukkes alle spoler, så motoren og driveren ikke bliver varme, når den står stille
void slukSpoler() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void setup() {
  Serial.begin(9600);
  motor.setSpeed(10);   // Her sættes farten i omdrejninger pr. minut (max ca. 15 for 28BYJ-48)
  Serial.println("Stepmotor test startet");
}

void loop() {
  Serial.println("En omgang med uret");
  motor.step(TRIN_PR_OMGANG);
  slukSpoler();
  delay(1000);

  Serial.println("En omgang mod uret");
  motor.step(-TRIN_PR_OMGANG);
  slukSpoler();
  delay(1000);

  Serial.println("Kvart omgang (90 grader) frem 4 gange");
  for (int i = 0; i < 4; i++) {
    motor.step(TRIN_PR_OMGANG / 4);
    delay(500);
  }
  slukSpoler();
  delay(2000);
}
