#include "display_controller.h"
#include "emoji_controller.h"
#include "roshi_logo.h"

Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);
RoboEyes<Adafruit_SSD1306> roboEyes(oled);
DisplayManager display;

// =====================================================================================
// TIME CONVERSION HELPERS
// =====================================================================================

time_t tmToEpoch(int y, int mo, int d, int h, int mi, int s) {
  if (mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
  int yFromEpoch = y - 1970;
  int leapDays = (yFromEpoch + 1) / 4 - (yFromEpoch + 1) / 100 + (yFromEpoch + 1) / 400;
  int daysUpToYear = yFromEpoch * 365 + leapDays;
  static const int cumDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  int daysUpToMonth = cumDays[mo - 1];
  if (mo > 2 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))) daysUpToMonth++;
  int totalDays = daysUpToYear + daysUpToMonth + (d - 1);
  return (time_t)totalDays * 86400L + (time_t)h * 3600L + (time_t)mi * 60L + (time_t)s;
}

void epochToTm(time_t epoch, int& y, int& mo, int& d, int& h, int& mi, int& s) {
  int secOfDay = (int)(epoch % 86400L);
  if (secOfDay < 0) secOfDay += 86400L;
  h = secOfDay / 3600;
  mi = (secOfDay % 3600) / 60;
  s = secOfDay % 60;

  long days = (long)(epoch / 86400L);
  if ((long)epoch < 0 && (long)epoch % 86400L != 0) days--;

  days += 719468;
  long era = (days >= 0 ? days : days - 146096) / 146097;
  long doe = days - era * 146097;
  long yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
  y = (int)(yoe) + (int)(era) * 400;
  long doy = doe - (365*yoe + yoe/4 - yoe/100);
  long mp = (5*doy + 2) / 153;
  d = (int)(doy - (153*mp + 2)/5 + 1);
  mo = (int)(mp < 10 ? mp + 3 : mp - 9);
  if (mo <= 2) y += 1;
}

// =====================================================================================
// DISPLAY MANAGER IMPLEMENTATION
// =====================================================================================

void DisplayManager::begin() {
  LOG_PRINTLN(F("\n========================================"));
  LOG_PRINTLN(F(" [OLED] KHOI TAO MAN HINH SSD1306..."));
  LOG_PRINTF(" [OLED] SDA=GPIO %d, SCL=GPIO %d, Clock=%d Hz\n",
             PIN_OLED_SDA, PIN_OLED_SCL, OLED_I2C_HZ);

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(OLED_I2C_HZ);
  Wire.setTimeOut(OLED_I2C_TIMEOUT_MS);

  uint8_t detectedAddr = 0;
  uint8_t activeSda = PIN_OLED_SDA;
  uint8_t activeScl = PIN_OLED_SCL;

  struct PinCandidate { uint8_t sda; uint8_t scl; const char* desc; };
  const PinCandidate candidates[] = {
    { PIN_OLED_SDA, PIN_OLED_SCL, "Chuan (SDA=8, SCL=9)" },
    { PIN_OLED_SCL, PIN_OLED_SDA, "Dao nguoc (SDA=9, SCL=8)" },
    { 1, 2, "Header (SDA=1, SCL=2)" },
    { 2, 1, "Header dao (SDA=2, SCL=1)" },
    { 41, 42, "IO41/42 (SDA=41, SCL=42)" },
    { 42, 41, "IO41/42 dao (SDA=42, SCL=41)" }
  };

  for (const auto& c : candidates) {
    Wire.end();
    pinMode(c.sda, INPUT_PULLUP);
    pinMode(c.scl, INPUT_PULLUP);
    delay(2);
    Wire.begin(c.sda, c.scl);
    Wire.setClock(100000);
    Wire.setTimeOut(30);

    Wire.beginTransmission(0x3C);
    if (Wire.endTransmission() == 0) {
      detectedAddr = 0x3C;
      activeSda = c.sda;
      activeScl = c.scl;
      LOG_PRINTF(" [OLED] => TIM THAY OLED SSD1306 [0x3C] tai %s!\n", c.desc);
      break;
    }
    Wire.beginTransmission(0x3D);
    if (Wire.endTransmission() == 0) {
      detectedAddr = 0x3D;
      activeSda = c.sda;
      activeScl = c.scl;
      LOG_PRINTF(" [OLED] => TIM THAY OLED SSD1306 [0x3D] tai %s!\n", c.desc);
      break;
    }
  }

  if (detectedAddr != 0) {
    Wire.setClock(OLED_I2C_HZ);
    Wire.setTimeOut(OLED_I2C_TIMEOUT_MS);
  } else {
    Wire.end();
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    Wire.setClock(100000);
    LOG_PRINTLN(F(" [OLED] !!! CANH BAO: KHONG TIM THAY OLED SSD1306 TREN CAC CHAN THU NGHIEM!"));
    LOG_PRINTLN(F(" [OLED] HUONG DAN NOI DAY CHUAN:"));
    LOG_PRINTLN(F("   1. VCC cua OLED    -> Chan 3V3 (3.3V) tren bo ESP32-S3"));
    LOG_PRINTLN(F("   2. GND cua OLED    -> Chan GND tren bo ESP32-S3"));
    LOG_PRINTLN(F("   3. SDA cua OLED    -> Chan GPIO 8 (IO8)"));
    LOG_PRINTLN(F("   4. SCL/SCK cua OLED -> Chan GPIO 9 (IO9)"));
  }

  uint8_t targetAddr = (detectedAddr != 0) ? detectedAddr : OLED_ADDR;
  LOG_PRINTF(" [OLED] Bat dau khoi tao Adafruit SSD1306 tai 0x%02X...\n", targetAddr);

  bool oledOk = oled.begin(SSD1306_SWITCHCAPVCC, targetAddr, false, false);
  LOG_PRINTF(" [OLED] Ket qua oled.begin -> %s\n", oledOk ? "OK (HOAT DONG TOT)" : "FAIL (THAT BAI)");

  if (oledOk) {
    drawBootLogo("ROSHI", 1200);
  }

  roboEyes.begin(SCREEN_W, SCREEN_H, 30);
  roboEyes.setAutoblinker(1, 3, 2);
  roboEyes.setIdleMode(1, 2, 2);
  roboEyes.setMood(MOOD_DEFAULT);
  roboEyes.setWidth(36, 36);
  roboEyes.setHeight(36, 36);
  roboEyes.open();
  roboEyes.eyeLheightCurrent = 36;
  roboEyes.eyeRheightCurrent = 36;

  bootMs = millis();
  bootProtectionUntilMs = millis() + 2500; // Bao ve man hinh khoi dong cua robot trong 2.5s dau
  fpsTimer = millis();
  lastPresence = millis();
  initClock();
  mode = DISP_EYES;

  if (oledOk) {
    drawEyes();
  }
  LOG_PRINTLN(F("========================================\n"));
}

