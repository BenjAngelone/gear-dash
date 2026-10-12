#include "gear.h"

#include <Preferences.h>
#include <Wire.h>
#include <math.h>

#include "board_pins.h"
#include "tokens.h"

namespace gear {

// Subir este numero fuerza borrar la calibracion y recalibrar de cero en el proximo arranque
// (una sola vez por valor: queda guardado en la placa).
constexpr uint32_t CAL_RESET_TOKEN = 2;

// Calibracion: por marcha, una nube de puntos (direccion de la gravedad) que cubre todo el juego de la
// palanca dentro de esa marcha. El punto 0 es el ancla (palanca quieta).
constexpr int CAL_PTS = 24;
struct CalData {
  uint8_t n[G_COUNT];
  float p[G_COUNT][CAL_PTS][3];
};

static uint8_t imuAddr = 0;
static Preferences prefs;
static CalData cal;
static bool calOk = false;

static float acc[3] = {0, 0, 1};  // acel filtrada (g)
static float mag = 1.0f;
static float dir[3] = {0, 0, 1};  // direccion unitaria
static float snap[3] = {0, 0, 1};
static float gBias[3] = {0, 0, 0};  // bias del giroscopio (dps), se sigue con la palanca quieta
static float gyro = 0;              // |velocidad angular| filtrada, sin bias (dps)
static uint32_t lastSnap = 0, lastMotion = 0, lastImu = 0, shiftUntil = 0;
static bool jolt = false;

static int curGear = -1, prevGear = -1, candGear = -1;
static uint32_t candSince = 0, changeMs = 0;
static float conf = 0, nearD = 0;

// calibracion guiada: por marcha, quieta (ancla) -> explorar el juego -> moverla a la siguiente
enum CalPhase { CP_HOLD, CP_EXPLORE, CP_WAIT_MOVE };
static bool calibrating_ = false;
static int calStep_ = 0;
static CalPhase calPhase = CP_HOLD;
static float calRef[3], calSum[3];
static int calN = 0;
static uint32_t calSince = 0;
constexpr uint32_t CAL_HOLD_MS = 1000;
constexpr uint32_t CAL_EXPLORE_MS = 6000;
constexpr float CAL_STABLE = 0.02f;
constexpr float CAL_MOVE = 0.03f;
constexpr float CAL_SPACING = 0.012f;     // separacion minima entre puntos de una misma marcha
constexpr float CAL_EXPLORE_MAX = 0.35f;  // mas lejos del ancla = se fue de la marcha: ignorar

static float dist3(const float* a, const float* b) {
  float d0 = a[0] - b[0], d1 = a[1] - b[1], d2 = a[2] - b[2];
  return sqrtf(d0 * d0 + d1 * d1 + d2 * d2);
}

// distancia al punto mas cercano de la nube de una marcha
static float distToGear(const float* v, int g) {
  float best = 1e9f;
  for (int k = 0; k < cal.n[g]; k++) best = fminf(best, dist3(v, cal.p[g][k]));
  return best;
}

static bool imuWrite(uint8_t reg, uint8_t v) {
  Wire.beginTransmission(imuAddr);
  Wire.write(reg);
  Wire.write(v);
  return Wire.endTransmission() == 0;
}

// acelerometro (g) y giroscopio (dps) en una sola lectura de 12 bytes desde AX_L
static bool imuRead(float a[3], float g[3]) {
  Wire.beginTransmission(imuAddr);
  Wire.write(0x35);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom((int)imuAddr, 12) != 12) return false;
  uint8_t b[12];
  for (auto& x : b) x = Wire.read();
  for (int i = 0; i < 3; i++) {
    a[i] = (int16_t)(b[2 * i] | (b[2 * i + 1] << 8)) / 8192.0f;      // +-4 g
    g[i] = (int16_t)(b[6 + 2 * i] | (b[7 + 2 * i] << 8)) / 64.0f;    // +-512 dps
  }
  return true;
}

bool begin() {
  for (uint8_t a : {QMI8658_ADDR_A, QMI8658_ADDR_B}) {
    Wire.beginTransmission(a);
    Wire.write(0x00);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom((int)a, 1) == 1 && Wire.read() == 0x05) {
      imuAddr = a;
      break;
    }
  }
  prefs.begin("geartest", false);
  if (prefs.getUInt("caltok", 0) != CAL_RESET_TOKEN) {
    prefs.remove("cal");
    prefs.remove("cal2");
    prefs.putUInt("caltok", CAL_RESET_TOKEN);
  }
  calOk = prefs.getBytesLength("cal2") == sizeof(cal) && prefs.getBytes("cal2", &cal, sizeof(cal)) == sizeof(cal);
  for (int g = 0; calOk && g < G_COUNT; g++)
    if (cal.n[g] < 1 || cal.n[g] > CAL_PTS) calOk = false;
  if (!imuAddr) return false;
  imuWrite(0x02, 0x40);  // CTRL1: auto-incremento de direccion
  imuWrite(0x03, 0x14);  // CTRL2: acel +-4g, ODR ~500 Hz
  imuWrite(0x04, 0x54);  // CTRL3: giro +-512 dps, ODR ~500 Hz
  imuWrite(0x06, 0x11);  // CTRL5: pasa-bajos de acel y giro (~12 Hz): saca la vibracion del motor
  imuWrite(0x08, 0x03);  // CTRL7: habilitar acelerometro y giroscopio
  delay(60);             // arranque del giroscopio
  float gSum[3] = {0, 0, 0};
  int n = 0;
  for (int i = 0; i < 30; i++) {  // pre-llenar el filtro y estimar el bias del giro
    float a[3], g[3];
    if (imuRead(a, g)) {
      memcpy(acc, a, sizeof(acc));
      for (int k = 0; k < 3; k++) gSum[k] += g[k];
      n++;
    }
    delay(10);
  }
  if (n) {
    float gm = 0;
    for (int k = 0; k < 3; k++) gm += (gSum[k] / n) * (gSum[k] / n);
    if (sqrtf(gm) < 10.0f)  // si se estaba moviendo no sirve: lo corrige el seguimiento
      for (int k = 0; k < 3; k++) gBias[k] = gSum[k] / n;
  }
  lastMotion = millis();
  if (!calOk) startCal();
  return true;
}

