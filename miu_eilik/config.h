#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <cmath>
#include <time.h>
#include <sys/time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <ArduinoJson.h>

// =====================================================================================
// MACROS: LOGGING RA CA SERIAL (USB CDC) VA SERIAL0 (UART0) CHO ARDUINO IDE MONITOR
// =====================================================================================
#define LOG_PRINT(...)   do { Serial.print(__VA_ARGS__);   Serial0.print(__VA_ARGS__); } while(0)
#define LOG_PRINTLN(...) do { Serial.println(__VA_ARGS__); Serial0.println(__VA_ARGS__); } while(0)
#define LOG_PRINTF(...)  do { Serial.printf(__VA_ARGS__);  Serial0.printf(__VA_ARGS__); } while(0)

// =====================================================================================
// CAU HINH TINH NANG:
// - ENABLE_WIFI = 0: Tat WiFi/HTTPClient/SNTP (tiet kiem 650KB Flash va RAM do dung USB Serial)
// - ENABLE_WIFI = 1: Bat WiFi cho phep dong bo gio qua NTP va keo REST qua mang LAN
// =====================================================================================
#define ENABLE_WIFI             0

// =====================================================================================
// PHẦN CỨNG (PINOUT) - CHUẨN ESP32-S3 & DESK-EMOJI-V3
// =====================================================================================

// OLED SSD1306 (128x64) I2C
#define PIN_OLED_SDA            8
#define PIN_OLED_SCL            9
#define OLED_ADDR               0x3C
#define SCREEN_W                128
#define SCREEN_H                64
#define OLED_I2C_HZ             400000  // 400kHz Fast Mode giam do tre man hinh tu 100ms xuong 22ms, phan hoi tuc thi
#define OLED_I2C_TIMEOUT_MS     30      // timeout tranh treo bus I2C

// =====================================================================================
// CẤU HÌNH CHÂN SERVO - HỖ TRỢ CẢ 2 SƠ ĐỒ PHẦN CỨNG:
// 1. CHUẨN XIAOZHI-DESKEMOJI-TAIBAI (MẶC ĐỊNH THEO REPO GITHUB):
//    - Trục ngang (Pan - X):  GPIO 11
//    - Trục dọc  (Tilt - Y): GPIO 12
// 2. CHUẨN DESK-EMOJI V3 CŨ:
//    - Trục ngang (Pan - X):  GPIO 12
//    - Trục dọc  (Tilt - Y): GPIO 13
// =====================================================================================
#define SERVO_PIN_XIAOZHI_PAN   11
#define SERVO_PIN_XIAOZHI_TILT  12

#define SERVO_PIN_V3_PAN        12
#define SERVO_PIN_V3_TILT       13

// LEDC Hardware PWM configuration (Native ESP-IDF từ xiaozhi-deskemoji-taibai)
#define LEDC_SERVO_MODE         LEDC_LOW_SPEED_MODE
#define LEDC_SERVO_TIMER        LEDC_TIMER_0
#define LEDC_SERVO_BIT_WIDTH    LEDC_TIMER_14_BIT   // 14-bit (0..16383)
#define LEDC_SERVO_FREQ_HZ      50                  // 50Hz chuẩn servo RC
#define LEDC_CHANNEL_PAN        LEDC_CHANNEL_0      // Kênh 0: Pan (Quay ngang)
#define LEDC_CHANNEL_TILT       LEDC_CHANNEL_1      // Kênh 1: Tilt (Lên xuống)

#define SERVO_MIN_PULSEWIDTH    500                 // 500us
#define SERVO_MAX_PULSEWIDTH    2500                // 2500us

#define SERVO_STEP              1
#define STEP                    1
#define SERVO_DELAY_NORMAL      10
#define SERVO_DELAY_FAST        1

