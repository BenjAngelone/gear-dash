#include "car_link.h"

// Public demo build: the vehicle link is simulated.
// The real transport (authenticated radio link to the car controller) is kept private;
// this file keeps the same job queue, session and verification logic so the UI behaves identically.

namespace car {

// ---- estado compartido con la UI -------------------------------------------------
static volatile Phase phase_ = P_IDLE;
static volatile Err err_ = E_NONE;
static volatile int step_ = 0;
static volatile logic::CarState state_ = logic::CAR_UNKNOWN;
static volatile uint32_t voltMv_ = 0;
static volatile uint32_t stateMs_ = 0;
static volatile bool haveState_ = false;
static volatile bool toggling_ = false;

enum Job : uint8_t { J_NONE, J_STATE, J_TOGGLE };
static volatile Job job_ = J_NONE;

static uint32_t session_ = 0;
static uint32_t sessionMs_ = 0;

// ---- simulacion (sin radio): para probar la UI sin el auto ------------------------
static bool simOn = false;
static bool openSession(uint32_t) {
  vTaskDelay(pdMS_TO_TICKS(500));
  session_ = 1;
  sessionMs_ = millis();
  return true;
}
static bool sendCmd(uint8_t, uint8_t) {
  vTaskDelay(pdMS_TO_TICKS(250));
  return true;
}
static bool queryState() {
  vTaskDelay(pdMS_TO_TICKS(300));
  state_ = simOn ? logic::CAR_ON : logic::CAR_OFF;
  voltMv_ = simOn ? 13800 : 12400;
  stateMs_ = millis();
  haveState_ = true;
  return true;
}

static bool sessionFresh() { return session_ != 0 && millis() - sessionMs_ < 8000; }

static void doState() {
  phase_ = P_CONNECTING;
  err_ = E_NONE;
  if (!sessionFresh() && !openSession(12000)) {
    phase_ = P_FAILED;
    err_ = E_NO_SIGNAL;
    return;
  }
  phase_ = P_QUERYING;
  if (queryState()) {
    phase_ = P_IDLE;
  } else {
    phase_ = P_FAILED;
    err_ = E_NO_STATE;
  }
}

static void doToggle() {
  err_ = E_NONE;
  step_ = 0;
  logic::CarState before = state_;
  const uint8_t* seqp = logic::sequenceFor(before);
  if (!seqp) {  // jamas se envia nada sin estado confirmado
    phase_ = P_FAILED;
    err_ = E_NO_STATE;
    return;
  }
  phase_ = P_CONNECTING;
  if (!openSession(12000)) {  // sesion nueva para tener los 10 s completos
    phase_ = P_FAILED;
    err_ = E_NO_SIGNAL;
    return;
  }
  phase_ = P_SENDING;
  for (size_t i = 0; i < logic::SEQ_LEN; i++) {
    step_ = (int)i;
    if (!sendCmd(seqp[i], 0)) {  // sin ACK: no sigue con el resto
      phase_ = P_FAILED;
      err_ = E_STEP;
      return;
    }
    if (i == logic::SEQ_LEN - 1) simOn = before == logic::CAR_OFF;
    vTaskDelay(pdMS_TO_TICKS(150));
  }
  step_ = (int)logic::SEQ_LEN;
  phase_ = P_QUERYING;
  bool ok = queryState();  // verificacion: el estado tiene que haber cambiado
  if (!ok) {
    phase_ = P_FAILED;
    err_ = E_NO_STATE;
  } else if (state_ == before) {
    phase_ = P_FAILED;
    err_ = E_MISMATCH;
  } else {
    phase_ = P_DONE;
  }
}

static void worker(void*) {
  for (;;) {
    Job j = job_;
    if (j == J_STATE) doState();
    else if (j == J_TOGGLE) {
      toggling_ = true;
      doToggle();
      toggling_ = false;
    }
    if (j != J_NONE) job_ = J_NONE;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void begin() {
  xTaskCreatePinnedToCore(worker, "car", 6144, nullptr, 1, nullptr, 0);
}

void requestState() {
  if (job_ == J_NONE) job_ = J_STATE;
}
void requestToggle() {
  if (job_ == J_NONE) job_ = J_TOGGLE;
}

void clearResult() {
  if (job_ == J_NONE) {
    phase_ = P_IDLE;
    err_ = E_NONE;
  }
}

void debugSetState(logic::CarState s) {
  state_ = s;
  stateMs_ = millis();
  haveState_ = true;
}

bool busy() { return job_ != J_NONE; }
bool toggling() { return toggling_; }
Phase phase() { return phase_; }
Err error() { return err_; }
int step() { return step_; }
logic::CarState state() { return state_; }
uint32_t voltageMv() { return voltMv_; }
uint32_t stateAgeMs() { return haveState_ ? millis() - stateMs_ : UINT32_MAX; }

}  // namespace car
