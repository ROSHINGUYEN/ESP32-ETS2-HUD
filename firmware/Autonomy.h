/**
 * =====================================================================================
 *  Autonomy.h — Tự hành theo mode (SLEEP/CALM/CURIOUS/PLAYFUL)
 *  - Weighted probability + cooldown
 *  - manual priority override (Section 16)
 * =====================================================================================
 */
#pragma once
#include <Arduino.h>
#include <stdint.h>

enum AutonomyMode : uint8_t {
  AM_SLEEP = 0,
  AM_CALM,
  AM_CURIOUS,
  AM_PLAYFUL,
  AM_COUNT
};

class Autonomy {
public:
  Autonomy();

  void   setMode(AutonomyMode m);
  AutonomyMode getMode() const { return mode; }

  void   notifyManual();   // gọi khi nhận lệnh từ Web/USB/IR
  bool   isManualActive() const;

  void   setHeadServo(class HeadServo* h) { _head = h; }
  void   setGestures(class Gestures* g)   { _ges  = g;  }

  // Gọi mỗi 50Hz (BrainTask)
  void   tick();

private:
  class HeadServo* _head;
  class Gestures*  _ges;
  AutonomyMode mode;
  unsigned long manualUntil;
  unsigned long lastActionMs;
  uint16_t     nextDelayMs;

  void   scheduleNext();
  void   doRandomAction();
};
