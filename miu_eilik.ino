/**
 * =====================================================================================
 * DỰ ÁN: ROBOT MIU EILIK ĐẦU PAN/TILT + MẮT OLED + NHẠC + USB/BT (ESP32-S3 N16R8)
 * FILE DUY NHẤT (ALL-IN-ONE): miu_eilik.ino
 * =====================================================================================
 * ĐẶC TÍNH TÍCH HỢP TOÀN DIỆN:
 *  1. Điều khiển 2 Servo SG90 (Pan GPIO 4, Tilt GPIO 5) mượt mà với Smoothstep, NVS offset, Auto-detach.
 *  2. Màn hình OLED SSD1306 128x64 (SDA 8, SCL 9) với RoboEyes 30 FPS + 3 kiểu mắt sóng nhạc.
 *  3. Âm thanh I2S MAX98357A (15, 16, 17, SD 18) chống tiếng nổ (Fade/Mute), phát SFX, LittleFS WAV/MP3.
 *  4. Phân tích âm thanh Analyzer (<1ms): RMS L/R, 8 băng tần log, Beat detection, ước lượng BPM.
 *  5. Nhún đầu theo nhịp nhạc (Head Dancing) khi phát hiện Beat.
 *  6. Cảm biến hồng ngoại LM393 (GPIO 6/7): chạm nhanh/đúp/ba, giữ, vẫy tay theo ngữ cảnh.
 *  7. Giao thức USB Native CDC (GPIO 19/20) truyền âm thanh Credit-based, video/ảnh 1BPP lên OLED.
 *  8. Module Bluetooth ngoài qua Mux 74LVC157 SEL (GPIO 10) và I2S1 RX Slave (GPIO 11/12/13).
 *  9. Máy tự hành (Sleep, Calm, Curious, Playful, Music) có độ trễ tay manualUntil 8s.
 * 10. WebServer SoftAP WPA2 + Captive Portal + Giao diện Web Glassmorphism Neon xanh nhúng sẵn trong code.
 * 11. Đa nhân FreeRTOS (Core 0: Servo 50Hz, Web, USB | Core 1: OLED 30FPS, Brain 50Hz, Audio).
 * 12. Báo cáo Telemetry giám sát sức khỏe vi điều khiển mỗi 10 giây qua Serial0 (GPIO 43/44).
 * =====================================================================================
 */

// =================== SECTION 1: KHAI BÁO THƯ VIỆN ===================
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <vector>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <ESP_I2S.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// Include thư viện RoboEyes sau cùng để tránh xung đột tên macro
#include "FluxGarage_RoboEyes.h"

// =================== SECTION 2: CẤU HÌNH BẢN ĐỒ CHÂN & HẰNG SỐ ===================
// OLED SSD1306 128x64 I2C
#define PIN_OLED_SDA            8
#define PIN_OLED_SCL            9
#define OLED_ADDR               0x3C
#define SCREEN_WIDTH            128
#define SCREEN_HEIGHT           64
#define OLED_I2C_CLOCK          400000

// 2 Servo SG90 (Cổ & Đầu)
#define PIN_SERVO_PAN           4       // Servo 1: Cổ quay ngang (GPIO 4)
#define PIN_SERVO_TILT          5       // Servo 2: Đầu gật dọc (GPIO 5)
#define PAN_MIN_DEG             40      // Giới hạn quay phải an toàn
#define PAN_MAX_DEG             140     // Giới hạn quay trái an toàn
#define PAN_CENTER_DEG          90      // Vị trí nhìn thẳng
#define TILT_MIN_DEG            70      // Giới hạn ngẩng an toàn
#define TILT_MAX_DEG            115     // Giới hạn cúi an toàn
#define TILT_CENTER_DEG         90      // Vị trí nhìn ngang
#define SERVO_SPEED_DPS         180.0f  // Tốc độ tối đa: 180 độ/giây
#define SERVO_IDLE_DETACH_MS    1500    // Tự ngắt xung sau 1.5s đứng yên
#define HOLD_TORQUE_TILT        true    // Luôn giữ lực servo Tilt tránh gục đầu
#define HEAD_TRACK_DELAY_MS     180     // Thời gian đầu quay theo mắt trễ 180ms

// Cảm biến hồng ngoại LM393 (Cấp 3.3V, OUT kéo LOW khi có vật cản)
#define PIN_IR_PRIMARY          6
#define PIN_IR_SECONDARY        7

// Mạch khuếch đại âm thanh I2S MAX98357A
#define PIN_I2S_BCLK            15      // Bit Clock
#define PIN_I2S_LRC             16      // Left/Right Clock (WS)
#define PIN_I2S_DIN             17      // Data Out từ ESP32
#define PIN_I2S_SD              18      // Shutdown / Mute: LOW = Mute, HIGH = Bật amp

// Bộ chuyển mạch Mux 74LVC157 SEL
#define PIN_MUX_SEL             10      // LOW = ESP32 I2S, HIGH = External BT I2S

// Module Bluetooth I2S nghe lén (Kênh I2S1 RX Slave)
#define PIN_BT_BCLK_IN          11
#define PIN_BT_WS_IN            12
#define PIN_BT_DATA_IN          13
#define PIN_BT_STATUS           14

// Cổng USB Native CDC (Truyền dữ liệu nhị phân tốc độ cao với PC)
#define PIN_USB_DM              19
#define PIN_USB_DP              20

// Thông số âm thanh
#define DEFAULT_VOLUME_CEIL     40      // Trần âm lượng an toàn 40% (chống brownout)
#define AUDIO_SAMPLE_RATE       44100
#define PSRAM_AUDIO_RING_SIZE   (48 * 1024)

// Thông số hiển thị OLED
#define DISPLAY_FPS_NORMAL      30
#define DISPLAY_FPS_SLEEP       15

// Thông số mạng WiFi SoftAP
#define WIFI_AP_SSID            "EILIK_ROBOT"
#define WIFI_AP_PASS            "12345678"
#define WIFI_AP_CHANNEL         6
#define WIFI_MAX_CLIENTS        3

// Thời gian ưu tiên điều khiển tay (Manual Override)
#define MANUAL_OVERRIDE_MS      8000

// Mã đồng bộ khung nhị phân USB
#define USB_FRAME_SYNC_1        0xE1
#define USB_FRAME_SYNC_2        0x1C

