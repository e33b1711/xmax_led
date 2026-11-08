#include "leds.h"

CRGB leds[NUM_LEDS];

namespace Leds {

void begin() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  // FastLED rechnet den Strom selbst und dimmt das ganze Bild, wenn es zu viel wird
  FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_MAX_MA);
  FastLED.setCorrection(TypicalLEDStrip);
  clear();
  FastLED.show();
}

uint16_t xy(int x, int y) {
  int panel = y / PANEL, py = y % PANEL;
#if !CHAIN_TOP_PANEL_FIRST
  panel = (MATRIX_H / PANEL - 1) - panel;
#endif
#if PANEL_ROWS_FROM_BOTTOM
  int row = PANEL - 1 - py;
#else
  int row = py;
#endif
  bool ltr = (row % 2 == 0) == (PANEL_FIRST_ROW_LEFT_TO_RIGHT == 1);
  int col = ltr ? x : (MATRIX_W - 1 - x);
  return panel * PANEL * MATRIX_W + row * MATRIX_W + col;
}

void set(int x, int y, const CRGB &c) {
  if (x < 0 || y < 0 || x >= MATRIX_W || y >= MATRIX_H) return;
  leds[xy(x, y)] = c;
}

void clear() { fill_solid(leds, NUM_LEDS, CRGB::Black); }

void show(uint8_t brightness) {
  FastLED.setBrightness(min<uint8_t>(brightness, BRIGHT_MAX));
  FastLED.show();
}

void testPattern() {
  clear();
  set(0, 0, CRGB::Red);
  set(MATRIX_W - 1, 0, CRGB::Green);
  set(0, MATRIX_H - 1, CRGB::Blue);
  set(MATRIX_W - 1, MATRIX_H - 1, CRGB::White);
  for (int y = PANEL - 1; y < MATRIX_H - 1; y += PANEL)
    for (int x = 2; x < MATRIX_W - 2; x++) set(x, y, CRGB(60, 50, 0));
}

// 3x5-Ziffern, je Zeile 3 Bit (links = Bit 2)
static const uint8_t FONT[10][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 7, 1, 7}, {5, 5, 7, 1, 1},
    {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2}, {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}};

void digit3x5(int x, int y, int d, const CRGB &c) {
  if (d < 0 || d > 9) return;
  for (int r = 0; r < 5; r++)
    for (int b = 0; b < 3; b++)
      if (FONT[d][r] & (4 >> b)) set(x + b, y + r, c);
}

void number(int value, int yTop, const CRGB &c) {
  if (value < 10) {
    digit3x5(MATRIX_W / 2 - 2, yTop, value, c);
  } else {
    digit3x5(MATRIX_W / 2 - 4, yTop, (value / 10) % 10, c);
    digit3x5(MATRIX_W / 2 + 1, yTop, value % 10, c);
  }
}

void barH(int y, float frac, const CRGB &c) {
  int n = (int)(frac * MATRIX_W + 0.5f);
  for (int x = 0; x < MATRIX_W; x++) set(x, y, x < n ? c : CRGB(6, 6, 6));
}

}  // namespace Leds