static void resetHold(uint32_t now) {
  memcpy(calRef, dir, sizeof(calRef));
  calSince = now;
  calN = 0;
  memset(calSum, 0, sizeof(calSum));
}

void startCal() {
  calibrating_ = true;
  calStep_ = 0;
  calPhase = CP_HOLD;
  memset(cal.n, 0, sizeof(cal.n));
  resetHold(millis());
  curGear = prevGear = candGear = -1;
}

// Agrega un punto a la nube de la marcha g. Si esta llena, reemplaza el punto mas redundante (el mas
// cercano a otro) cuando el nuevo cubre una zona mas lejana. El ancla (punto 0) no se reemplaza.
static void addCalPoint(int g, const float* v) {
  float dNew = distToGear(v, g);
  if (dNew < CAL_SPACING) return;
  if (cal.n[g] < CAL_PTS) {
    memcpy(cal.p[g][cal.n[g]++], v, sizeof(float) * 3);
    return;
  }
  int worst = -1;
  float worstD = 1e9f;
  for (int k = 1; k < CAL_PTS; k++) {
    float nn = 1e9f;
    for (int j = 0; j < CAL_PTS; j++)
      if (j != k) nn = fminf(nn, dist3(cal.p[g][k], cal.p[g][j]));
    if (nn < worstD) {
      worstD = nn;
      worst = k;
    }
  }
  if (worst > 0 && dNew > worstD) memcpy(cal.p[g][worst], v, sizeof(float) * 3);
}

// resumen por serial: puntos por marcha y separacion minima entre nubes (< 0,04 = pueden confundirse)
static void reportCal() {
  static const char* const NAME[] = {"N", "1/R", "2", "3", "4", "5"};
  for (int g = 0; g < G_COUNT; g++) {
    float sep = 1e9f;
    int other = -1;
    for (int h = 0; h < G_COUNT; h++) {
      if (h == g) continue;
      for (int k = 0; k < cal.n[g]; k++) {
        float d = distToGear(cal.p[g][k], h);
        if (d < sep) {
          sep = d;
          other = h;
        }
      }
    }
    Serial.printf("cal %-3s pts=%2d sep_min=%.3f (con %s)%s\n", NAME[g], cal.n[g], sep, other >= 0 ? NAME[other] : "-",
                  sep < 0.04f ? "  <-- CERCA" : "");
  }
}

static void updateCal(uint32_t now) {
  int g = calStep_;
  switch (calPhase) {
    case CP_WAIT_MOVE:  // esperar a que salga de la nube de la marcha anterior
      if (distToGear(dir, g - 1) > CAL_MOVE) {
        calPhase = CP_HOLD;
        resetHold(now);
      }
      return;
    case CP_HOLD:  // quieta CAL_HOLD_MS: ancla
      if (dist3(dir, calRef) > CAL_STABLE) {
        resetHold(now);
        return;
      }
      for (int i = 0; i < 3; i++) calSum[i] += dir[i];
      calN++;
      if (now - calSince >= CAL_HOLD_MS && calN > 0) {
        for (int i = 0; i < 3; i++) cal.p[g][0][i] = calSum[i] / calN;
        cal.n[g] = 1;
        calPhase = CP_EXPLORE;
        calSince = now;
      }
      return;
    case CP_EXPLORE:  // moverla en todo su juego dentro de la marcha: juntar puntos
      if (fabsf(mag - 1.0f) < tk::ACC_EXT_MAX && dist3(dir, cal.p[g][0]) < CAL_EXPLORE_MAX) addCalPoint(g, dir);
      if (now - calSince >= CAL_EXPLORE_MS) {
        calStep_++;
        if (calStep_ >= G_COUNT) {
          calibrating_ = false;
          calOk = true;
          prefs.putBytes("cal2", &cal, sizeof(cal));
          curGear = prevGear = candGear = -1;
          calStep_ = G_COUNT - 1;
          reportCal();
        } else {
          calPhase = CP_WAIT_MOVE;
        }
      }
      return;
  }
}

