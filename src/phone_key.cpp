#include "phone_key.h"

#include "tokens.h"

// Public demo build: the BLE phone key is simulated.
// The real implementation (bonded phone recognised by its resolvable private address) is kept private.
// Here enrolling a phone succeeds after a few seconds and the phone then counts as "near".
namespace phone {

static Enroll enroll_ = EN_OFF;
static uint32_t enrollStart_ = 0;
static uint32_t lastSeen_ = 0;
static bool scanning_ = false;
static int count_ = 0;

static constexpr uint32_t SIM_ENROLL_MS = 4000;

void begin() {}

void update(uint32_t now) {
  if (enroll_ == EN_WAITING && now - enrollStart_ >= SIM_ENROLL_MS) {
    enroll_ = EN_DONE;
    count_ = 1;
    lastSeen_ = now;
  }
  if (scanning_ && count_ > 0) lastSeen_ = now;
}

void setScanning(bool on) { scanning_ = on; }
bool scanning() { return scanning_; }
void probe(uint32_t, uint32_t) {}
bool probing() { return false; }
uint32_t probeHit() { return 0; }
void scanStats(char* out, size_t n) { snprintf(out, n, "sim"); }

bool near(uint32_t now) { return count_ > 0 && now - lastSeen_ < tk::PHONE_NEAR_WINDOW_MS; }
int lastRssi() { return count_ > 0 ? -60 : 0; }
uint32_t lastSeenAgeMs(uint32_t now) { return count_ > 0 ? now - lastSeen_ : UINT32_MAX; }
int count() { return count_; }

void startEnroll(uint32_t now) {
  enroll_ = EN_WAITING;
  enrollStart_ = now;
}
void stopEnroll() { enroll_ = EN_OFF; }
Enroll enrollState() { return enroll_; }
uint32_t passkey() { return 123456; }
uint32_t enrollLeftMs(uint32_t now) {
  uint32_t el = now - enrollStart_;
  return enroll_ == EN_WAITING && el < tk::PHONE_ENROLL_MS ? tk::PHONE_ENROLL_MS - el : 0;
}

int selfTest() { return 0; }

}  // namespace phone
