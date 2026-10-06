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
    _pan(90), _tilt(90), _clients(0),
    bootMs(0), baseTimeSec(0), tempC(26.8f), humidity(60.0f),
    lastTrendUpdateMs(0) {
  lastStreamFrameMs = 0;
  memset(streamBuf, 0, sizeof(streamBuf));
  strncpy(_mood, "NEUTRAL", sizeof(_mood) - 1);
  strncpy(_modeStr, "CALM", sizeof(_modeStr) - 1);
  strncpy(weatherCondition, "SUNNY", sizeof(weatherCondition) - 1);

  // Mẫu độ cao các cột biểu đồ xu hướng giống ảnh 1
  trendBars[0] = 5;
  trendBars[1] = 8;
  trendBars[2] = 11;
  trendBars[3] = 15;
  trendBars[4] = 13;
  trendBars[5] = 9;
  trendBars[6] = 6;
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
  lastTrendUpdateMs = millis();

  // Khởi tạo thời gian khởi điểm từ __DATE__ và __TIME__
  int bYr = 2026, bMon = 10, bMday = 6, bHr = 12, bMin = 58, bSec = 0;
  const char* dateStr = __DATE__;
  const char* timeStr = __TIME__;
  char mStr[4] = {0};
  sscanf(dateStr, "%3s %d %d", mStr, &bMday, &bYr);
  sscanf(timeStr, "%d:%d:%d", &bHr, &bMin, &bSec);
  const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
  for (int i = 0; i < 12; i++) {
    if (strncmp(mStr, months[i], 3) == 0) { bMon = i + 1; break; }
  }
  (void)bYr; (void)bMon;
  baseTimeSec = (unsigned long)bHr * 3600 + (unsigned long)bMin * 60 + bSec + (unsigned long)(bMday * 86400);

  oled.clearDisplay();
  oled.display();
  Serial0.println(F("[OK]   DisplayTask init (Weather Dashboard ready)"));
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

void DisplayTask::setTime(int hr, int min, int sec, int mday, int mon, int yr, int wday) {
  (void)mon; (void)yr; (void)wday;
  bootMs = millis();
  baseTimeSec = (unsigned long)hr * 3600 + (unsigned long)min * 60 + sec + (unsigned long)(mday * 86400);
}

void DisplayTask::setWeather(float temp, float humi, const char* condition) {
  tempC = temp;
  humidity = humi;
  if (condition) {
    strncpy(weatherCondition, condition, sizeof(weatherCondition) - 1);
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

// =================== ĐỒ HỌA WEATHER STATION (ẢNH 1) ===================

void DisplayTask::drawSunWithRays(int16_t cx, int16_t cy) {
  // Mặt trời hình tròn bán kính 5
  oled.fillCircle(cx, cy, 5, SSD1306_WHITE);
  // Mặt cười bên trong mặt trời
  oled.drawPixel(cx - 2, cy - 1, SSD1306_BLACK); // mắt trái
  oled.drawPixel(cx + 2, cy - 1, SSD1306_BLACK); // mắt phải
  oled.drawPixel(cx - 2, cy + 2, SSD1306_BLACK); // nụ cười
  oled.drawPixel(cx - 1, cy + 3, SSD1306_BLACK);
  oled.drawPixel(cx,     cy + 3, SSD1306_BLACK);
  oled.drawPixel(cx + 1, cy + 3, SSD1306_BLACK);
  oled.drawPixel(cx + 2, cy + 2, SSD1306_BLACK);

  // 8 tia nắng tỏa ra xung quanh
  oled.drawLine(cx, cy - 7, cx, cy - 9, SSD1306_WHITE);
  oled.drawLine(cx, cy + 7, cx, cy + 9, SSD1306_WHITE);
  oled.drawLine(cx - 7, cy, cx - 9, cy, SSD1306_WHITE);
  oled.drawLine(cx + 7, cy, cx + 9, cy, SSD1306_WHITE);
  oled.drawLine(cx - 5, cy - 5, cx - 7, cy - 7, SSD1306_WHITE);
  oled.drawLine(cx + 5, cy - 5, cx + 7, cy - 7, SSD1306_WHITE);
  oled.drawLine(cx - 5, cy + 5, cx - 7, cy + 7, SSD1306_WHITE);
  oled.drawLine(cx + 5, cy + 5, cx + 7, cy + 7, SSD1306_WHITE);
}

void DisplayTask::drawCloud(int16_t x, int16_t y) {
  // Cụm mây bồng bềnh
  oled.fillCircle(x + 5,  y + 6, 4, SSD1306_WHITE);
  oled.fillCircle(x + 11, y + 4, 5, SSD1306_WHITE);
  oled.fillCircle(x + 16, y + 7, 3, SSD1306_WHITE);
  oled.fillRect(x + 4,   y + 6, 13, 5, SSD1306_WHITE);
}

void DisplayTask::drawTrendBars(int16_t startX, int16_t baseY) {
  // 7 cột biểu đồ xu hướng nhiệt độ / hoạt động dạng thanh phân đoạn (Equalizer style)
  for (int i = 0; i < 7; i++) {
    int bx = startX + i * 6;
    int h = constrain(trendBars[i], 2, 18);
    for (int y = 0; y < h; y += 3) {
      oled.drawFastHLine(bx, baseY - y, 4, SSD1306_WHITE);
      if (y + 1 < h) {
        oled.drawFastHLine(bx, baseY - y - 1, 4, SSD1306_WHITE);
      }
    }
  }
}

void DisplayTask::drawDroplet(int16_t x, int16_t y) {
  // Icon giọt nước (độ ẩm)
  oled.drawPixel(x + 2, y, SSD1306_WHITE);
  oled.drawLine(x + 1, y + 1, x + 3, y + 1, SSD1306_WHITE);
  oled.drawLine(x,     y + 2, x + 4, y + 2, SSD1306_WHITE);
  oled.drawLine(x,     y + 3, x + 4, y + 3, SSD1306_WHITE);
  oled.drawLine(x + 1, y + 4, x + 3, y + 4, SSD1306_WHITE);
}

void DisplayTask::drawThermometer(int16_t x, int16_t y) {
  // Icon nhiệt kế
  oled.drawLine(x + 1, y, x + 1, y + 4, SSD1306_WHITE);
  oled.drawPixel(x,     y + 1, SSD1306_WHITE);
  oled.drawPixel(x + 2, y + 1, SSD1306_WHITE);
  oled.fillCircle(x + 1, y + 6, 2, SSD1306_WHITE);
}

void DisplayTask::drawClockFrame() {
  if (!oledOk) return;
  oled.clearDisplay();

  unsigned long now = millis();
  unsigned long curTotalSec = baseTimeSec + (now - bootMs) / 1000;
  int curSec  = curTotalSec % 60;
  int curMin  = (curTotalSec / 60) % 60;
  int curHour = (curTotalSec / 3600) % 24;
  unsigned long totalDays = curTotalSec / 86400;
  int weekday = (totalDays + 4) % 7; // 0=CN, 1=T2, ..., 6=SAT (base 1970 là Thứ 5)

  // Cập nhật hoạt ảnh biểu đồ trend
  if (now - lastTrendUpdateMs >= 2500) {
    lastTrendUpdateMs = now;
    for (int i = 0; i < 6; i++) {
      trendBars[i] = trendBars[i + 1];
    }
    int nextH = 6 + (int)(sin((float)now * 0.0015f) * 4.0f) + (random(0, 5));
    trendBars[6] = constrain(nextH, 2, 18);
  }

  // Đọc nhiệt độ chip ESP32-S3 ước lượng nhiệt độ phòng
  #if defined(ESP32)
  float rawChipTemp = temperatureRead();
  if (rawChipTemp > 10.0f && rawChipTemp < 85.0f) {
    float estAmbient = rawChipTemp - 11.0f;
    tempC = (tempC * 0.95f) + (estAmbient * 0.05f);
  }
  #endif

  // ================= 1. NỬA TRÊN: ICON THỜI TIẾT + ĐỒNG HỒ SỐ LỚN =================
  // Mặt Trời cười có tia nắng + Cụm mây (Ảnh 1 góc trái trên)
  drawSunWithRays(11, 11);
  drawCloud(22, 11);

  // Đồng hồ kỹ thuật số lớn: HH:MM
  oled.setTextSize(2);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(44, 2);
  bool colonBlink = ((now / 500) % 2 == 0);
  char timeStr[8];
  snprintf(timeStr, sizeof(timeStr), "%02d%c%02d", curHour, colonBlink ? ':' : ' ', curMin);
  oled.print(timeStr);

  // Giây nhỏ kế bên
  oled.setTextSize(1);
  oled.setCursor(106, 3);
  oled.printf(":%02d", curSec);

  // Khung thứ trong tuần [SAT] giống hệt ảnh 1
  static const char* daysVN[] = {"CN", "T2", "T3", "T4", "T5", "T6", "SAT"};
  oled.drawRoundRect(44, 19, 28, 9, 2, SSD1306_WHITE);
  oled.setCursor(48, 20);
  oled.print(daysVN[weekday]);

  // Ngày tháng kế bên khung thứ
  int mday = 1 + (totalDays % 30);
  int mon  = 1 + ((totalDays / 30) % 12);
  oled.setCursor(76, 20);
  oled.printf("%02d/%02d", mday, mon);

  // Trạng thái Robot rảnh
  oled.setCursor(106, 20);
  oled.print("IDLE");

  // ================= 2. ĐƯỜNG PHÂN CÁCH NGANG =================
  oled.drawLine(0, 30, SCREEN_W - 1, 30, SSD1306_WHITE);

  // ================= 3. NỬA DƯỚI: TEMPERATURE TREND + ĐỘ ẨM + NHIỆT ĐỘ =================
  // Nhãn TREND và biểu đồ cột xu hướng
  oled.setCursor(2, 33);
  oled.print("TREND");
  drawTrendBars(2, 62);

  // Vạch phân cách đứng
  oled.drawLine(48, 32, 48, 62, SSD1306_WHITE);

  // Độ ẩm: icon giọt nước 💧 + 60%
  drawDroplet(52, 34);
  oled.setCursor(60, 34);
  oled.printf("%d%%", (int)humidity);

  // Nhiệt độ: icon nhiệt kế 🌡️ + 26.8°C
  drawThermometer(52, 45);
  oled.setCursor(60, 45);
  oled.printf("%.1f C", tempC);

  // Trạng thái cảm biến hồng ngoại
  oled.setCursor(52, 55);
  oled.print("IR: SAN SANG");

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

  // Auto presence: có tương tác gần đây → EYES, không thì tự động về CLOCK (ảnh 1)
  if (mode != DM_STREAM) {
    if (mode == DM_EYES && (now - lastPresenceMs > PRESENCE_TIMEOUT_MS)) {
      setMode(DM_CLOCK);
    }
  }

  // FPS giới hạn theo mode
  uint16_t targetPeriod;
  if (mode == DM_EYES)       targetPeriod = 1000 / DISPLAY_FPS_HZ;
  else if (mode == DM_CLOCK) targetPeriod = 1000 / 10; // 10 FPS cho clock và animation mượt
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