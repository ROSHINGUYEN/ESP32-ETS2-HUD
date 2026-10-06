/**
 * =====================================================================================
 *  IrGesture.h — Phân loại gesture từ IR LM393 (Active LOW)
 *  - Tap / Double / Triple / Hold / Long hold / Wave
 *  - Polling-based edge detection + state machine
 * =====================================================================================
 */
#pragma once
#include <Arduino.h>
#include <stdint.h>

enum IrEvent : uint8_t {
  IR_EVT_NONE = 0,
  IR_EVT_TAP,
  IR_EVT_DOUBLE,
  IR_EVT_TRIPLE,
  IR_EVT_HOLD,
  IR_EVT_LONG_HOLD,
  IR_EVT_WAVE
};

class IrGesture {
public:
  IrGesture();

  void   begin(uint8_t pinPrimary, uint8_t pinSecondary = 0xFF, bool activeLow = true);
  void   tick();   // gọi ~20ms

  // Latest event (cleared khi đọc)
  IrEvent consumeEvent();

  // Status
  bool    isCurrentlyActive() const { return isActiveFlag; }
  bool    hasRecentMotion() const { return isActiveFlag || (millis() - lastChangeMs < 1500); }
  uint32_t eventCount() const { return eventCounter; }

private:
  uint8_t  pin1, pin2;
  bool     activeLow;
  bool     isActiveFlag;    // đổi tên tránh trùng macro
  uint32_t eventCounter;

  // Edge timing
  unsigned long lastEdgeMs1, lastEdgeMs2;
  bool     prevLevel1, prevLevel2;

  // Gesture detection
  uint8_t  tapsInWindow;
  unsigned long firstTapMs;
  bool     holding;
  unsigned long holdStartMs;
  unsigned long lastChangeMs;
  uint8_t  recentEdges;

  IrEvent pending;

  void   processLevel(uint8_t level, unsigned long now, bool isP1);
};
