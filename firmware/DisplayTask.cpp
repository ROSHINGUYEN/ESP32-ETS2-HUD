/**
 * DisplayTask.cpp — Quản lý OLED 128x64
 *  - DM_EYES: RoboEyes animation khi có tương tác
 *  - DM_CLOCK: Weather dashboard khi idle
 *  - DM_STREAM: Nhận bitmap 1bpp
 */
#include "DisplayTask.h"
#include "config.h"

static const uint8_t ROBO_DEFAULT = 0;
static const uint8_t ROBO_TIRED   = 1;
static const uint8_t ROBO_ANGRY   = 2;
static const uint8_t ROBO_HAPPY   = 3;

// =================== ===================
DisplayTask::DisplayTask()
  : oled(SCREEN_W, SCREEN_H, &Wire, -1), pEyes(nullptr), _emotion(nullptr),
    oledOk(false), mode(DM_CLOCK), streamHasNew(false),
    lastFrameMs(0), fps(0), frameCount(0), fpsTimerMs(0),
    prevBlinker(true), lastPresenceMs(0),
    _pan(90), _tilt(90), _clients(0), prevClockMin(-1) {
  lastStreamFrameMs = 0;
  memset(streamBuf, 0, sizeof(streamBuf));
  strncpy(_mood, "NEUTRAL", sizeof(_mood) - 1);
  strncpy(_modeStr, "CALM", sizeof(_modeStr) - 1);
}

void DisplayTask::begin() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(OLED_I2C_HZ);
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (!oledOk) {
    Serial0.println(F("[FAIL] OLED SSD1306 not found"));
    return;
  }
  pEyes = new RoboEyes<Adafruit_SSD1306>(oled);
  pEyes->begin(SCREEN_W, SCREEN_H, DISPLAY_FPS_HZ);
  pEyes->setAutoblinker(ON, 3, 2);
  pEyes->setIdleMode(ON, 2, 2);
  pEyes->setMood(DEFAULT);
  bootMs = millis();
  fpsTimerMs = millis();
  oled.clearDisplay();
  oled.display();
  Serial0.println(F("[OK]   DisplayTask init"));
}

void DisplayTask::setMode(DisplayMode m) {
  if (m == mode) return;
  mode = m;
  if (m == DM_EYES) {
    if (pEyes) pEyes->setMood(DEFAULT);
  }
  DBG_INFO("mode=%u", (unsigned)m);
}

void DisplayTask::notifyPresence() {
  lastPresenceMs = millis();
  if (mode != DM_STREAM) {
    setMode(DM_EYES);
  }
}

void DisplayTask::setStats(float pan, float tilt, const char* mood, const char* modeStr, int clients) {
  _pan = pan;
  _tilt = tilt;
  if (mood) {
    strncpy(_mood, mood, sizeof(_mood) - 1);
    _mood[sizeof(_mood) - 1] = '\0';
  }
  if (modeStr) {
    strncpy(_modeStr, modeStr, sizeof(_modeStr) - 1);
    _modeStr[sizeof(_modeStr) - 1] = '\0';
  }
  _clients = clients;
}

void DisplayTask::setStreamFrame(const uint8_t* bitmap1024) {
  if (!bitmap1024) return;
  memcpy(streamBuf, bitmap1024, 1024);
  streamHasNew = true;
  lastStreamFrameMs = millis();
}

void DisplayTask::applyEmotionToRobo() {
  if (!_emotion || !pEyes) return;
  uint8_t targetMood = _emotion->requestedRoboMood();
  switch (targetMood) {
    case ROBO_HAPPY: pEyes->setMood(HAPPY);   break;
    case ROBO_ANGRY: pEyes->setMood(ANGRY);   break;
    case ROBO_TIRED: pEyes->setMood(TIRED);   break;
    default:         pEyes->setMood(DEFAULT); break;
  }
  bool on = _emotion->autoblinkerOn();
  if (on != prevBlinker) {
    pEyes->setAutoblinker(on ? ON : OFF, 3, 2);
    prevBlinker = on;
  }
  static bool lastLaugh = false;
  bool laugh = _emotion->wantLaugh();
  if (laugh && !lastLaugh) pEyes->laugh = 1;
  lastLaugh = laugh;
  static bool lastConf = false;
  bool conf = _emotion->wantConfused();
  if (conf != lastConf) pEyes->anim_confused();
  lastConf = conf;
}

void DisplayTask::drawEyesFrame() {
  applyEmotionToRobo();
  if (pEyes) pEyes->update();
}

// =================== CLOCK FRAME (weather dashboard) ===================
static const char* daysVN[] = {"CN","T2","T3","T4","T5","T6","T7"};
static const char* monthsVN[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};

