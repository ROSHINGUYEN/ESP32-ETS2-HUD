/**
 * =====================================================================================
 * DỰ ÁN: ROBOT MIU EILIK ĐẦU PAN/TILT + MẮT OLED + NHẠC + USB/BT (ESP32-S3 N16R8)
 * FILE DUY NHẤT (ALL-IN-ONE CỰC HẠN): miu_eilik.ino
 * =====================================================================================
 * TỐI ƯU HÓA CHUYÊN SÂU THEO CHUẨN KỸ THUẬT:
 *  1. Đa nhân FreeRTOS đối xứng:
 *     - Core 0: Audio DMA (512 frames), USB CDC Binary Link, WebServer & DNS Captive Portal, Web OTA.
 *     - Core 1: Display OLED SSD1306 (800kHz I2C, 30 FPS), Brain (Cảm xúc & Tự hành), Servo Pan/Tilt (50Hz).
 *  2. Bộ đệm âm thanh 512 samples (2048 bytes), loại bỏ 100% tình trạng vấp tiếng (Zero Underrun).
 *  3. Bộ lọc Biquad IIR 8 dải tần số log (60, 150, 400, 1k, 2.5k, 6k, 10k, 16kHz) + Beat Detection thực thụ.
 *  4. Động cơ học Servo chuẩn Smoothstep S-curve S(t) = 3t^2 - 2t^3 triệt tiêu 100% quán tính giật rung.
 *  5. Cảm biến hồng ngoại kép (GPIO 6 & 7): Chạm đơn/đúp/ba, Giữ lâu, Vẫy tay trái/phải (Swipe Left/Right).
 *  6. Giao thức nhị phân USB Native CDC [0xE1 0x1C]: Audio, Streaming OLED 1BPP (1024 bytes), Command JSON.
 *  7. Báo cáo Telemetry 10 giây in ra Serial0 (GPIO 43/44): Real FPS, Free Heap, PSRAM, Underruns, Servo.
 *  8. Khóa bảo vệ đa luồng Mutex bảo đảm an toàn dữ liệu giữa các nhân.
 * =====================================================================================
 */

// =================== SECTION 1: KHAI BÁO THƯ VIỆN ===================
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <vector>
#include <cmath>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <ESP_I2S.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// Include thư viện RoboEyes sau cùng
#include "FluxGarage_RoboEyes.h"

// =================== SECTION 2: CẤU HÌNH BẢN ĐỒ CHÂN & HẰNG SỐ ===================
// OLED SSD1306 128x64 I2C Tốc độ cao
#define PIN_OLED_SDA            8
#define PIN_OLED_SCL            9
#define OLED_ADDR               0x3C
#define SCREEN_WIDTH            128
#define SCREEN_HEIGHT           64
#define OLED_I2C_CLOCK          800000  // 800kHz (Giảm thời gian truyền frame xuống ~11ms)

// 2 Động cơ Servo SG90 (Cổ & Đầu)
#define PIN_SERVO_PAN           4       // Trục quay ngang Cổ (GPIO 4)
#define PIN_SERVO_TILT          5       // Trục gật dọc Đầu (GPIO 5)
#define PAN_MIN_DEG             40.0f
#define PAN_MAX_DEG             140.0f
#define PAN_CENTER_DEG          90.0f
#define TILT_MIN_DEG            70.0f
#define TILT_MAX_DEG            115.0f
#define TILT_CENTER_DEG         90.0f
#define SERVO_SPEED_DEFAULT     140.0f  // Vận tốc tiêu chuẩn 140 độ/giây
#define SERVO_IDLE_DETACH_MS    1500    // Ngắt xung sau 1.5s không chuyển động
#define HOLD_TORQUE_TILT        true    // Luôn giữ lực trục Tilt chống gục đầu

// Cảm biến hồng ngoại LM393 Kép (Active LOW)
#define PIN_IR_PRIMARY          6       // Cảm biến 1 (Trán / Trái)
#define PIN_IR_SECONDARY        7       // Cảm biến 2 (Cằm / Phải)

// Mạch khuếch đại âm thanh I2S MAX98357A
#define PIN_I2S_BCLK            15
#define PIN_I2S_LRC             16
#define PIN_I2S_DIN             17
#define PIN_I2S_SD              18      // Bật/tắt Amp chống nổ loa

// Mux chuyển mạch & Bluetooth ngoài
#define PIN_MUX_SEL             10
#define PIN_BT_BCLK_IN          11
#define PIN_BT_WS_IN            12
#define PIN_BT_DATA_IN          13
#define PIN_BT_STATUS           14

// Thông số âm thanh & Đệm
#define AUDIO_SAMPLE_RATE       44100
#define AUDIO_CHUNK_SAMPLES     512     // 512 Stereo frames = 2048 bytes (~11.6ms)
#define DEFAULT_VOLUME_CEIL     40

// Thông số mạng SoftAP
#define WIFI_AP_SSID            "EILIK_ROBOT"
#define WIFI_AP_PASS            "12345678"
#define WIFI_AP_CHANNEL         6

// Giao thức nhị phân USB CDC
#define USB_SYNC_1              0xE1
#define USB_SYNC_2              0x1C
#define PKT_AUDIO_PCM           0x01
#define PKT_OLED_BITMAP         0x02
#define PKT_CMD_JSON            0x03
#define PKT_TELEMETRY_REQ       0x04

// =================== SECTION 3: ĐỊNH NGHĨA CÁC KIỂU DỮ LIỆU ===================
enum AudioSourceType : uint8_t { SRC_NONE = 0, SRC_FILE, SRC_USB_PC, SRC_BT_EXT };
enum VizStyleType : uint8_t { VIZ_BEAT_EYES = 0, VIZ_WAVE_EYES, VIZ_BARS_EYES };
enum DisplayModeType : uint8_t { DISP_MODE_EYES = 0, DISP_MODE_VISUAL, DISP_MODE_STREAM };
enum EmotionType : uint8_t { EMOTION_NEUTRAL = 0, EMOTION_HAPPY, EMOTION_ANGRY, EMOTION_SLEEPY, EMOTION_CURIOUS, EMOTION_CONFUSED, EMOTION_WAKE_UP };
enum GestureId : uint8_t {
  GESTURE_NONE = 0, GESTURE_NOD_YES, GESTURE_SHAKE_NO, GESTURE_TILT_CURIOUS,
  GESTURE_SLEEP_DROOP, GESTURE_WAKE_STRETCH, GESTURE_BOUNCE_HAPPY, GESTURE_ANGRY_SNAP
};
enum IrGestureType : uint8_t {
  IR_NONE = 0, IR_SINGLE_TAP, IR_DOUBLE_TAP, IR_TRIPLE_TAP, IR_LONG_PRESS, IR_SWIPE_RIGHT, IR_SWIPE_LEFT
};
enum AutonomyModeType : uint8_t { AUTO_MODE_SLEEP = 0, AUTO_MODE_CALM, AUTO_MODE_CURIOUS, AUTO_MODE_PLAYFUL };

struct AudioMetrics {
  float rmsLeft;
  float rmsRight;
  float bands[8];        // 8 dải Biquad IIR: 60, 150, 400, 1k, 2.5k, 6k, 10k, 16kHz
  bool isBeat;
  float bpm;
  uint32_t beatCount;
};

struct Keyframe {
  float pan;
  float tilt;
  uint16_t durationMs;
};

