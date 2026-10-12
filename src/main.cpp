// Gear Dash: marcha de la palanca (IMU en el pomo) + menu tactil con un boton ENCENDER/APAGAR
// que conmuta el auto (Renault 9) segun su estado real.
//
// Modos: MARCHAS (base) / MENU / DORMIDO.
// - Al menu se entra manteniendo el dedo 0,6 s en cualquier parte de la pantalla (con la palanca quieta).
// - Mientras se mantiene, un marco blanco se cierra desde las esquinas; al completarse explota en verde
//   (encender), rojo (apagar) o blanco (abrir el menu).
// - Al ENCENDER, tras LISTO vuelve solo a MARCHAS. Al APAGAR, tras LISTO apaga la pantalla y duerme hasta
//   una sacudida o un movimiento brusco de la palanca.
// - Sin movimiento durante 2 minutos consulta el estado del auto: si esta apagado, apaga la pantalla.
// BOOT: toque corto = girar pantalla 180 grados; mantener 1,5 s = recalibrar.
// Llave: ENCENDER solo con un telefono vinculado cerca (BLE) o con el PIN; APAGAR siempre.
// Sin telefono, el menu muestra un teclado. BT + PIN = vincular un telefono nuevo.
//
// Version publica: el enlace con el auto y la llave BLE estan simulados (car_link.cpp, phone_key.cpp).

#include <Arduino.h>
#include <esp_system.h>
#include <Preferences.h>
#include <Wire.h>

#include "board_pins.h"
#include "car_link.h"
#include "display.h"
#include "fx.h"
#include "gear.h"
#include "logic.h"
#include "phone_key.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif
#include "tokens.h"
#include "touch.h"
#include "ui.h"

enum Mode { MODE_GEAR, MODE_MENU, MODE_SLEEP };
static Mode mode = MODE_GEAR;

static logic::PressFx gearFx, menuFx;
static uint32_t menuLastActivity = 0, deadUntil = 0;
static bool needRelease = false;      // tras abrir el menu hay que soltar el dedo antes de aceptar toques
static bool suppressRefresh = false;  // tras un error no reintenta consultar hasta que haya un toque

// resultado de la ultima accion (LISTO / ERROR) que el menu muestra un rato
enum Result { RES_NONE, RES_DONE, RES_ERROR };
static Result result = RES_NONE;
static uint32_t resultSince = 0;
static bool resultNoResponse = false;
static bool turningOn = true;         // la secuencia en curso enciende (true) o apaga (false)
static uint32_t viewKindSince = 0;
static ui::MenuView::Kind lastKind = ui::MenuView::CONSULTING;

// inactividad: sin movimiento ni toques -> consulta el estado y, si esta apagado, duerme
static uint32_t lastActivity = 0;
static bool idleChecking = false;
static uint32_t sleepSince = 0;

// llave: telefono cerca o PIN
static logic::PinPad pin;
static uint32_t pinOkUntil = 0, pinWrongAt = 0, enrollEndAt = 0;
static bool pinOk = false, pinForEnroll = false, tapArmed = false;
static uint32_t phoneScanSince = 0;  // el escaneo BLE solo corre con el menu abierto
static int tapX = 0, tapY = 0;

static bool rot180 = false;
static Preferences prefs;

// Comandos por serie para reproducir el menu sin tocar la pantalla (solo por USB, no afecta el uso normal).
//   m = abrir el menu | 0-9 = tecla | X = borrar | B = tecla BT | S = estado del auto = apagado | O = encendido
static int dbgKey = -1;
static void keyCenter(int k, int& x, int& y) {
  int r, c;
  if (k == ui::KEY_BT) { r = 3; c = 0; }
  else if (k == ui::KEY_DEL) { r = 3; c = 2; }
  else if (k == 0) { r = 3; c = 1; }
  else { r = (k - 1) / 3; c = (k - 1) % 3; }
  x = tk::KEYPAD_X + c * (tk::KEY_W + tk::KEY_GAP) + tk::KEY_W / 2;
  y = tk::KEYPAD_Y + r * (tk::KEY_H + tk::KEY_GAP) + tk::KEY_H / 2;
}