void DisplayManager::drawBootLogo(const char* name, int durationMs) {
  oled.cp437(true);
  unsigned long start = millis();
  int frame = 0;

  while (millis() - start < (unsigned long)durationMs) {
    unsigned long elapsed = millis() - start;
    float progress = (float)elapsed / (float)durationMs;
    if (progress > 1.0f) progress = 1.0f;
    int percent = (int)(progress * 100.0f);

    oled.clearDisplay();

    // 1. Cyber Corner HUD Brackets
    oled.drawFastHLine(0, 0, 7, SSD1306_WHITE);
    oled.drawFastVLine(0, 0, 7, SSD1306_WHITE);
    oled.drawFastHLine(SCREEN_W - 7, 0, 7, SSD1306_WHITE);
    oled.drawFastVLine(SCREEN_W - 1, 0, 7, SSD1306_WHITE);
    oled.drawFastHLine(0, SCREEN_H - 1, 7, SSD1306_WHITE);
    oled.drawFastVLine(0, SCREEN_H - 7, 7, SSD1306_WHITE);
    oled.drawFastHLine(SCREEN_W - 7, SCREEN_H - 1, 7, SSD1306_WHITE);
    oled.drawFastVLine(SCREEN_W - 1, SCREEN_H - 7, 7, SSD1306_WHITE);

    // 2. Roshi Mascot Logo 32x32 với Animated Reticle Box
    int logoX = 5;
    int logoY = 4;
    oled.drawBitmap(logoX, logoY, roshi_logo_32x32, 32, 32, SSD1306_WHITE);

    oled.drawRoundRect(logoX - 2, logoY - 2, 36, 36, 2, SSD1306_WHITE);
    if ((frame / 3) % 2 == 0) {
      oled.drawPixel(logoX - 1, logoY - 1, SSD1306_WHITE);
      oled.drawPixel(logoX + 33, logoY - 1, SSD1306_WHITE);
      oled.drawPixel(logoX - 1, logoY + 33, SSD1306_WHITE);
      oled.drawPixel(logoX + 33, logoY + 33, SSD1306_WHITE);
    }

    // 3. Header Text: ROSHI & Radar Pulse Animation
    oled.setTextColor(SSD1306_WHITE);
    oled.setTextSize(2);
    oled.setCursor(44, 3);
    oled.print(name ? name : "ROSHI");

    // Radar pulse animation ở góc phải trên (x=116, y=10)
    int pulseR = (frame % 8);
    if (pulseR > 1 && pulseR < 7) {
      oled.drawCircle(116, 9, pulseR, SSD1306_WHITE);
    }
    oled.fillCircle(116, 9, 1, SSD1306_WHITE);

    // 4. Phụ đề & Dynamic Status System Log
    oled.setTextSize(1);
    oled.setCursor(45, 19);
    oled.print(F("CYBER ROBOT"));

    // Dynamic Module Status thay đổi mượt mà theo tiến trình %
    oled.setCursor(45, 29);
    if (percent < 20) {
      oled.print(F("> KHOI DONG RTOS"));
    } else if (percent < 45) {
      oled.print(F("> NAP USB PROTOCOL"));
    } else if (percent < 70) {
      oled.print(F("> KET NOI ROBOT"));
    } else if (percent < 90) {
      oled.print(F("> ETS2 TELEMETRY"));
    } else {
      oled.print(F("> SAN SANG CHOI!"));
    }

    // 5. Loading Bar Cyberpunk với Chevron Animation và %
    oled.setCursor(6, 41);
    oled.print(F("SYS BOOT:"));
    oled.setCursor(84, 41);
    if (percent < 10) oled.print(F("  "));
    else if (percent < 100) oled.print(F(" "));
    oled.print(percent);
    oled.print(F("%"));

    // Animated dots (...)
    int dots = (frame / 4) % 4;
    for (int d = 0; d < dots; d++) {
      oled.drawPixel(64 + d * 4, 45, SSD1306_WHITE);
    }

    // Khung thanh tiến trình
    int barX = 5;
    int barY = 51;
    int barW = 118;
    int barH = 10;
    oled.drawRoundRect(barX, barY, barW, barH, 2, SSD1306_WHITE);

    // Ruột thanh tiến trình: Animated Chevron Pattern (///) cuộn mượt sang phải
    int innerW = barW - 4;
    int fillW = (int)(progress * innerW);
    if (fillW > 0) {
      int animShift = (frame * 2) % 6;
      for (int x = 0; x < fillW; x++) {
        bool stripe = ((x + animShift) % 6 < 3);
        for (int y = 0; y < (barH - 4); y++) {
          if (stripe || percent >= 98) {
            oled.drawPixel(barX + 2 + x, barY + 2 + y, SSD1306_WHITE);
          }
        }
      }
    }


    // Hiệu ứng chớp sáng hoàn tất ở 250ms cuối
    if (percent >= 100 && (frame % 2 == 0)) {
      oled.invertDisplay(true);
    } else {
      oled.invertDisplay(false);
    }

    oled.display();
    frame++;
    delay(30);
  }

  oled.invertDisplay(false);
  oled.clearDisplay();
  oled.display();
}

bool DisplayManager::isLeap(int y) {
  return ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
}

int DisplayManager::daysInMonth(int y, int m) {
  const int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (m == 2 && isLeap(y)) return 29;
  if (m >= 1 && m <= 12) return dim[m - 1];
  return 30;
}

