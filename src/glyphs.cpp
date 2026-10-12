#include "glyphs.h"

#include <math.h>

#include "fbaccess.h"

namespace glyphs {

static inline float sampleA(const AlphaImg& im, float x, float y) {
  if (x < 0) x = 0;
  if (y < 0) y = 0;
  if (x > im.w - 1) x = im.w - 1;
  if (y > im.h - 1) y = im.h - 1;
  int x0 = (int)x, y0 = (int)y;
  int x1 = x0 + 1 < im.w ? x0 + 1 : x0, y1 = y0 + 1 < im.h ? y0 + 1 : y0;
  float fx = x - x0, fy = y - y0;
  float a = im.a[y0 * im.w + x0], b = im.a[y0 * im.w + x1];
  float c = im.a[y1 * im.w + x0], d = im.a[y1 * im.w + x1];
  return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy;
}

// Mezcla `color` sobre el buffer del sprite con el alpha de la mascara. Recorte propio contra la pantalla.
// Sin escala (sx = sy = 1) copia directo; con escala usa muestreo bilineal.
void draw(const AlphaImg& im, float cx, float cy, float sx, float sy, const tk::Rgb& color, float level) {
  if (level <= 0.01f || sx <= 0.01f || sy <= 0.01f) return;
  uint16_t* d = fb::buf();
  if (!d) return;
  const bool unit = fabsf(sx - 1.0f) < 0.002f && fabsf(sy - 1.0f) < 0.002f;
  int w = unit ? im.w : (int)ceilf(im.w * sx), h = unit ? im.h : (int)ceilf(im.h * sy);
  int left = (int)roundf(cx - w / 2.0f), top = (int)roundf(cy - h / 2.0f);
  if (level > 1) level = 1;
  int x0 = left < 0 ? 0 : left, y0 = top < 0 ? 0 : top;
  int x1 = left + w > tk::W ? tk::W : left + w, y1 = top + h > tk::H ? tk::H : top + h;
  if (x1 <= x0 || y1 <= y0) return;

  const int lvl = (int)(level * 256.0f);
  const int cr = color.r, cg = color.g, cb = color.b;
  const float ix = 1.0f / sx, iy = 1.0f / sy;
  for (int y = y0; y < y1; y++) {
    uint16_t* row = d + y * tk::W;
    const uint8_t* srow = unit ? im.a + (y - top) * im.w : nullptr;
    float syy = ((y - top) + 0.5f) * iy - 0.5f;
    for (int x = x0; x < x1; x++) {
      int a = unit ? srow[x - left] : (int)sampleA(im, ((x - left) + 0.5f) * ix - 0.5f, syy);
      a = (a * lvl) >> 8;
      if (!a) continue;
      int r, g, b;
      fb::unpack(fb::ld(row[x]), r, g, b);
      r += ((cr - r) * a) >> 8;
      g += ((cg - g) * a) >> 8;
      b += ((cb - b) * a) >> 8;
      row[x] = fb::st(fb::pack(r, g, b));
    }
  }
}

}  // namespace glyphs