// La marcha solo se decide con la ventana abierta (movimiento reciente de la palanca, o marcha
// desconocida), con la palanca asentada y sin aceleracion externa. Decidida, la ventana se cierra.
static void classify(uint32_t now) {
  if (!calOk) return;
  float d1 = 1e9f, d2 = 1e9f;
  int best = -1;
  for (int g = 0; g < G_COUNT; g++) {
    float d = distToGear(dir, g);
    if (d < d1) {
      d2 = d1;
      d1 = d;
      best = g;
    } else if (d < d2) {
      d2 = d;
    }
  }
  conf = d2 > 1e-6f ? constrain(1.0f - d1 / d2, 0.0f, 1.0f) : 0;
  nearD = d1;

  bool open = curGear < 0 || (int32_t)(shiftUntil - now) > 0;
  bool settled = gyro < tk::GYRO_STILL_DPS && fabsf(mag - 1.0f) < tk::ACC_EXT_MAX;
  bool sure = conf > tk::GEAR_MIN_CONF && d1 < tk::GEAR_MAX_DIST;
  if (!open || !settled || !sure) {
    candGear = -1;
    return;
  }
  if (best != candGear) {
    candGear = best;
    candSince = now;
    return;
  }
  if (now - candSince < tk::GEAR_CONFIRM_MS) return;
  if (best != curGear) {
    prevGear = curGear;
    curGear = best;
    changeMs = now;
  }
  shiftUntil = now;  // cerrar la ventana: queda fija hasta el proximo movimiento
  candGear = -1;
}

void update(uint32_t now) {
  if (!imuAddr || now - lastImu < 10) return;
  lastImu = now;
  float a[3], g[3];
  if (!imuRead(a, g)) return;
  for (int i = 0; i < 3; i++) acc[i] += 0.12f * (a[i] - acc[i]);
  mag = sqrtf(acc[0] * acc[0] + acc[1] * acc[1] + acc[2] * acc[2]);
  float rawMag = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
  if (fabsf(rawMag - 1.0f) > tk::JOLT_MAG) jolt = true;  // golpe seco
  if (mag > 0.05f)
    for (int i = 0; i < 3; i++) dir[i] = acc[i] / mag;

  float gc[3];
  for (int i = 0; i < 3; i++) gc[i] = g[i] - gBias[i];
  float gm = sqrtf(gc[0] * gc[0] + gc[1] * gc[1] + gc[2] * gc[2]);
  if (gm < tk::GYRO_STILL_DPS * 0.5f)
    for (int i = 0; i < 3; i++) gBias[i] += 0.002f * (g[i] - gBias[i]);  // deriva lenta del bias
  gyro += 0.3f * (gm - gyro);
  if (gyro > tk::GYRO_SHIFT_DPS) shiftUntil = now + tk::SHIFT_WINDOW_MS;

  if (now - lastSnap >= 100) {  // deteccion de movimiento de la palanca (cambio de direccion en 100 ms)
    float dm = dist3(dir, snap);
    if (dm > tk::LEVER_MOTION_DELTA) {
      lastMotion = now;
      shiftUntil = now + tk::SHIFT_WINDOW_MS;
    }
    if (dm > tk::JOLT_DELTA) jolt = true;  // cambio brusco de inclinacion
    memcpy(snap, dir, sizeof(snap));
    lastSnap = now;
  }
  if (calibrating_) updateCal(now);
  else classify(now);
}

bool imuOk() { return imuAddr != 0; }
int current() { return curGear; }
int previous() { return prevGear; }
uint32_t changedAt() { return changeMs; }
bool calibrating() { return calibrating_; }
int calStep() { return calStep_; }
bool calWaitingMove() { return calPhase == CP_WAIT_MOVE; }
bool calExploring() { return calPhase == CP_EXPLORE; }
int calPoints() { return cal.n[calStep_]; }
float calProgress(uint32_t now) {
  if (calPhase == CP_WAIT_MOVE) return 0.0f;
  float span = calPhase == CP_EXPLORE ? CAL_EXPLORE_MS : CAL_HOLD_MS;
  return constrain((now - calSince) / span, 0.0f, 1.0f);
}
// Quieta = no cambio de marcha en LEVER_QUIET_MS. No se usa la inclinacion cruda: presionar la pantalla con
// el dedo inclina un poco la placa y el filtro rechazaba el propio toque.
bool leverQuiet(uint32_t now) { return changeMs == 0 || now - changeMs >= tk::LEVER_QUIET_MS; }
uint32_t lastMotionAt() { return lastMotion; }
bool consumeJolt() {
  bool j = jolt;
  jolt = false;
  return j;
}

void debugLine(char* out, size_t n) {
  snprintf(out, n, "dir=%+.3f,%+.3f,%+.3f |a|=%.2f gyro=%.0f gear=%d conf=%.2f d=%.3f%s%s", dir[0], dir[1], dir[2], mag,
           gyro, curGear, conf, nearD, (int32_t)(shiftUntil - millis()) > 0 ? " [VENT]" : "",
           calibrating_ ? " [CAL]" : "");
}

}  // namespace gear