void DisplayManager::initClock() {
  setenv("TZ", "ICT-7", 1);
  tzset();

  const char* ds = __DATE__;
  const char* ts = __TIME__;
  char mStr[4] = {0};
  int yr = 2026, mo = 10, md = 10, h = 6, mn = 0, sc = 0;
  if (sscanf(ds, "%3s %d %d", mStr, &md, &yr) == 3) {
    static const char PROGMEM months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char* p = strstr_P(months, mStr);
    if (p) mo = ((p - months) / 3) + 1;
  }
  sscanf(ts, "%d:%d:%d", &h, &mn, &sc);

  time_t compile_local = tmToEpoch(yr, mo, md, h, mn, sc);
  time_t compile_utc   = compile_local - (7 * 3600);

  time_t cur = time(nullptr);
  if (cur < compile_utc || cur < 1735689600) {
    struct timeval tv = { .tv_sec = compile_utc, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    LOG_PRINTF(" [CLOCK] Khoi tao RTC tu gio compile (%s %s UTC+7): ", ds, ts);
  } else {
    LOG_PRINTF(" [CLOCK] Giu nguyen gio RTC dang chay: ");
  }

  updateClock();
  LOG_PRINTF("%02d:%02d:%02d - Ngay %02d/%02d/%04d (Thu %d)\n",
             clockHour, clockMin, clockSec, clockDay, clockMonth, clockYear, clockWday);
}

void DisplayManager::setTime(int h, int m, int s) {
  time_t nowUtc = time(nullptr);
  struct tm ti;
  localtime_r(&nowUtc, &ti);
  time_t localEpoch = tmToEpoch(ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
                                constrain(h, 0, 23),
                                constrain(m, 0, 59),
                                constrain(s, 0, 59));
  time_t newUtc = localEpoch - (7 * 3600);
  struct timeval tv = { .tv_sec = newUtc, .tv_usec = 0 };
  settimeofday(&tv, NULL);
  updateClock();
  LOG_PRINTF(" [CLOCK] Da cap nhat gio moi: %02d:%02d:%02d\n", clockHour, clockMin, clockSec);
}

void DisplayManager::setDate(int d, int m, int y) {
  time_t nowUtc = time(nullptr);
  struct tm ti;
  localtime_r(&nowUtc, &ti);
  int newY = constrain(y, 2020, 2099);
  int newM = constrain(m, 1, 12);
  int newD = constrain(d, 1, daysInMonth(newY, newM));
  time_t localEpoch = tmToEpoch(newY, newM, newD, ti.tm_hour, ti.tm_min, ti.tm_sec);
  time_t newUtc = localEpoch - (7 * 3600);
  struct timeval tv = { .tv_sec = newUtc, .tv_usec = 0 };
  settimeofday(&tv, NULL);
  updateClock();
  LOG_PRINTF(" [CLOCK] Da cap nhat ngay moi: %02d/%02d/%04d\n", clockDay, clockMonth, clockYear);
}

void DisplayManager::updateClock() {
  time_t nowUtc = time(nullptr);
  struct tm ti;
  localtime_r(&nowUtc, &ti);
  clockYear  = ti.tm_year + 1900;
  clockMonth = ti.tm_mon + 1;
  clockDay   = ti.tm_mday;
  clockHour  = ti.tm_hour;
  clockMin   = ti.tm_min;
  clockSec   = ti.tm_sec;
  clockWday  = ti.tm_wday;
}

void DisplayManager::notifyPresence() {
  lastPresence = millis();
  if (mode == DISP_CLOCK) {
    mode = DISP_EYES;
    roboEyes.setMood(MOOD_DEFAULT);
  }
}

void DisplayManager::drawSun(int16_t cx, int16_t cy) {
  oled.fillCircle(cx, cy, 4, SSD1306_WHITE);
  oled.drawFastVLine(cx, cy - 7, 2, SSD1306_WHITE);
  oled.drawFastVLine(cx, cy + 6, 2, SSD1306_WHITE);
  oled.drawFastHLine(cx - 7, cy, 2, SSD1306_WHITE);
  oled.drawFastHLine(cx + 6, cy, 2, SSD1306_WHITE);
  oled.drawPixel(cx - 4, cy - 4, SSD1306_WHITE);
  oled.drawPixel(cx + 4, cy - 4, SSD1306_WHITE);
  oled.drawPixel(cx - 4, cy + 4, SSD1306_WHITE);
  oled.drawPixel(cx + 4, cy + 4, SSD1306_WHITE);
}

void DisplayManager::drawMoon(int16_t cx, int16_t cy) {
  oled.fillCircle(cx, cy, 5, SSD1306_WHITE);
  oled.fillCircle(cx + 2, cy - 2, 4, SSD1306_BLACK);
}

void DisplayManager::drawStars(int16_t x, int16_t y) {
  bool blink = (millis() / 500) & 1;
  oled.drawPixel(x, y, SSD1306_WHITE);
  if (blink) {
    oled.drawPixel(x - 1, y, SSD1306_WHITE);
    oled.drawPixel(x + 1, y, SSD1306_WHITE);
    oled.drawPixel(x, y - 1, SSD1306_WHITE);
    oled.drawPixel(x, y + 1, SSD1306_WHITE);
  }
  oled.drawPixel(x + 5, y + 5, blink ? SSD1306_WHITE : SSD1306_BLACK);
}

void DisplayManager::drawThermometer(int16_t x, int16_t y) {
  oled.drawFastVLine(x + 1, y, 5, SSD1306_WHITE);
  oled.drawPixel(x, y + 1, SSD1306_WHITE);
  oled.drawPixel(x + 2, y + 1, SSD1306_WHITE);
  oled.fillCircle(x + 1, y + 6, 2, SSD1306_WHITE);
}

void DisplayManager::drawClock() {
  oled.clearDisplay();

  bool isNight = (clockHour >= 18 || clockHour < 6);

  static const char PROGMEM DAYS[] = "CN\0T2\0T3\0T4\0T5\0T6\0T7";
  char dayBuf[4];
  strncpy_P(dayBuf, &DAYS[clockWday * 3], 3);
  dayBuf[3] = '\0';

  oled.drawRoundRect(2, 1, 20, 10, 2, SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(6, 2);
  oled.print(dayBuf);

  oled.setCursor(28, 2);
  oled.printf("%02d/%02d/%04d", clockDay, clockMonth, clockYear);

  if (!isNight) {
    drawSun(116, 6);
  } else {
    drawMoon(118, 5);
    drawStars(104, 3);
  }

  oled.drawFastHLine(0, 13, SCREEN_W, SSD1306_WHITE);

  oled.setTextSize(3);
  oled.setCursor(12, 18);
  bool blink = (millis() / 500) & 1;
  oled.printf("%02d%c%02d", clockHour, blink ? ':' : ' ', clockMin);

  oled.setTextSize(1);
  oled.setCursor(105, 19);
  oled.printf(":%02d", clockSec);

  oled.drawRoundRect(103, 31, 22, 11, 2, SSD1306_WHITE);
  oled.setCursor(106, 33);
  oled.print(isNight ? F("Zz") : F("OK"));

  oled.drawFastHLine(0, 46, SCREEN_W, SSD1306_WHITE);

  // Cache nhiet do ESP32 noi bo: cap nhat 1 lan/giay thay vi moi frame (30x/giay)
  static unsigned long lastTempReadMs = 0;
  unsigned long nowClk = millis();
  if (nowClk - lastTempReadMs >= 1000) {
    lastTempReadMs = nowClk;
    float rawT = temperatureRead();
    if (rawT >= 20.0f && rawT <= 85.0f) {
      float estimatedRoomT = rawT - 28.5f;
      estimatedRoomT = constrain(estimatedRoomT, 22.0f, 38.0f);
      tempC = tempC * 0.92f + estimatedRoomT * 0.08f;
    }
  }

  drawThermometer(4, 50);
  oled.setTextSize(1);
  oled.setCursor(14, 52);
  oled.printf("%.1f%cC", tempC, (char)247);

  oled.setCursor(56, 52);
  oled.print(isNight ? F("| BAN DEM") : F("| BAN NGAY"));

  oled.display();
}

void DisplayManager::drawEyes() {
  roboEyes.drawEyes();
}

void DisplayManager::drawVisual() {
  oled.clearDisplay();
  oled.drawRoundRect(14, 14, 44, 36, 8, SSD1306_WHITE);
  oled.drawRoundRect(70, 14, 44, 36, 8, SSD1306_WHITE);
  float phase = millis() * 0.005f;
  for (int x = 18; x < 54; x++) {
    int y = 32 + (int)(sinf((x - 18) * 0.35f + phase) * 8.0f);
    oled.drawPixel(x, constrain(y, 18, 46), SSD1306_WHITE);
  }
  for (int x = 74; x < 110; x++) {
    int y = 32 + (int)(sinf((x - 74) * 0.35f + phase) * 8.0f);
    oled.drawPixel(x, constrain(y, 18, 46), SSD1306_WHITE);
  }
  oled.display();
}

void DisplayManager::drawStream() {
  if (streamNew) {
    oled.clearDisplay();
    oled.drawBitmap(0, 0, streamBuf[streamReadIdx], SCREEN_W, SCREEN_H, SSD1306_WHITE);
    oled.display();
    streamNew = false;
  }
}

void DisplayManager::setStreamFrame(const uint8_t* bmp) {
  if (!bmp) return;
  memcpy(streamBuf[streamWriteIdx], bmp, 1024);
  uint8_t newRead = streamWriteIdx;
  streamWriteIdx = streamReadIdx;
  streamReadIdx  = newRead;
  streamNew = true;
  streamLastMs = millis();
}

void DisplayManager::triggerEts2Splash() {
  ets2SplashUntilMs = millis() + 3000;
}

// Scania New Gen Container Bitmap (112x32, 448 bytes - Chuan xe dau keo Scania keo thung container bac)
static const unsigned char scania_container_112x32[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF, 0xFF, 0xFF, 0xC0, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xC0, 0x00, 0x00, 0x00, 0x3C, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
  0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0,
  0x00, 0x00, 0x01, 0xFF, 0xFF, 0x21, 0x0A, 0x03, 0xFC, 0x0F, 0xFF, 0xF8, 0x1F, 0xC0,
  0x00, 0x01, 0xFF, 0xFF, 0x84, 0x21, 0x0A, 0x03, 0xFC, 0x0F, 0xFF, 0xFF, 0xFF, 0xFC,
  0x01, 0xFF, 0xFE, 0x10, 0x84, 0x21, 0x8A, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFC,
  0x3F, 0xFE, 0x42, 0x10, 0x86, 0x31, 0x0A, 0x1F, 0xFF, 0xFF, 0xF0, 0x00, 0x00, 0xBC,
  0x3F, 0x08, 0x42, 0x10, 0xC4, 0x21, 0x0A, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFC,
  0x31, 0x08, 0x43, 0x18, 0x84, 0x21, 0x0A, 0x1F, 0x80, 0x00, 0x18, 0x00, 0x60, 0xBC,
  0x31, 0x0C, 0x62, 0x10, 0x84, 0x21, 0x0A, 0x1F, 0x00, 0x00, 0x60, 0x01, 0x81, 0x3C,
  0x31, 0x88, 0x42, 0x10, 0x84, 0x21, 0x0A, 0x1E, 0x80, 0x01, 0x80, 0x06, 0x01, 0x3C,
  0x31, 0x08, 0x42, 0x10, 0x84, 0x21, 0x0A, 0x1E, 0x80, 0x06, 0x00, 0x18, 0x01, 0x3C,
  0x31, 0x08, 0x42, 0x10, 0x84, 0x21, 0x0A, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
  0x31, 0x08, 0x42, 0x10, 0x84, 0x21, 0x0A, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
  0x31, 0x8C, 0x63, 0x18, 0xC6, 0x31, 0x8A, 0x00, 0xFF, 0xDD, 0xDD, 0xDD, 0xFF, 0x00,
  0x31, 0x08, 0x42, 0x10, 0x84, 0x21, 0x0A, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
  0x31, 0x08, 0x42, 0x10, 0x84, 0x21, 0x0A, 0x00, 0x40, 0x00, 0x00, 0x00, 0x02, 0x00,
  0x31, 0x08, 0x42, 0x10, 0x84, 0x21, 0x0A, 0x00, 0x3F, 0xFF, 0xFF, 0xFF, 0xFC, 0x00,
  0x30, 0x00, 0x00, 0x1F, 0xFF, 0xFF, 0xFA, 0x00, 0x11, 0x11, 0x11, 0x17, 0xF8, 0x00,
  0x3F, 0xFF, 0xFF, 0xEF, 0xFF, 0xFF, 0xFA, 0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xFC, 0x00,
  0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x8A, 0xE0, 0x04, 0x44, 0x44, 0x67, 0xF2, 0x00,
  0x07, 0xC3, 0xF8, 0x60, 0xA4, 0x00, 0x0B, 0x18, 0x07, 0xFF, 0xFF, 0xFC, 0x09, 0x00,
  0x00, 0x99, 0x13, 0x3F, 0xA7, 0xFF, 0x82, 0x08, 0x03, 0x11, 0x11, 0x99, 0xE4, 0x80,
  0x07, 0xBD, 0xF7, 0xA0, 0xA4, 0x00, 0x04, 0x05, 0xF9, 0x7F, 0xFF, 0xF3, 0xF3, 0xC0,
  0x00, 0xBD, 0x17, 0xA0, 0xFC, 0x00, 0x05, 0xF7, 0xBF, 0xFF, 0xFF, 0xF7, 0x7B, 0xC0,
  0x00, 0x99, 0x13, 0x20, 0x00, 0x00, 0x04, 0x07, 0xFF, 0xFF, 0xFF, 0xF3, 0xF3, 0xC0,
  0x00, 0x42, 0x08, 0x40, 0x00, 0x00, 0x02, 0x08, 0x00, 0x01, 0x52, 0x89, 0xE4, 0x00,
  0x00, 0x3C, 0x07, 0x80, 0x00, 0x00, 0x03, 0x18, 0x00, 0x01, 0xFF, 0x84, 0x08, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x00, 0x00, 0x00, 0x03, 0xF0, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
};

void DisplayManager::drawEts2SplashScreen() {
  unsigned long now = millis();

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // 1. Header: SCANIA TRUCK - ETS2 (y=0..9)
  oled.setTextSize(1);
  oled.setCursor(12, 1);
  oled.print(F("ETS2 - LOADING..."));
  oled.drawFastHLine(0, 10, 128, SSD1306_WHITE);

  // 3. Tinh toan tien do % loading muot ma
  int pct = 0;
  if (gameTelem.loadPct > 0) {
    pct = constrain(gameTelem.loadPct, 1, 100);
  } else if (ets2SplashUntilMs > now) {
    unsigned long elapsed = 3000 - (ets2SplashUntilMs - now);
    pct = constrain((int)(elapsed * 100 / 3000), 1, 99);
  } else {
    pct = 50 + (int)((now / 90) % 49);
  }

  // 2. Ve xe Scania - scale xuong 96x28 de du cho text va progress bar
  // Bitmap goc 112x32 -> scale de ve trong vung y=12..39 (28px)
  // Su dung draw 1:1 nhung dich ve trai de con cho cho text phai
  oled.drawBitmap(8, 12, scania_container_112x32, 112, 32, SSD1306_WHITE);

  // 4. Dong chu trang thai loading (y=45) - dung line moi sau bitmap
  oled.drawFastHLine(0, 44, 128, SSD1306_WHITE);
  oled.setCursor(4, 46);
  if (pct < 35) {
    oled.print(F("KHOI DONG SCS SDK"));
  } else if (pct < 85) {
    oled.print(F("NAP TELEMETRY ETS2"));
  } else {
    oled.print(F("SAN SANG LAI XE!"));
  }

  // % hien thi ben phai cung hang
  char pctBuf[5];
  snprintf(pctBuf, sizeof(pctBuf), "%3d%%", pct);
  oled.setCursor(98, 46);
  oled.print(pctBuf);

  // 5. Thanh tien trinh (y=55..63) - co khoang cach ro rang voi text tren
  int barX = 2;
  int barY = 56;
  int barW = 124;
  int barH = 7;
  oled.drawRoundRect(barX, barY, barW, barH, 2, SSD1306_WHITE);

  int innerW = barW - 4;
  int fillW = map(pct, 0, 100, 0, innerW);

  if (fillW > 0) {
    oled.fillRect(barX + 2, barY + 2, fillW, barH - 4, SSD1306_WHITE);

    // Hieu ung tia sang quet qua thanh loading
    int sheen = (int)((now / 20) % (unsigned long)(fillW + 14)) - 14;
    if (sheen >= 0 && sheen < fillW) {
      oled.drawFastVLine(barX + 2 + sheen, barY + 2, barH - 4, SSD1306_BLACK);
      if (sheen + 1 < fillW) {
        oled.drawFastVLine(barX + 3 + sheen, barY + 2, barH - 4, SSD1306_BLACK);
      }
    }
  }

  oled.display();
}

void DisplayManager::triggerEvent(const char* text, unsigned long durationMs) {
  if (!text || strlen(text) == 0) return;
  strncpy(gameTelem.eventText, text, sizeof(gameTelem.eventText) - 1);
  gameTelem.eventText[sizeof(gameTelem.eventText) - 1] = '\0';
  gameTelem.eventUntilMs = millis() + durationMs;
  notifyPresence();
}

void DisplayManager::updateGameTelemetry(const Ets2TelemetryData& data) {
  bool wasConnected = gameTelem.connected;
  unsigned long curEvtUntil = gameTelem.eventUntilMs;
  char curEvtText[32];
  strncpy(curEvtText, gameTelem.eventText, sizeof(curEvtText));

  gameTelem = data;
  gameLastPacketMs = millis();
  notifyPresence();

  // Giu lai toast event dang hien thi neu con thoi gian
  if (curEvtUntil > millis() && strlen(data.eventText) == 0) {
    gameTelem.eventUntilMs = curEvtUntil;
    strncpy(gameTelem.eventText, curEvtText, sizeof(gameTelem.eventText));
  } else if (strlen(data.eventText) > 0) {
    if (data.eventUntilMs > millis()) {
      gameTelem.eventUntilMs = data.eventUntilMs;
    } else {
      gameTelem.eventUntilMs = millis() + 2500;
    }
  }

  // Kiem tra bao ve man hinh khoi dong: Trong 2.5s dau khi bat may, chi cap nhat du lieu, khong cuop man hinh boot
  if (millis() < bootProtectionUntilMs) {
    return;
  }

  // CHỈ chuyển sang màn hình Game khi game ĐÃ KẾT NỐI (data.connected == true)
  if (data.connected) {
    if (!wasConnected || mode != DISP_GAME) {
      mode = DISP_GAME;
      // Chi hien thi man hinh Loading xe Scania neu game thuc su dang loading/intro
      if (data.loading || (data.loadPct > 0 && data.loadPct < 100)) {
        triggerEts2Splash();
        LOG_PRINTLN(F("[DISPLAY] Game ETS2 dang loading -> Hien thi Loading Splash Scania..."));
      } else {
        ets2SplashUntilMs = 0; // Vao thang Dashboard lai xe, khong de splash
        LOG_PRINTLN(F("[DISPLAY] Game ETS2 da san sang lai xe -> Vao thang Cockpit Dashboard!"));
      }
    }
    gameModeLocked = true;
  } else {
    // Game chua ket noi hoac da thoat -> ve ngay mat robot RoboEyes
    if (mode == DISP_GAME) {
      mode = DISP_EYES;
      gameModeLocked = false;
      gameTelem.loading = false;
      gameTelem.connected = false;
      ets2SplashUntilMs = 0;
      LOG_PRINTLN(F("[DISPLAY] Game ETS2 chua ket noi hoac da thoat -> Tro ve Mat RoboEyes"));
    }
  }
}

void DisplayManager::drawEventToast() {
  if (strlen(gameTelem.eventText) == 0) return;

  // Khung Toast noi bat o giua man hinh
  oled.fillRoundRect(4, 13, 120, 34, 3, SSD1306_BLACK);
  oled.drawRoundRect(4, 13, 120, 34, 3, SSD1306_WHITE);
  oled.drawRoundRect(6, 15, 116, 30, 2, SSD1306_WHITE);

  const char* txt = gameTelem.eventText;
  const char* colon = strchr(txt, ':');

  if (colon != nullptr) {
    char title[20];
    int titleLen = colon - txt;
    if (titleLen > 19) titleLen = 19;
    strncpy(title, txt, titleLen);
    title[titleLen] = '\0';

    const char* val = colon + 1;
    while (*val == ' ') val++; // trim leading space

    // Tieu de tren nen trang chu den noi bat
    oled.fillRect(7, 16, 114, 11, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setTextSize(1);
    int tx = 64 - (strlen(title) * 6 / 2);
    if (tx < 10) tx = 10;
    oled.setCursor(tx, 18);
    oled.print(title);

    // Gia tri hoac so tien ben duoi
    oled.setTextColor(SSD1306_WHITE);
    int valLen = strlen(val);
    if (valLen <= 9) {
      oled.setTextSize(2);
      int vx = 64 - (valLen * 12 / 2);
      if (vx < 10) vx = 10;
      oled.setCursor(vx, 30);
      oled.print(val);
    } else {
      oled.setTextSize(1);
      int vx = 64 - (valLen * 6 / 2);
      if (vx < 10) vx = 10;
      oled.setCursor(vx, 32);
      oled.print(val);
    }
  } else {
    // Truong hop khong co dau hai cham (vd: "VI PHAM", "VA CHAM", "-550 EUR", "CANH BAO")
    oled.fillRect(7, 16, 114, 11, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setTextSize(1);
    const char* defTitle = (strstr(txt, "EUR") != nullptr || strstr(txt, "PHAT") != nullptr || strstr(txt, "VI PHAM") != nullptr)
                           ? "VI PHAM" : "CANH BAO";
    int tx = 64 - (strlen(defTitle) * 6 / 2);
    oled.setCursor(tx, 18);
    oled.print(defTitle);

    oled.setTextColor(SSD1306_WHITE);
    int txtLen = strlen(txt);
    if (txtLen <= 9) {
      oled.setTextSize(2);
      int vx = 64 - (txtLen * 12 / 2);
      if (vx < 10) vx = 10;
      oled.setCursor(vx, 30);
      oled.print(txt);
    } else {
      oled.setTextSize(1);
      int vx = 64 - (txtLen * 6 / 2);
      if (vx < 10) vx = 10;
      oled.setCursor(vx, 32);
      oled.print(txt);
    }
  }
}

void DisplayManager::drawGameDashboard() {
  unsigned long now = millis();

  // 1. Uu tien kiem tra Timeout va ket noi truoc tien:
  // Timeout 3s de tranh nhap nhay khi goi telemetry den cham hon 1 chu ky
  bool isTimedOut = (gameLastPacketMs == 0 || (now - gameLastPacketMs > 3000));
  if (isTimedOut || !gameTelem.connected) {
    mode = DISP_EYES;
    gameModeLocked = false;
    gameTelem.loading = false;
    gameTelem.connected = false;
    ets2SplashUntilMs = 0;
    return;
  }

  // 2. Neu game dang ket noi nhung dang o man hinh loading / intro:
  if (gameTelem.loading || now < ets2SplashUntilMs) {
    drawEts2SplashScreen();
    return;
  }

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // =========================================================================
  // EURO TRUCK COCKPIT DASHBOARD (128 x 64 OLED) - THIET KE CAN DOI, RO NET
  // =========================================================================

  // Bitmap mini icons chuan taplo o to ISO cho cac den chuc nang Header
  static const uint8_t icon_high_beam[]  PROGMEM = { 0x18, 0xFC, 0x1C, 0xFC, 0x1C, 0xFC, 0x18 }; // Den pha (3 tia ngang)
  static const uint8_t icon_low_beam[]   PROGMEM = { 0x18, 0x9C, 0x5C, 0x3C, 0x1C, 0x38, 0x18 }; // Den cos (3 tia cheo)
  static const uint8_t icon_park_light[] PROGMEM = { 0x24, 0x66, 0x7E, 0x3C, 0x7E, 0x66, 0x24 }; // Den demi
  static const uint8_t icon_wiper[]      PROGMEM = { 0x7E, 0x83, 0x85, 0x89, 0x91, 0x7E, 0x10 }; // Gat mua kinh lai
  static const uint8_t icon_horn[]       PROGMEM = { 0x32, 0x74, 0xF9, 0xFA, 0xF9, 0x74, 0x32 }; // Ken / coi loa
  static const uint8_t icon_engine[]     PROGMEM = { 0x30, 0x7E, 0xDB, 0xC3, 0x7E, 0x24, 0x66 }; // Khoi dong co Check Engine chuan ISO
  static const uint8_t icon_hazard[]     PROGMEM = { 0x18, 0x3C, 0x5A, 0x5A, 0x99, 0x81, 0xFF }; // Tam giac den khan cap Hazard Flasher ISO

  // =========================================================================
  // HEADER ICON LAYOUT (y=0..10) - Vi tri co dinh tranh chong cheo:
  // x=0..7   : Xi-nhan trai / Hazard trai
  // x=9..16  : Den pha / cos / demi  (slot den)
  // x=18..25 : Gat mua (wiper icon)
  // x=27     : Cap do gat (1/2/3 digit)
  // x=33..40 : Hazard icon TAM GIAC hoac Horn icon
  // x=42..48 : Horn icon (khi co ca hazard luon)
  // x=46..87 : Dong ho gio cabin (trung tam)
  // x=80..87 : Engine icon
  // x=89..97 : Retarder R1..R5 hoac BCN
  // x=105..116: Cruise CC
  // x=119..127: Xi-nhan phai / Hazard phai
  // =========================================================================

  bool isHazard   = gameTelem.hazard || (gameTelem.blinkerL && gameTelem.blinkerR);
  bool blinkPhase = ((now / 350) % 2 == 0);

  // Xi-nhan trai (Left Blinker) hoac Den khan cap (Hazard):
  if (gameTelem.blinkerL || isHazard) {
    if (blinkPhase) {
      oled.fillTriangle(7, 1, 1, 5, 7, 9, SSD1306_WHITE);
    } else {
      oled.drawTriangle(7, 1, 1, 5, 7, 9, SSD1306_WHITE);
    }
  }

  // Den pha / cos / demi - slot co dinh x=9
  if (gameTelem.lightsBeamHigh) {
    oled.drawBitmap(9, 2, icon_high_beam, 8, 7, SSD1306_WHITE);
  } else if (gameTelem.lightsBeamLow) {
    oled.drawBitmap(9, 2, icon_low_beam, 8, 7, SSD1306_WHITE);
  } else if (gameTelem.lightsParking) {
    oled.drawBitmap(9, 2, icon_park_light, 8, 7, SSD1306_WHITE);
  }

  // Gat nuoc - slot co dinh x=18; cap do gat x=27
  if (gameTelem.wipers || gameTelem.wiperLevel > 0) {
    oled.drawBitmap(18, 2, icon_wiper, 8, 7, SSD1306_WHITE);
    int wLvl = (gameTelem.wiperLevel > 0) ? gameTelem.wiperLevel : 1;
    oled.setTextSize(1);
    oled.setCursor(27, 2);
    oled.print(wLvl);
  }

  // Hazard tam giac - slot x=33; Horn - slot x=42 (neu ca 2 cung active)
  if (isHazard) {
    if (blinkPhase) {
      oled.drawBitmap(33, 2, icon_hazard, 8, 7, SSD1306_WHITE);
    }
    if (gameTelem.horn) {
      oled.drawBitmap(42, 2, icon_horn, 8, 7, SSD1306_WHITE);
    }
  } else if (gameTelem.horn) {
    oled.drawBitmap(33, 2, icon_horn, 8, 7, SSD1306_WHITE);
  }

  // Dong ho gio cabin - trung tam an toan x=46..87 (xoa truoc de tranh ghost)
  // Uu tien: gio trong game (gameHour/gameMin), fallback dong ho that khi chua co
  int dispH = (gameTelem.gameHour >= 0) ? gameTelem.gameHour : clockHour;
  int dispM = (gameTelem.gameMin  >= 0) ? gameTelem.gameMin  : clockMin;
  bool blinkColon = ((now / 500) % 2 == 0);
  oled.setTextSize(1);
  oled.fillRect(46, 0, 34, 11, SSD1306_BLACK);
  oled.setCursor(47, 2);
  oled.printf("%02d%c%02d", dispH, blinkColon ? ':' : ' ', dispM);

  // Engine / Check Engine - slot x=81
  if (gameTelem.engineOn) {
    bool engWarn = (gameTelem.truckWearPct >= 15);
    if (!engWarn || ((now / 300) % 2 == 0)) {
      oled.drawBitmap(81, 2, icon_engine, 8, 7, SSD1306_WHITE);
    }
  } else {
    if ((now / 400) % 2 == 0) {
      oled.drawBitmap(81, 2, icon_engine, 8, 7, SSD1306_WHITE);
    }
  }

  // Retarder / Beacon - slot x=91
  if (gameTelem.retarder > 0) {
    oled.setCursor(91, 2);
    oled.printf("R%d", gameTelem.retarder);
  } else if (gameTelem.beacon && ((now / 300) % 2 == 0)) {
    oled.setCursor(91, 2);
    oled.print(F("BCN"));
  }

  // Cruise Control badge - slot x=106
  if (gameTelem.cruiseOn) {
    oled.setCursor(106, 2);
    oled.print(F("CC"));
  }

  // Xi-nhan phai - slot x=120
  if (gameTelem.blinkerR || isHazard) {
    if (blinkPhase) {
      oled.fillTriangle(120, 1, 126, 5, 120, 9, SSD1306_WHITE);
    } else {
      oled.drawTriangle(120, 1, 126, 5, 120, 9, SSD1306_WHITE);
    }
  }
  oled.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  // 2. VUNG TRUNG TAM (y = 12..44) - 3 COT RO RANG, CAN DOI
  // --- COT TRAI (x = 0..30): Bien bao gioi han toc do & Ga tu dong ---
  int cx = 15, cy = 23, r = 10;
  bool speeding = (gameTelem.speedLimit > 0 && gameTelem.speed > gameTelem.speedLimit);
  if (gameTelem.speedLimit > 0) {
    if (speeding && ((now / 220) % 2 == 0)) {
      // Canh bao qua toc do: Vong tron dac trang, chu den
      oled.fillCircle(cx, cy, r, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
      if (gameTelem.speedLimit >= 100) {
        oled.setCursor(cx - 8, cy - 3);
        oled.printf("%3d", gameTelem.speedLimit);
      } else {
        oled.setCursor(cx - 5, cy - 3);
        oled.printf("%2d", gameTelem.speedLimit);
      }
      oled.setTextColor(SSD1306_WHITE);
    } else {
      oled.drawCircle(cx, cy, r, SSD1306_WHITE);
      oled.drawCircle(cx, cy, r - 1, SSD1306_WHITE);
      if (gameTelem.speedLimit >= 100) {
        oled.setCursor(cx - 8, cy - 3);
        oled.printf("%3d", gameTelem.speedLimit);
      } else {
        oled.setCursor(cx - 5, cy - 3);
        oled.printf("%2d", gameTelem.speedLimit);
      }
    }
  } else {
    oled.drawCircle(cx, cy, r, SSD1306_WHITE);
    oled.setCursor(cx - 5, cy - 3);
    oled.print(F("--"));
  }

  // Toc do Cruise Control cai dat (dat ngay duoi bien bao, y=36):
  oled.setCursor(1, 36);
  if (gameTelem.cruiseOn && gameTelem.cruiseSpeed > 0) {
    oled.printf("C:%2d", gameTelem.cruiseSpeed);
  } else {
    oled.print(F("C:--"));
  }

  // --- COT GIUA (x = 32..94): Dong ho toc do dien tu lon (Digital Speedometer) ---
  int spd = constrain(abs(gameTelem.speed), 0, 199);
  oled.setTextSize(3);
  int xSpd = (spd < 10) ? 55 : ((spd < 100) ? 46 : 37);
  oled.setCursor(xSpd, 13);
  oled.print(spd);

  oled.setTextSize(1);
  // Khung trang tri hai ben chu km/h tao diem nhan the thao (y = 37):
  oled.drawFastHLine(34, 40, 15, SSD1306_WHITE);
  oled.setCursor(53, 37);
  oled.print(F("km/h"));
  oled.drawFastHLine(80, 40, 15, SSD1306_WHITE);

  // --- COT PHAI (x = 95..127): Hop so xe tai Chau Au & Phanh ---
  oled.drawRoundRect(95, 12, 32, 21, 2, SSD1306_WHITE);

  oled.setTextWrap(false);
  char cType = gameTelem.gearStr[0];
  if (cType == 'N' || (cType == '\0' && gameTelem.gear == 0)) {
    oled.setTextSize(2);
    oled.setCursor(106, 15);
    oled.print(F("N"));
  } else if (cType == 'R' || (cType == '\0' && gameTelem.gear < 0)) {
    const char* rNum = (cType == 'R' && strlen(gameTelem.gearStr) > 1) ? &gameTelem.gearStr[1] : nullptr;
    if (rNum && strlen(rNum) > 0 && strcmp(rNum, "1") != 0) {
      oled.setTextSize(1);
      oled.setCursor(97, 14);
      oled.print(F("R"));
      oled.setTextSize(2);
      oled.setCursor(109, 15);
      oled.print(rNum);
    } else {
      oled.setTextSize(2);
      oled.setCursor(106, 15);
      oled.print(F("R"));
    }
  } else {
    // D (Drive) hoac C (Crawler gear)
    char gMode = (cType == 'C' || cType == 'D') ? cType : 'D';
    oled.setTextSize(1);
    oled.setCursor(97, 14);
    oled.print(gMode);

    const char* numPart = (cType == 'C' || cType == 'D') ? &gameTelem.gearStr[1] : nullptr;
    char fallbackNum[4];
    if (!numPart || strlen(numPart) == 0) {
      snprintf(fallbackNum, sizeof(fallbackNum), "%d", abs(gameTelem.gear));
      numPart = fallbackNum;
    }

    oled.setTextSize(2);
    if (strlen(numPart) >= 2) {
      oled.setCursor(103, 15);
      oled.print(numPart);
    } else {
      oled.setCursor(109, 15);
      oled.print(numPart);
    }
  }
  oled.setTextWrap(true);

  // Chi bao phanh tay / Phanh dong co duoi hop so (y = 35..43):
  oled.setTextSize(1);
  if (gameTelem.parkBrake) {
    oled.fillRoundRect(96, 34, 14, 10, 2, SSD1306_WHITE);
    oled.setTextColor(SSD1306_BLACK);
    oled.setCursor(101, 35);
    oled.print(F("P"));
    oled.setTextColor(SSD1306_WHITE);
  } else if (gameTelem.motorBrake) {
    oled.setCursor(96, 35);
    oled.print(F("MBR"));
  }
  if (gameTelem.airPressureWarn && ((now / 250) % 2 == 0)) {
    oled.setCursor(113, 35);
    oled.print(F("!A"));
  }

  // 3. VUNG DAY (y = 45..63) - 2 HANG THONG SO ROUTE ADVISOR ETS2 CHUAN
  oled.drawFastHLine(0, 45, 128, SSD1306_WHITE);

  // HANG 1 (y = 47..53): Thanh bao nhien lieu (F), Vong tua (RPM) & Cu ly dich den (ETA)
  oled.setTextSize(1);
  oled.setCursor(1, 47);
  if (gameTelem.fuelWarning && ((now / 300) % 2 == 0)) {
    oled.print(F("F!"));
  } else {
    oled.print(F("F:"));
  }
  oled.drawRect(13, 47, 18, 6, SSD1306_WHITE);
  int fWidth = constrain(gameTelem.fuelPct * 14 / 100, 0, 14);
  if (fWidth > 0) oled.fillRect(15, 49, fWidth, 2, SSD1306_WHITE);
  oled.setCursor(33, 47);
  oled.printf("%2d%%", gameTelem.fuelPct);

  // Thanh RPM voi vach dai mo-men xoan kinh te Euro Truck (1000 - 1500 RPM)
  oled.setCursor(53, 47);
  oled.print(F("R:"));
  oled.drawRect(65, 47, 24, 6, SSD1306_WHITE);
  oled.drawFastVLine(74, 46, 2, SSD1306_WHITE);
  oled.drawFastVLine(80, 46, 2, SSD1306_WHITE);

  float maxR = (gameTelem.rpmMax > 500.0f) ? gameTelem.rpmMax : 2500.0f;
  int rWidth = constrain((int)(gameTelem.rpm / maxR * 20.0f), 0, 20);
  if (rWidth > 0) oled.fillRect(67, 49, rWidth, 2, SSD1306_WHITE);

  // Cu ly dich den (ETA) hoac So lit dau
  oled.setCursor(93, 47);
  if (gameTelem.etaDistanceKm > 0) {
    if (gameTelem.etaDistanceKm >= 1000) {
      oled.printf("%d.%dk", gameTelem.etaDistanceKm / 1000, (gameTelem.etaDistanceKm % 1000) / 100);
    } else {
      oled.printf("%dkm", gameTelem.etaDistanceKm);
    }
  } else if (gameTelem.fuelLiters > 0) {
    oled.printf("%dL", gameTelem.fuelLiters);
  } else if (gameTelem.odometerKm > 0) {
    oled.printf("%dk", gameTelem.odometerKm / 1000);
  } else {
    oled.print(F("ETS2"));
  }

  // HANG 2 (y = 56..63): 3 CUM DO HU HAI ROUTE ADVISOR ETS2 (XE TAI, RO-MOOC, HANG HOA)
  // Bitmap mini icons 8x6 chuan Route Advisor cua game Euro Truck Simulator 2
  static const uint8_t icon_truck_cab[] PROGMEM = { 0x81, 0x7E, 0x42, 0x7E, 0x7E, 0x81 }; // Dau keo xe tai
  static const uint8_t icon_trailer[]   PROGMEM = { 0x7E, 0x42, 0x7E, 0x7E, 0x3C, 0x42 }; // Ro-mooc thung
  static const uint8_t icon_cargo[]     PROGMEM = { 0x6C, 0x6C, 0x00, 0x6C, 0x6C, 0x00 }; // Hang hoa 4 o vuong

  // [Cum 1 - x = 0..40]: Do ben dau keo xe tai (Truck Wear % & Mini Bar)
  int x1 = 0;
  bool trkWarn = (gameTelem.truckWearPct >= 10);
  bool trkBlink = trkWarn && ((now / 300) % 2 == 0);
  if (!trkBlink) {
    oled.drawBitmap(x1, 57, icon_truck_cab, 8, 6, SSD1306_WHITE);
  }
  oled.setCursor(x1 + 9, 56);
  oled.printf("%2d%%", gameTelem.truckWearPct);
  oled.drawRect(x1 + 28, 57, 13, 5, SSD1306_WHITE);
  int w1 = (gameTelem.truckWearPct > 0) ? max(1, (int)round(gameTelem.truckWearPct * 11.0f / 100.0f)) : 0;
  w1 = constrain(w1, 0, 11);
  if (w1 > 0 && !trkBlink) {
    oled.fillRect(x1 + 29, 58, w1, 3, SSD1306_WHITE);
  }

  // [Cum 2 - x = 43..83]: Do ben ro-mooc (Trailer Wear % & Mini Bar)
  int x2 = 43;
  oled.drawBitmap(x2, 57, icon_trailer, 8, 6, SSD1306_WHITE);
  oled.setCursor(x2 + 9, 56);
  if (gameTelem.trailerAttached) {
    int tWear = (gameTelem.trailerWearPct >= 0) ? gameTelem.trailerWearPct : 0;
    oled.printf("%2d%%", tWear);
    oled.drawRect(x2 + 28, 57, 13, 5, SSD1306_WHITE);
    int w2 = (tWear > 0) ? max(1, (int)round(tWear * 11.0f / 100.0f)) : 0;
    w2 = constrain(w2, 0, 11);
    if (w2 > 0) oled.fillRect(x2 + 29, 58, w2, 3, SSD1306_WHITE);
  } else {
    oled.print(F("--%"));
    oled.drawRect(x2 + 28, 57, 13, 5, SSD1306_WHITE);
  }

  // [Cum 3 - x = 86..126]: Do ben hang hoa (Cargo Wear % & Mini Bar)
  int x3 = 86;
  oled.drawBitmap(x3, 57, icon_cargo, 8, 6, SSD1306_WHITE);
  oled.setCursor(x3 + 9, 56);
  if (gameTelem.trailerAttached && gameTelem.cargoWearPct >= 0) {
    oled.printf("%2d%%", gameTelem.cargoWearPct);
    oled.drawRect(x3 + 28, 57, 13, 5, SSD1306_WHITE);
    int w3 = (gameTelem.cargoWearPct > 0) ? max(1, (int)round(gameTelem.cargoWearPct * 11.0f / 100.0f)) : 0;
    w3 = constrain(w3, 0, 11);
    if (w3 > 0) oled.fillRect(x3 + 29, 58, w3, 3, SSD1306_WHITE);
  } else {
    oled.print(F("--%"));
    oled.drawRect(x3 + 28, 57, 13, 5, SSD1306_WHITE);
  }

  // 4. POPUP TOAST NOTIFICATION CHO SU KIEN GAME / COCKPIT
  // Khong hien thi toast khung to che giua man hinh cho cac thao tac phim thong thuong
  // (Cac tinh nang den, gat mua, ken, retarder, phanh tay da co icon ro net o Header va Taplo).
  // Chi hien thi toast khung to cho su kien vi pham, phat tien, thu phi, tai nan, canh bao hoac lenh API:
  if (now < gameTelem.eventUntilMs && strlen(gameTelem.eventText) > 0) {
    bool isRoutine = (strstr(gameTelem.eventText, "RETARDER") != nullptr ||
                      strstr(gameTelem.eventText, "GAT MUA") != nullptr ||
                      strstr(gameTelem.eventText, "DEN NOC") != nullptr ||
                      strstr(gameTelem.eventText, "PHANH MAY") != nullptr ||
                      strstr(gameTelem.eventText, "PHANH TAY") != nullptr ||
                      strstr(gameTelem.eventText, "CRUISE") != nullptr);
    if (!isRoutine) {
      drawEventToast();
    }
  } else if (gameTelem.paused) {
    oled.fillRoundRect(28, 20, 72, 20, 3, SSD1306_BLACK);
    oled.drawRoundRect(28, 20, 72, 20, 3, SSD1306_WHITE);
    oled.drawRoundRect(30, 22, 68, 16, 2, SSD1306_WHITE);
    oled.setTextSize(2);
    oled.setCursor(33, 23);
    oled.print(F("PAUSED"));
  }

  oled.display();
}

void DisplayManager::update() {
  if (emojiController.IsBusy() && (mode == DISP_EYES || mode == DISP_CLOCK)) return;

  unsigned long now = millis();
  unsigned long dt = (lastFrame > 0) ? (now - lastFrame) : 33;
  lastFrame = now;

  frameCount++;
  if (now - fpsTimer >= 1000) {
    fps = (fps > 0) ? fps * 0.7f + (1000.0f / (float)dt) * 0.3f : (1000.0f / (float)dt);
    frameCount = 0; fpsTimer = now;
  }

  updateClock();

  if (mode == DISP_STREAM && (now - streamLastMs) > 2000) mode = DISP_EYES;
  if (mode == DISP_GAME && (gameLastPacketMs == 0 || (now - gameLastPacketMs > 1500))) {
    mode = DISP_EYES; // Tu dong tra ve robot RoboEyes khi ngung nhan telemetry qua 1.5s
    gameModeLocked = false;
    gameTelem.loading = false;
    gameTelem.connected = false;
    ets2SplashUntilMs = 0;
  }

  switch (mode) {
    case DISP_CLOCK:  drawClock(); break;
    case DISP_EYES:   drawEyes(); break;
    case DISP_VISUAL: drawVisual(); break;
    case DISP_STREAM: drawStream(); break;
    case DISP_GAME:   drawGameDashboard(); break;
  }
}

const char* DisplayManager::modeStr() const {
  switch (mode) {
    case DISP_CLOCK:  return PSTR("CLOCK");
    case DISP_EYES:   return PSTR("EYES");
    case DISP_VISUAL: return PSTR("VISUAL");
    case DISP_STREAM: return PSTR("STREAM");
    case DISP_GAME:   return PSTR("GAME");
  }
  return PSTR("?");
}

