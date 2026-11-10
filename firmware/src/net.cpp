#include "net.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <time.h>
#include "config.h"
#include "settings.h"
#include "updater.h"

namespace Net {

static WebServer server(80);
static DNSServer dns;
static bool portal = false;
static uint32_t portalStart = 0, lastScan = 0;
static bool seen = false;
static String options;   // <option>-Liste der gefundenen Netze

// Mesh / Repeater: mehrere Zugangspunkte mit demselben Namen. Der ESP32 bleibt sonst ewig am
// ersten, mit dem er sich verbunden hat - auch wenn der nahe Knoten nur kurz weg war.
// Darum regelmäßig nachsehen und zu einem deutlich stärkeren Knoten wechseln.
static const int ROAM_BETTER_DB = 8;               // so viel stärker muss der andere sein
static uint32_t lastRoamCheck = 0;
static bool roamScan = false, printScan = false;
static uint8_t bestBssid[6];
static int bestRssi = -127, bestChannel = 0;
static const uint32_t PORTAL_TIMEOUT = 10 * 60 * 1000;

String deviceName() {
  uint64_t mac = ESP.getEfuseMac();
  char buf[24];
  snprintf(buf, sizeof(buf), "Sanduhr-%04X", (uint16_t)(mac >> 32));
  return String(buf);
}

// Netze suchen, ohne die Hauptschleife anzuhalten: ein Scan über alle Kanäle dauert mehrere
// Sekunden, länger als der Loop-Watchdog (5 s) erlaubt. Ergebnis holt pollScan() ab.
static bool scanning = false;

static void startScan() {
  if (scanning) return;
  scanning = WiFi.scanNetworks(true) == WIFI_SCAN_RUNNING;
  lastScan = millis();
}

// true, wenn gerade ein Scan fertig geworden ist
static bool pollScan() {
  if (!scanning) return false;
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return false;
  scanning = false;
  if (n < 0) return false;   // fehlgeschlagen
  options = "";
  seen = false;
  bestRssi = -127;
  if (printScan) Serial.printf("WLAN: %d Netze sichtbar (* = eigenes, > = verbunden)\n", n);
  String cur = connected() ? WiFi.BSSIDstr() : "";
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    bool own = s == settings.ssid && !s.isEmpty();
    if (printScan)
      Serial.printf("  %c%c %4d dBm  Kanal %2d  %s  %s\n", own ? '*' : ' ', WiFi.BSSIDstr(i) == cur ? '>' : ' ',
                    WiFi.RSSI(i), WiFi.channel(i), WiFi.BSSIDstr(i).c_str(), s.c_str());
    if (own) {
      seen = true;
      if (WiFi.RSSI(i) > bestRssi) {
        bestRssi = WiFi.RSSI(i);
        bestChannel = WiFi.channel(i);
        memcpy(bestBssid, WiFi.BSSID(i), 6);
      }
    }
    if (options.indexOf(">" + s + "<") >= 0 || s.isEmpty()) continue;
    options += "<option>" + s + "</option>";
  }
  WiFi.scanDelete();
  printScan = false;
  return true;
}

void listNetworks() {
  printScan = true;
  startScan();
}

// Nach einem Scan: zu einem deutlich stärkeren Knoten desselben Netzes wechseln
static void maybeRoam() {
  if (!connected() || bestRssi == -127) return;
  int cur = WiFi.RSSI();
  if (memcmp(bestBssid, WiFi.BSSID(), 6) == 0 || bestRssi < cur + ROAM_BETTER_DB) return;
  Serial.printf("WLAN: wechsle zu stärkerem Knoten %02X:%02X:%02X:%02X:%02X:%02X (%d dBm statt %d dBm)\n", bestBssid[0],
                bestBssid[1], bestBssid[2], bestBssid[3], bestBssid[4], bestBssid[5], bestRssi, cur);
  WiFi.begin(settings.ssid.c_str(), settings.pass.c_str(), bestChannel, bestBssid);
}

void begin() {
  WiFi.persistent(false);
  WiFi.setHostname(deviceName().c_str());
  configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
  // Alle Kanäle absuchen und den stärksten Zugangspunkt nehmen statt des ersten gefundenen
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  if (configured()) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(settings.ssid.c_str(), settings.pass.c_str());
    Serial.printf("WLAN: verbinde mit \"%s\"\n", settings.ssid.c_str());
  } else {
    WiFi.mode(WIFI_OFF);
    Serial.println("WLAN: nicht eingerichtet (A+B 5 s halten)");
  }
}

