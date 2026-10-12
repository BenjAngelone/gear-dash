#pragma once
// Logica pura (sin hardware): filtro de toques, boton de mantener, politica de timeouts y
// secuencias de encendido/apagado. Incluye un autotest que corre en la placa (no hay compilador
// de PC en este equipo): logic::selfTest() devuelve la cantidad de fallas.
#include <stddef.h>
#include <stdint.h>

#include "tokens.h"

namespace logic {

// ---- toques -------------------------------------------------------------------
struct TouchRaw {
  bool down;
  int x, y;
};

// Sigue un toque desde que baja hasta que sube. Rechaza (hasta soltar) los toques que empiezan en
// el borde, durante la zona muerta, con la palanca en movimiento, o que se desplazan demasiado.
class TouchTracker {
 public:
  void update(const TouchRaw& t, uint32_t now, bool leverQuiet, uint32_t deadUntil) {
    if (!t.down) {
      st_ = Idle;
      return;
    }
    if (st_ == Idle) {  // flanco de bajada
      bool edge = t.x < tk::TOUCH_EDGE_PX || t.y < tk::TOUCH_EDGE_PX || t.x >= tk::W - tk::TOUCH_EDGE_PX ||
                  t.y >= tk::H - tk::TOUCH_EDGE_PX;
      if (edge || !leverQuiet || (int32_t)(now - deadUntil) < 0) {
        st_ = Rejected;
        reject_ = edge ? 'E' : (!leverQuiet ? 'L' : 'D');
        return;
      }
      st_ = Tracking;
      sx_ = t.x;
      sy_ = t.y;
      t0_ = now;
    }
    if (st_ == Tracking) {
      int dx = t.x - sx_, dy = t.y - sy_;
      if (dx * dx + dy * dy > tk::TOUCH_MAX_DRIFT_PX * tk::TOUCH_MAX_DRIFT_PX || !leverQuiet) {
        st_ = Rejected;
        reject_ = leverQuiet ? 'M' : 'L';
      }
    }
  }
  // true si hay un toque aceptado y ya paso el tiempo minimo
  bool active(uint32_t now) const { return st_ == Tracking && now - t0_ >= tk::TOUCH_MIN_PRESS_MS; }
  uint32_t heldMs(uint32_t now) const { return active(now) ? now - t0_ : 0; }
  // ultimo motivo de rechazo: 'E' borde, 'L' palanca, 'D' zona muerta, 'M' desplazamiento (0 = ninguno)
  char lastReject() const { return reject_; }
  int startX() const { return sx_; }
  int startY() const { return sy_; }

 private:
  enum { Idle, Tracking, Rejected } st_ = Idle;
  int sx_ = 0, sy_ = 0;
  uint32_t t0_ = 0;
  char reject_ = 0;
};

// ---- boton de mantener ----------------------------------------------------------
// Progreso 0..1 segun el tiempo sostenido; devuelve true una sola vez al completarse.
class HoldButton {
 public:
  bool update(bool holding, uint32_t heldMs, uint32_t holdMs) {
    if (!holding) {
      progress_ = 0;
      fired_ = false;
      return false;
    }
    progress_ = heldMs >= holdMs ? 1.0f : (float)heldMs / (float)holdMs;
    if (progress_ >= 1.0f && !fired_) {
      fired_ = true;
      return true;
    }
    return false;
  }
  float progress() const { return progress_; }

