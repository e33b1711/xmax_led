#include "settings.h"
#include <Preferences.h>

Settings settings;

void Settings::load() {
  Preferences p;
  p.begin("sanduhr", true);
  brightIdx = p.getUChar("bright", brightIdx);
  durIdx = p.getUChar("dur", durIdx);
  ssid = p.getString("ssid", "");
  pass = p.getString("pass", "");
  channel = p.getString("channel", "stabil");
  bool hasMinute = p.isKey("otamin");
  otaMinute = p.getUChar("otamin", 0);
  p.end();
  if (!hasMinute) {
    otaMinute = esp_random() % 180;   // 0-179 Minuten nach Beginn des Nachtfensters
    save();
  }
}

void Settings::save() {
  Preferences p;
  p.begin("sanduhr", false);
  p.putUChar("bright", brightIdx);
  p.putUChar("dur", durIdx);
  p.putString("ssid", ssid);
  p.putString("pass", pass);
  p.putString("channel", channel);
  p.putUChar("otamin", otaMinute);
  p.end();
}
