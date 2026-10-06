/**
 * TaskEntry.cpp — FreeRTOS task functions
 */
#include "TaskEntry.h"
#include "config.h"
#include "HeadServo.h"
#include "Emotion.h"
#include "Gestures.h"
#include "Autonomy.h"
#include "DisplayTask.h"
#include "IrGesture.h"
#include "WebApi.h"
#include "Globals.h"

void taskServoEntry(void* arg) {
  HeadServo* head = (HeadServo*)arg;
  unsigned long prev = millis();
  for (;;) {
    unsigned long now = millis();
    float dt = (now - prev) / 1000.0f;
    if (dt > 0.1f) dt = 0.02f;
    if (head) head->update(dt);
    prev = now;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void taskDisplayEntry(void* arg) {
  DisplayTask* disp = (DisplayTask*)arg;
  for (;;) {
    if (disp) disp->tick();
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}

void taskBrainEntry(void* arg) {
  BrainArgs* ba = (BrainArgs*)arg;
  if (!ba) { vTaskDelete(NULL); return; }

  unsigned long prev = millis();
  for (;;) {
    unsigned long now = millis();
    float dt = (now - prev) / 1000.0f;
    if (dt > 0.1f) dt = 0.02f;

    // IR tick
    if (ba->ir) ba->ir->tick();
    IrEvent e = ba->ir ? ba->ir->consumeEvent() : IR_EVT_NONE;
    bool irActive = ba->ir ? (ba->ir->isCurrentlyActive() || ba->ir->hasRecentMotion()) : false;

    // Khi phát hiện vật thể/chuyển động qua mạch hồng ngoại -> Lập tức hiện mắt robot!
    if (irActive || e != IR_EVT_NONE) {
      DisplayTask* disp = globalsGetDisplay();
      if (disp) {
        disp->notifyPresence();
      }
      ba->auto_ptr->notifyManual();
    }

    if (e != IR_EVT_NONE) {
      switch (e) {
        case IR_EVT_TAP:       ba->emo->setMood(MOOD_HAPPY); break;
        case IR_EVT_DOUBLE:    ba->auto_ptr->setMode((AutonomyMode)((ba->auto_ptr->getMode() + 1) % 4)); break;
        case IR_EVT_TRIPLE:    ba->ges->play(GES_CURIOUS); break;
        case IR_EVT_HOLD:      ba->ges->play(GES_HAPPY); break;
        case IR_EVT_LONG_HOLD: ba->ges->play(ba->emo->getMood() == MOOD_SLEEPY ? GES_WAKE : GES_SLEEP); break;
        case IR_EVT_WAVE:      ba->emo->setMood(MOOD_CONFUSED); break;
        default: break;
      }
    }

    ba->ges->tick();
    ba->auto_ptr->tick();

    prev = now;
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
