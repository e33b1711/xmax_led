#pragma once
// Lagesensor ICM-20948: Schwerkraft in Matrix-Richtung, Bewegungserkennung.
#include <Arduino.h>

namespace Imu {
bool begin();          // false = kein Sensor gefunden (Sanduhr läuft dann wie senkrecht stehend)
void update();
bool ok();
// Schwerkraft in Matrix-Richtung (x rechts, y unten), in g, geglättet
float gx();
float gy();
float gz();            // senkrecht zur Matrix (liegt die Sanduhr flach, ist das ~1)
uint32_t lastMotionMs();   // millis() der letzten spürbaren Bewegung
void debugPrint();     // Rohwerte auf die serielle Konsole, um die Achsen festzulegen
}
