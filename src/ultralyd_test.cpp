#include <Arduino.h>

// Testprogram til HC-SR04 ultralydssensor
// Her sættes pins (7 og 8 er valgt, så PWM-pins 3, 5, 6, 9, 10, 11 er fri til motorerne)
const int TRIG_PIN = 7;
const int ECHO_PIN = 8;
const int LED_PIN = LED_BUILTIN;   // LED på pin 13 lyser, når noget er tæt på

const int MODSTANDER_CM = 30;      // Her sættes afstanden, hvor vi "ser" modstanderen

// Måler afstand i cm. Giver -1, hvis der ikke kommer noget ekko (intet inden for ca. 4 m)
long maalAfstand() {
  // Her sendes en 10 µs puls på TRIG, som får sensoren til at sende 8 lydpulser
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Her måles hvor længe ECHO er høj (tid frem og tilbage). Timeout 25 ms ≈ 4 m
  unsigned long tid = pulseIn(ECHO_PIN, HIGH, 25000UL);
  if (tid == 0) {
    return -1;
  }

  // Lyd: 343 m/s = 0,0343 cm/µs. Delt med 2, fordi lyden går frem og tilbage → tid / 58
  return tid / 58;
}

void setup() {
  Serial.begin(9600);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("HC-SR04 test startet");
}

void loop() {
  long afstand = maalAfstand();

  if (afstand < 0) {
    Serial.println("Intet ekko (for langt vaek eller fejl i ledninger)");
    digitalWrite(LED_PIN, LOW);
  } else {
    Serial.print("Afstand: ");
    Serial.print(afstand);
    Serial.print(" cm");

    if (afstand <= MODSTANDER_CM) {
      Serial.println("  <-- MODSTANDER!");
      digitalWrite(LED_PIN, HIGH);
    } else {
      Serial.println();
      digitalWrite(LED_PIN, LOW);
    }
  }

  delay(100);   // Her ventes, så ekkoet fra sidste måling er væk (min. 60 ms)
}
