#include <Arduino.h>
#include <Servo.h>

// Testprogram til servo (fx SG90)
// Her sættes pin. D11 er fri (de andre tests bruger 3-10, 12 og A0-A3)
const int SERVO_PIN = 11;

Servo servo;

// Her drejes servoen til en vinkel og venter, så den kan nå frem
void drejTil(int vinkel, int vent) {
  Serial.print("Vinkel: ");
  Serial.println(vinkel);
  servo.write(vinkel);
  delay(vent);
}

void setup() {
  Serial.begin(9600);
  servo.attach(SERVO_PIN);
  Serial.println("Servotest startet");
  drejTil(90, 1000);   // Her starter servoen i midten
}

void loop() {
  // 1) De tre faste positioner
  drejTil(0, 1000);
  drejTil(90, 1000);
  drejTil(180, 1000);
  drejTil(90, 1000);

  // 2) Langsom sving fra 0 til 180 og tilbage, 1 grad ad gangen
  Serial.println("Langsom sving");
  for (int v = 0; v <= 180; v++) {
    servo.write(v);
    delay(15);
  }
  for (int v = 180; v >= 0; v--) {
    servo.write(v);
    delay(15);
  }
  drejTil(90, 2000);
}
