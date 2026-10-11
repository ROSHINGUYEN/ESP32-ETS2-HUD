#pragma once

#include <Arduino.h>
#include "driver/gpio.h"
#include "driver/ledc.h"

#ifndef BOARD_NAME
#define BOARD_NAME "ESP32-S3N16R8-TAIBAI-EMOJI"
#endif

// Hiển thị màn hình OLED SSD1306 (128x64) I2C
#define DISPLAY_WIDTH          128
#define DISPLAY_HEIGHT         64
#define DISPLAY_I2C_SDA_PIN    8
#define DISPLAY_I2C_SCL_PIN    9
#define DISPLAY_I2C_ADDR       0x3C
#ifndef OLED_I2C_HZ
#define OLED_I2C_HZ            400000
#endif

// Cấu hình chân Servo chuẩn phần cứng Miu Eilik:
// Pan (Quay ngang / Trái Phải) = GPIO 12
// Tilt (Lên Xuống / Gật đầu)   = GPIO 13
#define SERVO_PIN_EILIK_PAN    12
#define SERVO_PIN_EILIK_TILT   13

#define SERVO_PIN_TAIBAI_PAN   11
#define SERVO_PIN_TAIBAI_TILT  12

#define SERVO_HORIZONTAL_PIN   SERVO_PIN_EILIK_PAN
#define SERVO_VERTICAL_PIN     SERVO_PIN_EILIK_TILT

// Cấu hình góc Servo - Chuẩn Taibai gốc
#define SERVO_CENTER_X         90
#define SERVO_CENTER_Y         90
#define SERVO_MIN_X            (SERVO_CENTER_X - 40)  // 50 độ
#define SERVO_MAX_X            (SERVO_CENTER_X + 40)  // 130 độ
#define SERVO_MIN_Y            (SERVO_CENTER_Y - 20)  // 70 độ
#define SERVO_MAX_Y            (SERVO_CENTER_Y + 20)  // 110 độ

#define SERVO_OFFSET_X         40
#define SERVO_OFFSET_Y         25

#define SERVO_STEP             1
#define SERVO_DELAY            10

// 舵机PWM配置 - Chuẩn LEDC Native ESP-IDF 14-bit 50Hz từ Taibai
#define SERVO_MIN_PULSEWIDTH   500        // 500us
#define SERVO_MAX_PULSEWIDTH   2500       // 2500us
#define LEDC_TIMER_BIT_WIDTH   LEDC_TIMER_14_BIT
#define LEDC_TIMER             LEDC_TIMER_0
#define LEDC_MODE              LEDC_LOW_SPEED_MODE
#define LEDC_FREQUENCY         50

#define SERVO_CHANNEL_COUNT    2
const ledc_channel_t servo_channels[SERVO_CHANNEL_COUNT] = {
    LEDC_CHANNEL_0,  // Trục ngang Pan
    LEDC_CHANNEL_1   // Trục dọc Tilt
};

// Boot hold
#define BOOT_SERVO_HOLD_MS     5000
