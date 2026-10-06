/**
 * =====================================================================================
 *  Emotion.h — Quản lý mood & ánh xạ sang OLED expression + gesture
 * =====================================================================================
 */
#pragma once
#include <Arduino.h>
#include <stdint.h>

enum Mood : uint8_t {
  MOOD_NEUTRAL = 0,
  MOOD_HAPPY,
  MOOD_ANGRY,
  MOOD_SLEEPY,
  MOOD_CURIOUS,
  MOOD_CONFUSED,
  MOOD_WAKE_UP,
  MOOD_COUNT
};

class Emotion {
public:
  Emotion();

  void   setMood(Mood m);
  Mood   getMood() const { return currentMood; }
  void   applyToOled();    // gọi từ DisplayTask qua EyeState

  // ---- RoboEyes mapping (chỉ dùng API có thật của FluxGarage v1.1.2) ----
  // Trả về các tham số để DisplayTask gọi RoboEyes
  uint8_t requestedRoboMood() const;  // DEFAULT/TIRED/ANGRY/HAPPY
  bool    wantLaugh()    const;
  bool    wantConfused() const;
  bool    autoblinkerOn() const;     // SLEEPY tắt blink tự động

  // ---- Gợi ý gesture kèm theo emotion ----
  // (được Brain xử lý, không gọi trực tiếp)
  bool    suggestsGesture() const;
  const char* name() const;

private:
  Mood    currentMood;
};
