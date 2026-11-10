#pragma once
// Dauerhaft gespeicherte Einstellungen (NVS, überleben Neustart und Updates).
#include <Arduino.h>

static const uint8_t BRIGHT_STEPS[] = {20, 45, 80, 120, 160};
static const uint16_t DURATIONS_S[] = {60, 180, 300, 600, 900, 1800};   // 1, 3, 5, 10, 15, 30 min
static const uint8_t N_BRIGHT = sizeof(BRIGHT_STEPS);
static const uint8_t N_DUR = sizeof(DURATIONS_S) / sizeof(DURATIONS_S[0]);
static const uint8_t LIFE_SPEEDS[] = {1, 2, 4, 6, 10, 15};                 // Game of Life: Generationen/s
static const uint8_t N_LIFE_SPEED = sizeof(LIFE_SPEEDS);

struct Settings {
  uint8_t brightIdx = 2;
  uint8_t durIdx = 2;
  String ssid, pass;
  String channel = "stabil";   // Update-Kanal: "stabil" oder "test"
  uint8_t otaMinute = 0;       // zufällige Minute im Nachtfenster, damit nicht alle gleichzeitig laden
  uint8_t mode = 0;            // 0 = Sanduhr, 1 = Game of Life
  uint8_t lifeSpeedIdx = 3;    // Game of Life: Index in LIFE_SPEEDS
  uint8_t height = 0;          // Matrixhöhe 16 oder 32; beim ersten Start = MATRIX_H

  void load();
  void save();
  uint16_t durationS() const { return DURATIONS_S[durIdx % N_DUR]; }
  uint8_t brightness() const { return BRIGHT_STEPS[brightIdx % N_BRIGHT]; }
  uint8_t lifeSpeed() const { return LIFE_SPEEDS[lifeSpeedIdx % N_LIFE_SPEED]; }
};

extern Settings settings;