static void resetKey() {
  pin.clear();
  pinOk = false;
  pinForEnroll = false;
  tapArmed = false;
  if (phone::enrollState() != phone::EN_OFF) phone::stopEnroll();
}

static void enterMenu(uint32_t now) {
  mode = MODE_MENU;
  resetKey();
  phone::setScanning(true);  // busca el telefono solo ahora que hace falta
  phoneScanSince = now;
  menuLastActivity = now;
  lastActivity = now;
  idleChecking = false;
  deadUntil = now + tk::TOUCH_DEAD_AFTER_OPEN_MS;
  needRelease = true;
  suppressRefresh = false;
  result = RES_NONE;
  menuFx.reset();
  car::clearResult();
  car::requestState();
}

static void leaveMenu(uint32_t now) {
  mode = MODE_GEAR;
  resetKey();
  phone::setScanning(false);
  lastActivity = now;
  gearFx.reset();
  menuFx.reset();
}

static void enterSleep(uint32_t now) {
  mode = MODE_SLEEP;
  resetKey();
  phone::setScanning(false);
  sleepSince = now;
  idleChecking = false;
  gearFx.reset();
  menuFx.reset();
  display::setBacklight(0);
  gear::consumeJolt();
}

static void wake(uint32_t now) {
  mode = MODE_GEAR;
  lastActivity = now;
  gearFx.reset();
  display::setBacklight(220);
  car::clearResult();
  car::requestState();  // refresca el estado para el color del fondo
}

static bool stateKnown() { return car::state() != logic::CAR_UNKNOWN && car::stateAgeMs() < 15000; }

void setup() {
  pinMode(PIN_SYS_EN, OUTPUT);
  digitalWrite(PIN_SYS_EN, HIGH);  // power latch
  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
  Serial.begin(115200);
  // motivo del ultimo reinicio: 1 encendido, 3 software, 4 panico, 5/6/7 watchdog, 9 BROWNOUT (caida de tension)
  Serial.printf("reinicio: motivo=%d\n", (int)esp_reset_reason());

  prefs.begin("geardash", false);
  rot180 = prefs.getBool("rot", false);

  display::begin(rot180);
  ui::drawSplash();
  fx::begin();

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_FREQ_HZ);
  bool imu = gear::begin();
  touch::begin();
  touch::setFlipped(rot180);
  car::begin();
  phone::begin();

  int fails = logic::selfTest() + phone::selfTest();
  Serial.printf("IMU %s | selftest logica: %s (%d fallas)\n", imu ? "ok" : "FALLA", fails ? "FALLA" : "ok", fails);
  delay(900);  // splash
  ui::bench();
  lastActivity = millis();
}

