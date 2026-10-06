/**
 * =====================================================================================
 *  DisplayTask.h — OLED eyes manager (DUY NHẤT thao tác OLED)
 *  - DM_EYES  : RoboEyes animation (30 FPS)
 *  - DM_CLOCK : Weather dashboard (clock, date, stats) — khi robot idle
 *  - DM_STREAM: Nhận bitmap 1bpp từ PC qua USB
 *  Auto: IR presence → chuyển sang DM_EYES; timeout 8s → về DM_CLOCK
 * =====================================================================================
 */
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Tránh xung đột với FluxGarage_RoboEyes.h (macro N/S/E/W)
#ifdef N
  #undef N
#endif
#ifdef S
  #undef S
#endif
#ifdef E
  #undef E
#endif
#ifdef W
  #undef W
#endif
#ifdef NE
  #undef NE
#endif
#ifdef NW
  #undef NW
#endif
#ifdef SE
  #undef SE
#endif
#ifdef SW
  #undef SW
#endif

#include <FluxGarage_RoboEyes.h>
#include "Emotion.h"

enum DisplayMode : uint8_t {
  DM_CLOCK = 0,    // Mặc định - weather dashboard
  DM_EYES  = 1,    // Khi có tương tác
  DM_STREAM= 2     // Bitmap từ PC
};

class DisplayTask {
public:
  DisplayTask();

  void   begin();
  void   tick();

  void   setEmotion(Emotion* e) { _emotion = e; }

  float  getFps() const { return fps; }
  bool   isOledOk() const { return oledOk; }

  void   setMode(DisplayMode m);
  DisplayMode getMode() const { return mode; }

  // Trigger từ IR — chuyển sang EYES, đặt lại timeout
  void   notifyPresence();

  // Set stats cho weather dashboard
  void   setStats(float pan, float tilt, const char* mood, const char* modeStr, int clients);

  // Stream
  void   setStreamFrame(const uint8_t* bitmap1024);
  bool   hasStreamFrame() const { return streamHasNew; }
  unsigned long lastStreamFrameMs;

private:
  Adafruit_SSD1306 oled;
  RoboEyes<Adafruit_SSD1306>* pEyes;
  Emotion* _emotion;
  bool oledOk;
  DisplayMode mode;
  uint8_t  streamBuf[1024];
  volatile bool streamHasNew;
  unsigned long lastFrameMs;
  float fps;
  uint16_t frameCount;
  unsigned long fpsTimerMs;
  bool prevBlinker;

  // Presence state
  unsigned long lastPresenceMs;
  static constexpr unsigned long PRESENCE_TIMEOUT_MS = 8000;

  // Stats snapshot
  float _pan;
  float _tilt;
  char _mood[16];
  char _modeStr[16];
  int _clients;

  // Clock internal
  unsigned long bootMs;
  int prevClockMin;

  void   drawEyesFrame();
  void   drawStreamFrame();
  void   drawClockFrame();
  void   applyEmotionToRobo();
};