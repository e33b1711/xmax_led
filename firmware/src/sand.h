#pragma once
// Sanduhr: jedes Korn eine LED. Übertragen aus simulation/sanduhr.html.
#include <Arduino.h>

namespace Sand {
void begin(float fill = 0.8f);       // Glasform berechnen, obere Hälfte füllen
void refill();                        // Sand wieder in die (in Weltrichtung) obere Hälfte
// gx/gy: Schwerkraft in Matrix-Richtung (x rechts, y unten), Betrag ~1 bei senkrechter Matrix
void step(float gx, float gy, float dt, float grainsPerSecond);
void render();
int count();
}
