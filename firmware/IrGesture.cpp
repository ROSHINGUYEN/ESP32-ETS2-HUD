/**
 * IrGesture.cpp
 */
#include "IrGesture.h"
#include "config.h"

IrGesture::IrGesture()
  : pin1(0), pin2(0xFF), activeLow(true), isActiveFlag(false), eventCounter(0),
    lastEdgeMs1(0), lastEdgeMs2(0), prevLevel1(HIGH), prevLevel2(HIGH),
    tapsInWindow(0), firstTapMs(0), holding(false), holdStartMs(0),
    lastChangeMs(0), recentEdges(0), pending(IR_EVT_NONE) {}

void IrGesture::begin(uint8_t p1, uint8_t p2, bool aLow) {
  pin1 = p1; pin2 = p2; activeLow = aLow;
  pinMode(pin1, INPUT_PULLUP);
  if (pin2 != 0xFF) pinMode(pin2, INPUT_PULLUP);
  prevLevel1 = digitalRead(pin1);
  prevLevel2 = (pin2 != 0xFF) ? digitalRead(pin2) : HIGH;
  lastEdgeMs1 = lastEdgeMs2 = millis();
  Serial0.printf("[IR] init P1=GPIO%d P2=GPIO%d Active=%s\n",
                 pin1, pin2, activeLow ? "LOW" : "HIGH");
}

IrEvent IrGesture::consumeEvent() {
  IrEvent e = pending;
  pending = IR_EVT_NONE;
  return e;
}

void IrGesture::processLevel(uint8_t level, unsigned long now, bool isP1) {
  bool isActiveNow = activeLow ? (level == LOW) : (level == HIGH);

  // Edge detection
  bool& prev = isP1 ? prevLevel1 : prevLevel2;
  if (level == prev) return;
  prev = level;

  // Debounce
  unsigned long& lastEdge = isP1 ? lastEdgeMs1 : lastEdgeMs2;
  if (now - lastEdge < IR_DEBOUNCE_MS) return;
  lastEdge = now;
  lastChangeMs = now;
  recentEdges++;

  if (isActiveNow) {
    holding = true;
    holdStartMs = now;
  } else {
    // Kết thúc active → có thể là tap
    holding = false;
    unsigned long dur = now - holdStartMs;
    if (dur < IR_TAP_MAX_MS) {
      if (tapsInWindow == 0) firstTapMs = now;
      if (now - firstTapMs < IR_DOUBLE_GAP_MS) {
        tapsInWindow++;
      } else {
        tapsInWindow = 1;
        firstTapMs = now;
      }
    } else {
      // Long event không tính là tap
      tapsInWindow = 0;
    }
  }
  eventCounter++;
}

void IrGesture::tick() {
  if (pin1 == 0) return;
  unsigned long now = millis();

  uint8_t r1 = digitalRead(pin1);
  uint8_t r2 = (pin2 != 0xFF) ? digitalRead(pin2) : (activeLow ? HIGH : LOW);
  bool a1 = activeLow ? (r1 == LOW) : (r1 == HIGH);
  bool a2 = (pin2 != 0xFF) && (activeLow ? (r2 == LOW) : (r2 == HIGH));
  isActiveFlag = a1 || a2;

  processLevel(r1, now, true);
  if (pin2 != 0xFF) processLevel(r2, now, false);

  // Reset wave window
  if (now - lastChangeMs > IR_WAVE_WINDOW_MS) {
    if (recentEdges >= IR_WAVE_MIN_EVENTS && pending == IR_EVT_NONE) {
      pending = IR_EVT_WAVE;
    }
    recentEdges = 0;
  }

  // Hold / Long hold
  if (holding && pending == IR_EVT_NONE) {
    unsigned long dur = now - holdStartMs;
    if (dur >= IR_LONG_HOLD_MS) {
      pending = IR_EVT_LONG_HOLD;
      holding = false;
      tapsInWindow = 0;
    }
  }

  // Tap / Double / Triple: timeout window
  if (tapsInWindow > 0 && (now - firstTapMs > IR_DOUBLE_GAP_MS) && pending == IR_EVT_NONE) {
    if (tapsInWindow == 1)       pending = IR_EVT_TAP;
    else if (tapsInWindow == 2) pending = IR_EVT_DOUBLE;
    else if (tapsInWindow >= 3)  pending = IR_EVT_TRIPLE;
    tapsInWindow = 0;
  }
}
