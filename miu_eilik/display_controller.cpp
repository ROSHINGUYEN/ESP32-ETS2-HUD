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
    { 42, 41, "IO41/42 dao (SDA=42, SCL=41)" },
    { 47, 48, "IO47/48 (SDA=47, SCL=48)" },
    { 48, 47, "IO47/48 dao (SDA=48, SCL=47)" }
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
    drawBootLogo("ROSHI", 4500);
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
      oled.print(F("> CAN CHINH SERVO"));
    } else if (percent < 70) {
      oled.print(F("> PHAT WIFI ROSHI"));
    } else if (percent < 90) {
      oled.print(F("> WEB UI CAPTIVE"));
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

    // Hiệu ứng quét tia Laser (Scanline) nhẹ ở 700ms đầu tiên
    if (elapsed < 700) {
      int scanY = (int)((elapsed * SCREEN_H) / 700);
      oled.drawFastHLine(0, scanY, SCREEN_W, SSD1306_WHITE);
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

  float rawT = temperatureRead();
  if (rawT >= 20.0f && rawT <= 85.0f) {
    float estimatedRoomT = rawT - 28.5f;
    estimatedRoomT = constrain(estimatedRoomT, 22.0f, 38.0f);
    tempC = tempC * 0.92f + estimatedRoomT * 0.08f;
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

void DisplayManager::triggerFs25Splash() {
  fs25SplashUntilMs = millis() + 2000;
}

void DisplayManager::drawEts2SplashScreen() {
  unsigned long now = millis();
  unsigned long remain = (ets2SplashUntilMs > now) ? (ets2SplashUntilMs - now) : 0;
  unsigned long elapsed = 3000 - remain;

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // Khung vien ngoai
  oled.drawRoundRect(2, 2, 124, 60, 3, SSD1306_WHITE);

  // Ten game ETS2
  oled.setTextSize(1);
  oled.setCursor(10, 6);
  oled.print(F("EURO TRUCK SIMULATOR 2"));
  oled.drawFastHLine(6, 16, 116, SSD1306_WHITE);

  // Do hoa Pixel Art xe tai ETS2 o giua
  int16_t tx = 46;
  int16_t ty = 20;
  oled.drawRect(tx + 10, ty, 20, 16, SSD1306_WHITE);
  oled.fillRect(tx + 20, ty + 2, 8, 6, SSD1306_WHITE);
  oled.drawFastVLine(tx + 9, ty - 2, 8, SSD1306_WHITE);
  oled.drawRect(tx, ty + 6, 10, 10, SSD1306_WHITE);
  oled.fillRect(tx + 28, ty + 9, 2, 3, SSD1306_WHITE);
  oled.fillCircle(tx + 5, ty + 16, 3, SSD1306_WHITE);
  oled.fillCircle(tx + 24, ty + 16, 3, SSD1306_WHITE);

  // Hang phat trien SCS Software
  oled.setCursor(28, 40);
  oled.print(F("SCS SOFTWARE"));

  // Thanh tien trinh loading
  oled.drawRect(16, 50, 96, 6, SSD1306_WHITE);
  int w = map(constrain(elapsed, 0, 3000), 0, 3000, 0, 92);
  if (w > 0) {
    oled.fillRect(18, 52, w, 2, SSD1306_WHITE);
  }

  oled.display();
}

void DisplayManager::drawFs25SplashScreen() {
  unsigned long now = millis();
  unsigned long remain = (fs25SplashUntilMs > now) ? (fs25SplashUntilMs - now) : 0;
  unsigned long elapsed = 2000 - remain;

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // Khung vien ngoai
  oled.drawRoundRect(2, 2, 124, 60, 3, SSD1306_WHITE);

  // Ten game FS25
  oled.setTextSize(1);
  oled.setCursor(8, 6);
  oled.print(F("FARMING SIMULATOR 25"));
  oled.drawFastHLine(6, 16, 116, SSD1306_WHITE);

  // Do hoa Pixel Art may cay FS25 o giua
  int16_t fx = 42;
  int16_t fy = 19;
  oled.drawRect(fx + 4, fy, 16, 13, SSD1306_WHITE);
  oled.drawFastHLine(fx + 2, fy, 20, SSD1306_WHITE);
  oled.fillRect(fx + 20, fy + 6, 18, 7, SSD1306_WHITE);
  oled.drawFastVLine(fx + 26, fy + 1, 5, SSD1306_WHITE);
  oled.fillRect(fx + 37, fy + 8, 2, 2, SSD1306_WHITE);
  oled.fillCircle(fx + 10, fy + 15, 6, SSD1306_WHITE);
  oled.drawCircle(fx + 10, fy + 15, 3, SSD1306_BLACK);
  oled.fillCircle(fx + 30, fy + 16, 3, SSD1306_WHITE);

  // Hang phat trien GIANTS Software
  oled.setCursor(20, 40);
  oled.print(F("GIANTS SOFTWARE"));

  // Thanh tien trinh loading
  oled.drawRect(16, 50, 96, 6, SSD1306_WHITE);
  int w = map(constrain(elapsed, 0, 2000), 0, 2000, 0, 92);
  if (w > 0) {
    oled.fillRect(18, 52, w, 2, SSD1306_WHITE);
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
    gameTelem.eventUntilMs = millis() + 2200;
  }

  // CHỈ chuyển sang màn hình Game khi game ĐÃ KẾT NỐI (data.connected == true)
  if (data.connected) {
    if (!wasConnected || mode != DISP_GAME) {
      mode = DISP_GAME;
      triggerEts2Splash();
      LOG_PRINTLN(F("[DISPLAY] Game ETS2 da ket noi! Hien thi Loading Splash ETS2..."));
    }
  } else {
    // Game chua ket noi hoac da thoat -> ve ngay mat robot RoboEyes
    if (mode == DISP_GAME) {
      mode = DISP_EYES;
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

    bool isMoney = (strstr(val, "EUR") != nullptr || strstr(title, "PHAT") != nullptr ||
                    strstr(title, "THU PHI") != nullptr || strstr(title, "VA CHAM") != nullptr);

    if (isMoney) {
      // Tieu de nen trang chu den noi bat
      oled.fillRect(7, 16, 114, 10, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
      oled.setTextSize(1);
      int tx = 64 - (strlen(title) * 6 / 2);
      if (tx < 10) tx = 10;
      oled.setCursor(tx, 17);
      oled.print(title);

      // So tien tru to ro
      oled.setTextColor(SSD1306_WHITE);
      oled.setTextSize(2);
      int vx = 64 - (strlen(val) * 12 / 2);
      if (vx < 10) vx = 10;
      oled.setCursor(vx, 29);
      oled.print(val);
    } else {
      // Su kien thao tac: Retarder, Gat mua, Den noc, Phanh tay...
      oled.setTextSize(1);
      oled.setTextColor(SSD1306_WHITE);
      int tx = 64 - (strlen(title) * 6 / 2);
      if (tx < 8) tx = 8;
      oled.setCursor(tx, 18);
      oled.print(title);

      oled.setTextSize(2);
      int vx = 64 - (strlen(val) * 12 / 2);
      if (vx < 8) vx = 8;
      oled.setCursor(vx, 29);
      oled.print(val);
    }
  } else {
    // Thong bao don
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    int tx = 64 - (strlen(txt) * 6 / 2);
    if (tx < 8) tx = 8;
    oled.setCursor(tx, 26);
    oled.print(txt);
  }
}

void DisplayManager::drawGameDashboard() {
  unsigned long now = millis();
  if (now < ets2SplashUntilMs) {
    drawEts2SplashScreen();
    return;
  }

  bool isTimedOut = (gameLastPacketMs == 0 || (now - gameLastPacketMs > 8000));

  // Neu khong khoa che do va het timeout hoac chua ket noi -> tro ve ngay Mat RoboEyes
  if (!gameModeLocked && (isTimedOut || !gameTelem.connected)) {
    mode = DISP_EYES;
    return;
  }

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // =========================================================================
  // EURO TRUCK COCKPIT DASHBOARD (128 x 64 OLED)
  // =========================================================================

  // 1. TOP HEADER (y = 0..11)
  // Xi-nhan trai (Left Blinker):
  if (gameTelem.blinkerL) {
    if ((now / 350) % 2 == 0) {
      oled.fillTriangle(6, 1, 1, 5, 6, 9, SSD1306_WHITE);
    } else {
      oled.drawTriangle(6, 1, 1, 5, 6, 9, SSD1306_WHITE);
    }
  }

  // Den pha/cos xe tai:
  oled.setTextSize(1);
  if (gameTelem.lightsBeamHigh) {
    oled.setCursor(9, 2);
    oled.print(F("D+"));
  } else if (gameTelem.lightsBeamLow) {
    oled.setCursor(9, 2);
    oled.print(F("D"));
  }

  // Badges trang thai buong lai Euro Truck:
  if (gameTelem.wipers) {
    oled.setCursor(24, 2);
    oled.print(F("WIP"));
  }
  if (gameTelem.beacon && ((now / 300) % 2 == 0)) {
    oled.setCursor(44, 2);
    oled.print(F("BCN"));
  } else if (gameTelem.retarder > 0) {
    oled.setCursor(44, 2);
    oled.printf("R%d", gameTelem.retarder);
  }

  // Dong ho gio tren xe tai / Cự ly den dich giao hang (In-Game Clock & Destination ETA):
  int dispH = (gameTelem.gameHour >= 0) ? gameTelem.gameHour : clockHour;
  int dispM = (gameTelem.gameMin >= 0) ? gameTelem.gameMin : clockMin;
  bool showEta = (gameTelem.etaDistanceKm > 0);

  if (showEta && ((now / 2500) % 2 == 1)) {
    // Icon cờ đích đến (Destination Flag)
    oled.drawFastVLine(56, 2, 8, SSD1306_WHITE);
    oled.fillRect(57, 2, 5, 4, SSD1306_WHITE);
    oled.setCursor(64, 2);
    oled.printf("%dkm", gameTelem.etaDistanceKm);
  } else {
    // Dong ho gio dien tu tren cabin xe
    oled.setCursor(56, 2);
    bool blinkColon = ((now / 500) % 2 == 0);
    oled.printf("%02d%c%02d", dispH, blinkColon ? ':' : ' ', dispM);
  }

  // Cruise Control badge o header:
  if (gameTelem.cruiseOn) {
    oled.setCursor(98, 2);
    oled.print(F("CC"));
  }

  // Xi-nhan phai (Right Blinker):
  if (gameTelem.blinkerR) {
    if ((now / 350) % 2 == 0) {
      oled.fillTriangle(121, 1, 126, 5, 121, 9, SSD1306_WHITE);
    } else {
      oled.drawTriangle(121, 1, 126, 5, 121, 9, SSD1306_WHITE);
    }
  }
  oled.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  // 2. VUNG TRUNG TAM (y = 12..45)
  // --- Cot trai: Bien bao tron gioi han toc do Chau Au & Ga tu dong ---
  int cx = 14, cy = 22, r = 9;
  bool speeding = (gameTelem.speedLimit > 0 && gameTelem.speed > gameTelem.speedLimit);
  if (gameTelem.speedLimit > 0) {
    if (speeding && ((now / 220) % 2 == 0)) {
      // Canh bao qua toc do: Chop dao mau (vong tron dac trang, chu den)
      oled.fillCircle(cx, cy, r, SSD1306_WHITE);
      oled.setTextColor(SSD1306_BLACK);
      oled.setCursor(cx - 5, cy - 3);
      oled.printf("%2d", gameTelem.speedLimit);
      oled.setTextColor(SSD1306_WHITE);
    } else {
      oled.drawCircle(cx, cy, r, SSD1306_WHITE);
      oled.drawCircle(cx, cy, r - 1, SSD1306_WHITE);
      oled.setCursor(cx - 5, cy - 3);
      oled.printf("%2d", gameTelem.speedLimit);
    }
  } else {
    oled.drawCircle(cx, cy, r, SSD1306_WHITE);
    oled.setCursor(cx - 5, cy - 3);
    oled.print(F("--"));
  }

  // Cruise control ben duoi bien bao toc do (y = 34):
  oled.setCursor(1, 34);
  if (gameTelem.cruiseOn && gameTelem.cruiseSpeed > 0) {
    oled.printf("C:%2d", gameTelem.cruiseSpeed);
  } else {
    oled.print(F("C:--"));
  }

  // --- Trung tam: Dong ho toc do dien tu lon (Digital Speedometer) ---
  int spd = constrain(gameTelem.speed, 0, 199);
  oled.setTextSize(3);
  int xSpd = (spd < 10) ? 58 : ((spd < 100) ? 46 : 34);
  oled.setCursor(xSpd, 13);
  oled.print(spd);

  oled.setTextSize(1);
  // Khung trang tri hai ben chu km/h tao diem nhan dong ho taplo (y = 36)
  oled.drawFastHLine(34, 39, 13, SSD1306_WHITE);
  oled.setCursor(51, 36);
  oled.print(F("km/h"));
  oled.drawFastHLine(79, 39, 13, SSD1306_WHITE);

  // --- Cot phai: Hop so xe tai Chau Au (TraXon / Scania Opticruise) (y = 12..30) ---
  oled.drawRoundRect(96, 12, 31, 19, 2, SSD1306_WHITE);
  oled.setTextSize(2);
  if (gameTelem.gear > 0) {
    if (gameTelem.gear >= 10) {
      oled.setTextSize(1);
      oled.setCursor(98, 18);
      oled.print(F("D"));
      oled.setTextSize(2);
      oled.setCursor(104, 15);
      oled.printf("%2d", gameTelem.gear);
    } else {
      oled.setCursor(100, 15);
      oled.printf("D%d", gameTelem.gear);
    }
  } else if (gameTelem.gear == 0) {
    oled.setCursor(106, 15);
    oled.print(F("N"));
  } else {
    int rGear = abs(gameTelem.gear);
    if (rGear == 1) {
      oled.setCursor(106, 15);
      oled.print(F("R"));
    } else {
      oled.setCursor(100, 15);
      oled.printf("R%d", rGear);
    }
  }

  // Chi bao phanh tay / Phanh dong co duoi hop so (y = 33..43):
  oled.setTextSize(1);
  if (gameTelem.parkBrake) {
    oled.fillRoundRect(96, 33, 14, 11, 2, SSD1306_WHITE);
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

  // 3. VUNG DAY (y = 46..63) - DONG HO XANG, RPM VA STATUS TICKER TOAN DIEN
  oled.drawFastHLine(0, 46, 128, SSD1306_WHITE);

  // DONG 1 (y = 48..54): Thanh bao nhien lieu & Thanh bao vong tua RPM
  // Thanh bao nhien lieu Diesel (x = 1..58):
  oled.setTextSize(1);
  oled.setCursor(1, 48);
  if (gameTelem.fuelWarning && ((now / 300) % 2 == 0)) {
    oled.print(F("F!"));
  } else {
    oled.print(F("F:"));
  }
  oled.drawRect(13, 48, 25, 5, SSD1306_WHITE);
  int fWidth = constrain(gameTelem.fuelPct * 21 / 100, 0, 21);
  if (fWidth > 0) oled.fillRect(15, 49, fWidth, 3, SSD1306_WHITE);
  oled.setCursor(41, 48);
  oled.printf("%2d%%", gameTelem.fuelPct);

  // Thanh bao vong tua RPM voi dai kinh te (Economic Green Band 1000-1500 RPM) (x = 66..127):
  oled.setCursor(67, 48);
  oled.print(F("R:"));
  oled.drawRect(79, 48, 48, 5, SSD1306_WHITE);
  // Vach danh dau dai mo-men xoan kinh te Euro Truck (1000 - 1500 RPM tai x=95 va x=105)
  oled.drawFastVLine(95, 46, 2, SSD1306_WHITE);
  oled.drawFastVLine(105, 46, 2, SSD1306_WHITE);
  oled.drawFastVLine(95, 53, 2, SSD1306_WHITE);
  oled.drawFastVLine(105, 53, 2, SSD1306_WHITE);

  float maxR = (gameTelem.rpmMax > 500.0f) ? gameTelem.rpmMax : 2500.0f;
  int rWidth = constrain((int)(gameTelem.rpm / maxR * 44.0f), 0, 44);
  if (rWidth > 0) oled.fillRect(81, 49, rWidth, 3, SSD1306_WHITE);

  // DONG 2 (y = 56..63): STATUS TICKER (DO BEN XE, DO BEN HANG HOA, XANG LIT, ODO/ETA)
  // [1] DO BEN XE (TRUCK DAMAGE / WEAR %):
  oled.setCursor(1, 56);
  if (gameTelem.truckWearPct >= 10 && ((now / 300) % 2 == 0)) {
    // Nhap nhay canh bao khi xe bi hong nang
    oled.printf("T!%d%%", gameTelem.truckWearPct);
  } else {
    oled.printf("T:%d%%", gameTelem.truckWearPct);
  }

  // [2] DO BEN HANG HOA / RO-MOOC (CARGO DAMAGE / WEAR %):
  oled.setCursor(32, 56);
  if (gameTelem.trailerAttached) {
    int cWear = (gameTelem.cargoWearPct >= 0) ? gameTelem.cargoWearPct : 0;
    if (cWear > 0 && ((now / 350) % 2 == 0)) {
      // Nhap nhay canh bao khi hang hoa bi hu hai
      oled.printf("C!%d%%", cWear);
    } else {
      oled.printf("C:%d%%", cWear);
    }
  } else {
    oled.print(F("C:--"));
  }

  // [3] SO LIT XANG THUC TE (REAL FUEL LITERS):
  oled.setCursor(62, 56);
  if (gameTelem.fuelLiters > 0) {
    oled.printf("%dL", gameTelem.fuelLiters);
  } else {
    oled.printf("%d%%", gameTelem.fuelPct);
  }

  // [4] CU LY DEN DICH GIAO HANG HOAC ODOMETER (ETA / ODO KM):
  oled.setCursor(93, 56);
  if (gameTelem.etaDistanceKm > 0) {
    oled.printf("%dkm", gameTelem.etaDistanceKm);
  } else if (gameTelem.odometerKm > 0) {
    if (gameTelem.odometerKm >= 10000) {
      oled.printf("%dk", gameTelem.odometerKm / 1000);
    } else {
      oled.printf("%d", gameTelem.odometerKm);
    }
  } else {
    oled.print(F("ETS2"));
  }

  // 4. POPUP TOAST NOTIFICATION CHO SU KIEN GAME / COCKPIT
  if (now < gameTelem.eventUntilMs && strlen(gameTelem.eventText) > 0) {
    drawEventToast();
  } else if (gameTelem.paused) {
    // Neu dang Pause trong game:
    oled.fillRoundRect(28, 20, 72, 20, 3, SSD1306_BLACK);
    oled.drawRoundRect(28, 20, 72, 20, 3, SSD1306_WHITE);
    oled.drawRoundRect(30, 22, 68, 16, 2, SSD1306_WHITE);
    oled.setTextSize(2);
    oled.setCursor(33, 23);
    oled.print(F("PAUSED"));
  }

  oled.display();
}

void DisplayManager::updateFs25Telemetry(const Fs25TelemetryData& data) {
  bool firstConnect = (fs25LastPacketMs == 0 || mode != DISP_FS25);
  fs25Telem = data;
  fs25LastPacketMs = millis();
  notifyPresence();

  if (firstConnect) {
    mode = DISP_FS25;
    triggerFs25Splash();
    LOG_PRINTLN(F("[DISPLAY] Da bat HUD Farming Simulator 25 (Kem Splash Intro)!"));
  }
}

void DisplayManager::drawFs25Dashboard() {
  unsigned long now = millis();
  if (now < fs25SplashUntilMs) {
    drawFs25SplashScreen();
    return;
  }

  bool isTimedOut = (fs25LastPacketMs == 0 || (now - fs25LastPacketMs > 6000));
  if (isTimedOut) {
    mode = DISP_EYES;
    return;
  }

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  bool blinkPhase = ((now / 350) % 2 == 0);

  // 1. Header (Y: 0..10)
  oled.setTextSize(1);
  oled.setCursor(0, 1);
  oled.print(F("FS25"));

  // Xi-nhan trai
  if (fs25Telem.blinkerL && blinkPhase) {
    oled.setCursor(28, 1);
    oled.print(F("<<"));
  }

  // Den chop xoay [BCN]
  if (fs25Telem.beacon) {
    oled.setCursor(44, 1);
    if (blinkPhase) {
      oled.print(F("[BCN]"));
    } else {
      oled.print(F(" BCN "));
    }
  }

  // Den lam viec [WRK]
  if (fs25Telem.workLight) {
    oled.setCursor(76, 1);
    oled.print(F("[WRK]"));
  }

  // Xi-nhan phai
  if (fs25Telem.blinkerR && blinkPhase) {
    oled.setCursor(106, 1);
    oled.print(F(">>"));
  }

  // Duong ke ngang phan cach header
  oled.drawFastHLine(0, 11, 128, SSD1306_WHITE);

  // 2. Khu vuc trung tam: Toc do so lon (X: 0..58, Y: 14..40)
  oled.setTextSize(3);
  oled.setCursor(4, 15);
  int spd = constrain(fs25Telem.speed, 0, 999);
  if (spd < 10) oled.print(F(" "));
  oled.print(spd);

  oled.setTextSize(1);
  oled.setCursor(44, 29);
  oled.print(F("KM"));
  oled.setCursor(44, 37);
  oled.print(F("/H"));

  // 3. Ben phai (X: 66..127): Danh sach trang thai chuc nang nong cu
  // Hang 1 (Y=14): May moc [PTO:ON] / [PTO:OFF]
  oled.setCursor(68, 14);
  if (fs25Telem.implementTurnedOn) {
    oled.print(F("[PTO:ON]"));
  } else {
    oled.print(F(" PTO:OFF"));
  }

  // Hang 2 (Y=24): Dan cay/nong cu [DOWN] / [UP]
  oled.setCursor(68, 24);
  if (fs25Telem.implementLowered) {
    oled.print(F("[DOWN v]"));
  } else {
    oled.print(F(" TOOL:^ "));
  }

  // Hang 3 (Y=34): Ong xa [PIPE] hoac Thung chua %
  oled.setCursor(68, 34);
  if (fs25Telem.pipeUnfolded) {
    oled.print(F("[PIPE->]"));
  } else {
    oled.printf("FIL:%d%%", constrain(fs25Telem.fillLevelPct, 0, 100));
  }

  // 4. Thanh Toc Do (Speed Bar) (Y: 44..50)
  int maxSpd = (fs25Telem.maxSpeed > 0) ? fs25Telem.maxSpeed : 50;
  int barW = map(constrain(spd, 0, maxSpd), 0, maxSpd, 0, 124);
  oled.drawRect(0, 44, 128, 7, SSD1306_WHITE);
  if (barW > 0) {
    oled.fillRect(2, 46, barW, 3, SSD1306_WHITE);
  }

  // 5. Hang thong tin day (Y: 54..63)
  oled.setCursor(0, 54);
  if (fs25Telem.cruiseOn) {
    oled.printf("CRU:%d", fs25Telem.cruiseSpeed);
  } else {
    oled.print(F("CRU:--"));
  }

  oled.setCursor(50, 54);
  oled.printf("OIL:%d%%", constrain(fs25Telem.fuelPct, 0, 100));

  oled.setCursor(94, 54);
  if (fs25Telem.operatingHours > 0.0f) {
    oled.printf("%.1fh", fs25Telem.operatingHours);
  } else {
    oled.printf("D:%d%%", constrain(fs25Telem.damagePct, 0, 100));
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
  if (mode == DISP_GAME && !gameModeLocked && (gameLastPacketMs > 0) && (now - gameLastPacketMs > 10000)) {
    mode = DISP_EYES; // Tu dong tra ve robot de ban khi ngung choi game qua 10s
  }
  if (mode == DISP_FS25 && !gameModeLocked && (fs25LastPacketMs > 0) && (now - fs25LastPacketMs > 10000)) {
    mode = DISP_EYES; // Tu dong tra ve robot de ban khi ngung choi FS25 qua 10s
  }

  switch (mode) {
    case DISP_CLOCK:  drawClock(); break;
    case DISP_EYES:   drawEyes(); break;
    case DISP_VISUAL: drawVisual(); break;
    case DISP_STREAM: drawStream(); break;
    case DISP_GAME:   drawGameDashboard(); break;
    case DISP_FS25:   drawFs25Dashboard(); break;
  }
}

const char* DisplayManager::modeStr() const {
  switch (mode) {
    case DISP_CLOCK:  return PSTR("CLOCK");
    case DISP_EYES:   return PSTR("EYES");
    case DISP_VISUAL: return PSTR("VISUAL");
    case DISP_STREAM: return PSTR("STREAM");
    case DISP_GAME:   return PSTR("GAME");
    case DISP_FS25:   return PSTR("FS25");
  }
  return PSTR("?");
}

