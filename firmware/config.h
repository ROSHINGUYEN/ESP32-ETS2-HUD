/**
 * =====================================================================================
 *  config.h — Tập trung mọi hằng số cấu hình phần cứng & runtime
 *  Áp dụng cho MIU EILIK ESP32-S3 N16R8 (16MB Flash, 8MB PSRAM OPI)
 * =====================================================================================
 */
#pragma once

// Force-include Arduino.h để có thể undef các macro của ESP-IDF
#ifdef ARDUINO
  // Không tự động undef - để các file khác có thể dùng macro.
  // DisplayTask.h sẽ tự undef sau khi include RoboEyes.
#endif

// =================== PHẦN CỨNG ===================
// Board
#define BOARD_NAME                "ESP32-S3-DevKitC-1-N16R8"
#define BOARD_FLASH_MB            16
#define BOARD_PSRAM_MB            8

// OLED SSD1306 128x64 I2C
#define PIN_OLED_SDA              8
#define PIN_OLED_SCL              9
#define OLED_ADDR                 0x3C
#define SCREEN_W                  128
#define SCREEN_H                  64
#define OLED_I2C_HZ               800000UL   // Có thể thử 400k / 800k / 1M tùy module

// 2 Servo SG90 — PAN (cổ) & TILT (đầu)
#define PIN_SERVO_PAN             4
#define PIN_SERVO_TILT            5
#define SERVO_PWM_FREQ_HZ         50         // 50Hz chuẩn servo
#define SERVO_PULSE_MIN_US        500
#define SERVO_PULSE_MAX_US        2400
#define SERVO_PULSE_CENTER_US     1450       // xung ứng với góc 90°

// Góc giới hạn (độ) - hard limits an toàn
#define PAN_MIN_DEG               40.0f
#define PAN_MAX_DEG               140.0f
#define TILT_MIN_DEG              70.0f
#define TILT_MAX_DEG              115.0f
#define PAN_CENTER_DEG            90.0f
#define TILT_CENTER_DEG           90.0f

// Servo motion
#define SERVO_MAX_SPEED_DPS        180.0f    // độ/giây tối đa khi di chuyển
#define SERVO_ACCEL_DPS2           600.0f    // gia tốc mượt
#define SERVO_IDLE_DETACH_MS       1500      // auto-detach servo sau khi không chuyển động

// IR LM393 (Active LOW) - 2 sensor
#define PIN_IR_PRIMARY             6
#define PIN_IR_SECONDARY           7
#define IR_ACTIVE_LEVEL            LOW
#define IR_DEBOUNCE_MS             25
#define IR_TAP_MAX_MS              400
#define IR_DOUBLE_GAP_MS           600
#define IR_HOLD_MS                 800
#define IR_LONG_HOLD_MS            2500
#define IR_WAVE_MIN_EVENTS         3         // >=3 lần đổi trạng thái trong IR_WAVE_WINDOW_MS
#define IR_WAVE_WINDOW_MS          1000

// USB Native CDC (GPIO 19/20 - mặc định của ESP32-S3)

// Serial0 Debug (UART0)
#define PIN_SERIAL0_TX             43
#define PIN_SERIAL0_RX             44
#define SERIAL0_BAUD               115200

// =================== WIFI (SoftAP) ===================
#define WIFI_AP_SSID               "EILIK_ROBOT"
#define WIFI_AP_PASS               "12345678"   // >= 8 ký tự
#define WIFI_AP_CHANNEL            6
#define WIFI_AP_MAX_CLIENTS        3
#define WIFI_AP_HIDDEN             0

// =================== TASK / TIMING ===================
// Stack size cho từng task (bytes)
#define STACK_SERVO_TASK           4096
#define STACK_DISPLAY_TASK         6144
#define STACK_BRAIN_TASK           4096
#define STACK_PCLINK_TASK          4096
#define STACK_WEB_TASK             8192
#define STACK_OTA_TASK             8192

// Priorities
#define PRIO_SERVO                 6
#define PRIO_DISPLAY               4
#define PRIO_BRAIN                 5
#define PRIO_PCLINK                4
#define PRIO_WEB                   3
#define PRIO_OTA                   2

// Affinity (core)
#define CORE_SERVO                 1
#define CORE_DISPLAY               1
#define CORE_BRAIN                 1
#define CORE_PCLINK                0
#define CORE_WEB                   0
#define CORE_OTA                   0

// FPS
#define DISPLAY_FPS_HZ             30
#define DISPLAY_FPS_SLEEP_HZ       15
#define SERVO_UPDATE_HZ            50
#define BRAIN_UPDATE_HZ            50

// Auto / Manual
#define MANUAL_PRIORITY_TIMEOUT_MS 8000   // Sau 8s không có lệnh thủ công → autonomy hoạt động lại

// Telemetry
#define TELEMETRY_PERIOD_MS        10000

// =================== NVS KEYS ===================
#define NVS_NAMESPACE              "eilik"
#define NVS_KEY_PAN_OFFSET         "pan_off"
#define NVS_KEY_TILT_OFFSET        "tilt_off"
#define NVS_KEY_PAN_MIN            "pan_min"
#define NVS_KEY_PAN_MAX            "pan_max"
#define NVS_KEY_TILT_MIN           "tilt_min"
#define NVS_KEY_TILT_MAX           "tilt_max"
#define NVS_KEY_IR_THRESH          "ir_thr"
#define NVS_KEY_IR_TIMING          "ir_timing"
#define NVS_KEY_OLED_HZ            "oled_hz"
#define NVS_KEY_ROBOT_CFG          "robot_cfg"

// =================== FIRMWARE ===================
#define FIRMWARE_VERSION           "5.1.0"
#define FIRMWARE_BUILD_TIME        __DATE__ " " __TIME__

// =================== USB DEVICE ===================
#ifndef USB_VID
  #define USB_VID  0x303A
#endif
#ifndef USB_PID
  #define USB_PID  0x4001
#endif

// =================== DEBUG ===================
#define DEBUG_LEVEL                2   // 0=off, 1=error, 2=warn, 3=info, 4=verbose
#define DBG_ERROR(fmt, ...)        do { if (1 <= DEBUG_LEVEL) Serial0.printf("[ERR] " fmt "\n", ##__VA_ARGS__); } while(0)
#define DBG_WARN(fmt, ...)         do { if (2 <= DEBUG_LEVEL) Serial0.printf("[WRN] " fmt "\n", ##__VA_ARGS__); } while(0)
#define DBG_INFO(fmt, ...)         do { if (3 <= DEBUG_LEVEL) Serial0.printf("[INF] " fmt "\n", ##__VA_ARGS__); } while(0)
#define DBG_VERBOSE(fmt, ...)      do { if (4 <= DEBUG_LEVEL) Serial0.printf("[VRB] " fmt "\n", ##__VA_ARGS__); } while(0)
