/**
 * Autonomy.cpp
 */
#include "Autonomy.h"
#include "HeadServo.h"
#include "Gestures.h"
#include "config.h"

Autonomy::Autonomy() : _head(nullptr), _ges(nullptr),
                       mode(AM_CALM),
                       manualUntil(0), lastActionMs(0), nextDelayMs(3000) {}

void Autonomy::setMode(AutonomyMode m) {
  if (m >= AM_COUNT) return;
  mode = m;
  lastActionMs = millis();
  DBG_INFO("mode=%u", (unsigned)mode);
}

void Autonomy::notifyManual() {
  manualUntil = millis() + MANUAL_PRIORITY_TIMEOUT_MS;
}

bool Autonomy::isManualActive() const {
  return (int32_t)(millis() - manualUntil) < 0;
}

void Autonomy::scheduleNext() {
  uint16_t base = 3000;
  switch (mode) {
    case AM_SLEEP:   base = 8000; break;
    case AM_CALM:    base = 4000; break;
    case AM_CURIOUS: base = 2500; break;
    case AM_PLAYFUL: base = 1500; break;
    default: break;
  }
  nextDelayMs = base + random(0, 3000);
  lastActionMs = millis();
}

void Autonomy::doRandomAction() {
  if (!_head) return;
  switch (mode) {
    case AM_SLEEP:
      if (_ges) _ges->play(GES_SLEEP);
      break;
    case AM_CALM:
      _head->setTarget(PAN_CENTER_DEG + (random(-15, 16)), TILT_CENTER_DEG);
      break;
    case AM_CURIOUS:
      if (random(0, 2) && _ges) _ges->play(GES_CURIOUS);
      else _head->setTarget(random(60, 121), random(82, 99));
      break;
    case AM_PLAYFUL:
      if (random(0, 3) == 0 && _ges) _ges->play(GES_HAPPY);
      else _head->setTarget(random(50, 131), random(80, 100));
      break;
    default: break;
  }
}

void Autonomy::tick() {
  if (isManualActive()) return;
  if (_ges && _ges->isPlaying()) return;
  if (_head && _head->isMoving()) return;

  if (millis() - lastActionMs < nextDelayMs) return;
  doRandomAction();
  scheduleNext();
}
