#pragma once
// LED-Matrix: Zuordnung x/y -> LED-Nummer, Strombegrenzung, kleine Zeichenhilfen.
#include <FastLED.h>
#include "config.h"

extern CRGB leds[NUM_LEDS];

namespace Leds {
void begin();
uint16_t xy(int x, int y);              // x 0..W-1 nach rechts, y 0..H-1 nach unten
void set(int x, int y, const CRGB &c);  // außerhalb wird ignoriert
void clear();
void show(uint8_t brightness);
void testPattern();                     // Ecken rot/grün/blau/weiß, Panelgrenze gelb
void digit3x5(int x, int y, int d, const CRGB &c);
void number(int value, int yTop, const CRGB &c);   // zentriert, bis 2 Stellen
void barH(int y, float frac, const CRGB &c);        // waagrechter Balken über die Breite
}
