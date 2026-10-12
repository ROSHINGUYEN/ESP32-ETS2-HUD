#include <Arduino.h>
#include "config.h"
#include "board_config.h"
#include "emoji_controller.h"
#include "display_controller.h"
#if ENABLE_WIFI
#include "ntp_sync.h"
#include "ets2_client.h"
#endif
#include "usb_protocol.h"

// =====================================================================================
// GLOBALS DÙNG CHUNG GIỮA CÁC TASK
// =====================================================================================

SemaphoreHandle_t gStateMutex = nullptr;
volatile uint32_t gUnderrunCount = 0;

static unsigned long lastTelem = 0;
static unsigned long lastNtpUpdate = 0;

// =====================================================================================
// FREERTOS TASKS (2 CORES)
// =====================================================================================

static void checkSerialCommands();

// CORE 0:
void taskUsb(void*) {
  while (true) {
    usb.update();
    checkSerialCommands();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// CORE 1:
void taskDisplay(void*) {
  TickType_t last = xTaskGetTickCount();
  while (true) {
    display.update();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(33)); // 30 FPS
  }
}

// =====================================================================================
// XỬ LÝ LỆNH SERIAL (ASCII TEXT & JSON TAIBAI)
// =====================================================================================

void processSerialLine(String line) {
  line.trim();
  if (line.length() == 0) return;

  if (line.startsWith("TIME=") || line.startsWith("TIME ")) {
    int h = 0, m = 0, s = 0;
    if (sscanf(line.c_str() + 5, "%d:%d:%d", &h, &m, &s) >= 2) {
      display.setTime(h, m, s);
      LOG_PRINTF("[OK] Da cap nhat gio: %02d:%02d:%02d\n", h, m, s);
    }
  } else if (line.startsWith("DATE=") || line.startsWith("DATE ")) {
    int d = 0, mo = 0, y = 0;
    if (sscanf(line.c_str() + 5, "%d/%d/%d", &d, &mo, &y) == 3) {
      display.setDate(d, mo, y);
      LOG_PRINTF("[OK] Da cap nhat ngay: %02d/%02d/%04d\n", d, mo, y);
    }
  } else if (line.startsWith("TEMP=") || line.startsWith("TEMP ")) {
    float t = line.substring(5).toFloat();
    if (t > 15.0f && t < 45.0f) {
      display.tempC = t;
      LOG_PRINTF("[OK] Da cai dat nhiet do phong: %.1fC\n", t);
    }
#if ENABLE_WIFI
  } else if (line.startsWith("NTP=ON") || line.startsWith("NTP=1")) {
    ntp.setEnabled(true);
  } else if (line.startsWith("NTP=OFF") || line.startsWith("NTP=0")) {
    ntp.setEnabled(false);
  } else if (line.startsWith("NTP?")) {
    ntp.printStatus();
  } else if (line.startsWith("NTPSET=")) {
    String creds = line.substring(7);
    int commaIdx = creds.indexOf(',');
    if (commaIdx > 0) {
      String ssid = creds.substring(0, commaIdx);
      String pass = creds.substring(commaIdx + 1);
      ntp.setCredentials(ssid.c_str(), pass.c_str());
    } else {
      LOG_PRINTLN(F("[NTP] Sai format. Su dung: NTPSET=SSID,PASS"));
    }
#endif
  } else if (line == "MODE=GAME" || line == "MODE=ETS2" || line == "mode=game" || line == "game") {
    display.mode = DISP_GAME;
    display.gameModeLocked = true;
    display.triggerEts2Splash();
    LOG_PRINTLN(F("[MODE] Da chuyen sang ETS2 Game Dashboard (Khoa che do + Splash Intro)"));
  } else if (line == "MODE=EYES" || line == "mode=eyes" || line == "eyes") {
    display.mode = DISP_EYES;
    display.gameModeLocked = false;
    display.gameTelem.loading = false;
    display.gameTelem.connected = false;
    display.ets2SplashUntilMs = 0;
    LOG_PRINTLN(F("[MODE] Da chuyen sang RoboEyes"));
  } else if (line == "MODE=CLOCK" || line == "mode=clock" || line == "clock") {
    display.mode = DISP_CLOCK;
    display.gameModeLocked = false;
    LOG_PRINTLN(F("[MODE] Da chuyen sang Clock"));
  } else if (line == "RESET" || line == "REBOOT" || line == "restart") {
    LOG_PRINTLN(F("[SYS] Lenh khoi dong lai he thong..."));
    delay(100);
    ESP.restart();
  } else if (line.startsWith("ETS2=") || line.startsWith("ETS2:") ||
             (line.indexOf('{') >= 0 && (line.indexOf("\"game\"") >= 0 || line.indexOf("\"spd\"") >= 0 || line.indexOf("\"gear\"") >= 0))) {
    int startIdx = line.indexOf('{');
    if (startIdx >= 0) {
      String jsonStr = line.substring(startIdx);
      usb.processJson(jsonStr.c_str(), jsonStr.length());
    }
  }
}

static char s0Buf[1280];
static uint16_t s0Len = 0;

static void checkSerialCommands() {
  while (Serial0.available()) {
    char c = (char)Serial0.read();
    if (c == '\n') {
      s0Buf[s0Len] = '\0';
      if (s0Len > 0) {
        // Toi uu hoa Zero-Allocation: Telemetry goi thang vao processJson khong tao String Heap
        if (strncmp(s0Buf, "ETS2=", 5) == 0 || strncmp(s0Buf, "ETS2:", 5) == 0) {
          const char* pJson = strchr(s0Buf, '{');
          if (pJson) {
            usb.processJson(pJson, strlen(pJson));
          }
        } else if (s0Buf[0] == '{' && (strstr(s0Buf, "\"spd\"") || strstr(s0Buf, "\"gear\"") || strstr(s0Buf, "\"game\""))) {
          usb.processJson(s0Buf, s0Len);
        } else {
          processSerialLine(String(s0Buf));
        }
      }
      s0Len = 0;
    } else if (c != '\r') {
      if (s0Len < sizeof(s0Buf) - 1) {
        s0Buf[s0Len++] = c;
      }
    }
  }
}

// =====================================================================================
// SETUP & LOOP
// =====================================================================================

void setup() {
  // Tat den LED RGB (WS2812 NeoPixel) tren bo mach ESP32-S3 (bat buoc gui 0,0,0)
#ifdef RGB_BUILTIN
  neopixelWrite(RGB_BUILTIN, 0, 0, 0);
#endif
  neopixelWrite(48, 0, 0, 0);
  neopixelWrite(38, 0, 0, 0);
  neopixelWrite(21, 0, 0, 0);
  // Dong thoi tat cac chan LED don thuong neu co
  pinMode(2, OUTPUT); digitalWrite(2, LOW);
  pinMode(10, OUTPUT); digitalWrite(10, LOW);

  Serial0.setRxBufferSize(2048);
  Serial0.begin(460800);
  Serial.begin(460800);
  delay(150);

  LOG_PRINTLN(F("\n====================================================="));
  LOG_PRINTLN(F(" MIU-ETS2 - HIGH PERFORMANCE COCKPIT DISPLAY        "));
  LOG_PRINTLN(F(" 460800 Baud Link | Zero-Allocation | FreeRTOS Dual "));
  LOG_PRINTLN(F("====================================================="));

  gStateMutex = xSemaphoreCreateMutex();
  display.begin();

  // Khoi dong nhanh man hinh OLED
  LOG_PRINTLN(F("[BOOT] Khoi dong he thong MIU-ETS2 (Telemetry Display)..."));
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(22, 15);
  oled.print(F("MIU-ETS2 READY"));
  oled.setCursor(24, 30);
  oled.print(F("ETS2 COCKPIT"));
  oled.setCursor(16, 45);
  oled.print(F("460800 BAUD LINK"));
  oled.display();
  delay(300);

  display.initClock();
#if ENABLE_WIFI
  ntp.begin();
  ets2Wifi.begin();
#endif

  xTaskCreatePinnedToCore(taskUsb,     "USB",     6144, NULL, 4, NULL, 0);
  xTaskCreatePinnedToCore(taskDisplay, "Display", 6144, NULL, 4, NULL, 1);

  LOG_PRINTLN(F("[OK] USB & Display tasks started on 2 cores. System ready at 460800 baud!"));
}

void loop() {
#if ENABLE_WIFI
  ets2Wifi.update();
#endif
  unsigned long now = millis();

#if ENABLE_WIFI
  if (now - lastNtpUpdate >= 15000UL) {
    lastNtpUpdate = now;
    ntp.update();
  }
#endif

  if (now - lastTelem >= 10000) {
    lastTelem = now;
    LOG_PRINTF("[TELEM] up=%lus fps=%.0f heap=%u und=%u mode=%s\n",
      now / 1000, display.fps, ESP.getFreeHeap(),
      gUnderrunCount, display.modeStr());
  }

  delay(5);
}