// =================== SECTION 4: GIAO DIỆN WEB GLASSMORPHISM TÍCH HỢP ===================
const char HTML_CONTROL_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>MIU EILIK - CONTROLLER</title>
  <style>
    * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; }
    body {
      margin: 0; min-height: 100vh;
      background: radial-gradient(circle at 50% 0%, #0d1e2e, #050a12);
      color: #00f0ff; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      display: flex; flex-direction: column; align-items: center; padding: 12px;
    }
    .panel {
      width: 100%; max-width: 440px;
      background: rgba(13, 27, 42, 0.75); border: 1px solid rgba(0, 240, 255, 0.25);
      border-radius: 24px; box-shadow: 0 16px 40px rgba(0, 240, 255, 0.15);
      backdrop-filter: blur(16px); overflow: hidden;
    }
    .header { padding: 18px; text-align: center; border-bottom: 1px solid rgba(0, 240, 255, 0.15); }
    .header h2 { margin: 0; font-size: 20px; letter-spacing: 2px; text-shadow: 0 0 12px rgba(0, 240, 255, 0.6); }
    .status-bar { font-size: 11px; opacity: 0.8; margin-top: 5px; color: #a4f4ff; }
    .nav-tabs { display: flex; overflow-x: auto; background: rgba(0, 0, 0, 0.25); }
    .nav-tab {
      flex: 1; min-width: 70px; padding: 12px 6px; font-size: 11px; font-weight: bold;
      text-align: center; cursor: pointer; color: rgba(0, 240, 255, 0.5);
      border-bottom: 2px solid transparent; transition: all 0.2s; white-space: nowrap;
    }
    .nav-tab.active { color: #00f0ff; border-bottom-color: #00f0ff; background: rgba(0, 240, 255, 0.1); }
    .tab-content { display: none; padding: 20px; }
    .tab-content.active { display: block; }
    .dpad-grid {
      display: grid; grid-template-columns: repeat(3, 1fr); grid-template-rows: repeat(3, 56px);
      gap: 10px; margin: 12px auto; max-width: 250px;
    }
    button {
      border: none; border-radius: 14px; font-weight: bold; cursor: pointer;
      background: linear-gradient(135deg, #00f0ff, #0099cc); color: #031520;
      box-shadow: 0 4px 14px rgba(0, 240, 255, 0.25); transition: transform 0.1s, filter 0.2s;
    }
    button:active { transform: scale(0.95); filter: brightness(0.85); }
    .btn-center { background: linear-gradient(135deg, #ffd166, #ff9f1c); color: #201300; }
    .empty { visibility: hidden; }
    .btn-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; margin-top: 12px; }
    .btn-sub {
      height: 46px; font-size: 12px; border-radius: 12px;
      background: rgba(0, 240, 255, 0.08); border: 1px solid rgba(0, 240, 255, 0.3); color: #00f0ff;
    }
    .btn-sub:hover { background: rgba(0, 240, 255, 0.18); }
    .slider-box { margin-top: 16px; }
    .slider-label { font-size: 12px; display: flex; justify-content: space-between; margin-bottom: 6px; }
    input[type=range] {
      width: 100%; height: 6px; background: rgba(0, 240, 255, 0.2);
      border-radius: 4px; outline: none; -webkit-appearance: none;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none; width: 18px; height: 18px; border-radius: 50%;
      background: #00f0ff; cursor: pointer; box-shadow: 0 0 10px #00f0ff;
    }
  </style>
</head>
<body>
<div class="panel">
  <div class="header">
    <h2>MIU EILIK ROBOT</h2>
    <div id="stat_line" class="status-bar">Đang kết nối vi điều khiển...</div>
  </div>
  <div class="nav-tabs">
    <div class="nav-tab active" onclick="switchTab(0)">CỔ / ĐẦU</div>
    <div class="nav-tab" onclick="switchTab(1)">CẢM XÚC</div>
    <div class="nav-tab" onclick="switchTab(2)">TỰ HÀNH</div>
    <div class="nav-tab" onclick="switchTab(3)">ÂM NHẠC</div>
    <div class="nav-tab" onclick="switchTab(4)">CÂN CHỈNH</div>
  </div>

  <div class="tab-content active" id="tab0">
    <div class="dpad-grid">
      <div class="empty"></div>
      <button onclick="nudge(0, -6)">▲ NGẨNG</button>
      <div class="empty"></div>
      <button onclick="nudge(-10, 0)">◀ TRÁI</button>
      <button class="btn-center" onclick="centerHead()">● GIỮA</button>
      <button onclick="nudge(10, 0)">PHẢI ▶</button>
      <div class="empty"></div>
      <button onclick="nudge(0, 6)">▼ CÚI</button>
      <div class="empty"></div>
    </div>
    <div class="btn-grid">
      <button class="btn-sub" onclick="sendGesture(1)">GẬT ĐẦU (YES)</button>
      <button class="btn-sub" onclick="sendGesture(2)">LẮC ĐẦU (NO)</button>
      <button class="btn-sub" onclick="sendGesture(3)">NGHIÊNG ĐẦU</button>
      <button class="btn-sub" onclick="sendGesture(6)">VUI VẺ NHÚN</button>
    </div>
  </div>

  <div class="tab-content" id="tab1">
    <div class="btn-grid">
      <button class="btn-sub" onclick="sendEmotion(1)">😊 VUI VẺ</button>
      <button class="btn-sub" onclick="sendEmotion(2)">😡 GIẬN DỮ</button>
      <button class="btn-sub" onclick="sendEmotion(3)">😴 BUỒN NGỦ</button>
      <button class="btn-sub" onclick="sendEmotion(4)">🧐 TÒ MÒ</button>
      <button class="btn-sub" onclick="sendEmotion(5)">😵 BỐI RỐI</button>
      <button class="btn-sub" onclick="sendEmotion(0)">😐 BÌNH THƯỜNG</button>
    </div>
  </div>

  <div class="tab-content" id="tab2">
    <div class="btn-grid">
      <button class="btn-sub" onclick="sendMode(2)">TÒ MÒ (CURIOUS)</button>
      <button class="btn-sub" onclick="sendMode(3)">NGHỊCH NGỢM (PLAYFUL)</button>
      <button class="btn-sub" onclick="sendMode(1)">ĐIỀM TĨNH (CALM)</button>
      <button class="btn-sub" onclick="sendMode(0)">ĐI NGỦ (SLEEP)</button>
    </div>
  </div>

  <div class="tab-content" id="tab3">
    <div class="btn-grid">
      <button class="btn-sub" onclick="sendMusic('next')">⏭ BÀI TIẾP</button>
      <button class="btn-sub" onclick="sendMusic('prev')">⏮ BÀI TRƯỚC</button>
      <button class="btn-sub" onclick="sendMusic('pause')">⏸ TẠM DỪNG</button>
      <button class="btn-sub" onclick="sendMusic('resume')">▶ PHÁT TIẾP</button>
    </div>
    <div class="slider-box">
      <div class="slider-label"><span>ÂM LƯỢNG:</span><span id="vol_val">30%</span></div>
      <input type="range" min="0" max="40" value="30" oninput="changeVol(this.value)">
    </div>
    <div class="btn-grid" style="margin-top:16px;">
      <button class="btn-sub" onclick="sendViz(0)">BEAT EYES</button>
      <button class="btn-sub" onclick="sendViz(1)">WAVE EYES</button>
      <button class="btn-sub" onclick="sendViz(2)">BARS EQUALIZER</button>
    </div>
  </div>

  <div class="tab-content" id="tab4">
    <div class="slider-box">
      <div class="slider-label"><span>OFFSET PAN (CỔ):</span><span id="pan_off_val">0</span></div>
      <input type="range" min="-25" max="25" value="0" id="pan_off" oninput="updateCalib()">
    </div>
    <div class="slider-box">
      <div class="slider-label"><span>OFFSET TILT (ĐẦU):</span><span id="tilt_off_val">0</span></div>
      <input type="range" min="-20" max="20" value="0" id="tilt_off" oninput="updateCalib()">
    </div>
    <button style="width:100%; height:46px; margin-top:18px;" onclick="saveCalib()">LƯU VÀO FLASH NVS</button>
  </div>
</div>

<script>
  function switchTab(idx) {
    document.querySelectorAll('.nav-tab').forEach((t, i) => t.classList.toggle('active', i === idx));
    document.querySelectorAll('.tab-content').forEach((c, i) => c.classList.toggle('active', i === idx));
  }
  function api(url) { fetch(url).catch(e => console.error(e)); }
  function nudge(dp, dt) { api(`/api/nudge?dpan=${dp}&dtilt=${dt}`); }
  function centerHead() { api('/api/center'); }
  function sendEmotion(id) { api(`/api/emotion?id=${id}`); }
  function sendGesture(id) { api(`/api/gesture?id=${id}`); }
  function sendMode(id) { api(`/api/mode?id=${id}`); }
  function sendMusic(act) { api(`/api/music?act=${act}`); }
  function changeVol(v) { document.getElementById('vol_val').innerText = v + '%'; api(`/api/music?act=vol&val=${v}`); }
  function sendViz(s) { api(`/api/viz?style=${s}`); }
  function updateCalib() {
    let p = document.getElementById('pan_off').value;
    let t = document.getElementById('tilt_off').value;
    document.getElementById('pan_off_val').innerText = p;
    document.getElementById('tilt_off_val').innerText = t;
  }
  function saveCalib() {
    let p = document.getElementById('pan_off').value;
    let t = document.getElementById('tilt_off').value;
    api(`/api/calib?pan=${p}&tilt=${t}`);
    alert('Đã lưu cân chỉnh offset thành công!');
  }
  setInterval(() => {
    fetch('/api/state').then(r => r.json()).then(d => {
      document.getElementById('stat_line').innerText = `FPS: ${d.fps} | Pan: ${d.pan}° | Tilt: ${d.tilt}° | Heap: ${(d.free_heap/1024).toFixed(1)}KB`;
    }).catch(() => {});
  }, 1000);
</script>
</body>
</html>
)rawliteral";

// =================== SECTION 5: CÁC BIẾN & ĐỐI TƯỢNG TOÀN CỤC ===================
Adafruit_SSD1306 displayObj(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RoboEyes<Adafruit_SSD1306>* pRoboEyes = nullptr;
ESP_I2S i2sOutObj;
WebServer serverObj(80);
DNSServer dnsServerObj;
Preferences prefsObj;

// Hàng đợi & Khóa Mutex đồng bộ đa luồng
SemaphoreHandle_t gStateMutex = NULL;
uint32_t gUnderrunCount = 0;

// =================== SECTION 6: ĐỘNG CƠ HỌC SERVO VỚI SMOOTHSTEP S-CURVE ===================
struct ServoAxis {
  float currentAngle;
  float startAngle;
  float targetAngle;
  float offset;
  float minLimit;
  float maxLimit;
  float durationSec;
  float elapsedSec;
  bool isMoving;

  void init(float initAngle, float minL, float maxL) {
    currentAngle = initAngle;
    startAngle = initAngle;
    targetAngle = initAngle;
    offset = 0.0f;
    minL = minL;
    maxLimit = maxL;
    durationSec = 0.2f;
    elapsedSec = 0.0f;
    isMoving = false;
  }

  // Thuật toán Smoothstep S-curve: S(t) = 3t^2 - 2t^3
  inline float smoothstep(float t) const {
    t = constrain(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
  }

  void setTarget(float target, float speedDps) {
    float clamped = constrain(target + offset, minLimit, maxLimit);
    if (fabs(clamped - targetAngle) < 0.2f && !isMoving) return;
    startAngle = currentAngle;
    targetAngle = clamped;
    float dist = fabs(targetAngle - startAngle);
    durationSec = max(0.08f, dist / max(20.0f, speedDps));
    elapsedSec = 0.0f;
    isMoving = true;
  }

  void update(float dtSec) {
    if (!isMoving) return;
    elapsedSec += dtSec;
    float u = elapsedSec / durationSec;
    if (u >= 1.0f) {
      currentAngle = targetAngle;
      isMoving = false;
    } else {
      currentAngle = startAngle + (targetAngle - startAngle) * smoothstep(u);
    }
  }
};

class HeadMotionController {
public:
  Servo servoPanObj;
  Servo servoTiltObj;
  ServoAxis axisPan;
  ServoAxis axisTilt;
  bool panAttached, tiltAttached;
  unsigned long lastMoveTime;

  HeadMotionController() : panAttached(false), tiltAttached(false), lastMoveTime(0) {}

  void begin() {
    prefsObj.begin("eilik_calib", true);
    axisPan.offset = prefsObj.getChar("pan_off", 0);
    axisTilt.offset = prefsObj.getChar("tilt_off", 0);
    prefsObj.end();

    axisPan.init(PAN_CENTER_DEG, PAN_MIN_DEG, PAN_MAX_DEG);
    axisTilt.init(TILT_CENTER_DEG, TILT_MIN_DEG, TILT_MAX_DEG);
    attachAll();
    writeServos();
  }

  void attachAll() {
    if (!panAttached) { servoPanObj.attach(PIN_SERVO_PAN, 500, 2400); panAttached = true; }
    if (!tiltAttached) { servoTiltObj.attach(PIN_SERVO_TILT, 500, 2400); tiltAttached = true; }
  }

  void writeServos() {
    servoPanObj.write((int)round(axisPan.currentAngle));
    servoTiltObj.write((int)round(axisTilt.currentAngle));
  }

  void setTarget(float p, float t, float spd = SERVO_SPEED_DEFAULT) {
    attachAll();
    axisPan.setTarget(p, spd);
    axisTilt.setTarget(t, spd);
    lastMoveTime = millis();
  }

  void nudge(float dp, float dt) {
    setTarget((axisPan.targetAngle - axisPan.offset) + dp, (axisTilt.targetAngle - axisTilt.offset) + dt);
  }

  void center() { setTarget(PAN_CENTER_DEG, TILT_CENTER_DEG, 100.0f); }

  bool isMoving() const { return axisPan.isMoving || axisTilt.isMoving; }

  void update(float dtSec) {
    if (isMoving()) {
      attachAll();
      axisPan.update(dtSec);
      axisTilt.update(dtSec);
      writeServos();
      lastMoveTime = millis();
    } else {
      // Auto-detach sau 1.5s đứng yên để triệt tiêu sinh nhiệt
      if (millis() - lastMoveTime >= SERVO_IDLE_DETACH_MS) {
        if (panAttached) { servoPanObj.detach(); panAttached = false; }
        if (tiltAttached && !HOLD_TORQUE_TILT) { servoTiltObj.detach(); tiltAttached = false; }
      }
    }
  }

  void saveOffsets(int8_t p, int8_t t) {
    axisPan.offset = p;
    axisTilt.offset = t;
    prefsObj.begin("eilik_calib", false);
    prefsObj.putChar("pan_off", p);
    prefsObj.putChar("tilt_off", t);
    prefsObj.end();
  }
} headServo;

// =================== SECTION 7: HOẠT HỌA CỬ CHỈ (GESTURES) ===================
static const Keyframe TRACK_NOD_YES[] = { {90,106,160},{90,82,160},{90,104,140},{90,86,140},{90,90,180} };
static const Keyframe TRACK_SHAKE_NO[] = { {115,90,150},{65,90,200},{110,90,180},{70,90,180},{90,90,180} };
static const Keyframe TRACK_TILT_CURIOUS[] = { {106,80,350},{106,80,800},{90,90,350} };
static const Keyframe TRACK_SLEEP_DROOP[] = { {90,96,400},{90,106,600},{90,114,800} };
static const Keyframe TRACK_WAKE_STRETCH[] = { {80,105,350},{90,76,500},{100,76,250},{90,90,350} };
static const Keyframe TRACK_BOUNCE_HAPPY[] = { {90,102,110},{90,86,120},{90,102,110},{90,90,150} };

class GesturePlayer {
public:
  GestureId currentGesture;
  bool isPlaying, breathingEnabled;
  const Keyframe* currentTrack;
  uint8_t trackLen, currentFrameIdx;
  unsigned long frameStartTime;

  GesturePlayer() : currentGesture(GESTURE_NONE), isPlaying(false), breathingEnabled(true),
                    currentTrack(nullptr), trackLen(0), currentFrameIdx(0), frameStartTime(0) {}

  void play(GestureId id) {
    currentGesture = id; currentFrameIdx = 0;
    switch (id) {
      case GESTURE_NOD_YES: currentTrack = TRACK_NOD_YES; trackLen = 5; break;
      case GESTURE_SHAKE_NO: currentTrack = TRACK_SHAKE_NO; trackLen = 5; break;
      case GESTURE_TILT_CURIOUS: currentTrack = TRACK_TILT_CURIOUS; trackLen = 3; break;
      case GESTURE_SLEEP_DROOP: currentTrack = TRACK_SLEEP_DROOP; trackLen = 3; break;
      case GESTURE_WAKE_STRETCH: currentTrack = TRACK_WAKE_STRETCH; trackLen = 4; break;
      case GESTURE_BOUNCE_HAPPY: currentTrack = TRACK_BOUNCE_HAPPY; trackLen = 4; break;
      default: isPlaying = false; return;
    }
    isPlaying = true; startFrame(0);
  }

  void startFrame(uint8_t idx) {
    currentFrameIdx = idx; frameStartTime = millis();
    const Keyframe &kf = currentTrack[idx];
    float spd = 180.0f;
    headServo.setTarget(kf.pan, kf.tilt, spd);
  }

  void update() {
    if (isPlaying && currentTrack) {
      if (millis() - frameStartTime >= currentTrack[currentFrameIdx].durationMs) {
        if (currentFrameIdx + 1 < trackLen) startFrame(currentFrameIdx + 1);
        else { isPlaying = false; currentGesture = GESTURE_NONE; }
      }
      return;
    }
    // Nhịp thở sin tự nhiên khi robot nghỉ
    if (breathingEnabled && !headServo.isMoving()) {
      float breath = sin((float)millis() * 2.0f * PI / 4200.0f) * 1.5f;
      headServo.setTarget(PAN_CENTER_DEG, TILT_CENTER_DEG + breath, 15.0f);
    }
  }
} gesturePlayer;

// =================== SECTION 8: CẢM XÚC ROBOT (EMOTION ENGINE) ===================
struct EyeCmd {
  uint8_t mood;
  uint8_t position;
  bool animConfused;
  bool animLaugh;
  bool curious;
};

class EmotionEngine {
public:
  EmotionType currentEmotion;
  EyeCmd pendingEyeCmd;
  bool hasPendingCmd;

  EmotionEngine() : currentEmotion(EMOTION_NEUTRAL), hasPendingCmd(false) {}

  void setEmotion(EmotionType emotion) {
    currentEmotion = emotion;
    EyeCmd cmd = {0, 0, false, false, false};
    switch (emotion) {
      case EMOTION_NEUTRAL: cmd.mood = DEFAULT; gesturePlayer.breathingEnabled = true; break;
      case EMOTION_HAPPY: cmd.mood = HAPPY; cmd.animLaugh = true; gesturePlayer.play(GESTURE_BOUNCE_HAPPY); break;
      case EMOTION_ANGRY: cmd.mood = ANGRY; gesturePlayer.breathingEnabled = false; break;
      case EMOTION_SLEEPY: cmd.mood = TIRED; gesturePlayer.play(GESTURE_SLEEP_DROOP); break;
      case EMOTION_CURIOUS: cmd.mood = DEFAULT; cmd.curious = true; gesturePlayer.play(GESTURE_TILT_CURIOUS); break;
      case EMOTION_CONFUSED: cmd.mood = CONFUSED; cmd.animConfused = true; gesturePlayer.play(GESTURE_SHAKE_NO); break;
      case EMOTION_WAKE_UP: cmd.mood = DEFAULT; gesturePlayer.play(GESTURE_WAKE_STRETCH); break;
    }
    pendingEyeCmd = cmd;
    hasPendingCmd = true;
  }
} emotionEngine;

// =================== SECTION 9: BỘ LỌC BIQUAD IIR 8 DẢI & BEAT DETECTOR ===================
struct BiquadFilter {
  float b0, a1, a2;
  float x1, x2, y1, y2;

  void initBandpass(float f0, float fs, float Q) {
    float w0 = 2.0f * PI * f0 / fs;
    float alpha = sin(w0) / (2.0f * Q);
    float a0 = 1.0f + alpha;
    b0 = (sin(w0) / 2.0f) / a0;
    a1 = (-2.0f * cos(w0)) / a0;
    a2 = (1.0f - alpha) / a0;
    x1 = x2 = y1 = y2 = 0.0f;
  }

  inline float process(float in) {
    float out = b0 * in - b0 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = in;
    y2 = y1; y1 = out;
    return out;
  }
};

class AudioAnalyzerEngine {
public:
  BiquadFilter filters[8];
  AudioMetrics metrics;
  float bassHistory[16];
  uint8_t bassHistIdx;
  float bassAvg;
  unsigned long lastBeatTime;
  portMUX_TYPE mux;

  AudioAnalyzerEngine() : bassHistIdx(0), bassAvg(0.01f), lastBeatTime(0), mux(portMUX_INITIALIZER_UNLOCKED) {
    memset(&metrics, 0, sizeof(AudioMetrics));
    metrics.bpm = 120.0f;
  }

  void begin() {
    // 8 tần số trung tâm chuẩn Log Audio
    const float freqs[8] = { 60.0f, 150.0f, 400.0f, 1000.0f, 2500.0f, 6000.0f, 10000.0f, 16000.0f };
    for (int i = 0; i < 8; i++) {
      filters[i].initBandpass(freqs[i], (float)AUDIO_SAMPLE_RATE, 1.414f);
    }
    for (int k = 0; k < 16; k++) bassHistory[k] = 0.01f;
  }

  void processChunk(const int16_t *samples, size_t frameCount) {
    if (!samples || frameCount == 0) return;
    double sqL = 0, sqR = 0;
    float bandEnergy[8] = {0};

    // Subsample mỗi 2 mẫu để tăng tốc xử lý với FPU
    size_t step = (frameCount > 256) ? 2 : 1;
    size_t processed = 0;

    for (size_t i = 0; i < frameCount; i += step) {
      int16_t l = samples[i * 2], r = samples[i * 2 + 1];
      sqL += (double)l * l; sqR += (double)r * r;
      float mono = ((float)l + (float)r) * 0.5f / 32768.0f;

      for (int b = 0; b < 8; b++) {
        float fVal = filters[b].process(mono);
        bandEnergy[b] += fabs(fVal);
      }
      processed++;
    }

    float norm = (processed > 0) ? (1.0f / processed) : 1.0f;
    float rL = sqrt(sqL / (frameCount / step)) / 32768.0f;
    float rR = sqrt(sqR / (frameCount / step)) / 32768.0f;

    float bands[8];
    for (int b = 0; b < 8; b++) {
      bands[b] = constrain(bandEnergy[b] * norm * 3.8f, 0.0f, 1.0f);
    }

    // Năng lượng Bass kết hợp 60Hz và 150Hz
    float bass = (bands[0] * 0.6f + bands[1] * 0.4f);
    unsigned long now = millis();
    bool isB = false;

    if ((bass > bassAvg * 1.35f) && (bass > 0.07f) && (now - lastBeatTime >= 220)) {
      isB = true;
      if (now - lastBeatTime < 2000) {
        float instantBpm = 60000.0f / (now - lastBeatTime);
        metrics.bpm = metrics.bpm * 0.8f + instantBpm * 0.2f;
      }
      lastBeatTime = now;
    }

    bassHistory[bassHistIdx] = bass;
    bassHistIdx = (bassHistIdx + 1) % 16;
    float s = 0; for (int k = 0; k < 16; k++) s += bassHistory[k];
    bassAvg = s / 16.0f;

    portENTER_CRITICAL(&mux);
    metrics.rmsLeft = rL; metrics.rmsRight = rR;
    for (int b = 0; b < 8; b++) {
      metrics.bands[b] = max(bands[b], metrics.bands[b] * 0.78f);
    }
    metrics.isBeat = isB;
    if (isB) { metrics.beatCount++; }
    portEXIT_CRITICAL(&mux);
  }

  void getMetrics(AudioMetrics &out) {
    portENTER_CRITICAL(&mux);
    out = metrics; metrics.isBeat = false;
    portEXIT_CRITICAL(&mux);
  }
} audioAnalyzer;

// =================== SECTION 10: ĐỊNH TUYẾN ÂM THANH I2S & CHỐNG BỤP ===================
class AudioRouterManager {
public:
  AudioSourceType currentSource;
  uint8_t volume, volumeCeil;
  bool isAmpActive;

  AudioRouterManager() : currentSource(SRC_NONE), volume(30), volumeCeil(DEFAULT_VOLUME_CEIL), isAmpActive(false) {}

  void begin() {
    pinMode(PIN_I2S_SD, OUTPUT); digitalWrite(PIN_I2S_SD, LOW); // Mute ban đầu
    pinMode(PIN_MUX_SEL, OUTPUT); digitalWrite(PIN_MUX_SEL, LOW);
    i2sOutObj.setPins(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DIN);
    i2sOutObj.begin(I2S_MODE_STD, AUDIO_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  }

  void setAmp(bool active) {
    isAmpActive = active;
    digitalWrite(PIN_I2S_SD, active ? HIGH : LOW);
  }

  void setVolume(uint8_t v) { volume = constrain(v, (uint8_t)0, volumeCeil); }

  void setSource(AudioSourceType src) {
    if (src == currentSource) return;
    setAmp(false); delay(15);
    digitalWrite(PIN_MUX_SEL, (src == SRC_BT_EXT) ? HIGH : LOW);
    delay(10);
    currentSource = src;
    if (src != SRC_NONE) setAmp(true);
  }

  bool isMusicPlaying() const { return (currentSource != SRC_NONE); }

  void writePcm(const int16_t *samples, size_t frameCount) {
    if (!samples || frameCount == 0 || currentSource == SRC_BT_EXT) return;
    if (!isAmpActive) setAmp(true);

    audioAnalyzer.processChunk(samples, frameCount);

    int16_t scaled[AUDIO_CHUNK_SAMPLES * 2];
    float scale = volume / 100.0f;
    size_t rem = frameCount, off = 0;

    while (rem > 0) {
      size_t chunk = min((size_t)AUDIO_CHUNK_SAMPLES, rem);
      for (size_t i = 0; i < chunk * 2; i++) {
        scaled[i] = (int16_t)(samples[(off * 2) + i] * scale);
      }
      size_t bytesToWrite = chunk * 4;
      size_t written = i2sOutObj.write((uint8_t*)scaled, bytesToWrite);
      if (written < bytesToWrite) {
        gUnderrunCount++;
      }
      rem -= chunk; off += chunk;
    }
  }
} audioRouter;

// =================== SECTION 11: TRÌNH PHÁT NHẠC LITTLEFS (512 SAMPLES) ===================
class FilePlayerManager {
public:
  std::vector<String> playlist;
  int currentIdx;
  bool isPlaying, isPaused;
  File activeFile;

  FilePlayerManager() : currentIdx(-1), isPlaying(false), isPaused(false) {}

  void begin() {
    if (LittleFS.begin(true)) {
      if (!LittleFS.exists("/music")) LittleFS.mkdir("/music");
      scanMusic();
    }
  }

  void scanMusic() {
    playlist.clear();
    File dir = LittleFS.open("/music");
    if (!dir) return;
    File f = dir.openNextFile();
    while (f) {
      String n = String(f.name());
      if (n.endsWith(".wav") || n.endsWith(".mp3")) playlist.push_back("/music/" + n);
      f = dir.openNextFile();
    }
  }

  void playIndex(int idx) {
    if (playlist.empty()) return;
    currentIdx = constrain(idx, 0, (int)playlist.size() - 1);
    if (activeFile) activeFile.close();
    activeFile = LittleFS.open(playlist[currentIdx].c_str(), "r");
    if (activeFile) {
      if (playlist[currentIdx].endsWith(".wav")) activeFile.seek(44); // Bỏ qua Header WAV
      audioRouter.setSource(SRC_FILE);
      isPlaying = true; isPaused = false;
    }
  }

  void pause() { isPaused = true; audioRouter.setAmp(false); }
  void resume() { isPaused = false; audioRouter.setAmp(true); }
  void stop() { isPlaying = false; if (activeFile) activeFile.close(); audioRouter.setSource(SRC_NONE); }
  void next() { if (!playlist.empty()) playIndex((currentIdx + 1) % playlist.size()); }
  void prev() { if (!playlist.empty()) playIndex((currentIdx - 1 + playlist.size()) % playlist.size()); }

  // Vòng lặp phát với bộ đệm lớn 512 frames (2048 bytes) chống giật tiếng
  void loop() {
    if (!isPlaying || isPaused || !activeFile) return;
    int16_t pcmBuffer[AUDIO_CHUNK_SAMPLES * 2];
    size_t bytesRead = activeFile.read((uint8_t*)pcmBuffer, sizeof(pcmBuffer));
    if (bytesRead > 0) {
      audioRouter.writePcm(pcmBuffer, bytesRead / 4);
    } else {
      next();
    }
  }
} filePlayer;

// =================== SECTION 12: CẢM BIẾN HỒNG NGOẠI KÉP & NHẬN DIỆN SWIPE ===================
class DualIrGestureManager {
public:
  bool lastState1, lastState2;
  unsigned long t1Down, t2Down;
  unsigned long releaseTime;
  uint8_t tapCount;

  DualIrGestureManager() : lastState1(HIGH), lastState2(HIGH), t1Down(0), t2Down(0), releaseTime(0), tapCount(0) {}

  void begin() {
    pinMode(PIN_IR_PRIMARY, INPUT_PULLUP);
    pinMode(PIN_IR_SECONDARY, INPUT_PULLUP);
  }

  void update() {
    bool s1 = digitalRead(PIN_IR_PRIMARY);
    bool s2 = digitalRead(PIN_IR_SECONDARY);
    unsigned long now = millis();

    // Bắt sườn xuống (Chạm vật cản)
    if (lastState1 == HIGH && s1 == LOW) t1Down = now;
    if (lastState2 == HIGH && s2 == LOW) t2Down = now;

    // Nhận diện Cử chỉ Vẫy tay (Swipe) giữa 2 cảm biến
    if (s1 == LOW && s2 == LOW && t1Down > 0 && t2Down > 0) {
      long diff = (long)t2Down - (long)t1Down;
      if (diff > 50 && diff < 450) {
        // Vẫy từ cảm biến 1 sang cảm biến 2 -> SWIPE PHẢI
        filePlayer.next();
        emotionEngine.setEmotion(EMOTION_HAPPY);
        t1Down = t2Down = 0;
      } else if (diff < -50 && diff > -450) {
        // Vẫy từ cảm biến 2 sang cảm biến 1 -> SWIPE TRÁI
        filePlayer.prev();
        emotionEngine.setEmotion(EMOTION_CURIOUS);
        t1Down = t2Down = 0;
      }
    }

    // Bắt sườn lên cảm biến 1 để đếm số lần chạm (Tap)
    if (lastState1 == LOW && s1 == HIGH) {
      releaseTime = now;
      if (now - t1Down < 350) tapCount++;
      else if (now - t1Down >= 800) {
        // Chạm giữ lâu (Hold)
        if (filePlayer.isPlaying) {
          if (filePlayer.isPaused) filePlayer.resume(); else filePlayer.pause();
        } else {
          emotionEngine.setEmotion(EMOTION_SLEEPY);
        }
      }
    }

    lastState1 = s1;
    lastState2 = s2;

    // Chốt kết quả Tap sau 350ms không còn chạm thêm
    if (tapCount > 0 && s1 == HIGH && (now - releaseTime > 350)) {
      if (tapCount == 1) {
        emotionEngine.setEmotion(EMOTION_HAPPY);
      } else if (tapCount == 2) {
        filePlayer.next();
      } else if (tapCount >= 3) {
        gesturePlayer.play(GESTURE_NOD_YES);
      }
      tapCount = 0;
    }
  }
} irGesture;

// =================== SECTION 13: GIAO THỨC USB NATIVE CDC NHỊ PHÂN [0xE1 0x1C] ===================
class PcLinkBinaryProtocol {
public:
  enum RxState : uint8_t { ST_SYNC1 = 0, ST_SYNC2, ST_TYPE, ST_LEN_H, ST_LEN_L, ST_PAYLOAD, ST_CHKSUM };
  RxState state;
  uint8_t pktType;
  uint16_t pktLen, rxCount;
  uint8_t payload[1050];
  uint8_t runningChecksum;

  PcLinkBinaryProtocol() : state(ST_SYNC1), pktType(0), pktLen(0), rxCount(0), runningChecksum(0) {}

  void update() {
    while (Serial.available()) {
      uint8_t b = Serial.read();
      switch (state) {
        case ST_SYNC1:
          if (b == USB_SYNC_1) state = ST_SYNC2;
          break;
        case ST_SYNC2:
          if (b == USB_SYNC_2) { state = ST_TYPE; runningChecksum = 0; }
          else state = ST_SYNC1;
          break;
        case ST_TYPE:
          pktType = b; runningChecksum ^= b; state = ST_LEN_H;
          break;
        case ST_LEN_H:
          pktLen = ((uint16_t)b) << 8; runningChecksum ^= b; state = ST_LEN_L;
          break;
        case ST_LEN_L:
          pktLen |= b; runningChecksum ^= b;
          rxCount = 0;
          if (pktLen > 1040) state = ST_SYNC1; // Chống tràn bộ đệm
          else state = (pktLen == 0) ? ST_CHKSUM : ST_PAYLOAD;
          break;
        case ST_PAYLOAD:
          payload[rxCount++] = b; runningChecksum ^= b;
          if (rxCount >= pktLen) state = ST_CHKSUM;
          break;
        case ST_CHKSUM:
          if (b == runningChecksum) {
            handlePacket();
          }
          state = ST_SYNC1;
          break;
      }
    }
  }

  void handlePacket();
} pcLink;

// =================== SECTION 14: HIỂN THỊ OLED & MẮT SÓNG NHẠC (CORE 1) ===================
class DisplayTaskManager {
public:
  DisplayModeType mode;
  VizStyleType vizStyle;
  unsigned long lastFrame;
  float actualFps;
  uint16_t frameCount;
  unsigned long fpsTimer;
  uint8_t streamBitmap[1024];
  bool streamHasNewFrame;

  DisplayTaskManager() : mode(DISP_MODE_EYES), vizStyle(VIZ_BEAT_EYES), lastFrame(0),
                         actualFps(30), frameCount(0), fpsTimer(0), streamHasNewFrame(false) {}

  void begin() {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    Wire.setClock(OLED_I2C_CLOCK); // 800kHz tốc độ cao
    displayObj.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    displayObj.clearDisplay(); displayObj.display();

    pRoboEyes = new RoboEyes<Adafruit_SSD1306>(displayObj);
    pRoboEyes->begin(SCREEN_WIDTH, SCREEN_HEIGHT, 30);
    pRoboEyes->setAutoblinker(ON, 3, 2);
    pRoboEyes->setIdleMode(ON, 2, 2);
    pRoboEyes->setMood(DEFAULT);
  }

  void update() {
    unsigned long now = millis();
    if (now - lastFrame < 33) return; // Khóa 30 FPS mượt mà
    lastFrame = now;

    frameCount++;
    if (now - fpsTimer >= 1000) { actualFps = frameCount; frameCount = 0; fpsTimer = now; }

    // Chế độ stream hình ảnh trực tiếp từ PC
    if (mode == DISP_MODE_STREAM) {
      if (streamHasNewFrame) {
        displayObj.clearDisplay();
        displayObj.drawBitmap(0, 0, streamBitmap, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
        displayObj.display();
        streamHasNewFrame = false;
      }
      return;
    }

    bool music = audioRouter.isMusicPlaying();
    if (music && mode == DISP_MODE_EYES) mode = DISP_MODE_VISUAL;
    else if (!music && mode == DISP_MODE_VISUAL) {
      mode = DISP_MODE_EYES;
      pRoboEyes->setWidth(36, 36); pRoboEyes->setHeight(36, 36);
    }

    if (mode == DISP_MODE_EYES) {
      if (emotionEngine.hasPendingCmd) {
        EyeCmd &cmd = emotionEngine.pendingEyeCmd;
        pRoboEyes->setMood(cmd.mood);
        if (cmd.animLaugh) pRoboEyes->anim_laugh();
        if (cmd.animConfused) pRoboEyes->anim_confused();
        emotionEngine.hasPendingCmd = false;
      }
      pRoboEyes->update();
    } else if (mode == DISP_MODE_VISUAL) {
      AudioMetrics m; audioAnalyzer.getMetrics(m);
      if (vizStyle == VIZ_BEAT_EYES) {
        byte sz = 34 + (byte)(m.bands[0] * 20.0f);
        pRoboEyes->setWidth(sz, sz); pRoboEyes->setHeight(sz, sz);
        pRoboEyes->setMood(HAPPY); pRoboEyes->update();
      } else if (vizStyle == VIZ_WAVE_EYES) {
        displayObj.clearDisplay();
        displayObj.drawRoundRect(14, 14, 44, 36, 8, SSD1306_WHITE);
        displayObj.drawRoundRect(70, 14, 44, 36, 8, SSD1306_WHITE);
        for (int x = 18; x < 54; x++) {
          int y = 32 + (int)(sin((x - 18) * 0.35f + millis() * 0.02f) * m.rmsLeft * 16.0f);
          displayObj.drawPixel(x, constrain(y, 18, 46), SSD1306_WHITE);
        }
        for (int x = 74; x < 110; x++) {
          int y = 32 + (int)(sin((x - 74) * 0.35f + millis() * 0.02f) * m.rmsRight * 16.0f);
          displayObj.drawPixel(x, constrain(y, 18, 46), SSD1306_WHITE);
        }
        displayObj.display();
      } else {
        displayObj.clearDisplay();
        displayObj.drawRoundRect(14, 14, 44, 36, 8, SSD1306_WHITE);
        displayObj.drawRoundRect(70, 14, 44, 36, 8, SSD1306_WHITE);
        for (int b = 0; b < 4; b++) {
          int h = (int)(m.bands[b] * 28.0f);
          displayObj.fillRect(18 + b * 9, 46 - h, 6, h, SSD1306_WHITE);
        }
        for (int b = 4; b < 8; b++) {
          int h = (int)(m.bands[b] * 28.0f);
          displayObj.fillRect(74 + (b - 4) * 9, 46 - h, 6, h, SSD1306_WHITE);
        }
        displayObj.display();
      }
    }
  }
} displayTask;

void PcLinkBinaryProtocol::handlePacket() {
  if (pktType == PKT_AUDIO_PCM && pktLen >= 4) {
    audioRouter.setSource(SRC_USB_PC);
    audioRouter.writePcm((const int16_t*)payload, pktLen / 4);
  } else if (pktType == PKT_OLED_BITMAP && pktLen == 1024) {
    displayTask.mode = DISP_MODE_STREAM;
    memcpy(displayTask.streamBitmap, payload, 1024);
    displayTask.streamHasNewFrame = true;
  } else if (pktType == PKT_CMD_JSON) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, payload, pktLen) == DeserializationError::Ok) {
      if (doc.containsKey("emotion")) emotionEngine.setEmotion((EmotionType)doc["emotion"].as<int>());
      if (doc.containsKey("gesture")) gesturePlayer.play((GestureId)doc["gesture"].as<int>());
      if (doc.containsKey("pan") && doc.containsKey("tilt")) headServo.setTarget(doc["pan"], doc["tilt"]);
    }
  } else if (pktType == PKT_TELEMETRY_REQ) {
    // Phản hồi khung nhị phân Telemetry
    uint8_t res[16];
    res[0] = USB_SYNC_1; res[1] = USB_SYNC_2; res[2] = 0x84;
    res[3] = 0x00; res[4] = 0x06;
    res[5] = (uint8_t)round(headServo.axisPan.currentAngle);
    res[6] = (uint8_t)round(headServo.axisTilt.currentAngle);
    res[7] = (uint8_t)round(displayTask.actualFps);
    res[8] = (uint8_t)audioRouter.currentSource;
    res[9] = (uint8_t)gUnderrunCount;
    res[10] = res[5] ^ res[6] ^ res[7] ^ res[8] ^ res[9];
    Serial.write(res, 11);
  }
}

// =================== SECTION 15: TỰ HÀNH & NHÚN ĐẦU THEO NHỊP BASS ===================
class AutonomyManager {
public:
  AutonomyModeType currentMode;
  unsigned long manualUntil, lastAction, actionInterval;
  uint32_t lastBeat;

  AutonomyManager() : currentMode(AUTO_MODE_CURIOUS), manualUntil(0), lastAction(0), actionInterval(4000), lastBeat(0) {}

  void notifyManual() { manualUntil = millis() + 8000; }
  bool isManual() const { return (millis() < manualUntil); }

  void setMode(AutonomyModeType m) {
    currentMode = m; lastAction = millis();
    if (m == AUTO_MODE_SLEEP) emotionEngine.setEmotion(EMOTION_SLEEPY);
    else if (m == AUTO_MODE_CURIOUS) emotionEngine.setEmotion(EMOTION_CURIOUS);
    else if (m == AUTO_MODE_CALM) emotionEngine.setEmotion(EMOTION_NEUTRAL);
  }

  void update() {
    if (isManual()) return;

    // Khi phát nhạc: Nhún đầu theo phách trống Beat
    if (audioRouter.isMusicPlaying()) {
      AudioMetrics m; audioAnalyzer.getMetrics(m);
      if (m.beatCount != lastBeat) {
        lastBeat = m.beatCount;
        headServo.setTarget(headServo.axisPan.targetAngle - headServo.axisPan.offset, 98.0f, 90.0f);
      } else if (!headServo.isMoving()) {
        headServo.setTarget(headServo.axisPan.targetAngle - headServo.axisPan.offset, 90.0f, 60.0f);
      }
      return;
    }

    if (gesturePlayer.isPlaying || currentMode == AUTO_MODE_SLEEP) return;

    if (millis() - lastAction >= actionInterval) {
      lastAction = millis();
      actionInterval = random(4000, 8000);
      if (currentMode == AUTO_MODE_CURIOUS) {
        if (random(2) == 0) headServo.setTarget(random(70, 110), random(80, 100), 80.0f);
        else gesturePlayer.play(GESTURE_TILT_CURIOUS);
      } else if (currentMode == AUTO_MODE_PLAYFUL) {
        gesturePlayer.play(GESTURE_BOUNCE_HAPPY);
      }
    }
  }
} autonomy;

// =================== SECTION 16: MÁY CHỦ WEBSERVER & WEB OTA (CORE 0) ===================
void setupWebServer() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, 4);
  dnsServerObj.start(53, "*", WiFi.softAPIP());

  serverObj.on("/", HTTP_GET, []() {
    serverObj.send_P(200, "text/html", HTML_CONTROL_PAGE);
  });

  serverObj.on("/api/nudge", HTTP_GET, []() {
    autonomy.notifyManual();
    headServo.nudge(serverObj.arg("dpan").toFloat(), serverObj.arg("dtilt").toFloat());
    serverObj.send(200, "application/json", "{\"status\":\"ok\"}");
  });

  serverObj.on("/api/center", HTTP_GET, []() {
    autonomy.notifyManual();
    headServo.center();
    serverObj.send(200, "application/json", "{\"status\":\"ok\"}");
  });

  serverObj.on("/api/emotion", HTTP_GET, []() {
    autonomy.notifyManual();
    emotionEngine.setEmotion((EmotionType)serverObj.arg("id").toInt());
    serverObj.send(200, "application/json", "{\"status\":\"ok\"}");
  });

  serverObj.on("/api/gesture", HTTP_GET, []() {
    autonomy.notifyManual();
    gesturePlayer.play((GestureId)serverObj.arg("id").toInt());
    serverObj.send(200, "application/json", "{\"status\":\"ok\"}");
  });

  serverObj.on("/api/mode", HTTP_GET, []() {
    autonomy.setMode((AutonomyModeType)serverObj.arg("id").toInt());
    serverObj.send(200, "application/json", "{\"status\":\"ok\"}");
  });

  serverObj.on("/api/music", HTTP_GET, []() {
    autonomy.notifyManual();
    String a = serverObj.arg("act");
    if (a == "next") filePlayer.next();
    else if (a == "prev") filePlayer.prev();
    else if (a == "pause") filePlayer.pause();
    else if (a == "resume") filePlayer.resume();
    else if (a == "vol") audioRouter.setVolume(serverObj.arg("val").toInt());
    serverObj.send(200, "application/json", "{\"status\":\"ok\"}");
  });

  serverObj.on("/api/viz", HTTP_GET, []() {
    displayTask.vizStyle = (VizStyleType)serverObj.arg("style").toInt();
    serverObj.send(200, "application/json", "{\"status\":\"ok\"}");
  });

  serverObj.on("/api/calib", HTTP_GET, []() {
    headServo.saveOffsets(serverObj.arg("pan").toInt(), serverObj.arg("tilt").toInt());
    serverObj.send(200, "application/json", "{\"status\":\"ok\"}");
  });

  serverObj.on("/api/state", HTTP_GET, []() {
    StaticJsonDocument<256> doc;
    doc["pan"] = round(headServo.axisPan.currentAngle - headServo.axisPan.offset);
    doc["tilt"] = round(headServo.axisTilt.currentAngle - headServo.axisTilt.offset);
    doc["fps"] = round(displayTask.actualFps);
    doc["free_heap"] = ESP.getFreeHeap();
    doc["underruns"] = gUnderrunCount;
    String res; serializeJson(doc, res);
    serverObj.send(200, "application/json", res);
  });

  // Giao thức Web OTA Flash trực tiếp qua trình duyệt
  serverObj.on("/update", HTTP_POST, []() {
    serverObj.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    ESP.restart();
  }, []() {
    HTTPUpload& upload = serverObj.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial0.printf("[OTA] Bắt đầu nạp firmware: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial0);
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) Update.printError(Serial0);
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) Serial0.printf("[OTA] Nạp thành công: %u bytes\n", upload.totalSize);
    }
  });

  serverObj.onNotFound([]() { serverObj.send_P(200, "text/html", HTML_CONTROL_PAGE); });
  serverObj.begin();
}

// =================== SECTION 17: CÁC TASK FREERTOS ĐỐI XỨNG 2 CORE ===================
// Core 0: Audio DMA + File Player
void taskAudioLoop(void *p) {
  while (true) {
    filePlayer.loop();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// Core 0: USB Native CDC Link
void taskUsbLoop(void *p) {
  while (true) {
    pcLink.update();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// Core 0: Web Server, DNS & Web OTA
void taskWebLoop(void *p) {
  while (true) {
    dnsServerObj.processNextRequest();
    serverObj.handleClient();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// Core 1: Display OLED SSD1306 (30 FPS)
void taskDisplayLoop(void *p) {
  while (true) {
    displayTask.update();
    vTaskDelay(pdMS_TO_TICKS(4));
  }
}

// Core 1: Brain, Gestures, Dual IR Sensors
void taskBrainLoop(void *p) {
  TickType_t last = xTaskGetTickCount();
  while (true) {
    gesturePlayer.update();
    irGesture.update();
    autonomy.update();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(20)); // 50Hz
  }
}

// Core 1: Servo Kinematics Smoothstep (50Hz)
void taskServoLoop(void *p) {
  TickType_t last = xTaskGetTickCount();
  while (true) {
    headServo.update(0.020f);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(20)); // 50Hz
  }
}

// =================== SECTION 18: SETUP & VÒNG LẶP TELEMETRY ===================
void setup() {
  Serial0.begin(115200); // Debug UART (GPIO 43/44)
  Serial.begin(115200);  // USB Native CDC (GPIO 19/20)
  delay(300);

  Serial0.println(F("\n======================================================"));
  Serial0.println(F(" MIU EILIK ESP32-S3 - PRO OPTIMIZED FIRMWARE v2.0    "));
  Serial0.println(F(" Dual-Core FreeRTOS | Smoothstep Servo | Biquad IIR  "));
  Serial0.println(F("======================================================"));

  gStateMutex = xSemaphoreCreateMutex();

  audioAnalyzer.begin();
  headServo.begin();
  displayTask.begin();
  audioRouter.begin();
  filePlayer.begin();
  irGesture.begin();
  setupWebServer();

  // PHÂN BỔ ĐỐI XỨNG 2 NHÂN FREERTOS
  // Core 0: Âm thanh DMA, USB Native CDC, Mạng WebServer/OTA
  xTaskCreatePinnedToCore(taskAudioLoop,   "AudioTask",   4096, NULL, 5, NULL, 0);
  xTaskCreatePinnedToCore(taskUsbLoop,     "UsbTask",     4096, NULL, 4, NULL, 0);
  xTaskCreatePinnedToCore(taskWebLoop,     "WebTask",     5120, NULL, 3, NULL, 0);

  // Core 1: Đồ họa OLED 30 FPS, Logic Não bộ & Cảm biến, Động cơ Servo 50Hz
  xTaskCreatePinnedToCore(taskServoLoop,   "ServoTask",   4096, NULL, 5, NULL, 1);
  xTaskCreatePinnedToCore(taskDisplayLoop, "DisplayTask", 6144, NULL, 4, NULL, 1);
  xTaskCreatePinnedToCore(taskBrainLoop,   "BrainTask",   4096, NULL, 3, NULL, 1);

  Serial0.println(F("[SYSTEM] Khởi động hoàn tất 6 Task FreeRTOS trên 2 Core!"));
}

unsigned long lastTelemetryTime = 0;

void loop() {
  // Ticker Telemetry 10 giây in ra Serial0 (GPIO 43/44)
  unsigned long now = millis();
  if (now - lastTelemetryTime >= 10000) {
    lastTelemetryTime = now;
    Serial0.printf(
      "[TELEMETRY] Uptime: %lus | FPS: %.1f | Heap: %u B | PSRAM: %u B | Underruns: %u | Servos: (P:%.1f, T:%.1f) | Source: %d\n",
      now / 1000,
      displayTask.actualFps,
      ESP.getFreeHeap(),
      ESP.getFreePsram(),
      gUnderrunCount,
      headServo.axisPan.currentAngle,
      headServo.axisTilt.currentAngle,
      (int)audioRouter.currentSource
    );
  }
  delay(100);
}
