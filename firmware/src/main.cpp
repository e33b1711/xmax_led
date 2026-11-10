// LED-Sanduhr - Hauptprogramm
//
// Bedienung:
//   Taste A kurz        Helligkeit (5 Stufen)
//   Taste B kurz        Sanduhr: Laufzeit (1, 3, 5, 10, 15, 30 min), wird kurz als Zahl gezeigt
//                       Game of Life: Geschwindigkeit (1, 2, 4, 6, 10, 15 Generationen/s)
//   Taste A 5 s         Betriebsart wechseln: Sanduhr <-> Game of Life
//   Taste B 5 s         jetzt nach Updates suchen
//   A + B 5 s           WLAN einrichten (eigenes WLAN "Sanduhr-XXXX", Handy verbinden)
//   Umdrehen            Sand läuft in die andere Richtung - wie bei einer echten Sanduhr
//
// Serielle Konsole (115200 Baud), ersetzt auch die Tasten (Prototyp ohne Touch):
//   a = wie Taste A, b = wie Taste B, u = Update suchen (wie B 5 s), w = WLAN einrichten (wie A+B 5 s)
//   t = Testbild 5 s, i = Sensorwerte, k = Touch-Werte, s = Status, ? = Hilfe,
//   m = Matrixhöhe 16/32 umschalten (wird gespeichert), g = Betriebsart wechseln (wie A 5 s),
//   r = Game of Life neu starten, n = sichtbare WLAN-Netze mit Pegel
//   , / . = Azimut -/+5°, - / + = Elevation -/+5°, 0 = Lage wieder vom Sensor (Lage von Hand)
#include <Arduino.h>
#include "config.h"
#include "settings.h"
#include "leds.h"
#include "sand.h"
#include "life.h"
#include "imu.h"
#include "touch.h"
#include "net.h"
#include "updater.h"

enum Overlay : uint8_t { OV_NONE, OV_BRIGHT, OV_DURATION, OV_SPEED, OV_SEARCH };
static Overlay overlay = OV_NONE;
static uint32_t overlayUntil = 0, testUntil = 0;
static bool streamImu = false, streamTouch = false;

// Lage von Hand (ohne Sensor oder zum Testen): Azimut = Drehung in der Matrixebene
// (0° aufrecht, 90° Schwerkraft nach rechts, 180° auf dem Kopf), Elevation = Neigung nach
// hinten (0° senkrecht, 90° flach liegend -> der Sand bleibt stehen).
static bool manualPose = false;
static int azimuth = 0, elevation = 0;

static void changePose(int dAz, int dEl) {
  manualPose = true;
  azimuth = ((azimuth + dAz) % 360 + 360) % 360;
  elevation = constrain(elevation + dEl, 0, 90);
  Serial.printf("Lage von Hand: Azimut %d°, Elevation %d°\n", azimuth, elevation);
}

static void showOverlay(Overlay o, uint32_t ms) {
  overlay = o;
  overlayUntil = millis() + ms;
}

