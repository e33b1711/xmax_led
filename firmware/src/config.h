#pragma once
// Alle Einstellungen, die von der Hardware abhängen, an einer Stelle.

#ifndef FW_VERSION
#define FW_VERSION "0.0.0-dev"
#endif

// ---------- Matrix ----------
#define MATRIX_W 16
// Höhe wird im Gerät gespeichert (serieller Befehl m), damit alle Geräte dieselbe Firmware
// bekommen. MATRIX_H ist nur der Startwert, solange nichts gespeichert ist.
#ifndef MATRIX_H
#define MATRIX_H 32            // zwei 16x16 übereinander; Prototyp: 16
#endif
#define MATRIX_H_MAX 32
#define PANEL 16               // Kantenlänge eines Panels
#define NUM_LEDS_MAX (MATRIX_W * MATRIX_H_MAX)
#define LED_PIN 16

// Verdrahtung innerhalb eines Panels (Schlangenlinien). Werte wie bei der HA-Matrix, wenn der
// Aufdruck lesbar ist: erste LED unten links, unterste Zeile links->rechts, darüber rechts->links.
// Stimmt das Testbild nicht, hier anpassen.
#define PANEL_ROWS_FROM_BOTTOM 1
#define PANEL_FIRST_ROW_LEFT_TO_RIGHT 1
// Kette: 1 = Daten gehen zuerst in das obere Panel, dann ins untere.
#define CHAIN_TOP_PANEL_FIRST 1

// Strom: Steckernetzteil 5 V / 6-8 A. Grenze für die LEDs (ESP und Ruhestrom kommen dazu).
#define LED_MAX_MA 3500
// Obergrenze der Helligkeit (0-255), unabhängig von der Stufe des Nutzers
#define BRIGHT_MAX 160

// ---------- Beschleunigungssensor ICM-20948 (I2C) ----------
#define I2C_SDA 21
#define I2C_SCL 22
// Welche Sensorachse zeigt in Matrix-Richtung +x (nach rechts) bzw. +y (nach unten)?
// Achse: 0 = X, 1 = Y, 2 = Z; Vorzeichen +1/-1. Am Prototyp per serieller Ausgabe bestimmen.
#define IMU_MX_AXIS 0
#define IMU_MX_SIGN 1
#define IMU_MY_AXIS 1
#define IMU_MY_SIGN -1

// ---------- Touch-Tasten ----------
// Sicher nutzbare Touch-Pins: 4, 13, 14, 27 (32/33 nur ohne Poti). -1 = nicht bestückt.
#define TOUCH_A 4              // Helligkeit
#define TOUCH_B 13             // Laufzeit
#define TOUCH_C -1
#define TOUCH_D -1

// ---------- Updates ----------
#define GITHUB_USER "e33b1711"
#define GITHUB_REPO "xmax_led"
// Manifest je Kanal: ota/manifest-<kanal>.json im Hauptzweig des Repos
#define OTA_MANIFEST_BASE "https://raw.githubusercontent.com/" GITHUB_USER "/" GITHUB_REPO "/main/ota/manifest-"
#define OTA_NIGHT_FROM 2       // Updates nachts zwischen 2:00 ...
#define OTA_NIGHT_TO 5         // ... und 5:00 Uhr
#define OTA_TEST_INTERVAL_MIN 15   // Kanal "test": alle 15 Minuten, auch tagsüber
#define OTA_IDLE_MIN 10        // nur wenn die Sanduhr so lange nicht bewegt wurde

#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"
