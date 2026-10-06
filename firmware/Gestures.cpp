/**
 * Gestures.cpp
 */
#include "Gestures.h"
#include "HeadServo.h"
#include "config.h"

// Keyframe tracks — Section 12
static const Keyframe TR_YES[] = {
  {90, 96, 180, true}, {90, 82, 180, true}, {90, 100, 140, true},
  {90, 86, 140, true}, {90, 90, 200, true}
};
static const Keyframe TR_NO[] = {
  {115, 90, 180, true}, {65, 90, 200, true}, {110, 90, 200, true},
  {70, 90, 200, true},  {90, 90, 200, true}
};
static const Keyframe TR_LOOK_L[] = { {70,  90, 350, true}, {70, 90, 500, true}, {90, 90, 300, true} };
static const Keyframe TR_LOOK_R[] = { {110, 90, 350, true}, {110, 90, 500, true}, {90, 90, 300, true} };
static const Keyframe TR_LOOK_U[] = { {90,  76, 400, true}, {90, 76, 500, true}, {90, 90, 300, true} };
static const Keyframe TR_LOOK_D[] = { {90, 105, 400, true}, {90, 105, 500, true}, {90, 90, 300, true} };
static const Keyframe TR_CURIOUS[] = { {105, 80, 350, true}, {105, 80, 700, true}, {90, 90, 400, true} };
static const Keyframe TR_CONFUSED[] = { {110, 88, 200, true}, {70, 92, 200, true}, {110, 88, 200, true}, {70, 92, 200, true}, {90, 90, 250, true} };
static const Keyframe TR_SLEEP[] = { {90, 96, 400, true}, {90, 106, 700, true}, {90, 113, 800, true} };
static const Keyframe TR_WAKE[] = { {85, 113, 350, true}, {90, 76, 500, true}, {100, 76, 250, true}, {90, 90, 350, true} };
static const Keyframe TR_HAPPY[] = { {90, 100, 110, true}, {90, 84, 130, true}, {90, 100, 110, true}, {90, 90, 200, true} };
static const Keyframe TR_ANGRY[] = { {115, 95, 250, true}, {65, 95, 250, true}, {90, 90, 300, true} };
// BREATH chạy vô tận - không dùng track; Gestures::tick sẽ tự oscillate

Gestures::Gestures() : _head(nullptr), playing(GES_NONE), track(nullptr),
                       trackLen(0), frameIdx(0), frameStartMs(0), active(false) {}

void Gestures::play(GestureId id) {
  if (id >= GES_COUNT) return;
  if (id == GES_BREATH) { playing = id; active = true; return; }
  switch (id) {
    case GES_YES:        track = TR_YES;       trackLen = 5; break;
    case GES_NO:         track = TR_NO;        trackLen = 5; break;
    case GES_LOOK_LEFT:  track = TR_LOOK_L;    trackLen = 3; break;
    case GES_LOOK_RIGHT: track = TR_LOOK_R;    trackLen = 3; break;
    case GES_LOOK_UP:    track = TR_LOOK_U;    trackLen = 3; break;
    case GES_LOOK_DOWN:  track = TR_LOOK_D;    trackLen = 3; break;
    case GES_CURIOUS:    track = TR_CURIOUS;   trackLen = 3; break;
    case GES_CONFUSED:   track = TR_CONFUSED;  trackLen = 5; break;
    case GES_SLEEP:      track = TR_SLEEP;     trackLen = 3; break;
    case GES_WAKE:       track = TR_WAKE;      trackLen = 4; break;
    case GES_HAPPY:      track = TR_HAPPY;     trackLen = 4; break;
    case GES_ANGRY:      track = TR_ANGRY;     trackLen = 3; break;
    default:             active = false; return;
  }
  playing = id;
  active  = true;
  frameIdx = 0;
  startFrame(0);
}

void Gestures::startFrame(uint8_t i) {
  frameIdx = i;
  frameStartMs = millis();
  if (_head && i < trackLen) {
    const Keyframe &kf = track[i];
    _head->setTarget(kf.pan, kf.tilt);
  }
}

void Gestures::stop() {
  active = false;
  playing = GES_NONE;
}

void Gestures::tick() {
  if (!active || !_head) return;
  if (playing == GES_BREATH) {
    // ±1.5° quanh center, chu kỳ 4s
    float t = (millis() % 4000) / 4000.0f;     // 0..1
    float wave = sinf(t * 2.0f * 3.14159f) * 1.5f;
    _head->setTarget(PAN_CENTER_DEG, TILT_CENTER_DEG + wave);
    return;
  }
  if (millis() - frameStartMs >= track[frameIdx].ms) {
    if (frameIdx + 1 < trackLen) startFrame(frameIdx + 1);
    else { active = false; playing = GES_NONE; }
  }
}
