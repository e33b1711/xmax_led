#pragma once
// WLAN: Verbindung mit dem gespeicherten Netz, Uhrzeit per NTP, Einrichtungsseite.
#include <Arduino.h>

namespace Net {
void begin();
void loop();
bool connected();
String status();           // "verbunden, -63 dBm (gut)", "getrennt", "nicht eingerichtet"
bool timeValid();          // Uhrzeit per NTP bekommen
bool configured();         // ein WLAN ist eingetragen
bool ssidVisible();        // eingetragenes Netz war beim letzten Suchen zu sehen
void listNetworks();       // sichtbare Netze mit Pegel ausgeben (Konsole n)
void startPortal();        // eigenes WLAN "Sanduhr-XXXX" mit Einrichtungsseite
bool portalActive();
String deviceName();
}