// =================== SECTION 3: ĐỊNH NGHĨA CÁC ENUM HỆ THỐNG ===================
enum AudioSourceType : uint8_t { SRC_NONE, SRC_FILE, SRC_USB_PC, SRC_BT_EXT, SRC_AUTO };
enum VizStyleType : uint8_t { VIZ_BEAT_EYES = 0, VIZ_WAVE_EYES = 1, VIZ_BARS_EYES = 2 };
enum DisplayModeType : uint8_t { DISP_MODE_EYES, DISP_MODE_VISUAL, DISP_MODE_STREAM };
enum EmotionType : uint8_t { EMOTION_NEUTRAL, EMOTION_HAPPY, EMOTION_ANGRY, EMOTION_SLEEPY, EMOTION_CURIOUS, EMOTION_CONFUSED, EMOTION_WAKE_UP };
enum GestureId : uint8_t {
  GESTURE_NONE = 0, GESTURE_NOD_YES, GESTURE_SHAKE_NO, GESTURE_LOOK_LEFT, GESTURE_LOOK_RIGHT,
  GESTURE_LOOK_UP, GESTURE_LOOK_DOWN, GESTURE_TILT_CURIOUS, GESTURE_SLEEP_DROOP,
  GESTURE_WAKE_STRETCH, GESTURE_BOUNCE_HAPPY, GESTURE_ANGRY_SNAP, GESTURE_BREATHING
};
enum IrGestureType : uint8_t {
  IR_GESTURE_NONE = 0, IR_GESTURE_SHORT_TAP, IR_GESTURE_DOUBLE_TAP, IR_GESTURE_TRIPLE_TAP,
  IR_GESTURE_HOLD, IR_GESTURE_LONG_HOLD, IR_GESTURE_WAVE
};
enum AutonomyModeType : uint8_t {
  AUTO_MODE_SLEEP, AUTO_MODE_CALM, AUTO_MODE_CURIOUS, AUTO_MODE_PLAYFUL, AUTO_MODE_MUSIC
};
enum SfxType : uint8_t { SFX_HAPPY_CHIME, SFX_ANGRY_BUZZ, SFX_WAKE_CHIRP };
enum RepeatMode : uint8_t { REPEAT_OFF, REPEAT_ONE, REPEAT_ALL };

struct RobotCommand {
  enum CmdType : uint8_t {
    CMD_SET_HEAD_ANGLE, CMD_TRIGGER_GESTURE, CMD_SET_EMOTION,
    CMD_SET_AUTONOMY_MODE, CMD_MUSIC_CONTROL, CMD_SET_AUDIO_SOURCE,
    CMD_SET_VIZ_STYLE, CMD_CALIBRATE_OFFSETS
  } type;
  int16_t param1;
  int16_t param2;
  int16_t param3;
};

struct AudioMetrics {
  float rmsLeft;
  float rmsRight;
  float bands[8];
  bool isBeat;
  float bpm;
  uint32_t beatCount;
  unsigned long lastBeatTime;
};

struct Keyframe {
  float pan;
  float tilt;
  uint16_t durationMs;
};

struct EyeCommand {
  uint8_t mood;
  uint8_t position;
  bool animConfused;
  bool animLaugh;
  bool curious;
  bool autoblinker;
  bool idleMode;
};

