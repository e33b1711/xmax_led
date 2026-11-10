#include "life.h"
#include "config.h"
#include "leds.h"

namespace Life {

static const int W = MATRIX_W, HMAX = MATRIX_H_MAX;
static int H = MATRIX_H;
static uint8_t age[W * HMAX], next[W * HMAX];   // 0 = tot, sonst Alter in Generationen (max. 255)
static uint32_t gen = 0, lastStep = 0, stuckSince = 0;
static int pop = 0;
static uint32_t stepMs = 166;                    // Takt, siehe setSpeed()
static const uint32_t STUCK_MS = 3000;           // so lange den Endzustand zeigen, dann neu
static const uint32_t MAX_GEN = 1500;            // spätestens dann neu, falls es sich langsam wiederholt
static const int HIST = 16;
static uint32_t hist[HIST];                      // Prüfsummen der letzten Generationen

static inline int at(int x, int y) { return ((y + H) % H) * W + (x + W) % W; }

static uint32_t hashField() {
  uint32_t h = 2166136261u;                      // FNV-1a über die lebenden Zellen
  for (int i = 0; i < W * H; i++) h = (h ^ (age[i] ? 1 : 0)) * 16777619u;
  return h;
}

void reseed() {
  pop = 0;
  for (int i = 0; i < W * H; i++) {
    age[i] = (esp_random() % 100) < 32 ? 1 : 0;
    pop += age[i] ? 1 : 0;
  }
  gen = 0;
  stuckSince = 0;
  for (int k = 0; k < HIST; k++) hist[k] = 0;
}

void begin(int height) {
  H = height;
  reseed();
  lastStep = millis();
}

void setSpeed(int gensPerSecond) { stepMs = 1000 / max(1, gensPerSecond); }

void step(uint32_t now) {
  if (now - lastStep < stepMs) return;
  lastStep = now;

  if (stuckSince) {
    if (now - stuckSince > STUCK_MS) reseed();
    return;
  }

  pop = 0;
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      int n = 0;
      for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
          if ((dx || dy) && age[at(x + dx, y + dy)]) n++;
      uint8_t a = age[at(x, y)];
      uint8_t v = 0;
      if (a && (n == 2 || n == 3)) v = a < 255 ? a + 1 : 255;   // überlebt, wird älter
      else if (!a && n == 3) v = 1;                               // Geburt
      next[at(x, y)] = v;
      if (v) pop++;
    }
  memcpy(age, next, W * H);
  gen++;

  // Stillstand, Aussterben oder Wiederholung (Blinker, Gleiter auf dem Torus …)
  uint32_t h = hashField();
  bool repeat = false;
  for (int k = 0; k < HIST; k++) if (hist[k] == h) repeat = true;
  hist[gen % HIST] = h;
  if (pop == 0 || repeat || gen >= MAX_GEN) stuckSince = now;
}

void render() {
  for (int y = 0; y < H; y++)
    for (int x = 0; x < W; x++) {
      uint8_t a = age[at(x, y)];
      if (!a) continue;
      // neu geboren hell grünweiß, mit dem Alter über Türkis nach Blau/Violett
      if (a == 1) { Leds::set(x, y, CRGB(150, 255, 170)); continue; }
      uint8_t hue = 96 + min<int>(a * 4, 100);
      uint8_t val = max<int>(110, 255 - a * 3);
      Leds::set(x, y, CHSV(hue, 230, val));
    }
}

int population() { return pop; }
uint32_t generation() { return gen; }

}  // namespace Life
