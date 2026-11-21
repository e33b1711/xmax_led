#include "settings.h"
#include <Preferences.h>
#include "config.h"

Settings settings;

void Settings::load() {
  Preferences p;
  p.begin("sanduhr", true);
  brightIdx = p.getUChar("bright", brightIdx);
  durIdx = p.getUChar("dur1", durIdx);   // neuer Schlüssel seit 0.1.9: Standard jetzt 1 min
  ssid = p.getString("ssid", "");
  pass = p.getString("pass", "");
  channel = p.getString("channel", "stabil");
  bool hasMinute = p.isKey("otamin");
  bool hasDur = p.isKey("dur1");
  otaMinute = p.getUChar("otamin", 0);
  height = p.getUChar("height", 0);
  mode = p.getUChar("mode", 0);
  lifeSpeedIdx = p.getUChar("lifespd", lifeSpeedIdx);
  p.end();
  bool changed = !hasDur;
  if (!hasMinute) {
    otaMinute = esp_random() % 180;   // 0-179 Minuten nach Beginn des Nachtfensters
    changed = true;
  }
  // Matrixhöhe beim ersten Start festhalten, damit spätere Updates sie nicht ändern
  if (height != 16 && height != 32) {
    height = MATRIX_H;
    changed = true;
  }
  if (changed) save();
}

void Settings::save() {
  Preferences p;
  p.begin("sanduhr", false);
  p.putUChar("bright", brightIdx);
  p.putUChar("dur1", durIdx);
  p.putString("ssid", ssid);
  p.putString("pass", pass);
  p.putString("channel", channel);
  p.putUChar("otamin", otaMinute);
  p.putUChar("height", height);
  p.putUChar("mode", mode);
  p.putUChar("lifespd", lifeSpeedIdx);
  p.end();
}
