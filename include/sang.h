// Pladsholder: overskrives af tools/midi2melodi.js
// Indtil da: en kort startnedtælling (bip-bip-bip-BIIP)
// To tal pr. node: frekvens i Hz (0 = pause), længde i ms.
#pragma once
#include <Arduino.h>

const uint16_t SANG[] PROGMEM = {
  880, 200,  0, 800,
  880, 200,  0, 800,
  880, 200,  0, 800,
  1760, 800,
};
const uint16_t SANG_ANTAL = 7;
