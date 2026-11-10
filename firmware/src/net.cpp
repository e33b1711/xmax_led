#include "net.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <time.h>
#include "config.h"
#include "settings.h"

namespace Net {

static WebServer server(80);
static DNSServer dns;
static bool portal = false;
static uint32_t portalStart = 0, lastScan = 0;
static bool seen = false;
static String options;   // <option>-Liste der gefundenen Netze
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
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s == settings.ssid) seen = true;
    if (options.indexOf(">" + s + "<") >= 0 || s.isEmpty()) continue;
    options += "<option>" + s + "</option>";
  }
  WiFi.scanDelete();
  return true;
}

void begin() {
  WiFi.persistent(false);
  WiFi.setHostname(deviceName().c_str());
  configTzTime(TZ_INFO, "pool.ntp.org", "time.google.com");
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
}

bool connected() { return WiFi.status() == WL_CONNECTED; }
bool configured() { return settings.ssid.length() > 0; }
bool ssidVisible() { return seen; }
bool portalActive() { return portal; }
bool timeValid() { return time(nullptr) > 1700000000; }

}  // namespace Net