 private:
  float progress_ = 0;
  bool fired_ = false;
};

// ---- sombra de pulsacion (gear_dash_menu/design/gear_menu.md §4.3) ------------------
// Calcula intensidad I y expansion R del halo segun el tiempo sostenido.
//   Rebote 0-60 ms: nada. Progreso: I = 0,40 + 0,60 p (lineal), R = 12 (1 - (1-p)^2).
//   Destello al completar: 60 ms (I 1,0 -> 1,8; R 12 -> 16) y 180 ms de salida I = 1,8 (1-u)^2.
//   Cancelado: I -> 0 en 150 ms con (1-u)^2, R congelado. `completed` sale una sola vez, al final
//   del destello (recien ahi se dispara la accion).
class PressFx {
 public:
  struct Out {
    float I = 0, R = 0;
    float p = 0;      // progreso del marco (0..1): se cierra el cuadrado; 1 durante el destello
    float flash = 0;  // 0 = sin destello; 0..1 = avance de la explosion (240 ms)
    bool completed = false;
  };
  // active: toque aceptado por el filtro; heldMs: tiempo sostenido; T: duracion total del mantener
  Out update(uint32_t now, bool active, uint32_t heldMs, uint32_t T) {
    Out o;
    if (st_ == Idle && active) st_ = Hold;
    if (st_ == Hold) {
      if (!active) {
        st_ = Cancel;
        t0_ = now;
        i0_ = lastI_;
      } else if (heldMs >= T) {
        st_ = FlashUp;
        t0_ = now;
      } else {
        float p = (float)heldMs / (float)T;
        lastP_ = p;
        lastI_ = 0.40f + 0.60f * p;
        lastR_ = 12.0f * (1.0f - (1.0f - p) * (1.0f - p));
        o.I = lastI_;
        o.R = lastR_;
        o.p = p;
        return o;
      }
    }
    if (st_ == FlashUp) {
      float u = (float)(now - t0_) / 60.0f;
      if (u < 1.0f) {
        o.I = 1.0f + 0.8f * u;
        o.R = 12.0f + 4.0f * u;
        o.p = 1.0f;
        o.flash = 0.25f * u;
        return o;
      }
      st_ = FlashDown;
      t0_ = now;
    }
    if (st_ == FlashDown) {
      float u = (float)(now - t0_) / 180.0f;
      if (u >= 1.0f) {
        st_ = WaitRelease;
        o.completed = true;
        o.p = 1.0f;
        o.flash = 1.0f;
        return o;
      }
      float k = 1.0f - u;
      o.I = 1.8f * k * k;
      o.R = 16.0f;
      o.p = 1.0f;
      o.flash = 0.25f + 0.75f * u;
      return o;
    }
    if (st_ == Cancel) {
      float u = (float)(now - t0_) / 150.0f;
      if (u >= 1.0f) {
        st_ = Idle;
        return o;
      }
      float k = 1.0f - u;
      o.I = i0_ * k * k;
      o.R = lastR_;
      o.p = lastP_ * k;  // el marco se retrae
      return o;
    }
    if (st_ == WaitRelease && !active) st_ = Idle;
    return o;
  }
  void reset() { st_ = Idle; }

 private:
  enum { Idle, Hold, FlashUp, FlashDown, Cancel, WaitRelease } st_ = Idle;
  uint32_t t0_ = 0;
  float lastI_ = 0, lastR_ = 0, lastP_ = 0, i0_ = 0;
};

// ---- PIN ------------------------------------------------------------------------------
// Teclado del PIN de respaldo. Al completar PIN_LEN digitos compara; tras PIN_MAX_FAILS errores
// seguidos se bloquea PIN_LOCK_MS.
class PinPad {
 public:
  enum Result { NONE, OK, WRONG };
  static constexpr int KEY_DEL = 10;
  Result press(int key, uint32_t now, const char* pin) {
    if (locked(now)) return NONE;
    if (key == KEY_DEL) {
      if (len_ > 0) len_--;
      return NONE;
    }
    if (key < 0 || key > 9 || len_ >= tk::PIN_LEN) return NONE;
    buf_[len_++] = (char)('0' + key);
    if (len_ < tk::PIN_LEN) return NONE;
    bool ok = true;
    for (int i = 0; i < tk::PIN_LEN; i++) ok &= buf_[i] == pin[i];
    len_ = 0;
    if (ok) {
      fails_ = 0;
      return OK;
    }
    if (++fails_ >= tk::PIN_MAX_FAILS) {
      fails_ = 0;
      lockUntil_ = now + tk::PIN_LOCK_MS;
      lockSet_ = true;
    }
    return WRONG;
  }
  void clear() { len_ = 0; }
  int len() const { return len_; }
  bool locked(uint32_t now) const { return lockSet_ && (int32_t)(lockUntil_ - now) > 0; }
  uint32_t lockLeftMs(uint32_t now) const { return locked(now) ? lockUntil_ - now : 0; }