void loop() {
  static uint32_t lastDraw = 0, lastLog = 0, btnDown = 0, lastSig = 0xFFFFFFFF;
  static uint32_t frames = 0, composeUs = 0, presentUs = 0, maxUs = 0, loops = 0;
  static bool btnWas = false, longDone = false;
  uint32_t now = millis();
  loops++;

  while (Serial.available()) {
    int c = Serial.read();
    if (c == 'P') phone::probe(now, 12000);
    else if (c == 'S') car::debugSetState(logic::CAR_OFF);
    else if (c == 'O') car::debugSetState(logic::CAR_ON);
    else if (c == 'm' && mode == MODE_GEAR) enterMenu(now);
    else if (c == 'B') dbgKey = ui::KEY_BT;
    else if (c == 'X') dbgKey = ui::KEY_DEL;
    else if (c >= '0' && c <= '9') dbgKey = c - '0';
  }
  gear::update(now);
  phone::update(now);
  bool quiet = gear::leverQuiet(now);
  touch::poll(now, true, deadUntil);  // el movimiento de la palanca ya no descarta el toque (solo alarga el mantener)

  // ---- dormido: sin dibujar, esperando una sacudida / movimiento brusco / toque
  if (mode == MODE_SLEEP) {
    bool jolt = gear::consumeJolt();
    if (now - sleepSince >= tk::SLEEP_WAKE_GUARD_MS && (jolt || touch::raw() || digitalRead(PIN_BTN_BOOT) == LOW))
      wake(now);
    else
      delay(15);
    return;
  }

  // ---- BOOT: corto = girar 180; 1.5 s = recalibrar
  bool down = digitalRead(PIN_BTN_BOOT) == LOW;
  if (down && !btnWas) {
    btnDown = now;
    longDone = false;
  }
  if (down && !longDone && now - btnDown >= 1500) {
    longDone = true;
    gear::startCal();
    leaveMenu(now);
  }
  if (!down && btnWas && !longDone && now - btnDown > 40) {
    rot180 = !rot180;
    prefs.putBool("rot", rot180);
    display::setRotation180(rot180);
    touch::setFlipped(rot180);
  }
  btnWas = down;

  // ---- actividad: movimiento de la palanca o dedo en la pantalla
  if (touch::raw()) lastActivity = now;
  if ((int32_t)(gear::lastMotionAt() - lastActivity) > 0) lastActivity = gear::lastMotionAt();

  ui::PressView pv;
  ui::MenuView view;

  if (gear::calibrating()) {
    // el asistente de calibracion no usa el menu
  } else if (mode == MODE_GEAR) {
    bool act = touch::active(now) && ui::inOpenZone(touch::startX(), touch::startY());
    // con la palanca en movimiento hace falta mantener mas tiempo, pero el toque se acepta igual
    auto o = gearFx.update(now, act, touch::heldMs(now), quiet ? tk::MENU_OPEN_HOLD_MS : tk::MENU_OPEN_HOLD_MS + 300);
    pv.I = o.I;
    pv.R = o.R;
    pv.p = o.p;
    pv.flash = o.flash;
    pv.flashColor = tk::FG;
    if (o.completed) enterMenu(now);  // recien despues de la explosion

    // 2 min sin movimiento: consultar el estado; si esta apagado, apagar la pantalla
    if (!idleChecking && now - lastActivity >= tk::IDLE_CHECK_MS && !car::busy()) {
      car::clearResult();
      car::requestState();
      idleChecking = true;
    } else if (idleChecking && !car::busy()) {
      idleChecking = false;
      bool fresh = car::phase() == car::P_IDLE && car::stateAgeMs() < 5000;
      if (fresh && car::state() == logic::CAR_OFF) {
        enterSleep(now);
        return;
      }
      car::clearResult();
      lastActivity = now;  // esta encendido o no respondio: vuelve a mirar en 2 minutos
    }
  } else {  // MODE_MENU
    if (touch::raw()) {
      menuLastActivity = now;
      suppressRefresh = false;
    }
    if (needRelease && !touch::raw()) needRelease = false;

    car::Phase ph = car::phase();
    bool busy = car::busy();

    // toque corto (para el teclado y para cancelar la vinculacion): se acepta al soltar
    bool tap = false;
    if (!needRelease && touch::active(now)) {
      tapArmed = true;
      tapX = touch::startX();
      tapY = touch::startY();
    } else if (tapArmed && !touch::active(now)) {
      tapArmed = false;
      tap = !touch::raw();  // soltado (no rechazado por desplazamiento o palanca)
    }

    if (dbgKey >= 0) {
      keyCenter(dbgKey, tapX, tapY);
      menuLastActivity = now;  // la tecla simulada cuenta como actividad
      tap = true;
      dbgKey = -1;
    }
    // llave
    if (pinOk && (int32_t)(now - pinOkUntil) >= 0) pinOk = false;
    bool phoneNear = phone::near(now);
    if (stateKnown() && car::state() == logic::CAR_ON) phone::setScanning(false);  // apagar no necesita telefono
    bool authorized = phoneNear || pinOk;
    phone::Enroll en = phone::enrollState();
    if (en == phone::EN_WAITING) {
      menuLastActivity = now;
      if (tap) {
        phone::stopEnroll();
        tap = false;
      }
    } else if (en != phone::EN_OFF) {  // VINCULADO / FALLO: se muestra un rato
      menuLastActivity = now;
      if (enrollEndAt == 0) enrollEndAt = now;
      if (now - enrollEndAt >= tk::ENROLL_RESULT_MS || tap) {
        phone::stopEnroll();
        tap = false;
      }
    }
    if (phone::enrollState() == phone::EN_OFF) enrollEndAt = 0;
    bool enrolling = phone::enrollState() != phone::EN_OFF;
    // con telefonos vinculados se espera un momento a que el escaneo lo encuentre antes de mostrar el teclado
    bool waitingPhone = phone::count() > 0 && !authorized && !pinForEnroll && now - phoneScanSince < tk::PHONE_SCAN_GRACE_MS;
    bool needPin = !enrolling && !busy && result == RES_NONE && stateKnown() && car::state() == logic::CAR_OFF &&
                   (!authorized || pinForEnroll) && !waitingPhone;
    if (needPin && tap) {
      int k = ui::keyAt(tapX, tapY);
      if (k == ui::KEY_BT) {
        pinForEnroll = !pinForEnroll;
        pin.clear();
      } else if (k >= 0) {
        auto r = pin.press(k == ui::KEY_DEL ? logic::PinPad::KEY_DEL : k, now, PIN_ARRANQUE);
        if (r == logic::PinPad::OK) {
          if (pinForEnroll) {
            pinForEnroll = false;
            phone::startEnroll(now);
          } else {
            pinOk = true;
            pinOkUntil = now + tk::PIN_OK_MS;
          }
        } else if (r == logic::PinPad::WRONG) {
          pinWrongAt = now;
        }
      }
    }

    // resultado de una accion: LISTO 0,8 s / ERROR 3 s (o hasta un toque)
    if (result == RES_NONE && !busy && (ph == car::P_DONE || ph == car::P_FAILED)) {
      result = ph == car::P_DONE ? RES_DONE : RES_ERROR;
      resultSince = now;
      resultNoResponse = car::error() == car::E_NO_SIGNAL || car::error() == car::E_NO_STATE;
    }
    if (result == RES_DONE && now - resultSince >= tk::DONE_SHOW_MS) {
      result = RES_NONE;
      car::clearResult();
      if (turningOn) leaveMenu(now);  // encendido: vuelve solo a marchas
      else {                          // apagado: apaga la pantalla hasta que se mueva la palanca
        enterSleep(now);
        return;
      }
    } else if (result == RES_ERROR &&
               (now - resultSince >= tk::ERROR_SHOW_MS || (!needRelease && touch::raw() && now - resultSince > 300))) {
      result = RES_NONE;
      car::clearResult();
      suppressRefresh = true;
      if (!stateKnown()) leaveMenu(now);  // sin estado no hay nada que mostrar: vuelve a las marchas
    }
    if (result != RES_NONE) menuLastActivity = now;

    // boton: mantener 1 s. La accion se dispara al terminar la explosion.
    if (mode == MODE_MENU) {
      bool canAct = !needRelease && stateKnown() && !busy && result == RES_NONE && !needPin && !enrolling &&
                    (car::state() == logic::CAR_ON || authorized);
      bool act = canAct && touch::active(now) && ui::inButton(touch::startX(), touch::startY());
      auto o = menuFx.update(now, act, touch::heldMs(now), tk::ACTION_HOLD_MS);
      pv.I = o.I;
      pv.R = o.R;
      pv.p = o.p;
      pv.flash = o.flash;
      pv.flashColor = car::state() == logic::CAR_OFF ? tk::GOOD : tk::DANGER;  // verde enciende, rojo apaga
      if (o.completed) {
        turningOn = car::state() == logic::CAR_OFF;
        car::requestToggle();
      }

      // refrescar el estado mientras el menu esta abierto y quieto
      if (!busy && ph == car::P_IDLE && result == RES_NONE && !suppressRefresh &&
          car::stateAgeMs() > tk::STATE_REFRESH_MS)
        car::requestState();

      if (!busy && result == RES_NONE && logic::menuExpired(now, menuLastActivity, tk::MENU_IDLE_MS)) leaveMenu(now);
    }

    // lo que muestra el menu
    if (enrolling) view.kind = ui::MenuView::ENROLL;
    else if (needPin) view.kind = ui::MenuView::PIN;
    else if (result == RES_DONE) view.kind = ui::MenuView::DONE;
    else if (result == RES_ERROR) view.kind = ui::MenuView::ERROR;
    else if (busy) view.kind = car::toggling() ? ui::MenuView::SENDING : ui::MenuView::CONSULTING;
    else if (stateKnown()) view.kind = ui::MenuView::REST;
    else view.kind = ui::MenuView::CONSULTING;
    view.state = car::state();
    view.turningOn = turningOn;
    view.step = car::step();
    view.noResponse = resultNoResponse;
    view.auth = phoneNear ? ui::MenuView::AUTH_PHONE : (pinOk ? ui::MenuView::AUTH_PIN : ui::MenuView::AUTH_NONE);
    view.pinLen = pin.len();
    view.pinForEnroll = pinForEnroll;
    view.pinWrongAt = pinWrongAt;
    view.lockLeftMs = pin.lockLeftMs(now);
    view.pressedKey = touch::active(now) && !needRelease ? ui::keyAt(touch::startX(), touch::startY()) : -1;
    view.enroll = phone::enrollState();
    view.passkey = phone::passkey();
    view.enrollLeftMs = phone::enrollLeftMs(now);
    view.phones = phone::count();
    if (view.kind != lastKind) {
      lastKind = view.kind;
      viewKindSince = now;
    }
    view.kindSince = viewKindSince;
  }

  // Solo se redibuja si algo cambia (animacion, pulsacion, fundido, estado nuevo): dibujar y volcar un cuadro
  // completo es caro (PSRAM + SPI) y el 90 % del tiempo la pantalla esta quieta.
  bool engineOn = car::state() == logic::CAR_ON && car::stateAgeMs() < 60000;
  uint32_t sig = (uint32_t)mode | (uint32_t)(gear::current() + 1) << 4 | (uint32_t)engineOn << 8 |
                 (uint32_t)gear::imuOk() << 9 | (uint32_t)view.kind << 10 | (uint32_t)view.state << 14 |
                 (uint32_t)gear::calibrating() << 16;
  bool animGear = gear::current() >= 0 && gear::changedAt() != 0 && now - gear::changedAt() < tk::GEAR_ANIM_MS + 50;
  bool animating = gear::calibrating() || pv.p > 0.001f || pv.I > 0.001f || pv.flash > 0.001f ||
                   fx::fading(now) || (mode == MODE_GEAR && animGear) ||
                   (mode == MODE_MENU && view.kind != ui::MenuView::REST);
  if ((animating || sig != lastSig) && now - lastDraw >= 33) {  // <= 30 fps
    lastDraw = now;
    lastSig = sig;
    uint32_t t0 = micros();
    if (gear::calibrating()) ui::drawCalibration(now);
    else if (mode == MODE_MENU) ui::drawMenu(now, view, pv);
    else ui::drawGear(now, pv);
    uint32_t t1 = micros();
    display::present();
    uint32_t t2 = micros();
    frames++;
    composeUs += t1 - t0;
    presentUs += t2 - t1;
    if (t2 - t0 > maxUs) maxUs = t2 - t0;
  }

  if (now - lastLog >= 500) {
    lastLog = now;
    char buf[144], tbuf[96];
    gear::debugLine(buf, sizeof(buf));
    touch::debugLine(tbuf, sizeof(tbuf));
    Serial.printf("%s mode=%d car=%d phase=%d tel=%d dBm hace %lu ms%s quiet=%d\n", buf, (int)mode, (int)car::state(),
                  (int)car::phase(), phone::lastRssi(), (unsigned long)phone::lastSeenAgeMs(now),
                  phone::near(now) ? " CERCA" : "", (int)quiet);
    Serial.printf("  %s | cuadros=%lu compose=%lums volcado=%lums max=%lums loops/s=%lu\n", tbuf,
                  (unsigned long)frames, (unsigned long)(frames ? composeUs / frames / 1000 : 0),
                  (unsigned long)(frames ? presentUs / frames / 1000 : 0), (unsigned long)(maxUs / 1000),
                  (unsigned long)(loops * 2));
    char sbuf[220];
    phone::scanStats(sbuf, sizeof(sbuf));
    Serial.printf("  %s\n", sbuf);
    frames = composeUs = presentUs = maxUs = loops = 0;
  }
}