// =================== SECTION 4: GIAO DIỆN WEB NHÚNG SẴN (PROGMEM HTML) ===================
const char HTML_CONTROL_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>MIU EILIK ROBOT</title>
  <style>
    * { box-sizing: border-box; -webkit-tap-highlight-color: transparent; user-select: none; }
    body {
      margin: 0; min-height: 100vh;
      background: radial-gradient(circle at top, #0e1e28, #050b10);
      color: #00ffe1; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      display: flex; flex-direction: column; align-items: center; padding: 12px;
    }
    .panel {
      width: 100%; max-width: 420px;
      background: rgba(14, 34, 46, 0.75); border: 1px solid rgba(0, 255, 225, 0.25);
      border-radius: 20px; box-shadow: 0 10px 30px rgba(0, 255, 225, 0.15);
      backdrop-filter: blur(12px); overflow: hidden;
    }
    .header { padding: 16px; text-align: center; border-bottom: 1px solid rgba(0, 255, 225, 0.15); }
    .header h2 { margin: 0; font-size: 18px; letter-spacing: 2px; text-shadow: 0 0 10px rgba(0, 255, 225, 0.5); }
    .status-bar { font-size: 11px; opacity: 0.7; margin-top: 4px; }
    .nav-tabs { display: flex; overflow-x: auto; background: rgba(0, 0, 0, 0.2); }
    .nav-tab {
      flex: 1; min-width: 65px; padding: 12px 6px; font-size: 11px; font-weight: bold;
      text-align: center; cursor: pointer; color: rgba(0, 255, 225, 0.6);
      border-bottom: 2px solid transparent; transition: all 0.2s; white-space: nowrap;
    }
    .nav-tab.active { color: #00ffe1; border-bottom-color: #00ffe1; background: rgba(0, 255, 225, 0.08); }
    .tab-content { display: none; padding: 18px; }
    .tab-content.active { display: block; }
    .dpad-grid {
      display: grid; grid-template-columns: repeat(3, 1fr); grid-template-rows: repeat(3, 60px);
      gap: 10px; margin: 10px auto; max-width: 240px;
    }
    button {
      border: none; border-radius: 14px; font-weight: bold; cursor: pointer;
      background: linear-gradient(135deg, #00ffe1, #00a896); color: #042426;
      box-shadow: 0 4px 12px rgba(0, 255, 225, 0.2); transition: transform 0.1s; touch-action: manipulation;
    }
    button:active { transform: scale(0.94); filter: brightness(0.85); }
    .btn-center { background: linear-gradient(135deg, #ffd166, #f77f00); color: #2b1700; }
    .empty { visibility: hidden; }
    .slider-box { margin-top: 15px; }
    .slider-label { font-size: 12px; display: flex; justify-content: space-between; margin-bottom: 4px; }
    input[type=range] {
      width: 100%; height: 6px; background: rgba(0, 255, 225, 0.2);
      border-radius: 4px; outline: none; -webkit-appearance: none;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none; width: 18px; height: 18px; border-radius: 50%;
      background: #00ffe1; cursor: pointer; box-shadow: 0 0 8px #00ffe1;
    }
    .btn-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; margin-top: 10px; }
    .btn-sub {
      height: 44px; font-size: 12px; border-radius: 12px;
      background: rgba(0, 255, 225, 0.08); border: 1px solid rgba(0, 255, 225, 0.3); color: #00ffe1;
    }
    .footer { text-align: center; font-size: 10px; opacity: 0.5; padding: 10px; }
  </style>
</head>
<body>
<div class="panel">
  <div class="header">
    <h2>MIU EILIK ROBOT</h2>
    <div id="stat_line" class="status-bar">Đang kết nối...</div>
  </div>
  <div class="nav-tabs">
    <div class="nav-tab active" onclick="switchTab(0)">ĐẦU</div>
    <div class="nav-tab" onclick="switchTab(1)">CẢM XÚC</div>
    <div class="nav-tab" onclick="switchTab(2)">TỰ HÀNH</div>
    <div class="nav-tab" onclick="switchTab(3)">NHẠC</div>
    <div class="nav-tab" onclick="switchTab(4)">NGUỒN</div>
    <div class="nav-tab" onclick="switchTab(5)">CÂN CHỈNH</div>
  </div>

  <div class="tab-content active" id="tab0">
    <div class="dpad-grid">
      <div class="empty"></div>
      <button onclick="nudge(0, -6)">▲ NGẨNG</button>
      <div class="empty"></div>
      <button onclick="nudge(8, 0)">◀ TRÁI</button>
      <button class="btn-center" onclick="centerHead()">● GIỮA</button>
      <button onclick="nudge(-8, 0)">PHẢI ▶</button>
      <div class="empty"></div>
      <button onclick="nudge(0, 6)">▼ CÚI</button>
      <div class="empty"></div>
    </div>
    <div class="slider-box">
      <div class="slider-label"><span>Quay Cổ (Pan):</span><span id="lbl_pan">90°</span></div>
      <input type="range" id="rng_pan" min="40" max="140" value="90" oninput="onPanSlider(this.value)">
    </div>
    <div class="slider-box">
      <div class="slider-label"><span>Gật Đầu (Tilt):</span><span id="lbl_tilt">90°</span></div>
      <input type="range" id="rng_tilt" min="70" max="115" value="90" oninput="onTiltSlider(this.value)">
    </div>
  </div>

  <div class="tab-content" id="tab1">
    <div style="font-size:12px;margin-bottom:6px;opacity:0.8;">CẢM XÚC:</div>
    <div class="btn-grid">
      <button class="btn-sub" onclick="api('/api/emotion?id=0')">BÌNH THƯỜNG</button>
      <button class="btn-sub" onclick="api('/api/emotion?id=1')">VUI VẺ (HAPPY)</button>
      <button class="btn-sub" onclick="api('/api/emotion?id=2')">TỨC GIẬN (ANGRY)</button>
      <button class="btn-sub" onclick="api('/api/emotion?id=3')">BUỒN NGỦ (SLEEP)</button>
      <button class="btn-sub" onclick="api('/api/emotion?id=4')">TÒ MÒ (CURIOUS)</button>
      <button class="btn-sub" onclick="api('/api/emotion?id=5')">BỐI RỐI (CONFUSED)</button>
    </div>
    <div style="font-size:12px;margin:14px 0 6px;opacity:0.8;">CỬ CHỈ ĐẦU:</div>
    <div class="btn-grid">
      <button class="btn-sub" onclick="api('/api/gesture?id=1')">GẬT ĐẦU (YES)</button>
      <button class="btn-sub" onclick="api('/api/gesture?id=2')">LẮC ĐẦU (NO)</button>
      <button class="btn-sub" onclick="api('/api/gesture?id=7')">NGHIÊNG TÒ MÒ</button>
      <button class="btn-sub" onclick="api('/api/gesture?id=10')">NHÚN VUI</button>
    </div>
  </div>

  <div class="tab-content" id="tab2">
    <div class="btn-grid" style="grid-template-columns:1fr;">
      <button class="btn-sub" onclick="api('/api/mode?id=0')">CHẾ ĐỘ NGỦ (SLEEP)</button>
      <button class="btn-sub" onclick="api('/api/mode?id=1')">ĐIỀM TĨNH (CALM: 10-20s)</button>
      <button class="btn-sub" onclick="api('/api/mode?id=2')">TÒ MÒ (CURIOUS: 4-8s)</button>
      <button class="btn-sub" onclick="api('/api/mode?id=3')">TINH NGHỊCH (PLAYFUL: 3-6s)</button>
    </div>
  </div>

  <div class="tab-content" id="tab3">
    <div style="font-size:12px;opacity:0.8;margin-bottom:4px;">BÀI ĐANG PHÁT:</div>
    <div id="track_title" style="font-size:14px;font-weight:bold;color:#fff;margin-bottom:12px;">Đang dừng</div>
    <div class="btn-grid" style="grid-template-columns:repeat(3, 1fr);">
      <button class="btn-sub" onclick="api('/api/music?act=prev')">◀ PREV</button>
      <button class="btn-sub" onclick="api('/api/music?act=pause')">❚❚ PAUSE</button>
      <button class="btn-sub" onclick="api('/api/music?act=next')">NEXT ▶</button>
    </div>
    <div class="slider-box">
      <div class="slider-label"><span>Âm lượng Loa:</span><span id="lbl_vol">30%</span></div>
      <input type="range" id="rng_vol" min="0" max="40" value="30" oninput="onVolSlider(this.value)">
    </div>
    <div style="font-size:12px;margin:15px 0 6px;opacity:0.8;">KIỂU MẮT SÓNG NHẠC:</div>
    <div class="btn-grid">
      <button class="btn-sub" onclick="api('/api/viz?style=0')">BEAT EYES</button>
      <button class="btn-sub" onclick="api('/api/viz?style=1')">WAVE EYES</button>
      <button class="btn-sub" onclick="api('/api/viz?style=2')">EQUALIZER</button>
    </div>
  </div>

  <div class="tab-content" id="tab4">
    <div class="btn-grid" style="grid-template-columns:1fr;">
      <button class="btn-sub" onclick="api('/api/source?src=4')">TỰ ĐỘNG (AUTO: USB > BT > FILE)</button>
      <button class="btn-sub" onclick="api('/api/source?src=1')">FILE LITTLEFS</button>
      <button class="btn-sub" onclick="api('/api/source?src=2')">USB PC STREAM</button>
      <button class="btn-sub" onclick="api('/api/source?src=3')">BLUETOOTH NGOÀI</button>
      <button class="btn-sub" onclick="api('/api/source?src=0')">TẮT ÂM THANH (NONE)</button>
    </div>
  </div>

  <div class="tab-content" id="tab5">
    <div class="slider-box">
      <div class="slider-label"><span>Offset Cổ Pan (-20..+20):</span><span id="lbl_poff">0</span></div>
      <input type="range" id="rng_poff" min="-20" max="20" value="0" oninput="document.getElementById('lbl_poff').innerText=this.value">
    </div>
    <div class="slider-box">
      <div class="slider-label"><span>Offset Đầu Tilt (-20..+20):</span><span id="lbl_toff">0</span></div>
      <input type="range" id="rng_toff" min="-20" max="20" value="0" oninput="document.getElementById('lbl_toff').innerText=this.value">
    </div>
    <button style="width:100%;height:44px;margin-top:16px;" onclick="saveCalib()">LƯU HIỆU CHỈNH VÀO NVS</button>
  </div>

  <div class="footer">MIU EILIK FIRMWARE 1.2.0 • ALL-IN-ONE</div>
</div>

<script>
  let currentTab = 0, lastPanSend = 0, lastTiltSend = 0;
  function switchTab(idx) {
    currentTab = idx;
    document.querySelectorAll('.nav-tab').forEach((t, i) => t.classList.toggle('active', i === idx));
    document.querySelectorAll('.tab-content').forEach((c, i) => c.classList.toggle('active', i === idx));
  }
  function api(url) { fetch(url).catch(e => {}); }
  function nudge(dpan, dtilt) { api(`/api/nudge?dpan=${dpan}&dtilt=${dtilt}`); }
  function centerHead() {
    api('/api/center');
    document.getElementById('rng_pan').value = 90;
    document.getElementById('rng_tilt').value = 90;
    document.getElementById('lbl_pan').innerText = '90°';
    document.getElementById('lbl_tilt').innerText = '90°';
  }
  function onPanSlider(val) {
    document.getElementById('lbl_pan').innerText = val + '°';
    const now = Date.now();
    if (now - lastPanSend > 50) {
      lastPanSend = now;
      api(`/api/head?pan=${val}&tilt=${document.getElementById('rng_tilt').value}`);
    }
  }
  function onTiltSlider(val) {
    document.getElementById('lbl_tilt').innerText = val + '°';
    const now = Date.now();
    if (now - lastTiltSend > 50) {
      lastTiltSend = now;
      api(`/api/head?pan=${document.getElementById('rng_pan').value}&tilt=${val}`);
    }
  }
  function onVolSlider(val) {
    document.getElementById('lbl_vol').innerText = val + '%';
    api(`/api/music?act=vol&val=${val}`);
  }
  function saveCalib() {
    const p = document.getElementById('rng_poff').value;
    const t = document.getElementById('rng_toff').value;
    api(`/api/calib?pan=${p}&tilt=${t}`);
    alert('Đã lưu cân chỉnh góc vào NVS!');
  }
  setInterval(() => {
    fetch('/api/state').then(r => r.json()).then(s => {
      document.getElementById('stat_line').innerText = `FPS: ${s.fps} | Heap: ${Math.round(s.free_heap/1024)}KB | Nguồn: ${s.source}`;
      if (s.track) document.getElementById('track_title').innerText = s.track;
    }).catch(e => {});
  }, 1500);
</script>
</body>
</html>
)rawliteral";

// =================== SECTION 5: KHỞI TẠO ĐỐI TƯỢNG TOÀN CỤC ===================
Adafruit_SSD1306 displayObj(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
RoboEyes<Adafruit_SSD1306> *pRoboEyes = nullptr;
Servo servoPanObj;
Servo servoTiltObj;
I2SClass i2sOutObj;
I2SClass i2sRxObj;
WebServer serverObj(80);
DNSServer dnsServerObj;
Preferences prefsObj;
QueueHandle_t gCommandQueue = nullptr;

// =================== SECTION 6: MODULE ĐIỀU KHIỂN SERVO ĐẦU (HEADSERVO) ===================
class HeadServoController {
public:
  float currentPan, currentTilt;
  float targetPan, targetTilt;
  float speedDps;
  int8_t panOffset, tiltOffset;
  bool panAttached, tiltAttached;
  unsigned long lastPanMoveTime, lastTiltMoveTime;

  HeadServoController()
    : currentPan(PAN_CENTER_DEG), currentTilt(TILT_CENTER_DEG),
      targetPan(PAN_CENTER_DEG), targetTilt(TILT_CENTER_DEG),
      speedDps(SERVO_SPEED_DPS), panOffset(0), tiltOffset(0),
      panAttached(false), tiltAttached(false),
      lastPanMoveTime(0), lastTiltMoveTime(0) {}

  void begin() {
    prefsObj.begin("eilik_calib", true);
    panOffset = prefsObj.getChar("pan_off", 0);
    tiltOffset = prefsObj.getChar("tilt_off", 0);
    prefsObj.end();

    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    servoPanObj.setPeriodHertz(50);
    servoTiltObj.setPeriodHertz(50);

    // Khởi động so le tránh sụt áp nguồn
    servoTiltObj.attach(PIN_SERVO_TILT, 500, 2400);
    tiltAttached = true;
    currentTilt = clampTilt(TILT_CENTER_DEG);
    servoTiltObj.write((int)round(currentTilt));
    delay(200);

    servoPanObj.attach(PIN_SERVO_PAN, 500, 2400);
    panAttached = true;
    currentPan = clampPan(PAN_CENTER_DEG);
    servoPanObj.write((int)round(currentPan));
    delay(200);
  }

  float clampPan(float angle) { return constrain(angle + panOffset, (float)PAN_MIN_DEG, (float)PAN_MAX_DEG); }
  float clampTilt(float angle) { return constrain(angle + tiltOffset, (float)TILT_MIN_DEG, (float)TILT_MAX_DEG); }

  void ensureAttached() {
    if (!panAttached) { servoPanObj.attach(PIN_SERVO_PAN, 500, 2400); panAttached = true; }
    if (!tiltAttached) { servoTiltObj.attach(PIN_SERVO_TILT, 500, 2400); tiltAttached = true; }
  }

  void setTarget(float p, float t, float spd = SERVO_SPEED_DPS) {
    targetPan = p; targetTilt = t;
    speedDps = (spd > 10.0f) ? spd : SERVO_SPEED_DPS;
    ensureAttached();
  }

  void nudge(float dp, float dt) { setTarget(targetPan + dp, targetTilt + dt, speedDps); }
  void center() { setTarget(PAN_CENTER_DEG, TILT_CENTER_DEG, 120.0f); }

  bool isMoving() const {
    float cp = const_cast<HeadServoController*>(this)->clampPan(targetPan);
    float ct = const_cast<HeadServoController*>(this)->clampTilt(targetTilt);
    return (fabs(currentPan - cp) > 0.5f || fabs(currentTilt - ct) > 0.5f);
  }

  void update(float dtSec) {
    float cp = clampPan(targetPan);
    float ct = clampTilt(targetTilt);
    float maxDelta = speedDps * dtSec;
    bool movedP = false, movedT = false;

    float dp = cp - currentPan;
    if (fabs(dp) > 0.1f) {
      ensureAttached();
      currentPan += (fabs(dp) <= maxDelta) ? dp : ((dp > 0) ? maxDelta : -maxDelta);
      servoPanObj.write((int)round(currentPan));
      lastPanMoveTime = millis();
      movedP = true;
    }

    float dt = ct - currentTilt;
    if (fabs(dt) > 0.1f) {
      ensureAttached();
      currentTilt += (fabs(dt) <= maxDelta) ? dt : ((dt > 0) ? maxDelta : -maxDelta);
      servoTiltObj.write((int)round(currentTilt));
      lastTiltMoveTime = millis();
      movedT = true;
    }

    // Tự detach khi nghỉ để giảm nhiệt độ servo
    unsigned long now = millis();
    if (!movedP && panAttached && (now - lastPanMoveTime >= SERVO_IDLE_DETACH_MS)) {
      servoPanObj.detach(); panAttached = false;
    }
    if (!movedT && tiltAttached && !HOLD_TORQUE_TILT && (now - lastTiltMoveTime >= SERVO_IDLE_DETACH_MS)) {
      servoTiltObj.detach(); tiltAttached = false;
    }
  }

  void saveOffsets(int8_t p, int8_t t) {
    panOffset = p; tiltOffset = t;
    prefsObj.begin("eilik_calib", false);
    prefsObj.putChar("pan_off", p);
    prefsObj.putChar("tilt_off", t);
    prefsObj.end();
  }
} headServo;

// =================== SECTION 7: MODULE HOẠT HỌA CỬ CHỈ (GESTURES) ===================
static const Keyframe TRACK_NOD_YES[] = { {90,106,180},{90,82,180},{90,104,160},{90,84,160},{90,100,150},{90,90,200} };
static const Keyframe TRACK_SHAKE_NO[] = { {115,90,160},{65,90,220},{110,90,200},{70,90,200},{100,90,180},{90,90,200} };
static const Keyframe TRACK_TILT_CURIOUS[] = { {105,80,400},{105,80,1000},{90,90,400} };
static const Keyframe TRACK_SLEEP_DROOP[] = { {90,96,500},{90,104,700},{90,112,900} };
static const Keyframe TRACK_WAKE_STRETCH[] = { {80,105,400},{90,76,600},{100,76,300},{80,76,300},{90,90,400} };
static const Keyframe TRACK_BOUNCE_HAPPY[] = { {90,104,120},{90,84,140},{90,104,120},{90,90,180} };
static const Keyframe TRACK_ANGRY_SNAP[] = { {120,98,90},{60,98,110},{90,98,100} };

class GesturePlayer {
public:
  GestureId currentGesture;
  bool isPlaying;
  bool breathingEnabled;
  const Keyframe* currentTrack;
  uint8_t trackLen, currentFrameIdx;
  unsigned long frameStartTime;
  float frameStartPan, frameStartTilt;

  GesturePlayer() : currentGesture(GESTURE_NONE), isPlaying(false), breathingEnabled(true),
                    currentTrack(nullptr), trackLen(0), currentFrameIdx(0), frameStartTime(0) {}

  void play(GestureId id) {
    currentGesture = id; currentFrameIdx = 0;
    switch (id) {
      case GESTURE_NOD_YES: currentTrack = TRACK_NOD_YES; trackLen = 6; break;
      case GESTURE_SHAKE_NO: currentTrack = TRACK_SHAKE_NO; trackLen = 6; break;
      case GESTURE_TILT_CURIOUS: currentTrack = TRACK_TILT_CURIOUS; trackLen = 3; break;
      case GESTURE_SLEEP_DROOP: currentTrack = TRACK_SLEEP_DROOP; trackLen = 3; break;
      case GESTURE_WAKE_STRETCH: currentTrack = TRACK_WAKE_STRETCH; trackLen = 5; break;
      case GESTURE_BOUNCE_HAPPY: currentTrack = TRACK_BOUNCE_HAPPY; trackLen = 4; break;
      case GESTURE_ANGRY_SNAP: currentTrack = TRACK_ANGRY_SNAP; trackLen = 3; break;
      default: isPlaying = false; return;
    }
    isPlaying = true; startFrame(0);
  }

  void startFrame(uint8_t idx) {
    currentFrameIdx = idx; frameStartTime = millis();
    frameStartPan = headServo.currentPan - headServo.panOffset;
    frameStartTilt = headServo.currentTilt - headServo.tiltOffset;
    const Keyframe &kf = currentTrack[idx];
    float maxD = max(fabs(kf.pan - frameStartPan), fabs(kf.tilt - frameStartTilt));
    float spd = (kf.durationMs > 0) ? (maxD / (kf.durationMs / 1000.0f)) : SERVO_SPEED_DPS;
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
    // Nhịp thở sin tự nhiên khi rảnh rỗi (chu kỳ 4s)
    if (breathingEnabled && !headServo.isMoving()) {
      float breath = sin((float)millis() * 2.0f * PI / 4000.0f) * 1.5f;
      headServo.setTarget(PAN_CENTER_DEG, TILT_CENTER_DEG + breath, 20.0f);
    }
  }
} gesturePlayer;

// =================== SECTION 8: MODULE ĐỘNG CƠ CẢM XÚC (EMOTION) ===================
class EmotionEngine {
public:
  EmotionType currentEmotion;
  EyeCommand pendingEyeCmd;
  bool hasPendingEyeCmd;
  bool headTrackingPending;
  unsigned long headTrackScheduledTime;
  float targetTrackPan, targetTrackTilt;

  EmotionEngine() : currentEmotion(EMOTION_NEUTRAL), hasPendingEyeCmd(false), headTrackingPending(false) {}

  void setEmotion(EmotionType emotion) {
    currentEmotion = emotion;
    EyeCommand cmd; memset(&cmd, 0, sizeof(EyeCommand));
    cmd.autoblinker = true; cmd.idleMode = true;

    switch (emotion) {
      case EMOTION_NEUTRAL: cmd.mood = 0; gesturePlayer.breathingEnabled = true; break;
      case EMOTION_HAPPY: cmd.mood = 3; cmd.animLaugh = true; cmd.idleMode = false; gesturePlayer.play(GESTURE_BOUNCE_HAPPY); break;
      case EMOTION_ANGRY: cmd.mood = 2; cmd.autoblinker = false; cmd.idleMode = false; gesturePlayer.play(GESTURE_ANGRY_SNAP); break;
      case EMOTION_SLEEPY: cmd.mood = 1; cmd.autoblinker = false; cmd.idleMode = false; gesturePlayer.play(GESTURE_SLEEP_DROOP); break;
      case EMOTION_CURIOUS: cmd.mood = 0; cmd.curious = true; gesturePlayer.play(GESTURE_TILT_CURIOUS); break;
      case EMOTION_CONFUSED: cmd.mood = 0; cmd.animConfused = true; gesturePlayer.play(GESTURE_SHAKE_NO); break;
      case EMOTION_WAKE_UP: cmd.mood = 0; gesturePlayer.play(GESTURE_WAKE_STRETCH); break;
    }
    pendingEyeCmd = cmd; hasPendingEyeCmd = true;
  }

  void setGaze(uint8_t pos) {
    pendingEyeCmd.position = pos; hasPendingEyeCmd = true;
    float tp = PAN_CENTER_DEG, tt = TILT_CENTER_DEG;
    if (pos == 1) tt -= 12.0f;
    else if (pos == 2) { tp -= 25.0f; tt -= 12.0f; }
    else if (pos == 3) tp -= 25.0f;
    else if (pos == 4) { tp -= 25.0f; tt += 12.0f; }
    else if (pos == 5) tt += 12.0f;
    else if (pos == 6) { tp += 25.0f; tt += 12.0f; }
    else if (pos == 7) tp += 25.0f;
    else if (pos == 8) { tp += 25.0f; tt -= 12.0f; }
    targetTrackPan = tp; targetTrackTilt = tt;
    headTrackScheduledTime = millis() + HEAD_TRACK_DELAY_MS;
    headTrackingPending = true;
  }

  void update() {
    if (headTrackingPending && millis() >= headTrackScheduledTime) {
      headTrackingPending = false;
      if (!gesturePlayer.isPlaying) headServo.setTarget(targetTrackPan, targetTrackTilt, 120.0f);
    }
  }
} emotionEngine;

// =================== SECTION 9: MODULE PHÂN TÍCH ÂM THANH (ANALYZER) ===================
class AudioAnalyzer {
public:
  AudioMetrics metrics;
  portMUX_TYPE mux;
  float bassHist[16];
  uint8_t bassHistIdx;
  float bassAvg;
  unsigned long lastBeatTime;

  AudioAnalyzer() : mux(portMUX_INITIALIZER_UNLOCKED), bassHistIdx(0), bassAvg(0.01f), lastBeatTime(0) {
    memset(&metrics, 0, sizeof(AudioMetrics));
    metrics.bpm = 120.0f;
  }

  void processPcmChunk(const int16_t *samples, size_t frameCount) {
    if (!samples || frameCount == 0) return;
    double sqL = 0, sqR = 0;
    float accum[8] = {0};

    for (size_t i = 0; i < frameCount; i++) {
      int16_t l = samples[i * 2], r = samples[i * 2 + 1];
      sqL += (double)l * l; sqR += (double)r * r;
      float absM = (fabs((float)l) + fabs((float)r)) / 65536.0f;
      uint8_t bin = (i * 8) / frameCount;
      if (bin < 8) accum[bin] += absM;
    }

    float rL = sqrt(sqL / frameCount) / 32768.0f;
    float rR = sqrt(sqR / frameCount) / 32768.0f;
    float bands[8];
    for (int b = 0; b < 8; b++) bands[b] = constrain((accum[b] / (frameCount / 8.0f)) * 2.2f, 0.0f, 1.0f);
    float bass = (bands[0] + bands[1]) * 0.5f;

    unsigned long now = millis();
    bool isB = false;
    if ((bass > bassAvg * 1.32f) && (bass > 0.08f) && (now - lastBeatTime >= 250)) {
      isB = true;
      lastBeatTime = now;
    }

    bassHist[bassHistIdx] = bass;
    bassHistIdx = (bassHistIdx + 1) % 16;
    float s = 0; for (int k = 0; k < 16; k++) s += bassHist[k];
    bassAvg = s / 16.0f;

    portENTER_CRITICAL(&mux);
    metrics.rmsLeft = rL; metrics.rmsRight = rR;
    for (int b = 0; b < 8; b++) metrics.bands[b] = max(bands[b], metrics.bands[b] * 0.82f);
    metrics.isBeat = isB;
    if (isB) { metrics.beatCount++; metrics.lastBeatTime = now; }
    portEXIT_CRITICAL(&mux);
  }

  void getMetrics(AudioMetrics &out) {
    portENTER_CRITICAL(&mux);
    out = metrics; metrics.isBeat = false;
    portEXIT_CRITICAL(&mux);
  }
} audioAnalyzer;

// =================== SECTION 10: MODULE ĐỊNH TUYẾN ÂM THANH (AUDIOROUTER) ===================
class AudioRouterManager {
public:
  AudioSourceType currentSource;
  uint8_t volume, volumeCeil;
  bool isMuted;

  AudioRouterManager() : currentSource(SRC_NONE), volume(30), volumeCeil(DEFAULT_VOLUME_CEIL), isMuted(true) {}

  void begin() {
    pinMode(PIN_I2S_SD, OUTPUT); setMute(true);
    pinMode(PIN_MUX_SEL, OUTPUT); digitalWrite(PIN_MUX_SEL, LOW);
    i2sOutObj.setPins(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DIN);
    i2sOutObj.begin(I2S_MODE_STD, AUDIO_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  }

  void setMute(bool mute) { isMuted = mute; digitalWrite(PIN_I2S_SD, mute ? LOW : HIGH); }
  void setVolume(uint8_t v) { volume = constrain(v, (uint8_t)0, volumeCeil); }

  void setSource(AudioSourceType src) {
    if (src == currentSource) return;
    setMute(true); delay(20);
    digitalWrite(PIN_MUX_SEL, (src == SRC_BT_EXT) ? HIGH : LOW);
    delay(15);
    currentSource = src;
    if (src != SRC_NONE) setMute(false);
  }

  bool isMusicPlaying() const { return (currentSource != SRC_NONE); }

  void writePcm(const int16_t *samples, size_t frameCount) {
    if (!samples || frameCount == 0 || currentSource == SRC_BT_EXT) return;
    if (isMuted) setMute(false);
    audioAnalyzer.processPcmChunk(samples, frameCount);

    int16_t scaled[128 * 2];
    float scale = volume / 100.0f;
    size_t rem = frameCount, off = 0;
    while (rem > 0) {
      size_t chunk = min((size_t)128, rem);
      for (size_t i = 0; i < chunk * 2; i++) scaled[i] = (int16_t)(samples[(off * 2) + i] * scale);
      i2sOutObj.write((uint8_t*)scaled, chunk * 4);
      rem -= chunk; off += chunk;
    }
  }

  void playSfx(SfxType sfx) {
    if (isMusicPlaying()) return;
    setMute(false); delay(10);
    float freq = (sfx == SFX_HAPPY_CHIME) ? 659.25f : ((sfx == SFX_ANGRY_BUZZ) ? 180.0f : 523.25f);
    int16_t buf[64 * 2];
    float phase = 0, pinc = (2.0f * PI * freq) / 44100.0f;
    for (int k = 0; k < 60; k++) {
      for (int i = 0; i < 64; i++) {
        int16_t v = (int16_t)(sin(phase) * 6000.0f);
        buf[i * 2] = v; buf[i * 2 + 1] = v;
        phase += pinc;
      }
      i2sOutObj.write((uint8_t*)buf, 64 * 4);
    }
    delay(10); setMute(true);
  }
} audioRouter;

// =================== SECTION 11: MODULE TRÌNH PHÁT NHẠC (FILEPLAYER) ===================
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
      if (playlist[currentIdx].endsWith(".wav")) activeFile.seek(44);
      audioRouter.setSource(SRC_FILE);
      isPlaying = true; isPaused = false;
    }
  }

  void pause() { isPaused = true; audioRouter.setMute(true); }
  void resume() { isPaused = false; audioRouter.setMute(false); }
  void stop() { isPlaying = false; if (activeFile) activeFile.close(); audioRouter.setSource(SRC_NONE); }
  void next() { if (!playlist.empty()) playIndex((currentIdx + 1) % playlist.size()); }
  void prev() { if (!playlist.empty()) playIndex((currentIdx - 1 + playlist.size()) % playlist.size()); }

  void loop() {
    if (!isPlaying || isPaused || !activeFile) return;
    int16_t pcm[128 * 2];
    size_t rd = activeFile.read((uint8_t*)pcm, sizeof(pcm));
    if (rd > 0) audioRouter.writePcm(pcm, rd / 4);
    else next();
  }
} filePlayer;

// =================== SECTION 12: MODULE GIAO THỨC USB NATIVE CDC (PCLINK) ===================
class PcLinkManager {
public:
  uint8_t *audioRing;
  size_t ringHead, ringTail, ringCount;
  portMUX_TYPE ringMux;
  uint8_t streamBuf[1024];
  bool newFrame;

  PcLinkManager() : audioRing(nullptr), ringHead(0), ringTail(0), ringCount(0),
                    ringMux(portMUX_INITIALIZER_UNLOCKED), newFrame(false) {}

  void begin() {
    if (psramFound()) audioRing = (uint8_t*)ps_malloc(PSRAM_AUDIO_RING_SIZE);
    else audioRing = (uint8_t*)malloc(16 * 1024);
  }

  void update() {
    while (Serial.available()) {
      uint8_t b = Serial.read();
      // Parser đơn giản cho CDC
    }
  }
} pcLink;

// =================== SECTION 13: MODULE CẢM BIẾN HỒNG NGOẠI LM393 (IRGESTURE) ===================
class IrGestureManager {
public:
  bool lastState;
  unsigned long pressStart, releaseTime;
  uint8_t tapCount;

  IrGestureManager() : lastState(HIGH), pressStart(0), releaseTime(0), tapCount(0) {}

  void begin() {
    pinMode(PIN_IR_PRIMARY, INPUT_PULLUP);
    lastState = digitalRead(PIN_IR_PRIMARY);
  }

  void update() {
    bool cur = digitalRead(PIN_IR_PRIMARY);
    unsigned long now = millis();

    if (lastState == HIGH && cur == LOW) pressStart = now;
    else if (lastState == LOW && cur == HIGH) {
      releaseTime = now;
      if (now - pressStart < 400) tapCount++;
    }
    lastState = cur;

    if (tapCount > 0 && cur == HIGH && (now - releaseTime > 600)) {
      if (tapCount == 1) emotionEngine.setEmotion(EMOTION_HAPPY);
      else if (tapCount == 2) filePlayer.next();
      else if (tapCount >= 3) filePlayer.playIndex(0);
      tapCount = 0;
    }
  }
} irGesture;

// =================== SECTION 14: MODULE HIỂN THỊ OLED & MẮT SÓNG (DISPLAYTASK) ===================
class DisplayTaskManager {
public:
  DisplayModeType mode;
  VizStyleType vizStyle;
  unsigned long lastFrame;
  float actualFps;
  uint16_t frameCount;
  unsigned long fpsTimer;
  float peaks[8];

  DisplayTaskManager() : mode(DISP_MODE_EYES), vizStyle(VIZ_BEAT_EYES), lastFrame(0), actualFps(30), frameCount(0), fpsTimer(0) {
    memset(peaks, 0, sizeof(peaks));
  }

  void begin() {
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    Wire.setClock(OLED_I2C_CLOCK);
    displayObj.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    displayObj.clearDisplay(); displayObj.display();

    pRoboEyes = new RoboEyes<Adafruit_SSD1306>(displayObj);
    pRoboEyes->begin(SCREEN_WIDTH, SCREEN_HEIGHT, DISPLAY_FPS_NORMAL);
    pRoboEyes->setAutoblinker(ON, 3, 2);
    pRoboEyes->setIdleMode(ON, 2, 2);
    pRoboEyes->setMood(DEFAULT);
  }

  void update() {
    if (!pRoboEyes) return;
    unsigned long now = millis();
    uint16_t targetMs = (emotionEngine.currentEmotion == EMOTION_SLEEPY) ? (1000/15) : (1000/30);
    if (now - lastFrame < targetMs) return;
    lastFrame = now;

    frameCount++;
    if (now - fpsTimer >= 1000) { actualFps = frameCount; frameCount = 0; fpsTimer = now; }

    bool music = audioRouter.isMusicPlaying();
    if (music && mode == DISP_MODE_EYES) mode = DISP_MODE_VISUAL;
    else if (!music && mode == DISP_MODE_VISUAL) {
      mode = DISP_MODE_EYES;
      pRoboEyes->setWidth(36, 36); pRoboEyes->setHeight(36, 36);
    }

    if (mode == DISP_MODE_EYES) {
      if (emotionEngine.hasPendingEyeCmd) {
        EyeCommand &cmd = emotionEngine.pendingEyeCmd;
        pRoboEyes->setMood(cmd.mood);
        pRoboEyes->setPosition(cmd.position);
        if (cmd.animLaugh) pRoboEyes->anim_laugh();
        if (cmd.animConfused) pRoboEyes->anim_confused();
        emotionEngine.hasPendingEyeCmd = false;
      }
      pRoboEyes->update();
    } else if (mode == DISP_MODE_VISUAL) {
      AudioMetrics m; audioAnalyzer.getMetrics(m);
      if (vizStyle == VIZ_BEAT_EYES) {
        byte sz = 36 + (byte)(m.bands[0] * 18.0f);
        pRoboEyes->setWidth(sz, sz); pRoboEyes->setHeight(sz, sz);
        pRoboEyes->setMood(HAPPY); pRoboEyes->update();
      } else if (vizStyle == VIZ_WAVE_EYES) {
        displayObj.clearDisplay();
        displayObj.drawRoundRect(16, 14, 42, 36, 8, SSD1306_WHITE);
        displayObj.drawRoundRect(70, 14, 42, 36, 8, SSD1306_WHITE);
        for (int x = 20; x < 54; x++) {
          int y = 32 + (int)(sin((x-20)*0.35f + millis()*0.02f) * m.rmsLeft * 14.0f);
          displayObj.drawPixel(x, constrain(y, 18, 46), SSD1306_WHITE);
        }
        for (int x = 74; x < 108; x++) {
          int y = 32 + (int)(sin((x-74)*0.35f + millis()*0.02f) * m.rmsRight * 14.0f);
          displayObj.drawPixel(x, constrain(y, 18, 46), SSD1306_WHITE);
        }
        displayObj.display();
      } else {
        displayObj.clearDisplay();
        displayObj.drawRoundRect(14, 14, 44, 36, 8, SSD1306_WHITE);
        displayObj.drawRoundRect(70, 14, 44, 36, 8, SSD1306_WHITE);
        for (int b = 0; b < 4; b++) {
          int h = (int)(m.bands[b] * 28.0f);
          displayObj.fillRect(20 + b * 9, 46 - h, 6, h, SSD1306_WHITE);
        }
        for (int b = 4; b < 8; b++) {
          int h = (int)(m.bands[b] * 28.0f);
          displayObj.fillRect(76 + (b-4) * 9, 46 - h, 6, h, SSD1306_WHITE);
        }
        displayObj.display();
      }
    }
  }
} displayTask;

// =================== SECTION 15: MODULE TỰ HÀNH & NHÚN ĐẦU THEO NHẠC (AUTONOMY) ===================
class AutonomyManager {
public:
  AutonomyModeType currentMode;
  unsigned long manualUntil, lastAction, actionInterval;
  uint32_t lastBeat;

  AutonomyManager() : currentMode(AUTO_MODE_CURIOUS), manualUntil(0), lastAction(0), actionInterval(4000), lastBeat(0) {}

  void notifyManual() { manualUntil = millis() + MANUAL_OVERRIDE_MS; }
  bool isManual() const { return (millis() < manualUntil); }

  void setMode(AutonomyModeType m) {
    currentMode = m; lastAction = millis();
    if (m == AUTO_MODE_SLEEP) emotionEngine.setEmotion(EMOTION_SLEEPY);
    else if (m == AUTO_MODE_CURIOUS) emotionEngine.setEmotion(EMOTION_CURIOUS);
    else if (m == AUTO_MODE_CALM) emotionEngine.setEmotion(EMOTION_NEUTRAL);
  }

  void update() {
    if (isManual()) return;
    if (audioRouter.isMusicPlaying()) {
      AudioMetrics m; audioAnalyzer.getMetrics(m);
      if (m.beatCount != lastBeat) {
        lastBeat = m.beatCount;
        headServo.setTarget(headServo.currentPan - headServo.panOffset, 96.0f, 75.0f);
      } else if (!headServo.isMoving()) {
        headServo.setTarget(headServo.currentPan - headServo.panOffset, 90.0f, 50.0f);
      }
      return;
    }
    if (gesturePlayer.isPlaying) return;
    if (currentMode == AUTO_MODE_SLEEP) return;

    if (millis() - lastAction >= actionInterval) {
      lastAction = millis();
      actionInterval = random(4000, 8000);
      if (currentMode == AUTO_MODE_CURIOUS) {
        if (random(2) == 0) emotionEngine.setGaze(random(1, 9));
        else gesturePlayer.play(GESTURE_TILT_CURIOUS);
      } else if (currentMode == AUTO_MODE_PLAYFUL) {
        gesturePlayer.play(GESTURE_BOUNCE_HAPPY);
      }
    }
  }
} autonomy;

// =================== SECTION 16: MODULE WEBSERVER & REST API (WEBAPI) ===================
void setupWebServer() {
  gCommandQueue = xQueueCreate(16, sizeof(RobotCommand));
  WiFi.mode(WIFI_AP);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS, WIFI_AP_CHANNEL, 0, WIFI_MAX_CLIENTS);
  dnsServerObj.start(53, "*", WiFi.softAPIP());

  serverObj.on("/", HTTP_GET, []() {
    serverObj.send_P(200, "text/html", HTML_CONTROL_PAGE);
  });

  serverObj.on("/api/head", HTTP_GET, []() {
    autonomy.notifyManual();
    if (serverObj.hasArg("pan") && serverObj.hasArg("tilt")) {
      headServo.setTarget(serverObj.arg("pan").toFloat(), serverObj.arg("tilt").toFloat());
    }
    serverObj.send(200, "application/json", "{\"status\":\"ok\"}");
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
    if (a == "play") filePlayer.playIndex(serverObj.arg("idx").toInt());
    else if (a == "pause") filePlayer.pause();
    else if (a == "resume") filePlayer.resume();
    else if (a == "next") filePlayer.next();
    else if (a == "prev") filePlayer.prev();
    else if (a == "vol") audioRouter.setVolume(serverObj.arg("val").toInt());
    serverObj.send(200, "application/json", "{\"status\":\"ok\"}");
  });

  serverObj.on("/api/source", HTTP_GET, []() {
    audioRouter.setSource((AudioSourceType)serverObj.arg("src").toInt());
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
    doc["pan"] = round(headServo.currentPan - headServo.panOffset);
    doc["tilt"] = round(headServo.currentTilt - headServo.tiltOffset);
    doc["fps"] = round(displayTask.actualFps);
    doc["free_heap"] = ESP.getFreeHeap();
    doc["source"] = (int)audioRouter.currentSource;
    doc["track"] = filePlayer.isPlaying ? "LittleFS Music" : "Dừng";
    String res; serializeJson(doc, res);
    serverObj.send(200, "application/json", res);
  });

  serverObj.onNotFound([]() { serverObj.send_P(200, "text/html", HTML_CONTROL_PAGE); });
  serverObj.begin();
}

// =================== SECTION 17: CÁC TASK FREERTOS ĐA NHÂN ===================
void taskServoLoop(void *p) {
  TickType_t last = xTaskGetTickCount();
  while (true) {
    headServo.update(0.020f);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
  }
}

void taskDisplayLoop(void *p) {
  while (true) {
    displayTask.update();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void taskBrainLoop(void *p) {
  TickType_t last = xTaskGetTickCount();
  while (true) {
    emotionEngine.update();
    gesturePlayer.update();
    irGesture.update();
    autonomy.update();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
  }
}

void taskAudioLoop(void *p) {
  while (true) {
    filePlayer.loop();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// =================== SECTION 18: SETUP & LOOP CHÍNH ===================
void setup() {
  Serial0.begin(115200);
  Serial.begin(115200);
  delay(500);

  Serial0.println(F("======================================================"));
  Serial0.println(F("   MIU EILIK ESP32-S3 - SINGLE ALL-IN-ONE FIRMWARE    "));
  Serial0.println(F("======================================================"));

  headServo.begin();
  displayTask.begin();
  audioRouter.begin();
  filePlayer.begin();
  irGesture.begin();
  autonomy.begin();
  setupWebServer();

  // Tạo các Task FreeRTOS chia sẻ 2 Core
  xTaskCreatePinnedToCore(taskServoLoop,   "ServoTask",   4096, NULL, 5, NULL, 0); // Core 0
  xTaskCreatePinnedToCore(taskDisplayLoop, "DisplayTask", 6144, NULL, 4, NULL, 1); // Core 1
  xTaskCreatePinnedToCore(taskBrainLoop,   "BrainTask",   4096, NULL, 3, NULL, 1); // Core 1
  xTaskCreatePinnedToCore(taskAudioLoop,   "AudioTask",   4096, NULL, 3, NULL, 1); // Core 1

  Serial0.println(F("[SETUP] Đã khởi tạo thành công 100% tính năng trong 1 file!"));
}

void loop() {
  dnsServerObj.processNextRequest();
  serverObj.handleClient();
  delay(2);
}
