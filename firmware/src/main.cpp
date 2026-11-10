// LED-Sanduhr - Hauptprogramm
//
// Bedienung:
//   Taste A kurz        Helligkeit (5 Stufen)
//   Taste B kurz        Laufzeit (1, 3, 5, 10, 15, 30 min), wird kurz als Zahl gezeigt
//   Taste B 5 s         jetzt nach Updates suchen
//   A + B 5 s           WLAN einrichten (eigenes WLAN "Sanduhr-XXXX", Handy verbinden)
//   Umdrehen            Sand läuft in die andere Richtung - wie bei einer echten Sanduhr
//
// Serielle Konsole (115200 Baud), ersetzt auch die Tasten (Prototyp ohne Touch):
//   a = wie Taste A, b = wie Taste B, u = Update suchen (wie B 5 s), w = WLAN einrichten (wie A+B 5 s)
//   t = Testbild 5 s, i = Sensorwerte, k = Touch-Werte, s = Status, ? = Hilfe
#include <Arduino.h>
#include "config.h"
#include "settings.h"
#include "leds.h"
#include "sand.h"
#include "imu.h"
#include "touch.h"
#include "net.h"
#include "updater.h"

enum Overlay : uint8_t { OV_NONE, OV_BRIGHT, OV_DURATION, OV_SEARCH };
static Overlay overlay = OV_NONE;
static uint32_t overlayUntil = 0, testUntil = 0;
static bool streamImu = false, streamTouch = false;

static void showOverlay(Overlay o, uint32_t ms) {
  overlay = o;
  overlayUntil = millis() + ms;
}

static void drawOverlay() {
  if (overlay == OV_NONE || millis() > overlayUntil) { overlay = OV_NONE; return; }
  // Fläche abdunkeln, damit die Anzeige lesbar ist
  int y0 = MATRIX_H / 2 - 4;
  for (int y = y0; y < y0 + 8; y++)
    for (int x = 0; x < MATRIX_W; x++) Leds::set(x, y, CRGB::Black);
  if (overlay == OV_BRIGHT) {
    float f = (settings.brightIdx + 1) / (float)N_BRIGHT;
    Leds::barH(y0 + 3, f, CRGB(255, 200, 120));
    Leds::barH(y0 + 4, f, CRGB(255, 200, 120));
  } else if (overlay == OV_DURATION) {
    Leds::number(settings.durationS() / 60, y0 + 1, CRGB(255, 150, 30));
  } else if (overlay == OV_SEARCH) {
    int k = (millis() / 150) % MATRIX_W;
    Leds::set(k, y0 + 3, CRGB(40, 120, 255));
    Leds::set(k, y0 + 4, CRGB(40, 120, 255));
  }
}

// WLAN-Symbol, blinkt, solange die Einrichtung läuft
static void drawPortal() {
  Leds::clear();
  bool on = (millis() / 600) % 2;
  int cx = MATRIX_W / 2, cy = MATRIX_H / 2 + 3;
  CRGB c = on ? CRGB(40, 120, 255) : CRGB(8, 24, 50);
  for (int r = 1; r <= 3; r++)
    for (int a = -45; a <= 45; a += 9) {
      float rad = radians(a - 90.0f);
      Leds::set(cx + (int)roundf(cosf(rad) * r * 2.2f), cy + (int)roundf(sinf(rad) * r * 2.2f), c);
    }
  Leds::set(cx, cy, c);
  Leds::set(cx - 1, cy, c);
}

static void drawUpdate() {
  Leds::clear();
  float p = Updater::progress();
  int n = (int)(p * NUM_LEDS);
  // füllt sich von unten wie Sand
  for (int i = 0; i < n; i++) Leds::set(i % MATRIX_W, MATRIX_H - 1 - i / MATRIX_W, CRGB(10, 40, 90));
}

static void printHelp() {
  Serial.println("Befehle: a Helligkeit, b Laufzeit, u Update suchen, w WLAN einrichten,\n"
                 "         t Testbild, i Sensorwerte, k Touch-Werte, s Status, ? Hilfe");
}

// Liefert ein Tasten-Ereignis, wenn ein Befehl eine Taste ersetzt
static Touch::Event serialCommands() {
  Touch::Event ev = Touch::NONE;
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'a') ev = Touch::A_SHORT;
    if (c == 'b') ev = Touch::B_SHORT;
    if (c == 'u') ev = Touch::B_LONG;
    if (c == 'w') ev = Touch::AB_LONG;
    if (c == '?' || c == 'h') printHelp();
    if (c == 't') testUntil = millis() + 5000;
    if (c == 'i') streamImu = !streamImu;
    if (c == 'k') streamTouch = !streamTouch;
    if (c == 's')
      Serial.printf("Version %s, Kanal %s, WLAN %s, Zeit %s, Sensor %s, Update: %s, Körner %d, Probe %s\n", FW_VERSION,
                    settings.channel.c_str(), Net::connected() ? "verbunden" : (Net::configured() ? "getrennt" : "nicht eingerichtet"),
                    Net::timeValid() ? "ok" : "fehlt", Imu::ok() ? "ok" : "fehlt", Updater::lastResult(), Sand::count(),
                    Health::pending() ? "ja" : "nein");
  }
  return ev;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\nLED-Sanduhr %s (%dx%d)\n", FW_VERSION, MATRIX_W, MATRIX_H);
  settings.load();
  Leds::begin();
  Health::begin();
  Imu::begin();
  Touch::begin();
  Sand::begin(0.8f);
  Net::begin();
  Updater::begin();
  printHelp();
  enableLoopWDT();   // hängt die Hauptschleife > 5 s, startet der ESP neu (und fällt ggf. zurück)
}

void loop() {
  static uint32_t lastFrame = 0, lastDebug = 0;
  uint32_t now = millis();
  if (now - lastFrame < 16) return;           // ~60 Bilder/s
  float dt = min(0.05f, (now - lastFrame) / 1000.0f);
  lastFrame = now;

  Touch::Event cmd = serialCommands();
  Net::loop();
  Health::loop();
  Updater::loop();
  Imu::update();

  Touch::Event ev = Touch::update();
  if (cmd != Touch::NONE) ev = cmd;
  switch (ev) {
    case Touch::A_SHORT:
      settings.brightIdx = (settings.brightIdx + 1) % N_BRIGHT;
      settings.save();
      showOverlay(OV_BRIGHT, 1200);
      break;
    case Touch::B_SHORT:
      settings.durIdx = (settings.durIdx + 1) % N_DUR;
      settings.save();
      showOverlay(OV_DURATION, 1500);
      break;
    case Touch::B_LONG:
      showOverlay(OV_SEARCH, 4000);
      Updater::checkNow();
      break;
    case Touch::AB_LONG:
      Net::startPortal();
      break;
    default:
      break;
  }

  if (now - lastDebug > 300) {
    lastDebug = now;
    if (streamImu) Imu::debugPrint();
    if (streamTouch) Touch::debugPrint();
  }

  if (now < testUntil) {
    Leds::testPattern();
  } else if (Net::portalActive()) {
    drawPortal();
  } else if (Updater::state() == Updater::DOWNLOADING) {
    drawUpdate();
  } else {
    float gx = 0, gy = 1;
    if (Imu::ok()) { gx = Imu::gx(); gy = Imu::gy(); }
    Sand::step(gx, gy, dt, Sand::count() / (float)settings.durationS());
    Leds::clear();
    Sand::render();
    drawOverlay();
  }
  Leds::show(settings.brightness());
}
