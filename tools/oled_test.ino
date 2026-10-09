/**
 * OLED Test Sketch - Äá»™c láº­p, chá»‰ Ä‘á»ƒ test pháº§n cá»©ng OLED
 * Flash sketch nÃ y Ä‘á»ƒ xÃ¡c nháº­n OLED + I2C hoáº¡t Ä‘á»™ng
 *
 * CÃ¡ch flash:
 *   1. Táº¡m thá»i Ä‘á»•i tÃªn miu_eilik.ino thÃ nh miu_eilik.ino.bak
 *   2. Copy file nÃ y vÃ o miu_eilik/ vá»›i tÃªn miu_eilik.ino
 *   3. pio run -t upload
 *   4. pio device monitor -p COM5 -b 115200
 *   5. Quan sÃ¡t OLED: pháº£i tháº¥y 3 giai Ä‘oáº¡n:
 *      [0-2s]   full screen WHITE (toÃ n bá»™ pixel tráº¯ng)
 *      [2-4s]   full screen BLACK
 *      [4-6s]   text "OLED OK 0x3C" á»Ÿ giá»¯a
 *      [6s+]    láº·p láº¡i
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

Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);

void setup() {
  Serial0.begin(115200);
  delay(1000);
  Serial0.println("\n=== OLED Test Sketch ===");

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(400000);
  Wire.setTimeOut(50);

  // I2C scanner
  Serial0.printf("[I2C] scanning SDA=%d SCL=%d...\n", PIN_OLED_SDA, PIN_OLED_SCL);
  uint8_t found = 0;
  for (uint8_t a = 0x03; a < 0x78; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial0.printf("[I2C]   0x%02X\n", a);
      found++;
    }
  }
  Serial0.printf("[I2C] %d device(s) found\n", found);

  bool ok = oled.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  Serial0.printf("[OLED] begin @ 0x%02X -> %s\n", OLED_ADDR, ok ? "OK" : "FAIL");
  if (!ok) {
    Serial0.println("!!! OLED INIT FAILED - check wiring/PSRAM");
    while (1) delay(1000);
  }

  Serial0.println("[OLED] Watch the screen:");
  Serial0.println("  [0-2s]   full WHITE");
  Serial0.println("  [2-4s]   full BLACK");
  Serial0.println("  [4-6s]   text 'OLED OK'");
  Serial0.println("  repeat...");
}

unsigned long phaseStart = 0;
uint8_t phase = 0;

void loop() {
  unsigned long now = millis();
  if (now - phaseStart < 2000) return;
  phaseStart = now;

  if (phase == 0) {
    // full WHITE
    oled.fillScreen(SSD1306_WHITE);
    oled.display();
    Serial0.println("[OLED] -> full WHITE");
  } else if (phase == 1) {
    // full BLACK
    oled.fillScreen(SSD1306_BLACK);
    oled.display();
    Serial0.println("[OLED] -> full BLACK");
  } else {
    // text
    oled.clearDisplay();
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(10, 10);
    oled.print(F("OLED OK"));
    oled.setCursor(20, 35);
    oled.printf("0x%02X", OLED_ADDR);
    oled.display();
    Serial0.println("[OLED] -> text OLED OK");
  }
  phase = (phase + 1) % 3;
}
