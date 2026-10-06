/**
 * =====================================================================================
 *  Gestures.h — Keyframe-based gesture player
 *  Hỗ trợ: YES, NO, LOOK_LEFT, LOOK_RIGHT, LOOK_UP, LOOK_DOWN,
 *           CURIOUS, CONFUSED, SLEEP, WAKE, HAPPY, ANGRY, BREATH
 * =====================================================================================
 */
#pragma once
#include <Arduino.h>
#include <stdint.h>

enum GestureId : uint8_t {
  GES_NONE = 0,
  GES_YES, GES_NO,
  GES_LOOK_LEFT, GES_LOOK_RIGHT, GES_LOOK_UP, GES_LOOK_DOWN,
  GES_CURIOUS, GES_CONFUSED, GES_SLEEP, GES_WAKE,
  GES_HAPPY, GES_ANGRY, GES_BREATH,
  GES_COUNT
};

struct Keyframe { float pan, tilt; uint16_t ms; bool ease; };

class Gestures {
public:
  Gestures();

  void   play(GestureId id);
  void   stop();
  bool   isPlaying() const { return active; }
  GestureId current() const { return playing; }

  // Gọi mỗi 20ms (BrainTask). Trả về true nếu đang play.
  // Tự động ghi target vào head.setTarget().
  // HeadServo được truyền qua callback hoặc pointer.
  void   setHeadServo(class HeadServo* h) { _head = h; }
  void   tick();

private:
  class HeadServo* _head;
  GestureId playing;
  const Keyframe* track;
  uint8_t trackLen;
  uint8_t frameIdx;
  unsigned long frameStartMs;
  bool active;

  void startFrame(uint8_t i);
};
