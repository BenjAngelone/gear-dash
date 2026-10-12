#pragma once
// Acceso directo al buffer del sprite (RGB565). LovyanGFX guarda los 2 bytes de cada pixel invertidos
// (big endian) en sprites de 16 bits; `swapped` se detecta al crear el sprite (display::begin), asi que el
// codigo de composicion no depende de ese detalle. Dibujar directo sobre el sprite evita copiar pantallas
// completas por la PSRAM, que es lenta (~20 MB/s).
#include <stdint.h>

namespace fb {

extern bool swapped;
uint16_t* buf();  // buffer del sprite; nullptr si no existe

inline uint16_t ld(uint16_t raw) { return swapped ? __builtin_bswap16(raw) : raw; }
inline uint16_t st(uint16_t v) { return swapped ? __builtin_bswap16(v) : v; }

inline void unpack(uint16_t p, int& r, int& g, int& b) {
  r = (p >> 11) << 3;
  g = ((p >> 5) & 0x3F) << 2;
  b = (p & 0x1F) << 3;
}
inline uint16_t pack(int r, int g, int b) {
  if (r > 255) r = 255;
  if (g > 255) g = 255;
  if (b > 255) b = 255;
  return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

}  // namespace fb
