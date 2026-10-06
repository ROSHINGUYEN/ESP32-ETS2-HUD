/**
 * AppMain.cpp — setup() + loop() chính
 */
#include "config.h"
#include "HeadServo.h"
#include "Emotion.h"
#include "Gestures.h"
#include "Autonomy.h"
#include "DisplayTask.h"
#include "IrGesture.h"
#include "WebApi.h"
#include "TaskEntry.h"
#include "Globals.h"

HeadServo   gHead;
Emotion     gEmotion;
Gestures    gGest;
Autonomy    gAutonomy;
BrainArgs   gBrainArgs;
TaskHandle_t hServo   = NULL;
TaskHandle_t hDisplay = NULL;
TaskHandle_t hBrain   = NULL;
unsigned long lastTelemMs = 0;

void appSetup() {
  Serial0.begin(SERIAL0_BAUD, SERIAL_8N1, PIN_SERIAL0_RX, PIN_SERIAL0_TX);
  delay(200);
  Serial0.println(F("\n=== EILIK v" FIRMWARE_VERSION " ==="));
  Serial0.println(FIRMWARE_BUILD_TIME);

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  globalsInit();
  DisplayTask* disp = globalsGetDisplay();
  IrGesture*   ir   = globalsGetIr();
  WebApi*      web  = globalsGetWeb();

  gHead.begin();
  gEmotion.setMood(MOOD_NEUTRAL);
  gGest.setHeadServo(&gHead);
  gAutonomy.setHeadServo(&gHead);
  gAutonomy.setGestures(&gGest);
  gAutonomy.setMode(AM_CALM);
  disp->begin();
  disp->setEmotion(&gEmotion);
  ir->begin(PIN_IR_PRIMARY, PIN_IR_SECONDARY, IR_ACTIVE_LEVEL == LOW);

  web->begin(&gHead, &gEmotion, &gGest, &gAutonomy);

  gBrainArgs.ir       = ir;
  gBrainArgs.emo      = &gEmotion;
  gBrainArgs.ges      = &gGest;
  gBrainArgs.auto_ptr = &gAutonomy;

  xTaskCreatePinnedToCore(taskServoEntry,   "Servo",   STACK_SERVO_TASK,   &gHead,      PRIO_SERVO,   &hServo,   CORE_SERVO);
  xTaskCreatePinnedToCore(taskDisplayEntry, "Display", STACK_DISPLAY_TASK, disp,        PRIO_DISPLAY, &hDisplay, CORE_DISPLAY);
  xTaskCreatePinnedToCore(taskBrainEntry,   "Brain",   STACK_BRAIN_TASK,   &gBrainArgs, PRIO_BRAIN,   &hBrain,   CORE_BRAIN);

  Serial0.println(F("[OK] Tasks started"));
}

void appLoop() {
  WebApi* web = globalsGetWeb();
  if (web) web->tick();
  unsigned long now = millis();
  if (now - lastTelemMs >= TELEMETRY_PERIOD_MS) {
    lastTelemMs = now;
    DisplayTask* disp = globalsGetDisplay();
    Serial0.printf("[TELEM] up=%lus heap=%u min=%u oledFps=%.1f pan=%.0f tilt=%.0f\n",
                   now / 1000, ESP.getFreeHeap(), ESP.getMinFreeHeap(),
                   disp ? disp->getFps() : 0.0f,
                   gHead.getCurrentPan(), gHead.getCurrentTilt());
  }
  delay(10);
}