void DisplayTask::drawClockFrame() {
  if (!oledOk) return;
  oled.clearDisplay();

  // ---- Tính thời gian giả định (offline, không NTP) ----
  // base: 2026-01-01 00:00:00 UTC, mỗi millis() = thời gian thực từ boot
  // Hiển thị tương đối: dựa trên millis() để tránh phụ thuộc WiFi
  unsigned long upMs = millis() - bootMs;
  unsigned long secs = upMs / 1000;
  unsigned long mins_l = (secs / 60) % 60;
  unsigned long hours_l = (secs / 3600) % 24;
  unsigned long days_l = secs / 86400;

  // Date giả - base 2026-01-01 (thứ 5)
  struct tm fake = {0};
  fake.tm_year = 2026 - 1900;
  fake.tm_mon  = 0;
  fake.tm_mday = 1 + days_l;
  fake.tm_hour = hours_l;
  fake.tm_min  = mins_l;
  fake.tm_sec  = secs % 60;
  // tính thức đơn giản (không chính xác tuyệt đối - không cần thiết)
  int weekday = (5 + days_l) % 7;  // 2026-01-01 là T5 (weekday index 5)

  // ============= TOP ROW: Mood + Clients =============
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.printf(">%s", _mood);
  oled.setCursor(SCREEN_W - 32, 0);
  oled.printf("W%d", _clients);

  // ============= BIG CLOCK (center, 2x scale) =============
  oled.setTextSize(2);
  char timeFmt[12];
  snprintf(timeFmt, sizeof(timeFmt), "%02lu:%02lu", hours_l, mins_l);
  int16_t x1, y1;
  uint16_t w, h;
  oled.getTextBounds(timeFmt, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((SCREEN_W - w) / 2, 14);
  oled.print(timeFmt);

  // ============= DATE (under clock) =============
  oled.setTextSize(1);
  char dateBuf[24];
  snprintf(dateBuf, sizeof(dateBuf), "%s %d %s",
           daysVN[weekday], fake.tm_mday, monthsVN[fake.tm_mon % 12]);
  oled.getTextBounds(dateBuf, 0, 0, &x1, &y1, &w, &h);
  oled.setCursor((SCREEN_W - w) / 2, 32);
  oled.print(dateBuf);

  // ============= BOTTOM: stats + uptime =============
  oled.setCursor(0, 44);
  oled.printf("PAN%3d", (int)_pan);
  oled.setCursor(44, 44);
  oled.printf("TILT%3d", (int)_tilt);
  oled.setCursor(SCREEN_W - 22, 44);
  oled.printf("+%luh", hours_l);

  oled.setCursor(0, 54);
  oled.printf("MODE:%s", _modeStr);
  oled.setCursor(SCREEN_W - 22, 54);
  oled.printf("D%ld", days_l);

  // separator lines
  oled.drawLine(0, 11, SCREEN_W, 11, SSD1306_WHITE);
  oled.drawLine(0, 42, SCREEN_W, 42, SSD1306_WHITE);
  oled.display();
}

void DisplayTask::drawStreamFrame() {
  if (!oledOk) return;
  oled.clearDisplay();
  oled.drawBitmap(0, 0, streamBuf, SCREEN_W, SCREEN_H, SSD1306_WHITE);
  oled.display();
}

void DisplayTask::tick() {
  if (!oledOk) return;
  unsigned long now = millis();

  // Stream timeout → về CLOCK
  if (mode == DM_STREAM && (now - lastStreamFrameMs > 2000)) {
    setMode(DM_CLOCK);
  }

  // Auto presence: có tương tác gần đây → EYES, không thì CLOCK
  if (mode != DM_STREAM) {
    if (mode == DM_EYES && (now - lastPresenceMs > PRESENCE_TIMEOUT_MS)) {
      setMode(DM_CLOCK);
    }
  }

  // FPS giới hạn theo mode
  uint16_t targetPeriod;
  if (mode == DM_EYES)       targetPeriod = 1000 / DISPLAY_FPS_HZ;
  else if (mode == DM_CLOCK) targetPeriod = 1000 / 4; // 4 FPS cho clock
  else                        targetPeriod = 33;
  if (now - lastFrameMs < targetPeriod) return;
  lastFrameMs = now;

  switch (mode) {
    case DM_EYES:   drawEyesFrame();   break;
    case DM_CLOCK:  drawClockFrame();  break;
    case DM_STREAM: drawStreamFrame(); break;
    default: break;
  }
  streamHasNew = false;

  frameCount++;
  if (now - fpsTimerMs >= 1000) {
    fps = (float)frameCount * 1000.0f / (now - fpsTimerMs);
    frameCount = 0;
    fpsTimerMs = now;
  }
}