static String page() {
  String h = F("<!doctype html><html lang=de><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
               "<title>Sanduhr einrichten</title><style>body{font-family:system-ui,sans-serif;max-width:420px;margin:24px auto;padding:0 16px;"
               "background:#14110d;color:#eee}input,select,button{width:100%;font-size:17px;padding:10px;margin:6px 0 14px;box-sizing:border-box;"
               "border-radius:6px;border:1px solid #555;background:#221d17;color:#eee}button{background:#f0a63a;color:#000;border:0;font-weight:600}"
               "small{color:#aaa}</style><h2>Sanduhr einrichten</h2>"
               "<p>Hier kommt die Sanduhr ins Heimnetz. Das braucht sie nur für Updates, die nachts automatisch kommen.</p>"
               "<form method=post action=/save><label>WLAN</label><select name=s>");
  h += options.isEmpty() ? String(F("<option value=''>Suche läuft - Seite gleich neu laden</option>")) : options;
  h += F("</select><label>oder Name von Hand <small>(versteckte Netze)</small></label><input name=m autocomplete=off>"
         "<label>Passwort</label><input name=p type=password><label>Update-Kanal</label><select name=c>");
  h += String("<option value=stabil") + (settings.channel == "stabil" ? " selected" : "") + ">stabil</option>";
  h += String("<option value=test") + (settings.channel == "test" ? " selected" : "") + ">test (Prototyp)</option>";
  h += F("</select><button>Speichern und neu starten</button></form><p><small>Version " FW_VERSION "</small></p>");
  return h;
}

void startPortal() {
  if (portal) return;
  Serial.println("WLAN: Einrichtung gestartet");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(deviceName().c_str());
  startScan();
  dns.start(53, "*", WiFi.softAPIP());
  server.on("/", HTTP_GET, [] { server.send(200, "text/html", page()); });
  server.on("/save", HTTP_POST, [] {
    String s = server.arg("m");
    if (s.isEmpty()) s = server.arg("s");
    settings.ssid = s;
    settings.pass = server.arg("p");
    settings.channel = server.arg("c") == "test" ? "test" : "stabil";
    settings.save();
    server.send(200, "text/html", F("<meta charset=utf-8><body style='font-family:sans-serif;background:#14110d;color:#eee;padding:24px'>"
                                    "<h2>Gespeichert</h2><p>Die Sanduhr startet neu und verbindet sich mit dem WLAN.</p>"));
    delay(1500);
    ESP.restart();
  });
  // Handys prüfen auf Internet; jede fremde Adresse zur Einrichtungsseite umleiten
  server.onNotFound([] {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
  });
  server.begin();
  portal = true;
  portalStart = millis();
}

void loop() {
  bool scanDone = pollScan();
  if (portal) {
    dns.processNextRequest();
    server.handleClient();
    if (millis() - portalStart > PORTAL_TIMEOUT) {
      Serial.println("WLAN: Einrichtung ohne Eingabe beendet");
      ESP.restart();
    }
    return;
  }
  // Ist das Netz überhaupt zu sehen? (für die Gesundheitsprüfung nach Updates)
  if (configured() && !connected() && !scanning && millis() - lastScan > 60000) startScan();
  if (scanDone && configured() && !connected()) WiFi.begin(settings.ssid.c_str(), settings.pass.c_str());

  // Knoten prüfen: 1 min nach dem Start, dann bei schwachem Pegel alle 10 min, sonst stündlich.
  // Nicht während eines Updates oder des Probelaufs (kurzer Verbindungsabbruch beim Wechsel).
  if (scanDone && roamScan) {
    roamScan = false;
    maybeRoam();
  }
  uint32_t every = !lastRoamCheck ? 60000UL : (WiFi.RSSI() < -70 ? 10 * 60000UL : 60 * 60000UL);
  bool busy = Updater::state() == Updater::CHECKING || Updater::state() == Updater::DOWNLOADING || Health::pending();
  if (connected() && !scanning && !busy && millis() - lastRoamCheck > every) {
    lastRoamCheck = millis();
    roamScan = true;
    startScan();
  }
}

bool connected() { return WiFi.status() == WL_CONNECTED; }

String status() {
  if (!configured()) return "nicht eingerichtet";
  if (!connected()) return "getrennt";
  int r = WiFi.RSSI();
  const char *q = r > -67 ? "gut" : r > -75 ? "mittel" : r > -85 ? "schwach" : "sehr schwach";
  return "verbunden, " + String(r) + " dBm (" + q + "), Kanal " + String(WiFi.channel()) + ", Knoten " + WiFi.BSSIDstr();
}
bool configured() { return settings.ssid.length() > 0; }
bool ssidVisible() { return seen; }
bool portalActive() { return portal; }
bool timeValid() { return time(nullptr) > 1700000000; }

}  // namespace Net
