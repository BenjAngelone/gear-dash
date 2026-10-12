#include "fx.h"

#include <esp_heap_caps.h>
#include <math.h>

#include "display.h"
#include "fbaccess.h"
#include "ui_assets.h"

// el "work" de cada frame es el propio buffer del sprite (sin copias intermedias)
static uint16_t* workPtr = nullptr;  // buffer del sprite (se fija en begin(): pedirlo por pixel es lento)
#define workBuf workPtr

namespace fx {

constexpr int W = tk::W, H = tk::H;
constexpr int CORNER = 140;           // lado del mapa fondo_esquina
constexpr int LOGO_X = 88, LOGO_Y = 71;

static uint16_t* bgBuf = nullptr;     // fondo renderizado (cache)

// parametros del fundido
static Bg from_{tk::BG, 0, 0}, to_{tk::BG, 0, 0};
static uint32_t fadeStart = 0;
static bool haveTarget = false;
// ultimo fondo renderizado (para saber si hay que re-renderizar)
static Bg rendered_{tk::BG, -1, -1};

using fb::pack;
using fb::unpack;
static inline void addPx(uint16_t& px, int ar, int ag, int ab) {
  int r, g, b;
  unpack(fb::ld(px), r, g, b);
  px = fb::st(pack(r + ar, g + ag, b + ab));
}

void begin() {
  workPtr = fb::buf();
  if (!bgBuf) bgBuf = (uint16_t*)heap_caps_calloc(W * H, 2, MALLOC_CAP_SPIRAM);
}

static bool sameBg(const Bg& a, const Bg& b) {
  return a.color.r == b.color.r && a.color.g == b.color.g && a.color.b == b.color.b &&
         fabsf(a.corner - b.corner) < 0.002f && fabsf(a.logo - b.logo) < 0.002f;
}

static float easeInOut(float t) { return t < 0.5f ? 2 * t * t : 1 - 2 * (1 - t) * (1 - t); }

static Bg current(uint32_t now) {
  if (!haveTarget) return to_;
  float t = (float)(now - fadeStart) / (float)tk::BG_FADE_MS;
  if (t >= 1.0f) return to_;
  float e = easeInOut(t < 0 ? 0 : t);
  Bg o;
  o.color.r = (uint8_t)(from_.color.r + (to_.color.r - from_.color.r) * e);
  o.color.g = (uint8_t)(from_.color.g + (to_.color.g - from_.color.g) * e);
  o.color.b = (uint8_t)(from_.color.b + (to_.color.b - from_.color.b) * e);
  o.corner = from_.corner + (to_.corner - from_.corner) * e;
  o.logo = from_.logo + (to_.logo - from_.logo) * e;
  return o;
}

void setBg(const Bg& target, uint32_t now) {
  if (!haveTarget) {
    from_ = to_ = target;
    haveTarget = true;
    fadeStart = now - tk::BG_FADE_MS;
    return;
  }
  if (sameBg(target, to_)) return;
  from_ = current(now);
  to_ = target;
  fadeStart = now;
}

static void setHud(uint16_t* buf, int x, int y, uint16_t c) {
  if (x >= 0 && x < W && y >= 0 && y < H) buf[y * W + x] = fb::st(c);
}

// "L" de brazos de 20 px, trazo 2 px, a 12 px del borde, con el codo cortado a 45 grados (chaflan 6).
static void hudCorner(uint16_t* buf, bool right, bool bottom) {
  const uint16_t c = pack(tk::HUD.r, tk::HUD.g, tk::HUD.b);
  auto put = [&](int dx, int dy) {  // dx, dy desde el vertice de la esquina, hacia adentro
    int x = right ? W - 1 - 12 - dx : 12 + dx;
    int y = bottom ? H - 1 - 12 - dy : 12 + dy;
    setHud(buf, x, y, c);
  };
  for (int t = 0; t < 2; t++)
    for (int i = 6; i < 20; i++) {
      put(i, t);  // brazo horizontal
      put(t, i);  // brazo vertical
    }
  for (int i = 0; i <= 6; i++) {  // diagonal del chaflan
    put(6 - i, i);
    put(6 - i + 1 < 7 ? 6 - i + 1 : 6, i);
  }
}

static void renderBg(const Bg& p) {
  memset(bgBuf, 0, W * H * 2);
  // luz de esquina: arriba-izquierda tal cual y abajo-derecha espejada en X e Y (aditiva)
  int kr = (int)(p.color.r * p.corner), kg = (int)(p.color.g * p.corner), kb = (int)(p.color.b * p.corner);
  if (p.corner > 0.001f) {
    for (int y = 0; y < CORNER; y++)
      for (int x = 0; x < CORNER; x++) {
        int a = FONDO_ESQUINA.a[y * CORNER + x];
        if (!a) continue;
        int ar = a * kr / 255, ag = a * kg / 255, ab = a * kb / 255;
        addPx(bgBuf[y * W + x], ar, ag, ab);
        addPx(bgBuf[(H - 1 - y) * W + (W - 1 - x)], ar, ag, ab);
      }
  }
  // logo traslucido (aditivo, en el color de la luz)
  int lr = (int)(p.color.r * p.logo), lg = (int)(p.color.g * p.logo), lb = (int)(p.color.b * p.logo);
  if (p.logo > 0.001f) {
    for (int y = 0; y < LOGO_FONDO.h; y++)
      for (int x = 0; x < LOGO_FONDO.w; x++) {
        int a = LOGO_FONDO.a[y * LOGO_FONDO.w + x];
        if (!a) continue;
        int px = LOGO_X + x, py = LOGO_Y + y;
        if (px >= W || py >= H) continue;
        addPx(bgBuf[py * W + px], a * lr / 255, a * lg / 255, a * lb / 255);
      }
  }
  hudCorner(bgBuf, false, false);
  hudCorner(bgBuf, true, false);
  hudCorner(bgBuf, false, true);
  hudCorner(bgBuf, true, true);
  rendered_ = p;
}

void bgFrame(uint32_t now, float cornerMul) {
  if (!bgBuf || !workBuf) return;
  Bg p = current(now);
  p.corner *= cornerMul;
  if (!sameBg(p, rendered_)) renderBg(p);
  memcpy(workBuf, bgBuf, W * H * 2);
}

void shadow(const ShadowMap& m, int x0, int y0, const tk::Rgb& color, float I, float R) {
  if (!workBuf || I <= 0.001f) return;
  // LUT v -> alfa (0..256):  d = 24 (1 - v/255);  g = clamp(1 - d/(6+R), 0, 1)^2;  alfa = 0,30 I g
  uint16_t lut[256];
  for (int v = 0; v < 256; v++) {
    float d = 24.0f * (1.0f - v / 255.0f);
    float g = 1.0f - d / (6.0f + R);
    g = g < 0 ? 0 : (g > 1 ? 1 : g);
    float a = 0.30f * I * g * g;
    lut[v] = (uint16_t)(a * 256.0f > 256.0f ? 256 : a * 256.0f);
  }
  const int sc = m.scale;
  for (int my = 0; my < m.h; my++) {
    for (int mx = 0; mx < m.w; mx++) {
      uint16_t al = lut[m.a[my * m.w + mx]];
      if (!al) continue;
      for (int sy = 0; sy < sc; sy++) {
        int y = y0 + my * sc + sy;
        if (y < 0 || y >= H) continue;
        for (int sx = 0; sx < sc; sx++) {
          int x = x0 + mx * sc + sx;
          if (x < 0 || x >= W) continue;
          uint16_t& px = workBuf[y * W + x];
          int r, g, b;
          unpack(fb::ld(px), r, g, b);
          r += ((color.r - r) * (int)al) >> 8;
          g += ((color.g - g) * (int)al) >> 8;
          b += ((color.b - b) * (int)al) >> 8;
          px = fb::st(pack(r, g, b));
        }
      }
    }
  }
}

void frame(float p, const tk::Rgb& color, float level, int thick) {
  if (!workBuf || p <= 0.001f) return;
  if (p > 1.0f) p = 1.0f;
  if (level > 1.0f) level = 1.0f;
  const uint16_t c = pack((int)(color.r * level), (int)(color.g * level), (int)(color.b * level));
  const uint16_t cs = fb::st(c);
  const int x0 = 12, y0 = 12, x1 = W - 13, y1 = H - 13, s = 6;  // s = chaflan de la esquina (ver hudCorner)
  const int lx = (int)(p * (W / 2 - x0 - s)), ly = (int)(p * (H / 2 - y0 - s));
  auto fill = [&](int x, int y, int w, int h) {
    for (int yy = y; yy < y + h; yy++)
      for (int xx = x; xx < x + w; xx++)
        if (xx >= 0 && xx < W && yy >= 0 && yy < H) workBuf[yy * W + xx] = cs;
  };
  fill(x0 + s, y0, lx, thick);              // arriba, desde la izquierda
  fill(x1 - s - lx + 1, y0, lx, thick);     // arriba, desde la derecha
  fill(x0 + s, y1 - thick + 1, lx, thick);  // abajo
  fill(x1 - s - lx + 1, y1 - thick + 1, lx, thick);
  fill(x0, y0 + s, thick, ly);              // izquierda, desde arriba
  fill(x0, y1 - s - ly + 1, thick, ly);     // izquierda, desde abajo
  fill(x1 - thick + 1, y0 + s, thick, ly);  // derecha
  fill(x1 - thick + 1, y1 - s - ly + 1, thick, ly);
}

void flash(const tk::Rgb& color, float alpha) {
  if (!workBuf || alpha <= 0.002f) return;
  int ar = (int)(color.r * alpha), ag = (int)(color.g * alpha), ab = (int)(color.b * alpha);
  for (int i = 0; i < W * H; i++) addPx(workBuf[i], ar, ag, ab);
}

bool fading(uint32_t now) { return haveTarget && now - fadeStart < tk::BG_FADE_MS; }

void present() {}  // el buffer de trabajo ya es el sprite: nada que copiar

}  // namespace fx
