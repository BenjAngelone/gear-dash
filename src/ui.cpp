#include "ui.h"

#include <math.h>

#include "car_link.h"
#include "fbaccess.h"
#include "fx.h"
#include "gear.h"
#include "glyph_data.h"
#include "glyphs.h"
#include "logo.h"
#include "phone_key.h"
#include "ui_assets.h"

namespace ui {

using tk::Rgb;

// ---- utilidades --------------------------------------------------------------------
static float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static float easeOutBack(float t) {
  const float c1 = 1.70158f, c3 = c1 + 1;
  t -= 1;
  return 1 + c3 * t * t * t + c1 * t * t;
}
static float easeInCubic(float t) { return t * t * t; }
static float easeOutCubic(float t) {
  t = 1 - t;
  return 1 - t * t * t;
}

void drawSplash() {
  tft.fillScreen(TFT_BLACK);
  tft.pushImage((tk::W - LOGO_SPLASH_W) / 2, (tk::H - LOGO_SPLASH_H) / 2, LOGO_SPLASH_W, LOGO_SPLASH_H,
                (const lgfx::rgb565_t*)LOGO_SPLASH);
}

// Feedback de pulsacion: marco blanco que se cierra desde las esquinas; al completarse explota en el
// color de la accion (verde al encender, rojo al apagar, blanco al abrir el menu).
static void drawPress(const PressView& pv) {
  if (pv.p <= 0.001f) return;
  bool fl = pv.flash > 0.001f;
  if (fl) {
    float k = 1.0f - pv.flash;
    fx::frame(1.0f, pv.flashColor, 1.0f, 2 + (int)(4.0f * k));
    fx::flash(pv.flashColor, 0.35f * k * k);
  } else {
    fx::frame(pv.p, tk::FG, 0.35f + 0.65f * pv.p, 2);
  }
}

static void drawBrandMark() {  // rombo de la marca (sin texto), blanco, en (106, 16)
  glyphs::draw(LOGO_MARCHAS, 106 + LOGO_MARCHAS.w / 2.0f, 16 + LOGO_MARCHAS.h / 2.0f, 1, 1, tk::FG, 1);
}

// ---- marchas ------------------------------------------------------------------------
static const AlphaImg* const GEAR_IMG[] = {&G_N, &G_1R, &G_2, &G_3, &G_4, &G_5};
static const ShadowMap* const GEAR_SHADOW[] = {&SH_G_N, &SH_G_1R, &SH_G_2, &SH_G_3, &SH_G_4, &SH_G_5};

// toda la pantalla cuenta (mas tolerante), menos el margen de borde que el filtro de toques ya rechaza
bool inOpenZone(int x, int y) { return x >= tk::TOUCH_EDGE_PX && x < tk::W - tk::TOUCH_EDGE_PX && y >= tk::TOUCH_EDGE_PX && y < tk::H - tk::TOUCH_EDGE_PX; }
bool inButton(int x, int y) { return inOpenZone(x, y); }

void drawGear(uint32_t now, const PressView& pv) {
  // fondo: luz ambar al 16 % con el motor en marcha, blanca al 10 % con el motor apagado
  bool engineOn = car::state() == logic::CAR_ON && car::stateAgeMs() < 60000;
  fx::setBg({engineOn ? tk::ACCENT : tk::FG, engineOn ? 0.16f : 0.10f, 0.10f}, now);
  fx::bgFrame(now);

  const float cx = tk::W / 2, cy = tk::GEAR_CY;
  int cur = gear::current(), prev = gear::previous();
  uint32_t since = now - gear::changedAt();
  bool animating = cur >= 0 && gear::changedAt() != 0 && since < tk::GEAR_ANIM_MS;

  // sombra de pulsacion detras del numero (sigue su silueta)
  if (cur >= 0 && pv.I > 0.001f) {
    const ShadowMap& m = *GEAR_SHADOW[cur];
    fx::shadow(m, (int)(cx - m.w * m.scale / 2.0f), (int)(cy - m.h * m.scale / 2.0f), tk::FG, pv.I, pv.R);
  }
  drawPress(pv);
  fx::present();
  drawBrandMark();

  if (cur < 0) {  // sin marcha definida
    spr.fillRect(cx - 28, cy - 4, 56, 8, display::col(tk::MUTE, 0.5f));
  } else if (!animating) {
    glyphs::draw(*GEAR_IMG[cur], cx, cy, 1, 1, tk::FG, 1);
  } else {
    float t = clamp01(since / (float)tk::GEAR_ANIM_MS);
    float dir = (prev < 0 || cur > prev) ? 1.0f : -1.0f;  // +1 = marcha mas alta: el numero nuevo sube

    // franja amarilla que barre el numero en la direccion del cambio
    float sy = cy + dir * (1 - easeOutCubic(t)) * 120.0f;
    float sl = 1.0f - t;
    spr.fillRect(28, (int)sy - 1, tk::W - 56, 3, display::col(tk::ACCENT, sl));
    spr.fillRect(28, (int)(sy - dir * 9) - 1, tk::W - 56, 1, display::col(tk::ACCENT, sl * 0.5f));

    // numero anterior: sale en la direccion del cambio, se achica y se desvanece
    float tOut = clamp01(since / (float)tk::GEAR_OUT_MS);
    if (prev >= 0 && tOut < 1.0f) {
      float e = easeInCubic(tOut);
      glyphs::draw(*GEAR_IMG[prev], cx, cy - dir * 80.0f * e, 1.0f - 0.18f * e, 1.0f + 0.25f * e, tk::FG,
                   1.0f - tOut);
    }
    // numero nuevo: entra desde el lado opuesto con rebote, algo estirado en el movimiento
    float e = easeOutBack(t);
    float stretch = 1.0f + 0.22f * (1 - clamp01(t * 1.6f));
    glyphs::draw(*GEAR_IMG[cur], cx, cy + dir * 100.0f * (1 - e), 1.0f, stretch, tk::FG, clamp01(t * 3.0f));
  }

  if (!gear::imuOk()) glyphs::draw(S_ERROR, tk::W / 2, 262, 1, 1, tk::DANGER, 1);
}

void bench() {
  Serial.printf("sprite: buffer=%s bytes invertidos=%d\n", fb::buf() ? "ok" : "NULO", (int)fb::swapped);
  const int N = 20;
  uint32_t t[10] = {0};
  uint32_t now = millis();
  for (int i = 0; i < N; i++) {
    uint32_t n2 = now + i * 33;
    uint32_t a0 = micros();
    fx::setBg({tk::FG, 0.10f, 0.10f}, n2);
    fx::bgFrame(n2);
    uint32_t a1 = micros();
    fx::shadow(SH_G_3, 60, 60, tk::FG, 0.9f, 9);
    uint32_t a2 = micros();
    fx::frame(0.6f, tk::FG, 0.7f, 2);
    fx::flash(tk::GOOD, 0.1f);
    uint32_t a3 = micros();
    fx::present();
    uint32_t a4 = micros();
    glyphs::draw(G_3, 120, 172, 1, 1, tk::FG, 1);
    uint32_t a5 = micros();
    glyphs::draw(G_3, 120, 172, 1.1f, 1.2f, tk::FG, 0.8f);
    uint32_t a6 = micros();
    drawBrandMark();
    uint32_t a7 = micros();
    display::present();
    uint32_t a8 = micros();
    t[0] += a1 - a0; t[1] += a2 - a1; t[2] += a3 - a2; t[3] += a4 - a3;
    t[4] += a5 - a4; t[5] += a6 - a5; t[6] += a7 - a6; t[7] += a8 - a7;
  }
  Serial.printf("bench us/cuadro: fondo(memcpy)=%lu sombra=%lu marco+destello=%lu fx->spr=%lu glifo(x1)=%lu glifo(escala)=%lu logo=%lu spr->panel=%lu\n",
                (unsigned long)(t[0] / N), (unsigned long)(t[1] / N), (unsigned long)(t[2] / N), (unsigned long)(t[3] / N),
                (unsigned long)(t[4] / N), (unsigned long)(t[5] / N), (unsigned long)(t[6] / N), (unsigned long)(t[7] / N));
}

// ---- calibracion --------------------------------------------------------------------
static const char* const CAL_NAME[] = {"N", "1 / R", "2", "3", "4", "5"};

void drawCalibration(uint32_t now) {
  spr.fillSprite(TFT_BLACK);
  drawBrandMark();
  spr.setTextDatum(middle_center);
  spr.setFont(&fonts::DejaVu18);
  spr.setTextColor(display::col(tk::ACCENT), TFT_BLACK);
  spr.drawString("CALIBRAR", tk::W / 2, 92);
  spr.setFont(&fonts::DejaVu12);
  spr.setTextColor(display::col(tk::MUTE), TFT_BLACK);
  spr.drawString("Pone la palanca en:", tk::W / 2, 116);
  spr.setFont(&fonts::DejaVu72);
  spr.setTextColor(display::col(tk::FG), TFT_BLACK);
  spr.drawString(CAL_NAME[gear::calStep()], tk::W / 2, 172);

  float p = gear::calProgress(now);
  spr.drawRect(30, 222, tk::W - 60, 12, display::col(tk::LINE));
  spr.fillRect(32, 224, (int)((tk::W - 64) * p), 8, display::col(tk::ACCENT));
  spr.setFont(&fonts::DejaVu12);
  spr.setTextColor(display::col(tk::MUTE), TFT_BLACK);
  const char* hint = gear::calWaitingMove() ? "movela a la siguiente posicion"
                     : gear::calExploring() ? "movela en todo su juego"
                                            : "mantenela quieta...";
  spr.drawString(hint, tk::W / 2, 248);
  char buf[32];
  if (gear::calExploring())
    snprintf(buf, sizeof(buf), "%d / %d  -  %d pts", gear::calStep() + 1, (int)gear::G_COUNT, gear::calPoints());
  else
    snprintf(buf, sizeof(buf), "%d / %d", gear::calStep() + 1, (int)gear::G_COUNT);
  spr.drawString(buf, tk::W / 2, 266);
}

// ---- menu ---------------------------------------------------------------------------
constexpr float ICON_CX = 120, ICON_CY = 116;  // icono 96x96 con esquina en (72, 68)
constexpr int GLOW_X = 36, GLOW_Y = 32;        // halo 168x168
constexpr int LABEL_Y = 214;
constexpr uint32_t CARGA_FRAME_MS = 80;

// Frame de la carga: rombo de 12 segmentos; el brillo de cada uno depende de su distancia a la cabeza
// (100 %, 55 %, 28 %, 12 %). Se arma desde el mapa de segmentos (ver tools/make_ui_assets.py).
static uint8_t cargaBuf[96 * 96];
static const AlphaImg cargaFrame = {96, 96, cargaBuf};

static void buildCargaFrame(int f) {
  static const uint8_t TAIL[4] = {255, 140, 71, 30};
  for (int i = 0; i < 96 * 96; i++) {
    uint8_t id = CARGA_ID.a[i];
    if (id == 255) {
      cargaBuf[i] = 0;
      continue;
    }
    int k = (f - id + 12) % 12;
    cargaBuf[i] = k < 4 ? (uint8_t)(CARGA_PISTA.a[i] * TAIL[k] / 255) : 0;
  }
}

static void drawDiamond(float cx, float cy, float half, const Rgb& c) {
  uint32_t col = display::col(c);
  spr.fillTriangle(cx, cy - half, cx + half, cy, cx, cy + half, col);
  spr.fillTriangle(cx, cy - half, cx - half, cy, cx, cy + half, col);
}

// ---- PIN y vinculacion -----------------------------------------------------------------
// teclas: filas 0..2 = 1..9; fila 3 = BT, 0, borrar
static int keyCode(int r, int c) {
  if (r < 3) return r * 3 + c + 1;
  return c == 0 ? KEY_BT : (c == 1 ? 0 : KEY_DEL);
}

int keyAt(int x, int y) {
  int dx = x - tk::KEYPAD_X, dy = y - tk::KEYPAD_Y;
  if (dx < 0 || dy < 0) return -1;
  int c = dx / (tk::KEY_W + tk::KEY_GAP), r = dy / (tk::KEY_H + tk::KEY_GAP);
  if (c > 2 || r > 3) return -1;
  // el hueco entre teclas cuenta para la tecla de la izquierda/arriba (mas tolerante)
  return keyCode(r, c);
}

static void text(const char* s, int x, int y, const lgfx::IFont* f, const Rgb& c) {
  spr.setTextDatum(middle_center);
  spr.setFont(f);
  spr.setTextColor(display::col(c));
  spr.drawString(s, x, y);
}

static void drawPin(uint32_t now, const MenuView& v) {
  fx::setBg({tk::FG, 0.10f, 0.10f}, now);
  fx::bgFrame(now);
  fx::present();

  bool locked = v.lockLeftMs > 0;
  if (locked) text("BLOQUEADO", tk::W / 2, 24, &fonts::DejaVu12, tk::DANGER);
  else if (v.pinForEnroll) text("PIN PARA VINCULAR", tk::W / 2, 24, &fonts::DejaVu12, tk::ACCENT);
  else text("PIN PARA ARRANCAR", tk::W / 2, 24, &fonts::DejaVu12, tk::MUTE);

  if (locked) {
    char buf[12];
    snprintf(buf, sizeof(buf), "%u s", (unsigned)((v.lockLeftMs + 999) / 1000));
    text(buf, tk::W / 2, tk::PIN_DOTS_Y, &fonts::DejaVu18, tk::DANGER);
  } else {  // 4 rombos: llenos los tipeados; rojos y sacudiendose tras un error
    uint32_t tw = now - v.pinWrongAt;
    bool wrong = v.pinWrongAt != 0 && tw < tk::PIN_WRONG_SHOW_MS;
    float shake = wrong ? 8.0f * (1.0f - tw / (float)tk::PIN_WRONG_SHOW_MS) * sinf(tw * 0.06f) : 0;
    for (int i = 0; i < tk::PIN_LEN; i++) {
      const Rgb& c = wrong ? tk::DANGER : (i < v.pinLen ? tk::FG : tk::TRACK);
      drawDiamond(tk::W / 2 + (i - 1.5f) * 24 + shake, tk::PIN_DOTS_Y, 7, c);
    }
  }

  for (int r = 0; r < 4; r++)
    for (int c = 0; c < 3; c++) {
      int k = keyCode(r, c);
      int x = tk::KEYPAD_X + c * (tk::KEY_W + tk::KEY_GAP), y = tk::KEYPAD_Y + r * (tk::KEY_H + tk::KEY_GAP);
      bool pressed = !locked && k == v.pressedKey;
      bool bt = k == KEY_BT && v.pinForEnroll;
      spr.fillRect(x, y, tk::KEY_W, tk::KEY_H, display::col(pressed ? tk::TRACK : tk::PANEL));
      spr.drawRect(x, y, tk::KEY_W, tk::KEY_H, display::col(pressed ? tk::FG : (bt ? tk::ACCENT : tk::LINE)));
      int cx = x + tk::KEY_W / 2, cy = y + tk::KEY_H / 2;
      const Rgb& lc = locked ? tk::MUTE : tk::FG;
      if (k == KEY_DEL) {  // flecha de borrar
        uint32_t col = display::col(lc);
        spr.fillTriangle(cx - 12, cy, cx - 4, cy - 7, cx - 4, cy + 7, col);
        spr.fillRect(cx - 4, cy - 2, 16, 4, col);
      } else if (k == KEY_BT) {
        text("BT", cx, cy, &fonts::DejaVu12, bt ? tk::ACCENT : lc);
      } else {
        char d[2] = {(char)('0' + k), 0};
        text(d, cx, cy, &fonts::DejaVu18, lc);
      }
    }
}

static void drawEnroll(uint32_t now, const MenuView& v) {
  bool waiting = v.enroll == phone::EN_WAITING, done = v.enroll == phone::EN_DONE;
  fx::setBg({waiting ? tk::ACCENT : (done ? tk::GOOD : tk::DANGER), 0.14f, 0.13f}, now);
  fx::bgFrame(now, waiting ? 0.8f + 0.2f * cosf(2.0f * (float)M_PI * (now % 1920) / 1920.0f) : 1.0f);
  fx::present();

  char buf[32];
  if (waiting) {
    text("VINCULAR TELEFONO", tk::W / 2, 32, &fonts::DejaVu12, tk::ACCENT);
    text("En el iPhone:", tk::W / 2, 72, &fonts::DejaVu12, tk::MUTE);
    text("Ajustes > Bluetooth", tk::W / 2, 92, &fonts::DejaVu12, tk::FG);
    text("> Gear Dash", tk::W / 2, 110, &fonts::DejaVu12, tk::FG);
    text("y tipea el codigo:", tk::W / 2, 140, &fonts::DejaVu12, tk::MUTE);
    snprintf(buf, sizeof(buf), "%03u %03u", (unsigned)(v.passkey / 1000), (unsigned)(v.passkey % 1000));
    text(buf, tk::W / 2, 182, &fonts::DejaVu24, tk::FG);
    snprintf(buf, sizeof(buf), "%u s - toca para cancelar", (unsigned)((v.enrollLeftMs + 999) / 1000));
    text(buf, tk::W / 2, 248, &fonts::DejaVu12, tk::MUTE);
  } else if (done) {
    text("VINCULADO", tk::W / 2, 128, &fonts::DejaVu24, tk::GOOD);
    snprintf(buf, sizeof(buf), "%d telefono%s", v.phones, v.phones == 1 ? "" : "s");
    text(buf, tk::W / 2, 164, &fonts::DejaVu12, tk::MUTE);
  } else {
    text("NO SE VINCULO", tk::W / 2, 128, &fonts::DejaVu18, tk::DANGER);
    text("Olvida 'Gear Dash' en el", tk::W / 2, 164, &fonts::DejaVu12, tk::MUTE);
    text("iPhone y proba de nuevo", tk::W / 2, 182, &fonts::DejaVu12, tk::MUTE);
  }
}

void drawMenu(uint32_t now, const MenuView& v, const PressView& pv) {
  if (v.kind == MenuView::PIN) return drawPin(now, v);
  if (v.kind == MenuView::ENROLL) return drawEnroll(now, v);

  // ---- fondo segun el estado -----------------------------------------------------
  bool loading = v.kind == MenuView::CONSULTING || v.kind == MenuView::SENDING;
  bool on = v.state == logic::CAR_ON;
  fx::Bg bg;
  float cornerMul = 1.0f;
  if (v.kind == MenuView::ERROR) bg = {tk::DANGER, 0.24f, 0.13f};
  else if (v.kind == MenuView::DONE) bg = {tk::ACCENT, 0.24f, 0.13f};
  else if (loading) {
    bg = {tk::ACCENT, 0.14f, 0.13f};
    cornerMul = 0.8f + 0.2f * cosf(2.0f * (float)M_PI * (now % 1920) / 1920.0f);  // respira x0,6..1,0
  } else if (on) bg = {tk::ACCENT, 0.24f, 0.13f};
  else bg = {tk::FG, 0.10f, 0.10f};
  fx::setBg(bg, now);
  fx::bgFrame(now, cornerMul);

  // ---- halo (sombra de pulsacion o destello) ---------------------------------------
  uint32_t t = now - v.kindSince;
  if (v.kind == MenuView::REST) {
    fx::shadow(on ? GL_APAGAR : GL_ENCENDER, GLOW_X, GLOW_Y, tk::FG, pv.I, pv.R);
    drawPress(pv);
  } else if (v.kind == MenuView::DONE) {  // destello I 2,0 -> 0 en 400 ms, R = 16
    float u = clamp01(t / 400.0f);
    fx::shadow(GL_LISTO, GLOW_X, GLOW_Y, tk::ACCENT, 2.0f * (1 - u) * (1 - u), 16);
  } else if (v.kind == MenuView::ERROR) {  // doble destello rojo: 120 ms on / 120 ms off x2
    if (t < 480 && (t / 120) % 2 == 0) fx::shadow(GL_ERROR, GLOW_X, GLOW_Y, tk::DANGER, 1.6f, 12);
  }
  fx::present();

  // ---- icono / carga ---------------------------------------------------------------
  const AlphaImg* label = &M_CONSULTANDO;
  Rgb labelCol = tk::MUTE;
  switch (v.kind) {
    case MenuView::REST:
      if (on) {
        glyphs::draw(ICON_APAGAR, ICON_CX, ICON_CY, 1, 1, tk::ACCENT, 1);
        label = &M_APAGAR;
        labelCol = tk::ACCENT;
      } else {
        glyphs::draw(ICON_ENCENDER, ICON_CX, ICON_CY, 1, 1, tk::FG, 1);
        label = &M_ENCENDER;
        labelCol = tk::FG;
      }
      break;
    case MenuView::DONE:
      glyphs::draw(ICON_LISTO, ICON_CX, ICON_CY, 1, 1, tk::ACCENT, 1);
      label = &M_LISTO;
      labelCol = tk::ACCENT;
      break;
    case MenuView::ERROR:
      glyphs::draw(ICON_ERROR, ICON_CX, ICON_CY, 1, 1, tk::DANGER, 1);
      label = v.noResponse ? &M_SINRESP : &M_ERROR;
      labelCol = tk::DANGER;
      break;
    default: {  // CONSULTING / SENDING
      glyphs::draw(CARGA_PISTA, ICON_CX, ICON_CY, 1, 1, tk::TRACK, 1);
      buildCargaFrame((now / CARGA_FRAME_MS) % 12);
      glyphs::draw(cargaFrame, ICON_CX, ICON_CY, 1, 1, tk::ACCENT, 1);
      if (v.kind == MenuView::SENDING) {
        label = v.turningOn ? &M_ARRANCANDO : &M_APAGANDO;
        for (int i = 0; i < (int)logic::SEQ_LEN; i++) {  // 5 rombitos de 12 px, paso 20 px, y = 176
          const Rgb& c = i < v.step ? tk::ACCENT : (i == v.step ? tk::DIM_ACCENT : tk::TRACK);
          drawDiamond(120 + (i - 2) * 20, 176, 6, c);
        }
      }
      break;
    }
  }
  glyphs::draw(*label, tk::W / 2, LABEL_Y, 1, 1, labelCol, 1);
  if (v.kind == MenuView::REST && !on && v.auth != MenuView::AUTH_NONE)
    text(v.auth == MenuView::AUTH_PHONE ? "TELEFONO CERCA" : "PIN OK", tk::W / 2, 252, &fonts::DejaVu12, tk::MUTE);
}

}  // namespace ui
