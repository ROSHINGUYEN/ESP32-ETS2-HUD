/**
 * HeadServo.cpp
 */
#include "HeadServo.h"
#include <Preferences.h>

HeadServo::HeadServo()
  : currentPan(PAN_CENTER_DEG), currentTilt(TILT_CENTER_DEG),
    targetPan(PAN_CENTER_DEG),  targetTilt(TILT_CENTER_DEG),
    velPan(0), velTilt(0),
    offsetPan(0), offsetTilt(0),
    attached(false), _holdTilt(false),
    lastMoveMs(0), lastUpdateMs(0) {}

float HeadServo::clampPan(float d)  const { return constrain(d, PAN_MIN_DEG,  PAN_MAX_DEG);  }
float HeadServo::clampTilt(float d) const { return constrain(d, TILT_MIN_DEG, TILT_MAX_DEG); }

float HeadServo::smoothstep(float t) {
  if (t < 0) t = 0; if (t > 1) t = 1;
  return t * t * (3.0f - 2.0f * t);
}

void HeadServo::loadCalibration() {
  Preferences pf;
  if (!pf.begin(NVS_NAMESPACE, true)) return;
  offsetPan  = (int8_t)pf.getChar(NVS_KEY_PAN_OFFSET,  0);
  offsetTilt = (int8_t)pf.getChar(NVS_KEY_TILT_OFFSET, 0);
  pf.end();
}

void HeadServo::saveCalibration(int8_t p, int8_t t) {
  offsetPan = p; offsetTilt = t;
  Preferences pf;
  if (!pf.begin(NVS_NAMESPACE, false)) return;
  pf.putChar(NVS_KEY_PAN_OFFSET,  p);
  pf.putChar(NVS_KEY_TILT_OFFSET, t);
  pf.end();
}

void HeadServo::attachHardware() {
  if (attached) return;
  panServo.setPeriodHertz(SERVO_PWM_FREQ_HZ);
  tiltServo.setPeriodHertz(SERVO_PWM_FREQ_HZ);
  panServo.attach(PIN_SERVO_PAN,   SERVO_PULSE_MIN_US, SERVO_PULSE_MAX_US);
  tiltServo.attach(PIN_SERVO_TILT, SERVO_PULSE_MIN_US, SERVO_PULSE_MAX_US);
  attached = true;
  DBG_INFO("attached");
}

void HeadServo::detachHardware() {
  if (!attached) return;
  panServo.detach();
  tiltServo.detach();
  attached = false;
  DBG_INFO("detached (idle)");
}

void HeadServo::writeImmediate(float pan, float tilt) {
  panServo.writeMicroseconds((uint16_t)(SERVO_PULSE_MIN_US +
    (pan / 180.0f) * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)));
  tiltServo.writeMicroseconds((uint16_t)(SERVO_PULSE_MIN_US +
    (tilt / 180.0f) * (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)));
}

void HeadServo::begin() {
  loadCalibration();
  currentPan = targetPan = PAN_CENTER_DEG + offsetPan;
  currentTilt = targetTilt = TILT_CENTER_DEG + offsetTilt;
  attachHardware();
  writeImmediate(currentPan, currentTilt);
  lastUpdateMs = millis();
  lastMoveMs   = lastUpdateMs;
  DBG_INFO("begin (offP=%d offT=%d)", offsetPan, offsetTilt);
}

void HeadServo::setTarget(float p, float t) {
  targetPan  = clampPan(p);
  targetTilt = clampTilt(t);
  lastMoveMs = millis();
}

void HeadServo::setTargetPan(float p)  { setTarget(p, targetTilt); }
void HeadServo::setTargetTilt(float t) { setTarget(targetPan, t); }

void HeadServo::nudge(float dpan, float dtilt) {
  setTarget(targetPan + dpan, targetTilt + dtilt);
}

void HeadServo::center() { setTarget(PAN_CENTER_DEG, TILT_CENTER_DEG); }

void HeadServo::stop() {
  // Freeze tại current - không auto-detach
  targetPan = currentPan;
  targetTilt = currentTilt;
  velPan = velTilt = 0;
}

void HeadServo::panic() {
  detachHardware();
}

bool HeadServo::isMoving() const {
  return (fabsf(targetPan - currentPan) > 0.5f) || (fabsf(targetTilt - currentTilt) > 0.5f);
}

void HeadServo::update(float dt) {
  unsigned long now = millis();

  // Easing: trapezoidal velocity profile
  float distP = targetPan - currentPan;
  float distT = targetTilt - currentTilt;

  // Tính max velocity cần thiết (v^2 = 2*a*s), giới hạn bởi SERVO_MAX_SPEED_DPS
  float reqV_p = sqrtf(2.0f * SERVO_ACCEL_DPS2 * fabsf(distP));
  float reqV_t = sqrtf(2.0f * SERVO_ACCEL_DPS2 * fabsf(distT));
  float vMax = SERVO_MAX_SPEED_DPS;
  float vp = min(reqV_p, vMax);
  float vt = min(reqV_t, vMax);

  // Smooth ramp velocity
  velPan  += constrain(vp  - velPan,  -SERVO_ACCEL_DPS2 * dt, SERVO_ACCEL_DPS2 * dt);
  velTilt += constrain(vt  - velTilt, -SERVO_ACCEL_DPS2 * dt, SERVO_ACCEL_DPS2 * dt);

  // Áp dụng
  float stepP = velPan  * dt;
  float stepT = velTilt * dt;

  if (fabsf(distP) <= stepP) { currentPan = targetPan; velPan = 0; }
  else currentPan += (distP > 0 ? +stepP : -stepP);

  if (fabsf(distT) <= stepT) { currentTilt = targetTilt; velTilt = 0; }
  else currentTilt += (distT > 0 ? +stepT : -stepT);

  // Clamp + write
  currentPan  = clampPan(currentPan);
  currentTilt = clampTilt(currentTilt);
  if (attached) writeImmediate(currentPan, currentTilt);

  // Auto-detach
  if (attached && !isMoving() && (now - lastMoveMs > SERVO_IDLE_DETACH_MS)) {
    if (!_holdTilt) detachHardware();
  }
  // Re-attach nếu cần chuyển động trở lại
  if (!attached && isMoving()) attachHardware();
}
