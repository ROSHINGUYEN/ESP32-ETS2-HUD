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

void taskBrain(void*) {
  TickType_t last = xTaskGetTickCount();
  while (true) {
    emojiController.Update();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(50));
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
  } else if (line.startsWith("ACT=") || line.startsWith("ACT ")) {
    String actStr = line.substring(4);
    actStr.trim();
    LOG_PRINTF("[ACT] Nhan lenh hanh dong: %s\n", actStr.c_str());
    async_sequent_act(actStr);
  } else if (line.startsWith("AUTOTIME=")) {
    int sec = line.substring(9).toInt();
    if (sec >= 4 && sec <= 600) {
      act_interval = sec * 1000UL;
      emojiController.SetAutoInterval(act_interval);
      LOG_PRINTF("[ACT] Da cai thoi gian nghi: %d giay\n", sec);
    }
#if ENABLE_WIFI
  } else if (line.startsWith("ETS2IP=")) {
    String ip = line.substring(7);
    ip.trim();
    if (ip == "OFF" || ip == "0") {
      ets2Wifi.setEnabled(false);
      LOG_PRINTLN(F("[ETS2 WIFI] Da tat keo telemetry qua WiFi."));
    } else {
      ets2Wifi.setServer(ip, true);
      LOG_PRINTF("[ETS2 WIFI] Da bat keo du lieu tu: %s\n", ets2Wifi.serverIp.c_str());
    }
  } else if (line == "ETS2WIFI=ON" || line == "ETS2WIFI=1") {
    ets2Wifi.setEnabled(true);
  } else if (line == "ETS2WIFI=OFF" || line == "ETS2WIFI=0") {
    ets2Wifi.setEnabled(false);
  } else if (line == "ETS2?") {
    LOG_PRINTF("[ETS2] IP=%s, Enabled=%d, Status=%s\n",
               ets2Wifi.serverIp.c_str(), ets2Wifi.enabled,
               display.gameTelem.connected ? "CONNECTED" : "DISCONNECTED");
#endif
  } else if (line == "MODE=GAME" || line == "MODE=ETS2" || line == "mode=game" || line == "game") {
    display.mode = DISP_GAME;
    display.gameModeLocked = true;
    display.triggerEts2Splash();
    LOG_PRINTLN(F("[MODE] Da chuyen sang ETS2 Game Dashboard (Khoa che do + Splash Intro)"));
  } else if (line == "MODE=FS25" || line == "mode=fs25" || line == "fs25") {
    display.mode = DISP_FS25;
    display.gameModeLocked = true;
    display.triggerFs25Splash();
    LOG_PRINTLN(F("[MODE] Da chuyen sang Farming Simulator 25 Dashboard (Khoa che do + Splash Intro)"));
  } else if (line == "MODE=EYES" || line == "mode=eyes" || line == "eyes") {
    display.mode = DISP_EYES;
    display.gameModeLocked = false;
    LOG_PRINTLN(F("[MODE] Da chuyen sang RoboEyes"));
  } else if (line == "MODE=CLOCK" || line == "mode=clock" || line == "clock") {
    display.mode = DISP_CLOCK;
    display.gameModeLocked = false;
    LOG_PRINTLN(F("[MODE] Da chuyen sang Clock"));
  } else if (line.startsWith("FS25=") || line.startsWith("FS25:") || line.startsWith("ETS2=") || line.startsWith("ETS2:") ||
             (line.indexOf('{') >= 0 && (line.indexOf("\"game\"") >= 0 || line.indexOf("\"spd\"") >= 0 || line.indexOf("\"fs25\"") >= 0 || line.indexOf("\"gear\"") >= 0))) {
    int startIdx = line.indexOf('{');
    if (startIdx >= 0) {
      String jsonStr = line.substring(startIdx);
      usb.processJson(jsonStr.c_str(), jsonStr.length());
    }
  } else {
    // Chuyển toàn bộ các lệnh còn lại (motion, servo, pin, config) sang EmojiController
    emojiController.ExecuteCommand(line);
  }
}

static char s0Buf[768];
static uint16_t s0Len = 0;

static void checkSerialCommands() {
  while (Serial0.available()) {
    char c = (char)Serial0.read();
    if (c == '\n') {
      s0Buf[s0Len] = '\0';
      if (s0Len > 0) {
        processSerialLine(String(s0Buf));
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
  Serial0.setRxBufferSize(1024);
  Serial0.begin(115200);
  Serial.begin(115200);
  delay(300);

  LOG_PRINTLN(F("\n====================================================="));
  LOG_PRINTLN(F(" MIU EILIK - TAIBAI EMOJI HARDWARE ARCHITECTURE       "));
  LOG_PRINTLN(F(" OLED Dashboard (ETS2 + FS25) | Dual-core FreeRTOS   "));
  LOG_PRINTLN(F("====================================================="));

  gStateMutex = xSemaphoreCreateMutex();
  display.begin();

  // Khoi dong nhanh man hinh OLED
  LOG_PRINTLN(F("[BOOT] Khoi dong he thong Miu Eilik (OLED Dashboard)..."));
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(20, 15);
  oled.print(F("MIU EILIK READY"));
  oled.setCursor(16, 32);
  oled.print(F("ETS2 & FS25 DASH"));
  oled.setCursor(24, 46);
  oled.print(F("DESKTOP COMPANION"));
  oled.display();
  delay(600);

  emojiController.begin();
  display.initClock();
#if ENABLE_WIFI
  ntp.begin();
  ets2Wifi.begin();
#endif

  xTaskCreatePinnedToCore(taskUsb,     "USB",     4096, NULL, 4, NULL, 0);
  xTaskCreatePinnedToCore(taskDisplay, "Display", 6144, NULL, 4, NULL, 1);
  xTaskCreatePinnedToCore(taskBrain,   "Brain",   4096, NULL, 3, NULL, 1);

  LOG_PRINTLN(F("[OK] All tasks started on 2 cores. System is ready!"));
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
