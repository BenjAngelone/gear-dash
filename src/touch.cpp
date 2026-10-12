#include "touch.h"

#include <Wire.h>

#include "board_pins.h"
#include "logic.h"

namespace touch {

static logic::TouchTracker tracker;
static bool flipped = false, rawDown = false;
static uint32_t lastPoll = 0, readsOk = 0, readsFail = 0, downs = 0, ghosts = 0, outliers = 0;
static uint32_t lastDownAt = 0;
static int lastX = -1, lastY = -1;
static bool wasDown = false;
static int outlierRun = 0;
static logic::TouchRaw last = {false, 0, 0};

void begin() {
  pinMode(PIN_TP_RST, OUTPUT);
  digitalWrite(PIN_TP_RST, LOW);
  delay(10);
  digitalWrite(PIN_TP_RST, HIGH);
  delay(60);
  // Registro 0xFE = DisAutoSleep: evita que el CST816 deje de responder. TODO VERIFICAR en hardware
  Wire.beginTransmission(CST816_ADDR);
  Wire.write(0xFE);
  Wire.write(0xFF);
  Wire.endTransmission();
}

void setFlipped(bool f) { flipped = f; }

// Una lectura del chip puede ser: dedo valido, "sin dedo", o basura (coordenadas fuera del panel o un salto
// imposible desde el punto anterior). La basura es ruido electrico (relés, alternador): se IGNORA, no cuenta como
// soltar el dedo. El toque solo se da por terminado si pasan TOUCH_DROPOUT_MS sin ninguna lectura valida con dedo.
void poll(uint32_t now, bool leverQuiet, uint32_t deadUntil) {
  if (now - lastPoll >= 10) {
    lastPoll = now;
    // 0x01 gesto, 0x02 nº dedos, 0x03-0x04 X (12 bit), 0x05-0x06 Y (12 bit)
    uint8_t buf[6] = {0};
    Wire.beginTransmission(CST816_ADDR);
    Wire.write(0x01);
    bool ok = Wire.endTransmission(false) == 0 && Wire.requestFrom((int)CST816_ADDR, 6) == 6;
    if (ok) for (int i = 0; i < 6; i++) buf[i] = Wire.read();
    if (ok) readsOk++;
    else readsFail++;

    bool valid = false;  // hay una muestra de dedo confiable en esta lectura
    int x = 0, y = 0;
    if (ok && buf[1] > 0) {
      x = ((buf[2] & 0x0F) << 8) | buf[3];
      y = ((buf[4] & 0x0F) << 8) | buf[5];
      if (x >= tk::W || y >= tk::H) {
        ghosts++;  // fuera del panel: basura
      } else {
        if (flipped) {
          x = tk::W - 1 - x;
          y = tk::H - 1 - y;
        }
        int dx = x - last.x, dy = y - last.y;
        bool jump = last.down && now - lastDownAt < 60 && dx * dx + dy * dy > 80 * 80;
        if (jump && outlierRun < 2) {  // un salto grande aislado: se descarta; si se repite, el dedo se movio de verdad
          outlierRun++;
          outliers++;
        } else {
          outlierRun = 0;
          valid = true;
        }
      }
    }

    if (valid) {
      last = {true, x, y};
      lastDownAt = now;
      lastX = x;
      lastY = y;
      if (!wasDown) downs++;
      wasDown = true;
    } else if (last.down && now - lastDownAt >= tk::TOUCH_DROPOUT_MS) {
      last = {false, 0, 0};  // paso demasiado tiempo sin lecturas validas: solto el dedo
      wasDown = false;
    }
    rawDown = last.down;
  }
  tracker.update(last, now, leverQuiet, deadUntil);

  // diagnostico: una linea por evento del toque (baja/sube, aceptado)
  static bool prevRaw = false, prevAct = false;
  if (rawDown != prevRaw) {
    Serial.printf("T %s x=%d y=%d t=%lu\n", rawDown ? "baja" : "sube", last.x, last.y, (unsigned long)now);
    prevRaw = rawDown;
  }
  bool a = tracker.active(now);
  if (a != prevAct) {
    Serial.printf("T %s sostenido=%lu ms\n", a ? "ACEPTADO" : "fin", (unsigned long)tracker.heldMs(now));
    prevAct = a;
  }
}

bool raw() { return rawDown; }
bool active(uint32_t now) { return tracker.active(now); }
uint32_t heldMs(uint32_t now) { return tracker.heldMs(now); }
int startX() { return tracker.startX(); }
int startY() { return tracker.startY(); }

void debugLine(char* out, size_t n) {
  snprintf(out, n, "tp ok=%lu fail=%lu toques=%lu fantasmas=%lu saltos=%lu ult=(%d,%d)", (unsigned long)readsOk,
           (unsigned long)readsFail, (unsigned long)downs, (unsigned long)ghosts, (unsigned long)outliers, lastX, lastY);
}

}  // namespace touch
