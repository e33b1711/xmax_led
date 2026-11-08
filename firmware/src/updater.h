#pragma once
// Updates übers Internet: Manifest prüfen, Firmware laden, Signatur prüfen, aufspielen.
// Läuft in einer eigenen Aufgabe, damit die Sanduhr dabei weiterläuft.
#include <Arduino.h>

namespace Updater {
enum State : uint8_t { IDLE, CHECKING, DOWNLOADING, DONE, FAILED };
void begin();
void loop();                 // entscheidet, wann geprüft wird (nachts / Testkanal / auf Wunsch)
void checkNow();             // Taste B lang: sofort suchen und ggf. installieren
State state();
float progress();            // 0..1 während DOWNLOADING
const char *lastResult();
}

// Gesundheitsprüfung nach einem Update: erst wenn die neue Version eine Weile sauber läuft,
// wird sie behalten. Stürzt sie vorher ab, startet der ESP automatisch wieder die alte.
namespace Health {
void begin();
void loop();
bool pending();              // neue Version noch nicht bestätigt
}
