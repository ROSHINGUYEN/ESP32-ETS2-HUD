/**
 * OLED Test Sketch - Độc lập, test phần cứng OLED SSD1306 I2C
 * Hoạt động tốt trên cả Arduino IDE và PlatformIO
 *
 * Mở Serial Monitor: 115200 baud
 * Quan sát OLED:
 *   [0-2s]   Full screen WHITE (toàn bộ pixel trắng)
 *   [2-4s]   Full screen BLACK
 *   [4-6s]   Text "OLED OK" + địa chỉ I2C (0x3C / 0x3D)
 *   [6s+]    Lặp lại
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define PIN_OLED_SDA  8
#define PIN_OLED_SCL  9
#define OLED_ADDR     0x3C
#define SCREEN_W      128
#define SCREEN_H      64

#define LOG_PRINT(...)   do { Serial.print(__VA_ARGS__);   Serial0.print(__VA_ARGS__); } while(0)
#define LOG_PRINTLN(...) do { Serial.println(__VA_ARGS__); Serial0.println(__VA_ARGS__); } while(0)
#define LOG_PRINTF(...)  do { Serial.printf(__VA_ARGS__);  Serial0.printf(__VA_ARGS__); } while(0)

Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);
uint8_t actualAddr = OLED_ADDR;

void setup() {
  Serial0.begin(115200);
  Serial.begin(115200);
  delay(1000);
  LOG_PRINTLN(F("\n========================================"));
  LOG_PRINTLN(F("   MIU EILIK - OLED TEST STANDALONE     "));
  LOG_PRINTLN(F("========================================"));

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(100000); // 100kHz chuẩn ổn định
  Wire.setTimeOut(50);

  // I2C scanner
  LOG_PRINTF("[I2C] Scanning SDA=GPIO %d, SCL=GPIO %d...\n", PIN_OLED_SDA, PIN_OLED_SCL);
  uint8_t found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      LOG_PRINTF("  -> Thay thiet bi I2C tai: 0x%02X\n", a);
      found++;
      if (a == 0x3C || a == 0x3D) actualAddr = a;
    }
  }
  LOG_PRINTF("[I2C] Tim thay %d thiet bi.\n", found);

  // Khoi tao SSD1306 voi periphBegin = false de khong reset chan Wire
  bool ok = oled.begin(SSD1306_SWITCHCAPVCC, actualAddr, false, false);
  LOG_PRINTF("[OLED] oled.begin @ 0x%02X -> %s\n", actualAddr, ok ? "OK (SUCCESS)" : "FAIL (THAT BAI)");

  if (!ok) {
    LOG_PRINTLN(F("!!! OLED INIT FAILED - kiem tra nguon VCC, GND, chan SDA/SCL!"));
    while (1) delay(1000);
  }

  LOG_PRINTLN(F("[OLED] Quan sat man hinh:"));
  LOG_PRINTLN(F("  [0-2s] full WHITE"));
  LOG_PRINTLN(F("  [2-4s] full BLACK"));
  LOG_PRINTLN(F("  [4-6s] text 'OLED OK'"));
}

unsigned long phaseStart = 0;
uint8_t phase = 0;

void loop() {
  unsigned long now = millis();
  if (now - phaseStart < 2000) return;
  phaseStart = now;

  if (phase == 0) {
    // Full WHITE
    oled.fillScreen(SSD1306_WHITE);
    oled.display();
    LOG_PRINTLN(F("[OLED] -> full WHITE"));
  } else if (phase == 1) {
    // Full BLACK
    oled.fillScreen(SSD1306_BLACK);
    oled.display();
    LOG_PRINTLN(F("[OLED] -> full BLACK"));
  } else {
    // Text
    oled.clearDisplay();
    oled.drawRoundRect(0, 0, SCREEN_W, SCREEN_H, 4, SSD1306_WHITE);
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(18, 14);
    oled.print(F("OLED OK"));
    oled.setTextSize(1);
    oled.setCursor(24, 38);
    oled.printf("Addr: 0x%02X (128x64)", actualAddr);
    oled.display();
    LOG_PRINTLN(F("[OLED] -> text OLED OK"));
  }
  phase = (phase + 1) % 3;
}