// =====================================================================================
// CẤU HÌNH GÓC SERVO CHUẨN MIU EILIK / DESK-EMOJI:
// - Trục X (Pan - quay ngang): Tâm 90°, tự hành trái 1-3°, phải 1-3°
// - Trục Y (Tilt - lên xuống): Cân bằng mặc định ngước 8° lên trời (82°)
//   => Cúi xuống 2-4° (84°..86°), Ngước lên 2-4° (80°..78°)
// - Chu kỳ tự hành ngắt quãng: 20 giây 1 lần (20000ms)
// - Khởi động: Giữ tâm cân bằng 5 giây để ổn định cơ khí servo trước khi loading
// =====================================================================================
#define PAN_CENTER_DEG          90
#define PAN_LEFT_MIN            1
#define PAN_LEFT_MAX            3
#define PAN_RIGHT_MIN           1
#define PAN_RIGHT_MAX           3
#define X_MIN_LIMIT             (PAN_CENTER_DEG - 25)            // 65° (giới hạn cơ khí an toàn)
#define X_MAX_LIMIT             (PAN_CENTER_DEG + 25)            // 115°

#define TILT_UP_DEFAULT_DEG     8                                // Mặc định ngước lên 8°
#define TILT_CENTER_DEG         (90 - TILT_UP_DEFAULT_DEG)       // 82° (cân bằng ngước 8°)
#define TILT_DOWN_MIN           2
#define TILT_DOWN_MAX           4
#define TILT_UP_MIN             2
#define TILT_UP_MAX             4
#define Y_MIN_LIMIT             (TILT_CENTER_DEG - 15)           // 67° (giới hạn an toàn)
#define Y_MAX_LIMIT             (TILT_CENTER_DEG + 15)           // 97°

#define X_OFFSET_DEG            10
#define Y_OFFSET_DEG            10

#define BOOT_SERVO_HOLD_MS      5000                             // 5s giữ tâm cân chỉnh khi khởi động
#define AUTO_ACT_INTERVAL_MS    20000UL                          // 20s quay 1 lần

// Audio I2S
#define PIN_I2S_BCLK            15
#define PIN_I2S_LRC             16
#define PIN_I2S_DIN             17
#define PIN_I2S_SD              18
#define PIN_MUX_SEL             10
#define AUDIO_SAMPLE_RATE       44100
#define AUDIO_CHUNK_SAMPLES     512
#define DEFAULT_VOLUME_CEIL     40

// NVS Keys
#define NVS_NAMESPACE           "miu_eilik"
#define NVS_KEY_PAN_OFF         "pan_off"
#define NVS_KEY_TILT_OFF        "tilt_off"
#define NVS_KEY_PIN_MODE        "pin_mode"       // 0: xiaozhi (11,12), 1: v3 (12,13)
#define NVS_KEY_SERVO_MODE      "servo_mode"     // 0: 180 standard, 1: 360 modded
#define NVS_KEY_VOL             "vol"
#define NVS_KEY_VOL_CEIL        "vol_ceil"

// Giao thức USB
#define USB_SYNC_1              0xE1
#define USB_SYNC_2              0x1C
#define PKT_AUDIO_PCM           0x01
#define PKT_OLED_BITMAP         0x02
#define PKT_CMD_JSON            0x03
#define PKT_TELEMETRY_REQ       0x04
#define PKT_GAME_TELEMETRY      0x05

// =====================================================================================
// ENUMS HỆ THỐNG
// =====================================================================================

