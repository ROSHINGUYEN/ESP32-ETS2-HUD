/**
 * TaskEntry.h — Task function signatures
 */
#pragma once
#include <Arduino.h>

class HeadServo;
class DisplayTask;
class IrGesture;
class WebApi;
class Emotion;
class Gestures;
class Autonomy;

struct BrainArgs {
  IrGesture*  ir;
  Emotion*    emo;
  Gestures*   ges;
  Autonomy*   auto_ptr;
};

void taskServoEntry(void* arg);
void taskDisplayEntry(void* arg);
void taskBrainEntry(void* arg);
