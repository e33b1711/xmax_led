# LED-Sanduhr

Weihnachtsgeschenk: eine Sanduhr aus zwei 16×16-LED-Matrizen (WS2812B) mit ESP32, Lagesensor und
Touch-Tasten. Jedes Sandkorn ist eine LED, der Sand folgt der Schwerkraft. Neue Funktionen kommen
später per Update übers Internet, nachts automatisch.

4 Geschenke + 1 Prototyp. Ausgeliefert wird Version 1.x mit der Sanduhr.

## Ordner

| | |
|---|---|
| `firmware/` | PlatformIO-Projekt für den ESP32 |
| `tools/` | Release bauen und signieren, für alle freigeben, Notbremse |
| `ota/` | Update-Manifeste, die die Sanduhren abfragen (`manifest-test.json`, `manifest-stabil.json`) |
| `simulation/` | Browser-Simulationen: Sanduhr/Uhr/Ball, Wasserglas |

## Bedienung

| | |
|---|---|
| Umdrehen | Sand läuft in die andere Hälfte |
| Taste A kurz | Helligkeit (5 Stufen) |
| Taste B kurz | Sanduhr: Laufzeit 1 / 3 / 5 / 10 / 15 / 30 min, wird kurz als Zahl gezeigt; Game of Life: neu starten |
| Taste A 5 s | Betriebsart wechseln: Sanduhr ↔ Game of Life (wird gespeichert) |
| Taste B 5 s | jetzt nach Updates suchen |
| A + B 5 s | WLAN einrichten: WLAN-Symbol blinkt, Handy mit „Sanduhr-XXXX“ verbinden, Netz wählen |

Ohne WLAN läuft die Sanduhr ganz normal, sie bekommt dann nur keine Updates.

## Erstinstallation (per USB, einmal je Gerät)

```
cd firmware
pio run -e sanduhr -t upload -t monitor        # 16×32
pio run -e prototyp16 -t upload -t monitor     # Prototyp mit einer 16×16
```

Serielle Konsole (115200 Baud): `t` Testbild, `i` Sensorwerte, `k` Touch-Werte, `s` Status, `?` Hilfe,
`m` Matrixhöhe 16/32 umschalten (wird im Gerät gespeichert), `g` Sanduhr ↔ Game of Life.
Lage von Hand (ohne Sensor): `,` / `.` Azimut −/+5° (Drehung in der Ebene, 180° = auf dem Kopf),
`-` / `+` Elevation −/+5° (nach hinten kippen, 90° = flach), `0` Lage wieder vom Sensor.
Die Tasten lassen sich dort ersetzen (Prototyp ohne Touch): `a` wie Taste A, `b` wie Taste B,
`u` Update suchen, `w` WLAN einrichten.

Alle Geräte bekommen per Update dieselbe Firmware. Die Matrixhöhe merkt sich jedes Gerät selbst:
beim ersten Start wird sie festgehalten (`prototyp16` → 16, sonst 32); umstellen mit `m` auf der Konsole.

Am Prototyp einmal festlegen (in `firmware/src/config.h`):
- **Verdrahtung:** `t` zeigt oben links rot, oben rechts grün, unten links blau, unten rechts weiß und
  gelb die Panelgrenze. Passt das nicht, `PANEL_*` / `CHAIN_*` anpassen.
- **Sensorachsen:** `i` zeigt die Rohwerte. Matrix senkrecht hinstellen: gy muss ~ +1 sein, nach rechts
  kippen: gx ~ +1. Sonst `IMU_*` anpassen.
- **Touch:** `k` zeigt Messwert/Grundwert; beim Berühren muss der Wert deutlich (> 30 %) fallen.

## Updates

```
Release bauen ──► GitHub Release (firmware.bin) + ota/manifest-<kanal>.json
                                ▲
Sanduhr fragt nach ─────────────┘  neuer? → laden → Signatur prüfen → aufspielen → Probelauf
```