enum AudioSource : uint8_t { SRC_NONE = 0, SRC_FILE, SRC_USB_PC, SRC_BT_EXT };
enum VizStyle    : uint8_t { VIZ_BEAT_EYES = 0, VIZ_WAVE_EYES, VIZ_BARS_EYES };
enum DisplayMode : uint8_t { DISP_CLOCK = 0, DISP_EYES = 1, DISP_VISUAL = 2, DISP_STREAM = 3, DISP_GAME = 4 };
enum Emotion     : uint8_t { EMO_NEUTRAL, EMO_HAPPY, EMO_ANGRY, EMO_SLEEPY, EMO_CURIOUS, EMO_CONFUSED, EMO_WAKE_UP, EMO_COUNT };
enum Gesture     : uint8_t {
  GES_NONE = 0,
  GES_NOD_YES,
  GES_SHAKE_NO,
  GES_TILT_CURIOUS,
  GES_SLEEP_DROOP,
  GES_WAKE_STRETCH,
  GES_BOUNCE_HAPPY,
  GES_ANGRY_SNAP,
  GES_HEAD_LOOK_LEFT,
  GES_HEAD_LOOK_RIGHT,
  GES_HEAD_LOOK_UP,
  GES_HEAD_LOOK_DOWN,
  GESTURE_COUNT = 11
};
enum IrEvent     : uint8_t { IR_NONE, IR_TAP, IR_DOUBLE, IR_TRIPLE, IR_HOLD, IR_SWIPE_RIGHT, IR_SWIPE_LEFT };
enum AutoMode    : uint8_t { AUTO_SLEEP, AUTO_CALM, AUTO_CURIOUS, AUTO_PLAYFUL };

// =====================================================================================
// FNV-1a HASH (Tối ưu chuỗi JSON sang enum O(1))
// =====================================================================================

constexpr uint32_t fnv1a(const char* s, size_t i = 0, uint32_t h = 0x811c9dc5) {
  return s[i] ? fnv1a(s, i + 1, (h ^ s[i]) * 0x01000193) : h;
}

namespace CmdHash {
  constexpr uint32_t EMOTION  = fnv1a("emotion");
  constexpr uint32_t GESTURE  = fnv1a("gesture");
  constexpr uint32_t PAN      = fnv1a("pan");
  constexpr uint32_t TILT     = fnv1a("tilt");
  constexpr uint32_t CENTER   = fnv1a("center");
  constexpr uint32_t CALIB_P  = fnv1a("calib_pan");
  constexpr uint32_t CALIB_T  = fnv1a("calib_tilt");
  constexpr uint32_t SOURCE   = fnv1a("source");
  constexpr uint32_t VOL      = fnv1a("vol");
  constexpr uint32_t PLAY     = fnv1a("play");
  constexpr uint32_t PAUSE    = fnv1a("pause");
  constexpr uint32_t NEXT     = fnv1a("next");
  constexpr uint32_t PREV     = fnv1a("prev");
  constexpr uint32_t MODE     = fnv1a("mode");
}

namespace EmoHash {
  constexpr uint32_t EMO_NEUTRAL  = fnv1a("NEUTRAL");
  constexpr uint32_t EMO_HAPPY    = fnv1a("HAPPY");
  constexpr uint32_t EMO_ANGRY    = fnv1a("ANGRY");
  constexpr uint32_t EMO_SLEEPY   = fnv1a("SLEEPY");
  constexpr uint32_t EMO_CURIOUS  = fnv1a("CURIOUS");
  constexpr uint32_t EMO_CONFUSED = fnv1a("CONFUSED");
  constexpr uint32_t EMO_WAKE_UP  = fnv1a("WAKE_UP");
}

namespace ModeHash {
  constexpr uint32_t MODE_SLEEP    = fnv1a("SLEEP");
  constexpr uint32_t MODE_CALM     = fnv1a("CALM");
  constexpr uint32_t MODE_CURIOUS  = fnv1a("CURIOUS");
  constexpr uint32_t MODE_PLAYFUL  = fnv1a("PLAYFUL");
}

static inline uint32_t hashKey(const char* k) {
  uint32_t h = 0x811c9dc5;
  while (*k) h = (h ^ *k++) * 0x01000193;
  return h;
}

// Forward declarations
void processSerialLine(String line);

// Globals dùng chung giữa các task
extern SemaphoreHandle_t gStateMutex;
extern volatile uint32_t gUnderrunCount;

#endif // CONFIG_H
