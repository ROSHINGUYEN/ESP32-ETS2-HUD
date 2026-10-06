/**
 * Emotion.cpp
 */
#include "Emotion.h"
#include "config.h"

// RoboEyes mood constants (trùng file FluxGarage_RoboEyes.h)
static const uint8_t ROBO_DEFAULT = 0;
static const uint8_t ROBO_TIRED   = 1;
static const uint8_t ROBO_ANGRY   = 2;
static const uint8_t ROBO_HAPPY   = 3;

Emotion::Emotion() : currentMood(MOOD_NEUTRAL) {}

void Emotion::setMood(Mood m) {
  if (m >= MOOD_COUNT) return;
  currentMood = m;
  DBG_INFO("-> %s", name());
}

uint8_t Emotion::requestedRoboMood() const {
  switch (currentMood) {
    case MOOD_HAPPY:    return ROBO_HAPPY;
    case MOOD_ANGRY:    return ROBO_ANGRY;
    case MOOD_SLEEPY:   return ROBO_TIRED;
    default:            return ROBO_DEFAULT;
  }
}

bool Emotion::wantLaugh()    const { return currentMood == MOOD_HAPPY; }
bool Emotion::wantConfused() const { return currentMood == MOOD_CONFUSED; }

bool Emotion::autoblinkerOn() const {
  // Section 13: SLEEPY tắt autoblink
  return (currentMood != MOOD_SLEEPY);
}

bool Emotion::suggestsGesture() const {
  switch (currentMood) {
    case MOOD_HAPPY: case MOOD_CURIOUS: case MOOD_CONFUSED:
    case MOOD_SLEEPY: case MOOD_WAKE_UP: return true;
    default: return false;
  }
}

const char* Emotion::name() const {
  switch (currentMood) {
    case MOOD_NEUTRAL: return "NEUTRAL";
    case MOOD_HAPPY:   return "HAPPY";
    case MOOD_ANGRY:   return "ANGRY";
    case MOOD_SLEEPY:  return "SLEEPY";
    case MOOD_CURIOUS: return "CURIOUS";
    case MOOD_CONFUSED:return "CONFUSED";
    case MOOD_WAKE_UP: return "WAKE_UP";
    default:           return "?";
  }
}
