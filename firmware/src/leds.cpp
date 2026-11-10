#include "leds.h"

CRGB leds[NUM_LEDS_MAX];

namespace Leds {

static int H = MATRIX_H;
static CLEDController *ctrl = nullptr;

int height() { return H; }
int count() { return MATRIX_W * H; }

void setHeight(int h) {
  H = (h == 16) ? 16 : 32;
  fill_solid(leds, NUM_LEDS_MAX, CRGB::Black);
  if (ctrl) ctrl->setLeds(leds, count());
}

void begin(int h) {
  setHeight(h);
  ctrl = &FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, count());
  // FastLED rechnet den Strom selbst und dimmt das ganze Bild, wenn es zu viel wird
  FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_MAX_MA);
  FastLED.setCorrection(TypicalLEDStrip);
  clear();
  FastLED.show();
}

uint16_t xy(int x, int y) {
  int panel = y / PANEL, py = y % PANEL;
#if !CHAIN_TOP_PANEL_FIRST
  panel = (H / PANEL - 1) - panel;
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
  if (x < 0 || y < 0 || x >= MATRIX_W || y >= H) return;
  leds[xy(x, y)] = c;
}

void clear() { fill_solid(leds, NUM_LEDS_MAX, CRGB::Black); }

void show(uint8_t brightness) {
  FastLED.setBrightness(min<uint8_t>(brightness, BRIGHT_MAX));
  FastLED.show();
}

void testPattern() {
  clear();
  set(0, 0, CRGB::Red);
  set(MATRIX_W - 1, 0, CRGB::Green);
  set(0, H - 1, CRGB::Blue);
  set(MATRIX_W - 1, H - 1, CRGB::White);
  for (int y = PANEL - 1; y < H - 1; y += PANEL)
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
