#pragma once
// Touch-Tasten (Kupferfläche am Touch-Pin). Liefert kurze und lange Drücke.
#include <Arduino.h>

namespace Touch {
enum Event : uint8_t { NONE, A_SHORT, B_SHORT, C_SHORT, D_SHORT, A_LONG, B_LONG, AB_LONG };
void begin();
Event update();        // einmal pro Bild aufrufen
bool anyHeld();
void debugPrint();
}
