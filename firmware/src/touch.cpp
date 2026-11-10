#include "touch.h"
#include "config.h"

namespace Touch {

static const int PINS[4] = {TOUCH_A, TOUCH_B, TOUCH_C, TOUCH_D};
static float base[4];
static bool held[4];
static uint32_t since[4];
static bool longFired = false;
static const uint32_t LONG_MS = 5000;

static uint16_t readPin(int p) { return p < 0 ? 0 : touchRead(p); }

void begin() {
  for (int i = 0; i < 4; i++) {
    held[i] = false;
    if (PINS[i] < 0) continue;
    uint32_t sum = 0;
    for (int k = 0; k < 16; k++) { sum += readPin(PINS[i]); delay(5); }
    base[i] = sum / 16.0f;
  }
}

Event update() {
  uint32_t now = millis();
  Event ev = NONE;
  bool before[4];
  for (int i = 0; i < 4; i++) {
    before[i] = held[i];
    if (PINS[i] < 0) continue;
    float v = readPin(PINS[i]);
    // Berührung senkt den Messwert deutlich (ESP32). Mit Abstand zum Grundwert entprellen.
    if (!held[i] && v < base[i] * 0.70f) { held[i] = true; since[i] = now; }
    else if (held[i] && v > base[i] * 0.85f) held[i] = false;
    // Grundwert langsam nachführen, solange nicht berührt (Temperatur, Feuchte)
    if (!held[i]) base[i] += 0.01f * (v - base[i]);
  }
  // A + B lange zusammen: WLAN-Einrichtung
  if (held[0] && held[1]) {
    if (!longFired && now - max(since[0], since[1]) > LONG_MS) { longFired = true; return AB_LONG; }
    return NONE;
  }
  // A allein lange: Betriebsart wechseln
  if (held[0] && !held[1] && !longFired && now - since[0] > LONG_MS) { longFired = true; return A_LONG; }
  // B allein lange: jetzt nach Updates suchen
  if (held[1] && !held[0] && !longFired && now - since[1] > LONG_MS) { longFired = true; return B_LONG; }
  // kurzer Druck = beim Loslassen, wenn kein langer Druck ausgelöst wurde
  for (int i = 0; i < 4; i++) {
    if (before[i] && !held[i]) {
      bool other = false;
      for (int k = 0; k < 4; k++) if (k != i && held[k]) other = true;
      if (!longFired && !other && now - since[i] < 1500) ev = (Event)(A_SHORT + i);
    }
  }
  if (!anyHeld()) longFired = false;
  return ev;
}

bool anyHeld() { return held[0] || held[1] || held[2] || held[3]; }

void debugPrint() {
  Serial.print("Touch:");
  for (int i = 0; i < 4; i++)
    if (PINS[i] >= 0) Serial.printf(" %c=%u/%.0f%s", 'A' + i, readPin(PINS[i]), base[i], held[i] ? "*" : "");
  Serial.println();
}

}  // namespace Touch
