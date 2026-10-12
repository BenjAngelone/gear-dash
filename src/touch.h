#pragma once
#include <Arduino.h>

// Driver CST816T (I2C) + filtro de toques falsos (ver logic.h / tokens.h).
namespace touch {

void begin();                           // requiere Wire iniciado
void setFlipped(bool f);                // pantalla girada 180: espejar coordenadas
void poll(uint32_t now, bool leverQuiet, uint32_t deadUntil);
bool raw();                             // hay dedo sobre el panel (sin filtrar)
bool active(uint32_t now);              // toque aceptado por el filtro
uint32_t heldMs(uint32_t now);          // tiempo sostenido de un toque aceptado
int startX();
int startY();
void debugLine(char* out, size_t n);  // contadores del CST816 (lecturas ok/fallidas, toques, ultima posicion)

}  // namespace touch