- **Kanäle:** `test` (Prototyp, prüft alle 15 Minuten) und `stabil` (Geschenke, einmal pro Nacht zwischen
  2 und 5 Uhr, jedes Gerät zu einer anderen Minute). Der Kanal wird auf der WLAN-Einrichtungsseite gewählt.
- Gesucht wird nur, wenn die Sanduhr 10 Minuten nicht bewegt wurde.
- **Signatur:** Jede Firmware ist mit einem privaten Schlüssel signiert (ECDSA P-256 über
  `xmax_led|<version>|<sha256>`). Die Sanduhr installiert nur, was dazu passt, und nur höhere Versionen.
- **Probelauf und Rückfall:** Eine neue Version läuft erst auf Probe. Nach 90 s ohne Absturz und mit
  WLAN-Verbindung gilt sie als gut. Stürzt sie vorher ab, hängt sie (Watchdog) oder bekommt sie kein WLAN,
  obwohl das Netz da ist, startet der ESP wieder die vorherige Version.

### Ablauf für eine neue Version

```
tools/release.sh 1.0.1 "Was ist neu"     # baut, signiert, Release + Kanal test
# … auf dem Prototyp ausprobieren …
tools/freigeben.sh 1.0.1                 # Kanal stabil: die Geschenke holen sie nachts
tools/zurueckziehen.sh 1.0.0             # Notbremse, falls noch nicht alle geladen haben
```

### ⚠️ Signaturschlüssel sichern

`~/.config/xmax_led/signatur-privat.pem` ist der einzige Schlüssel, mit dem die ausgelieferten Sanduhren
Updates annehmen. **Geht er verloren, gibt es für die Geschenke nie wieder ein Update** (außer per USB).
Also sicher aufbewahren, z. B. im Passwortmanager. Er gehört nie ins Repo.

## Hardware

- **Matrix:** 2× 16×16 WS2812B übereinander, Daten an GPIO16 über 330–470 Ω, besser mit Pegelwandler
  74AHCT125. Strom an beiden Panels einspeisen, Kabel ≥ 1,5 mm², Sicherung 5 A, 1000 µF am Eingang.
- **Netzteil:** geschlossenes 5-V-Steckernetzteil 6–8 A mit Hohlstecker (CE). Kein offenes 230-V-Netzteil
  im Geschenk. Die Firmware begrenzt die LEDs auf 3,5 A (`LED_MAX_MA`), die Sanduhr braucht meist 1–2 A.
- **Lagesensor:** ICM-20948 (9-DoF: Beschleunigung, Gyroskop, Kompass), I²C an GPIO21 (SDA) / 22 (SCL),
  3,3 V, Adresse 0x68 oder 0x69 wird erkannt. Fest mit dem Rahmen verbinden; Kompass mit Abstand zu den
  Stromleitungen.
- **Touch:** Kupferfläche an GPIO4 (A) und GPIO13 (B); frei für mehr: GPIO14, 27 (32/33 ohne Poti).
  Nicht GPIO0 und GPIO12. Funktioniert durch wenige mm Holz/Acryl bei ~2×2 cm Fläche.
- **ADC** (falls Potis): mit WLAN nur GPIO32–36, 39; Poti an 3,3 V.

## Simulationen

| Datei | Inhalt | Link |
|---|---|---|
| `simulation/sanduhr.html` | 16×32: Sanduhr mit Laufzeit, Zeigeruhr (12 bleibt oben), springender Ball | https://claude.ai/artifact/NCaJZoKSqQvejgco5VA5sw |
| `simulation/glas.html` | 8×32 als oben offenes Glas: Wasser kippen, ausgießen, nachfüllen | https://claude.ai/artifact/BgBgS6anMkW49H1yR6XqMF |

Die Sand-Physik der Firmware (`firmware/src/sand.cpp`) ist aus `sanduhr.html` übertragen.
