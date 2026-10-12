#pragma once
#include <Arduino.h>

// Mascara de alpha de 8 bits (255 = opaco). El color lo pone el codigo.
struct AlphaImg {
  uint16_t w, h;
  const uint8_t* a;
};

// Mapa de sombra: v = 255 dentro de la forma; afuera 255*(1 - d/24), d = distancia al borde en px.
// scale = 1 (resolucion completa) o 2 (cada muestra cubre 2x2 px).
struct ShadowMap {
  uint16_t w, h;
  uint8_t scale;
  const uint8_t* a;
};