static void drawOverlay() {
  if (overlay == OV_NONE || millis() > overlayUntil) { overlay = OV_NONE; return; }
  // Fläche abdunkeln, damit die Anzeige lesbar ist
  int y0 = Leds::height() / 2 - 4;
  for (int y = y0; y < y0 + 8; y++)
    for (int x = 0; x < MATRIX_W; x++) Leds::set(x, y, CRGB::Black);
  if (overlay == OV_BRIGHT) {
    float f = (settings.brightIdx + 1) / (float)N_BRIGHT;
    Leds::barH(y0 + 3, f, CRGB(255, 200, 120));
    Leds::barH(y0 + 4, f, CRGB(255, 200, 120));
  } else if (overlay == OV_DURATION) {
    Leds::number(settings.durationS() / 60, y0 + 1, CRGB(255, 150, 30));
  } else if (overlay == OV_SPEED) {
    Leds::number(settings.lifeSpeed(), y0 + 1, CRGB(150, 255, 170));
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
  int cx = MATRIX_W / 2, cy = Leds::height() / 2 + 3;
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
  int n = (int)(p * Leds::count());
  // füllt sich von unten wie Sand
  for (int i = 0; i < n; i++) Leds::set(i % MATRIX_W, Leds::height() - 1 - i / MATRIX_W, CRGB(10, 40, 90));
}

static void printHelp() {
  Serial.println("Befehle: a Helligkeit, b Laufzeit (Game of Life: Tempo), u Update suchen, w WLAN einrichten,\n"
                 "         g Sanduhr/Game of Life, r Game of Life neu, t Testbild, i Sensorwerte, k Touch-Werte, s Status,\n"
                 "         m Matrix 16/32, n WLAN-Netze, ? Hilfe\n"
                 "         , . Azimut -/+5°, - + Elevation -/+5°, 0 Lage wieder vom Sensor");
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
    if (c == 'g') ev = Touch::A_LONG;
    if (c == 'r') Life::reseed();
    if (c == 'n') Net::listNetworks();
    if (c == '?' || c == 'h') printHelp();
    if (c == 't') testUntil = millis() + 5000;
    if (c == 'i') streamImu = !streamImu;
    if (c == 'k') streamTouch = !streamTouch;
    if (c == ',' || c == '<') changePose(-5, 0);
    if (c == '.' || c == '>') changePose(5, 0);
    if (c == '-') changePose(0, -5);
    if (c == '+' || c == '=') changePose(0, 5);
    if (c == '0') {
      manualPose = false;
      azimuth = elevation = 0;
      Serial.println(Imu::ok() ? "Lage wieder vom Sensor" : "Lage zurückgesetzt (kein Sensor: aufrecht)");
    }
    if (c == 'm') {
      settings.height = Leds::height() == 32 ? 16 : 32;
      settings.save();
      Leds::setHeight(settings.height);
      Sand::begin(settings.height, 0.8f);
      Life::begin(settings.height);
      Serial.printf("Matrix jetzt %dx%d (gespeichert)\n", MATRIX_W, settings.height);
    }
    if (c == 's')
      Serial.printf("Version %s, %s, Matrix %dx%d, Kanal %s, WLAN %s, Zeit %s, Sensor %s, Update: %s, Körner %d, Probe %s\n", FW_VERSION,
                    settings.mode ? "Game of Life" : "Sanduhr", MATRIX_W, Leds::height(), settings.channel.c_str(), Net::status().c_str(),
                    Net::timeValid() ? "ok" : "fehlt", Imu::ok() ? "ok" : "fehlt", Updater::lastResult(), Sand::count(),
                    Health::pending() ? "ja" : "nein");
  }
  return ev;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  settings.load();
  int h = settings.height;
  Serial.printf("\nLED-Sanduhr %s (%dx%d)\n", FW_VERSION, MATRIX_W, h);
  Leds::begin(h);
  Health::begin();
  Imu::begin();
  Touch::begin();
  Sand::begin(h, 0.8f);
  Life::begin(h);
  Life::setSpeed(settings.lifeSpeed());
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
      if (settings.mode == 1) {
        settings.lifeSpeedIdx = (settings.lifeSpeedIdx + 1) % N_LIFE_SPEED;
        settings.save();
        Life::setSpeed(settings.lifeSpeed());
        showOverlay(OV_SPEED, 1500);
        break;
      }
      settings.durIdx = (settings.durIdx + 1) % N_DUR;
      settings.save();
      showOverlay(OV_DURATION, 1500);
      break;
    case Touch::A_LONG:
      settings.mode = settings.mode ? 0 : 1;
      settings.save();
      if (settings.mode == 1) Life::reseed();
      Serial.printf("Betriebsart: %s\n", settings.mode ? "Game of Life" : "Sanduhr");
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
  } else if (settings.mode == 1) {
    Life::step(now);
    Leds::clear();
    Life::render();
    drawOverlay();
  } else {
    float gx = 0, gy = 1;
    if (manualPose) {
      float el = radians((float)elevation), az = radians((float)azimuth);
      gx = cosf(el) * sinf(az);
      gy = cosf(el) * cosf(az);
    } else if (Imu::ok()) {
      gx = Imu::gx();
      gy = Imu::gy();
    }
    Sand::step(gx, gy, dt, Sand::count() / (float)settings.durationS());
    Leds::clear();
    Sand::render();
    drawOverlay();
  }
  Leds::show(settings.brightness());
}