 private:
  char buf_[tk::PIN_LEN] = {};
  int len_ = 0, fails_ = 0;
  uint32_t lockUntil_ = 0;
  bool lockSet_ = false;
};

// ---- timeouts ---------------------------------------------------------------------
inline bool menuExpired(uint32_t now, uint32_t lastActivity, uint32_t idleMs) { return now - lastActivity >= idleMs; }

// ---- secuencias -------------------------------------------------------------------
// Pasos de las secuencias (identificadores locales; el protocolo real con el auto es privado).
enum Cmd : uint8_t {
  CMD_LUCES_ON = 1, CMD_LUCES_OFF, CMD_CONTACTO_ON, CMD_CONTACTO_OFF, CMD_ARRANQUE_ON, CMD_ARRANQUE_OFF,
  CMD_GAS_ON, CMD_GAS_OFF, CMD_LUCES_HAB_ON, CMD_LUCES_HAB_OFF, CMD_BLUETOOTH_ON, CMD_BLUETOOTH_OFF,
};

enum CarState : uint8_t { CAR_UNKNOWN, CAR_OFF, CAR_ON };

constexpr uint8_t SEQ_ENCENDER[] = {CMD_CONTACTO_ON, CMD_GAS_ON, CMD_LUCES_HAB_ON, CMD_LUCES_ON, CMD_BLUETOOTH_ON};
constexpr uint8_t SEQ_APAGAR[] = {CMD_BLUETOOTH_OFF, CMD_LUCES_OFF, CMD_LUCES_HAB_OFF, CMD_GAS_OFF, CMD_CONTACTO_OFF};
constexpr size_t SEQ_LEN = 5;

// Secuencia que corresponde ejecutar segun el estado confirmado; nullptr si el estado es desconocido.
inline const uint8_t* sequenceFor(CarState s) {
  if (s == CAR_OFF) return SEQ_ENCENDER;
  if (s == CAR_ON) return SEQ_APAGAR;
  return nullptr;
}

// ---- autotest ---------------------------------------------------------------------
inline int selfTest() {
  int fails = 0;
  auto check = [&](bool ok) { if (!ok) fails++; };

  // toque normal: aceptado tras 60 ms
  TouchTracker t;
  t.update({true, 120, 140}, 1000, true, 0);
  check(!t.active(1030));
  t.update({true, 121, 141}, 1070, true, 0);
  check(t.active(1070) && t.heldMs(1070) == 70);
  t.update({false, 0, 0}, 1100, true, 0);
  check(!t.active(1100));

  // borde: rechazado hasta soltar
  TouchTracker e;
  e.update({true, 5, 140}, 2000, true, 0);
  e.update({true, 120, 140}, 2100, true, 0);
  check(!e.active(2500));

  // palanca en movimiento: rechazado
  TouchTracker m;
  m.update({true, 120, 140}, 3000, false, 0);
  check(!m.active(3500));

  // zona muerta
  TouchTracker d;
  d.update({true, 120, 140}, 4000, true, 4300);
  check(!d.active(4500));
  d.update({false, 0, 0}, 4600, true, 4300);
  d.update({true, 120, 140}, 4700, true, 4300);
  check(d.active(4800));

  // desplazamiento: cancela
  TouchTracker g;
  g.update({true, 100, 140}, 5000, true, 0);
  g.update({true, 150, 140}, 5100, true, 0);
  check(!g.active(5200));

  // boton de mantener: dispara una vez en 1500 ms, se resetea al soltar
  HoldButton b;
  check(!b.update(true, 700, 1500) && b.progress() > 0.4f && b.progress() < 0.5f);
  check(b.update(true, 1500, 1500));
  check(!b.update(true, 1700, 1500));
  check(!b.update(false, 0, 1500) && b.progress() == 0);
  check(b.update(true, 1600, 1500));

  // PressFx: sin efecto antes de activarse; I = 0,40..1,00 lineal; destello; completed una sola vez
  {
    PressFx f;
    auto o = f.update(0, false, 0, 1000);
    check(o.I == 0);
    o = f.update(60, true, 60, 1000);
    check(o.I > 0.42f && o.I < 0.44f);
    o = f.update(500, true, 500, 1000);
    check(o.I > 0.69f && o.I < 0.71f && o.R > 8.9f && o.R < 9.1f);  // p=0,5: R = 12*(1-0,25) = 9
    o = f.update(1000, true, 1000, 1000);                          // empieza el destello
    check(o.I >= 1.0f && !o.completed);
    o = f.update(1030, true, 1030, 1000);
    check(o.I > 1.3f && o.I < 1.5f && !o.completed);
    o = f.update(1060, true, 1060, 1000);                          // salida del destello
    check(o.I > 1.7f && !o.completed);
    int done = 0;
    for (uint32_t t = 1100; t < 1500; t += 20) {
      o = f.update(t, true, t, 1000);
      if (o.completed) done++;
    }
    check(done == 1);                                              // una sola vez aunque siga el dedo
    o = f.update(1600, false, 0, 1000);                            // suelta: vuelve a armarse
    o = f.update(1700, true, 60, 1000);
    check(o.I > 0.4f);
  }
  {
    PressFx f;  // cancelado: I baja a 0 en 150 ms y no completa
    f.update(60, true, 60, 1500);
    f.update(700, true, 700, 1500);
    auto o = f.update(750, false, 0, 1500);
    check(o.I > 0.2f && !o.completed);
    o = f.update(920, false, 0, 1500);
    check(o.I == 0 && !o.completed);
  }

  // PIN: correcto, borrar, 3 errores bloquean 30 s
  {
    PinPad p;
    check(p.press(1, 0, "1327") == PinPad::NONE && p.len() == 1);
    p.press(PinPad::KEY_DEL, 0, "1327");
    check(p.len() == 0);
    p.press(1, 0, "1327");
    p.press(3, 0, "1327");
    p.press(2, 0, "1327");
    check(p.press(7, 0, "1327") == PinPad::OK && p.len() == 0);
    for (int k = 0; k < 3; k++) p.press(0, 100, "1327");
    check(p.press(0, 100, "1327") == PinPad::WRONG);  // 1er error
    for (int r = 0; r < 2; r++)
      for (int k = 0; k < 4; k++) p.press(9, 200, "1327");
    check(p.locked(300) && p.press(1, 300, "1327") == PinPad::NONE);
    check(!p.locked(200 + tk::PIN_LOCK_MS));
    uint32_t t = 200 + tk::PIN_LOCK_MS;
    p.press(1, t, "1327");
    p.press(3, t, "1327");
    p.press(2, t, "1327");
    check(p.press(7, t, "1327") == PinPad::OK);
  }

  // timeouts
  check(!menuExpired(5999, 0, 6000) && menuExpired(6000, 0, 6000));

  // secuencias: ENCENDER arranca por contacto, APAGAR termina en contacto off; desconocido = nada
  check(sequenceFor(CAR_OFF) == SEQ_ENCENDER && SEQ_ENCENDER[0] == CMD_CONTACTO_ON);
  check(sequenceFor(CAR_ON) == SEQ_APAGAR && SEQ_APAGAR[SEQ_LEN - 1] == CMD_CONTACTO_OFF);
  check(sequenceFor(CAR_UNKNOWN) == nullptr);
  // jamas ArranqueOn en ninguna secuencia
  for (size_t i = 0; i < SEQ_LEN; i++) check(SEQ_ENCENDER[i] != CMD_ARRANQUE_ON && SEQ_APAGAR[i] != CMD_ARRANQUE_ON);
  return fails;
}

}  // namespace logic
