#pragma once
// Game of Life (Conway) über die ganze Matrix, Ränder verbunden (Torus).
// Steht das Spiel still oder wiederholt es sich, startet es nach kurzer Pause neu.
#include <Arduino.h>

namespace Life {
void begin(int height);        // Feldgröße MATRIX_W x height, zufällig füllen
void reseed();                  // neu zufällig füllen
void setSpeed(int gensPerSecond);
void step(uint32_t now);        // rechnet im eingestellten Takt die nächste Generation
void render();
int population();
uint32_t generation();
}
