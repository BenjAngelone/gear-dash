#pragma once
#include "display.h"
#include "alpha_img.h"

namespace glyphs {

// Dibuja un bitmap alpha centrado en (cx,cy) con escala (sx,sy), color y brillo (level 0..1).
// Mezcla contra lo que ya haya en el sprite (alpha real), con muestreo bilineal.
void draw(const AlphaImg& im, float cx, float cy, float sx, float sy, const tk::Rgb& color, float level);

}  // namespace glyphs
