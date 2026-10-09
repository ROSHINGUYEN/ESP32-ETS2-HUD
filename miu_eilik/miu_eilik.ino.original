#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <cmath>
#include <time.h>
#include <sys/time.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <LittleFS.h>   // Fix C28: FilePlayer cần LittleFS/File

#include "ESP_I2S.h"
// Macro collision guard:
//   RoboEyes.h defines DEFAULT, TIRED, ANGRY, HAPPY, SAD, CRYING, WINK, SHOCKED at top level.
//   Some Arduino/Adafruit headers may also define these -> redefine warning/error.
//   Solution: undefine them before including, then redefine our own values.
#undef DEFAULT
#undef TIRED
#undef ANGRY
#undef HAPPY
#undef SAD
#undef CRYING
#undef WINK
#undef SHOCKED
#include "FluxGarage_RoboEyes.h"
// Bring back RoboEyes values
#define DEFAULT 0
#define TIRED   1
#define ANGRY   2
#define HAPPY   3
#define SAD     4
#define CRYING  5
#define WINK    6
#define SHOCKED 7

// =====================================================================================
// MACROS: LOGGING RA CA SERIAL (USB CDC) VA SERIAL0 (UART0) CHO ARDUINO IDE MONITOR
// =====================================================================================
#define LOG_PRINT(...)   do { Serial.print(__VA_ARGS__);   Serial0.print(__VA_ARGS__); } while(0)
#define LOG_PRINTLN(...) do { Serial.println(__VA_ARGS__); Serial0.println(__VA_ARGS__); } while(0)
#define LOG_PRINTF(...)  do { Serial.printf(__VA_ARGS__);  Serial0.printf(__VA_ARGS__); } while(0)

// =====================================================================================
// SECTION A: CONFIG (PROGMEM const để tiết kiệm RAM)
// =====================================================================================

// OLED SSD1306 (128x64) I2C
// Mặc định: SDA = GPIO 8, SCL = GPIO 9 (phù hợp với ESP32-S3-WROOM-1 / DevKitC-1)
// Tự động nhận diện địa chỉ 0x3C hoặc 0x3D khi khởi động
#define PIN_OLED_SDA            8
#define PIN_OLED_SCL            9
#define OLED_ADDR               0x3C
#define SCREEN_W                128
#define SCREEN_H                64
#define OLED_I2C_HZ             100000  // 100kHz chuan on dinh cao cho moi loai module & day cam Dupont
#define OLED_I2C_TIMEOUT_MS     50      // timeout tránh treo khi bus I2C lỗi

// Servo
#define PIN_SERVO_PAN           4
#define PIN_SERVO_TILT          5
#define SERVO_PULSE_MIN_US      500
#define SERVO_PULSE_MAX_US      2400
#define SERVO_PWM_FREQ_HZ       50
#define SERVO_PULSE_RANGE_US    (SERVO_PULSE_MAX_US - SERVO_PULSE_MIN_US)

// Góc
#define PAN_CENTER_DEG          90.0f
#define PAN_MIN_DEG             40.0f
#define PAN_MAX_DEG             140.0f
#define TILT_CENTER_DEG         90.0f
#define TILT_MIN_DEG            65.0f
#define TILT_MAX_DEG            115.0f

// Servo motion (tối ưu)
#define SERVO_MAX_SPEED_DPS     220.0f
#define SERVO_MIN_SPEED_DPS     20.0f
#define SERVO_ACCEL_DPS2        600.0f
#define SERVO_IDLE_DETACH_MS    1500
#define HOLD_TORQUE_TILT        1

// IR
#define PIN_IR_PRIMARY          6
#define PIN_IR_SECONDARY        7
#define IR_ACTIVE_LEVEL         LOW
#define IR_DEBOUNCE_MS          25
#define IR_PRESENCE_TIMEOUT_MS  15000
#define IR_TAP_MAX_MS           400
#define IR_DOUBLE_GAP_MS        500
#define IR_LONG_HOLD_MS         800

// Audio I2S
#define PIN_I2S_BCLK            15
#define PIN_I2S_LRC             16
#define PIN_I2S_DIN             17
#define PIN_I2S_SD              18
#define PIN_MUX_SEL             10
#define PIN_BT_BCLK_IN          11
#define PIN_BT_WS_IN            12
#define PIN_BT_DATA_IN          13
#define PIN_BT_STATUS           14
#define AUDIO_SAMPLE_RATE       44100
#define AUDIO_CHUNK_SAMPLES     512
#define DEFAULT_VOLUME_CEIL     40

// USB protocol
#define USB_SYNC_1              0xE1
#define USB_SYNC_2              0x1C
#define PKT_AUDIO_PCM           0x01
#define PKT_OLED_BITMAP         0x02
#define PKT_CMD_JSON            0x03
#define PKT_TELEMETRY_REQ       0x04

// NVS
#define NVS_NAMESPACE           "eilik"
#define NVS_KEY_PAN_OFF         "pan_off"
#define NVS_KEY_TILT_OFF        "tilt_off"

// Bounds (giúp constrain an toàn enum từ int)
#define EMOTION_COUNT           11
#define GESTURE_COUNT           11
#define AUDIO_SRC_COUNT         4

// =====================================================================================
// SECTION A2: ENUMS
// =====================================================================================

enum AudioSource : uint8_t { SRC_NONE = 0, SRC_FILE, SRC_USB_PC, SRC_BT_EXT };
enum VizStyle    : uint8_t { VIZ_BEAT_EYES = 0, VIZ_WAVE_EYES, VIZ_BARS_EYES };
enum DisplayMode : uint8_t { DISP_CLOCK = 0, DISP_EYES = 1, DISP_VISUAL = 2, DISP_STREAM = 3 };
enum Emotion     : uint8_t {
  EMO_NEUTRAL = 0,
  EMO_HAPPY,
  EMO_ANGRY,
  EMO_SLEEPY,
  EMO_CURIOUS,
  EMO_CONFUSED,
  EMO_WAKE_UP,
  EMO_SAD,
  EMO_CRYING,
  EMO_WINK,
  EMO_SHOCKED
};
enum Gesture     : uint8_t {
  GES_NONE = 0,
  GES_NOD_YES,       // Gật đầu
  GES_SHAKE_NO,      // Lắc đầu
  GES_TILT_LEFT,     // Ngửa trái (như pet cún)
  GES_TILT_RIGHT,    // Ngửa phải (như pet cún)
  GES_LOOK_LEFT,     // Quay đầu nhìn trái
  GES_LOOK_RIGHT,    // Quay đầu nhìn phải
  GES_LOOK_UP,       // Ngóc đầu lên cao
  GES_BOW_SAD,       // Cúi gục đầu (khi buồn)
  GES_BOUNCE_HAPPY,  // Nhún nhảy vui mừng
  GES_SLEEP_DROOP,   // Cúi gục ngủ
  GES_WAKE_STRETCH   // Vươn vai thức dậy
};
enum IrEvent     : uint8_t { IR_NONE, IR_TAP, IR_DOUBLE, IR_TRIPLE, IR_HOLD, IR_SWIPE_RIGHT, IR_SWIPE_LEFT };
enum AutoMode    : uint8_t { AUTO_SLEEP, AUTO_CALM, AUTO_CURIOUS, AUTO_PLAYFUL };

// =====================================================================================
// SECTION C: FNV-1a HASH (nhanh hơn strcmp 10-50x)
// =====================================================================================

constexpr uint32_t fnv1a(const char* s, size_t i = 0, uint32_t h = 0x811c9dc5) {
  return s[i] ? fnv1a(s, i + 1, (h ^ s[i]) * 0x01000193) : h;
}

// Hash compile-time cho JSON command keys
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

// Hash cho emotion string values
// Fix macro clash: HAPPY/ANGRY are defined by FluxGarage_RoboEyes.h as #define HAPPY 3
// We use distinct names (EMO_*) to avoid preprocessor expansion issues
namespace EmoHash {
  constexpr uint32_t EMO_NEUTRAL  = fnv1a("NEUTRAL");
  constexpr uint32_t EMO_HAPPY    = fnv1a("HAPPY");
  constexpr uint32_t EMO_ANGRY    = fnv1a("ANGRY");
  constexpr uint32_t EMO_SLEEPY   = fnv1a("SLEEPY");
  constexpr uint32_t EMO_CURIOUS  = fnv1a("CURIOUS");
  constexpr uint32_t EMO_CONFUSED = fnv1a("CONFUSED");
  constexpr uint32_t EMO_WAKE_UP  = fnv1a("WAKE_UP");
  constexpr uint32_t EMO_SAD      = fnv1a("SAD");
  constexpr uint32_t EMO_CRYING   = fnv1a("CRYING");
  constexpr uint32_t EMO_WINK     = fnv1a("WINK");
  constexpr uint32_t EMO_SHOCKED  = fnv1a("SHOCKED");
}

namespace ModeHash {
  constexpr uint32_t MODE_SLEEP    = fnv1a("SLEEP");
  constexpr uint32_t MODE_CALM     = fnv1a("CALM");
  constexpr uint32_t MODE_CURIOUS  = fnv1a("CURIOUS");
  constexpr uint32_t MODE_PLAYFUL  = fnv1a("PLAYFUL");
}

// Hash ArduinoJson key
static uint32_t hashKey(const char* k) {
  uint32_t h = 0x811c9dc5;
  while (*k) h = (h ^ *k++) * 0x01000193;
  return h;
}

// =====================================================================================
// SECTION D: DATA STRUCT
// =====================================================================================

struct AudioMetrics {
  float rmsL, rmsR;
  float bands[8];
  bool  isBeat;
  float bpm;
  uint32_t beatCount;
};

struct Keyframe {
  uint8_t pan;        // 0-180 thay vì float
  uint8_t tilt;
  uint16_t ms;
};

// =====================================================================================
// SECTION E: GLOBALS (khởi tạo 1 lần, không dùng new)
// =====================================================================================

Adafruit_SSD1306 oled(SCREEN_W, SCREEN_H, &Wire, -1);
// RoboEyes<T>(AdafruitDisplay &disp) - constructor lấy REFERENCE (không phải pointer)
RoboEyes<Adafruit_SSD1306> roboEyes(oled);
ESP_I2S i2sOut;
Preferences prefs;
SemaphoreHandle_t gStateMutex = nullptr;  // (dự phòng cho tương lai, hiện chưa dùng)
volatile uint32_t gUnderrunCount = 0;

// =====================================================================================
// SECTION F: SERVO AXIS - THUẬT TOÁN TRAPEZOIDAL
// =====================================================================================

struct ServoAxis {
  float current = 90.0f;
  float start   = 90.0f;
  float target  = 90.0f;
  float offset  = 0.0f;
  float minLim  = 40.0f;
  float maxLim  = 140.0f;
  float vel     = 0.0f;
  float maxV    = SERVO_MAX_SPEED_DPS;
  float accel   = SERVO_ACCEL_DPS2;
  bool  moving  = false;

  void init(float ang, float mn, float mx) {
    current = start = target = ang;
    minLim = mn; maxLim = mx;
    vel = 0; moving = false;
  }

  // FIX B5: setTarget giữ vận tốc hiện tại nếu cùng chiều -> mượt khi chuỗi lệnh liên tiếp
  void setTarget(float t, float speedDps) {
    float clamped = constrain(t + offset, minLim, maxLim);
    if (!moving && fabs(clamped - current) < 0.3f) {
      target = current; return;
    }
    start = current; target = clamped;
    maxV = constrain(speedDps, SERVO_MIN_SPEED_DPS, SERVO_MAX_SPEED_DPS);
    // Nếu đang moving và đã có vận tốc, giữ nguyên (không reset)
    // Nếu đổi chiều, mới về 0 để mượt
    if (moving && (target - current) * vel < 0) vel = 0;
    moving = true;
  }

  void update(float dt) {
    if (!moving) return;
    float dist = target - current;
    float absDist = fabs(dist);
    if (absDist < 0.15f) {
      current = target; vel = 0; moving = false; return;
    }
    float sign = (dist > 0) ? 1.0f : -1.0f;
    float decelDist = (vel * vel) / (2.0f * accel);

    if (absDist <= decelDist + 0.1f) {
      // FIX M2: rõ ràng giảm tốc về 0 theo chiều chuyển động
      vel -= sign * accel * dt;
      // Không cho vel đảo chiều quá mức
      if ((sign > 0 && vel < 0) || (sign < 0 && vel > 0)) vel = 0;
    } else if (vel < maxV) {
      // Tăng tốc tới maxV
      vel += accel * dt;
      if (vel > maxV) vel = maxV;
    }

    float step = vel * dt * sign;
    if (fabs(step) > absDist) {
      current = target; vel = 0; moving = false;
    } else {
      current += step;
    }
  }

  void snapToTarget() { current = target; vel = 0; moving = false; }
};

// =====================================================================================
// SECTION G: HEAD SERVO CONTROLLER
// =====================================================================================

class HeadServo {
public:
  Servo panHw, tiltHw;
  ServoAxis pan, tilt;
  bool panAttached = false, tiltAttached = false;
  unsigned long lastMoveMs = 0;

  void begin() {
    prefs.begin(NVS_NAMESPACE, true);
    pan.offset  = (float)prefs.getChar(NVS_KEY_PAN_OFF,  0);
    tilt.offset = (float)prefs.getChar(NVS_KEY_TILT_OFF, 0);
    prefs.end();

    pan.init(PAN_CENTER_DEG,  PAN_MIN_DEG,  PAN_MAX_DEG);
    tilt.init(TILT_CENTER_DEG, TILT_MIN_DEG, TILT_MAX_DEG);
    attach();
    write();
  }

  void attach() {
    if (!panAttached) {
      panHw.setPeriodHertz(SERVO_PWM_FREQ_HZ);
      panHw.attach(PIN_SERVO_PAN, SERVO_PULSE_MIN_US, SERVO_PULSE_MAX_US);
      panAttached = true;
    }
    if (!tiltAttached) {
      tiltHw.setPeriodHertz(SERVO_PWM_FREQ_HZ);
      tiltHw.attach(PIN_SERVO_TILT, SERVO_PULSE_MIN_US, SERVO_PULSE_MAX_US);
      tiltAttached = true;
    }
  }

  // Inline: ghi trực tiếp microseconds (chính xác, nhanh hơn write() 5-8µs)
  inline void write() {
    uint16_t p = (uint16_t)(SERVO_PULSE_MIN_US + (pan.current  / 180.0f) * SERVO_PULSE_RANGE_US);
    uint16_t t = (uint16_t)(SERVO_PULSE_MIN_US + (tilt.current / 180.0f) * SERVO_PULSE_RANGE_US);
    panHw.writeMicroseconds(p);
    tiltHw.writeMicroseconds(t);
  }

  void setTarget(float p, float t, float spd = SERVO_MAX_SPEED_DPS) {
    attach();
    pan.setTarget(p, spd);
    tilt.setTarget(t, spd);
    lastMoveMs = millis();
  }

  void nudge(float dp, float dt) {
    setTarget(pan.target - pan.offset + dp, tilt.target - tilt.offset + dt, SERVO_MAX_SPEED_DPS);
  }

  void center() { setTarget(PAN_CENTER_DEG, TILT_CENTER_DEG, 120.0f); }

  bool isMoving() const { return pan.moving || tilt.moving; }

  void update(float dt) {
    if (isMoving()) {
      attach();
      pan.update(dt);
      tilt.update(dt);
      write();
      lastMoveMs = millis();
    } else if (millis() - lastMoveMs > SERVO_IDLE_DETACH_MS) {
      if (panAttached) { panHw.detach(); panAttached = false; }
      if (tiltAttached && !HOLD_TORQUE_TILT) { tiltHw.detach(); tiltAttached = false; }
    }
  }

  void stop() { pan.snapToTarget(); tilt.snapToTarget(); }

  void saveCalib(int8_t p, int8_t to) {
    pan.offset = p; tilt.offset = to;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putChar(NVS_KEY_PAN_OFF,  p);
    prefs.putChar(NVS_KEY_TILT_OFF, to);
    prefs.end();
  }
} head;

// =====================================================================================
// SECTION H: GESTURE PLAYER (keyframe data đặt trong PROGMEM)
// =====================================================================================

static const Keyframe PROGMEM TRACK_NOD[]        = { {90,106,160},{90,80,160},{90,105,140},{90,86,140},{90,90,180} };
static const Keyframe PROGMEM TRACK_SHAKE[]      = { {118,90,150},{62,90,200},{114,90,180},{66,90,180},{90,90,180} };
static const Keyframe PROGMEM TRACK_TILT_LEFT[]  = { {106,90,220},{106,78,220},{106,78,700},{106,90,200},{90,90,220} };  // Xoay Pan truoc, ngua Tilt sau (tack bach)
static const Keyframe PROGMEM TRACK_TILT_RIGHT[] = { {74,90,220},{74,78,220},{74,78,700},{74,90,200},{90,90,220} };    // Xoay Pan truoc, ngua Tilt sau (tack bach)
static const Keyframe PROGMEM TRACK_LOOK_LEFT[]  = { {125,90,300},{125,90,500},{90,90,300} };  // Chi xoay Pan trai (Tilt=90 co dinh)
static const Keyframe PROGMEM TRACK_LOOK_RIGHT[] = { {55,90,300},{55,90,500},{90,90,300} };    // Chi xoay Pan phai (Tilt=90 co dinh)
static const Keyframe PROGMEM TRACK_LOOK_UP[]    = { {90,72,300},{90,72,700},{90,90,350} };    // Chi ngua Tilt len (Pan=90 co dinh)
static const Keyframe PROGMEM TRACK_BOW_SAD[]    = { {90,114,450},{90,115,1200},{90,100,400},{90,90,300} }; // Chi cui Tilt xuong (Pan=90 co dinh)
static const Keyframe PROGMEM TRACK_BOUNCE[]     = { {90,102,110},{90,84,120},{90,102,110},{90,90,150} };
static const Keyframe PROGMEM TRACK_SLEEP[]      = { {90,96,400},{90,108,600},{90,115,1000} };
static const Keyframe PROGMEM TRACK_WAKE[]       = { {80,105,350},{90,74,500},{100,74,250},{90,90,350} };

// Table các gesture (PROGMEM)
struct GestureTrack {
  const Keyframe* data;  // pointer tới PROGMEM
  uint8_t len;
};

static const GestureTrack PROGMEM GESTURE_TABLE[] = {
  { TRACK_NOD,        5 },
  { TRACK_SHAKE,      5 },
  { TRACK_TILT_LEFT,  5 },
  { TRACK_TILT_RIGHT, 5 },
  { TRACK_LOOK_LEFT,  3 },
  { TRACK_LOOK_RIGHT, 3 },
  { TRACK_LOOK_UP,    3 },
  { TRACK_BOW_SAD,    4 },
  { TRACK_BOUNCE,     4 },
  { TRACK_SLEEP,      3 },
  { TRACK_WAKE,       4 },
};

class GesturePlayer {
public:
  Gesture current = GES_NONE;
  bool playing = false, breathingEnabled = true;
  uint8_t idx = 0;
  unsigned long frameStartMs = 0;
  unsigned long lastBreathMs = 0;

  void play(Gesture g) {
    // FIX M1: bounds check nghiêm ngặt
    if (g == GES_NONE || g >= GES_NONE + GESTURE_COUNT) { playing = false; return; }
    current = g; idx = 0;
    playing = true;
    startFrame(0);
  }

  // Helper: đọc 1 Keyframe từ PROGMEM an toàn
  static bool readKeyframe(uint8_t gestureIdx, uint8_t frameIdx, Keyframe& out) {
    // FIX C1: validate gestureIdx trước khi dùng để tránh underflow uint8_t
    if (gestureIdx == 0 || gestureIdx > GESTURE_COUNT) return false;
    // gestureIdx 1-based (0 = NONE), GESTURE_TABLE 0-based tương ứng gestureIdx-1
    const GestureTrack* entry = &GESTURE_TABLE[gestureIdx - 1];
    // Đọc con trỏ data từ PROGMEM (32-bit pointer trên ESP32)
    const Keyframe* trackPtr;
    memcpy_P(&trackPtr, &entry->data, sizeof(trackPtr));
    if (!trackPtr) return false;
    // Đọc Keyframe thứ frameIdx
    memcpy_P(&out, &trackPtr[frameIdx], sizeof(Keyframe));
    return true;
  }

  static bool readTrackLen(uint8_t gestureIdx, uint8_t& outLen) {
    if (gestureIdx == 0 || gestureIdx > GESTURE_COUNT) return false;
    const GestureTrack* entry = &GESTURE_TABLE[gestureIdx - 1];
    outLen = pgm_read_byte(&entry->len);
    return true;
  }

  void startFrame(uint8_t i) {
    idx = i;
    frameStartMs = millis();
    Keyframe kf;
    if (readKeyframe(current, i, kf)) {
      head.setTarget(kf.pan, kf.tilt, 200.0f);
    } else {
      playing = false; current = GES_NONE;
    }
  }

  void update() {
    if (playing) {
      Keyframe kf;
      if (!readKeyframe(current, idx, kf)) {
        playing = false; current = GES_NONE; return;
      }
      if (millis() - frameStartMs >= kf.ms) {
        uint8_t len = 0;
        if (!readTrackLen(current, len) || idx + 1 >= len) {
          playing = false; current = GES_NONE;
        } else {
          startFrame(idx + 1);
        }
      }
      return;
    }
    // Nhịp thở tự nhiên (20Hz thay vì liên tục -> giảm CPU)
    if (breathingEnabled && !head.isMoving()) {
      unsigned long now = millis();
      if (now - lastBreathMs >= 50) {
        lastBreathMs = now;
        float breath = sinf((float)now * 0.001496f) * 1.5f;  // 2π/4200
        head.setTarget(PAN_CENTER_DEG, TILT_CENTER_DEG + breath, 30.0f);
      }
    }
  }
} gesture;

// =====================================================================================
// SECTION I: EMOTION ENGINE
// =====================================================================================

class EmotionEngine {
public:
  Emotion current = EMO_NEUTRAL;
  bool hasPending = false;
  uint8_t pendingMood = 0;
  bool pendingLaugh = false, pendingConfused = false;

  // Lookup emotion từ hash (O(1) thay vì strcmp chuỗi)
  static Emotion fromHash(uint32_t h) {
    switch (h) {
      case EmoHash::EMO_HAPPY:    return EMO_HAPPY;
      case EmoHash::EMO_ANGRY:    return EMO_ANGRY;
      case EmoHash::EMO_SLEEPY:   return EMO_SLEEPY;
      case EmoHash::EMO_CURIOUS:  return EMO_CURIOUS;
      case EmoHash::EMO_CONFUSED: return EMO_CONFUSED;
      case EmoHash::EMO_WAKE_UP:  return EMO_WAKE_UP;
      case EmoHash::EMO_SAD:      return EMO_SAD;
      case EmoHash::EMO_CRYING:   return EMO_CRYING;
      case EmoHash::EMO_WINK:     return EMO_WINK;
      case EmoHash::EMO_SHOCKED:  return EMO_SHOCKED;
      default:                    return EMO_NEUTRAL;
    }
  }

  void setEmotion(Emotion e) {
    current = e;
    pendingLaugh = pendingConfused = false;

    switch (e) {
      case EMO_NEUTRAL:
        pendingMood = DEFAULT; gesture.breathingEnabled = true; break;
      case EMO_HAPPY:
        pendingMood = HAPPY; pendingLaugh = true;
        gesture.play(GES_BOUNCE_HAPPY); break;
      case EMO_ANGRY:
        pendingMood = ANGRY; gesture.breathingEnabled = false;
        gesture.play(GES_SHAKE_NO); break;
      case EMO_SLEEPY:
        pendingMood = TIRED; gesture.play(GES_SLEEP_DROOP); break;
      case EMO_CURIOUS:
        pendingMood = DEFAULT; gesture.play(GES_TILT_LEFT); break;
      case EMO_CONFUSED:
        pendingMood = DEFAULT; pendingConfused = true;
        gesture.play(GES_SHAKE_NO); break;
      case EMO_WAKE_UP:
        pendingMood = DEFAULT; gesture.play(GES_WAKE_STRETCH); break;
      case EMO_SAD:
        pendingMood = SAD; gesture.play(GES_BOW_SAD); break;
      case EMO_CRYING:
        pendingMood = CRYING; gesture.play(GES_BOW_SAD); break;
      case EMO_WINK:
        pendingMood = WINK; gesture.play(GES_TILT_RIGHT); break;
      case EMO_SHOCKED:
        pendingMood = SHOCKED; gesture.play(GES_LOOK_UP); break;
    }
    hasPending = true;
  }
} emotion;

// =====================================================================================
// SECTION J: AUDIO BIQUAD
// =====================================================================================

struct Biquad {
  float b0, a1, a2;
  float x1 = 0, x2 = 0, y1 = 0, y2 = 0;

  void initBP(float f0, float fs, float Q) {
    float w0 = 2.0f * PI * f0 / fs;
    float alpha = sinf(w0) / (2.0f * Q);
    float a0 = 1.0f + alpha;
    b0 = (sinf(w0) / 2.0f) / a0;
    a1 = (-2.0f * cosf(w0)) / a0;
    a2 = (1.0f - alpha) / a0;
  }

  inline float process(float in) {
    float y = b0 * (in - x2) - a1 * y1 - a2 * y2;
    x2 = x1; x1 = in; y2 = y1; y1 = y;
    return y;
  }
};

class AudioAnalyzer {
public:
  Biquad filters[8];
  AudioMetrics m;
  float bassHist[16];
  uint8_t bassIdx = 0;
  float bassAvg = 0.01f;
  unsigned long lastBeatMs = 0;
  portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

  void begin() {
    // FIX M6: float không portable trên PROGMEM (chỉ ESP32 hỗ trợ). Để RAM cho chắc.
    static const float FREQS[8] = {60, 150, 400, 1000, 2500, 6000, 10000, 16000};
    for (int i = 0; i < 8; i++) {
      filters[i].initBP(FREQS[i], (float)AUDIO_SAMPLE_RATE, 1.414f);
    }
    memset(&m, 0, sizeof(m));
    m.bpm = 120.0f;
    for (int k = 0; k < 16; k++) bassHist[k] = 0.01f;
  }

  // Single-pass: subsample 2x để giảm 50% CPU (FPU vẫn đủ chính xác cho beat detect)
  void process(const int16_t* samples, size_t frames) {
    if (!samples || !frames) return;
    double sqL = 0, sqR = 0;
    float bandE[8] = {0};
    const float inv32768 = 1.0f / 32768.0f;
    // FIX M3: step=2 giảm CPU ~50% (đủ chính xác cho music reactive)
    const size_t step = 2;
    size_t processed = 0;

    for (size_t i = 0; i < frames; i += step) {
      int16_t l = samples[i * 2], r = samples[i * 2 + 1];
      sqL += (double)l * l;
      sqR += (double)r * r;
      float mono = ((float)l + (float)r) * 0.5f * inv32768;
      for (int b = 0; b < 8; b++) bandE[b] += fabsf(filters[b].process(mono));
      processed++;
    }

    float invN = (processed > 0) ? (1.0f / (float)processed) : 1.0f;
    float bands[8];
    for (int b = 0; b < 8; b++) bands[b] = constrain(bandE[b] * invN * 3.8f, 0.0f, 1.0f);
    float bass = bands[0] * 0.6f + bands[1] * 0.4f;
    unsigned long now = millis();
    bool isB = false;

    if (bass > bassAvg * 1.35f && bass > 0.07f && (now - lastBeatMs) >= 220) {
      isB = true;
      if (now - lastBeatMs < 2000) {
        float instantBpm = 60000.0f / (float)(now - lastBeatMs);
        m.bpm = m.bpm * 0.8f + instantBpm * 0.2f;
      }
      lastBeatMs = now;
    }

    bassHist[bassIdx] = bass;
    bassIdx = (bassIdx + 1) & 0x0F;  // % 16 nhưng nhanh hơn
    float s = 0;
    for (int k = 0; k < 16; k++) s += bassHist[k];
    bassAvg = s * 0.0625f;  // /16 nhanh hơn

    portENTER_CRITICAL(&mux);
    m.rmsL = sqrt(sqL * invN) * inv32768;
    m.rmsR = sqrt(sqR * invN) * inv32768;
    for (int b = 0; b < 8; b++) {
      float nw = bands[b];
      float old = m.bands[b] * 0.78f;
      m.bands[b] = nw > old ? nw : old;
    }
    m.isBeat = isB;
    if (isB) m.beatCount++;
    portEXIT_CRITICAL(&mux);
  }

  void get(AudioMetrics& out) {
    portENTER_CRITICAL(&mux);
    out = m; m.isBeat = false;
    portEXIT_CRITICAL(&mux);
  }
} audio;

// =====================================================================================
// SECTION K: AUDIO ROUTER (streaming - bỏ buffer copy)
// =====================================================================================

class AudioRouter {
public:
  AudioSource src = SRC_NONE;
  uint8_t vol = 30, volCeil = DEFAULT_VOLUME_CEIL;
  bool ampOn = false;
  // FIX C6: buffer thành member (heap-equivalent trong BSS, 0 alloc)
  // Tránh stack overflow khi writePnh gọi trong task 4KB stack
  int16_t scratch[AUDIO_CHUNK_SAMPLES * 2];

  void begin() {
    pinMode(PIN_I2S_SD, OUTPUT); digitalWrite(PIN_I2S_SD, LOW);
    pinMode(PIN_MUX_SEL, OUTPUT); digitalWrite(PIN_MUX_SEL, LOW);
    i2sOut.setPins(PIN_I2S_BCLK, PIN_I2S_LRC, PIN_I2S_DIN);
    i2sOut.begin(I2S_MODE_STD, AUDIO_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  }

  void setAmp(bool on) { ampOn = on; digitalWrite(PIN_I2S_SD, on ? HIGH : LOW); }
  void setVol(uint8_t v) { vol = constrain(v, (uint8_t)0, volCeil); }

  void setSource(AudioSource s) {
    if (s == src) return;
    setAmp(false); delay(15);
    digitalWrite(PIN_MUX_SEL, (s == SRC_BT_EXT) ? HIGH : LOW);
    delay(10);
    src = s;
    if (s != SRC_NONE) setAmp(true);
  }

  bool isMusic() const { return src != SRC_NONE; }

  // OPTIMIZED: streaming trực tiếp từ source PCM, scale on-the-fly
  // giảm 2KB stack buffer và 1 pass copy
  void writePcm(const int16_t* samples, size_t frames) {
    if (!samples || !frames || src == SRC_BT_EXT) return;
    if (!ampOn) setAmp(true);
    audio.process(samples, frames);

    float scale = vol / 100.0f;
    size_t rem = frames, off = 0;
    while (rem > 0) {
      size_t chunk = rem > AUDIO_CHUNK_SAMPLES ? AUDIO_CHUNK_SAMPLES : rem;
      // FIX C6: dùng scratch[] ở member (BSS) thay vì stack alloc
      int16_t* buf = scratch;
      for (size_t i = 0; i < chunk * 2; i++) {
        buf[i] = (int16_t)(samples[off * 2 + i] * scale);
      }
      size_t wrote = i2sOut.write((uint8_t*)buf, chunk * 4);
      if (wrote < chunk * 4) gUnderrunCount++;
      rem -= chunk; off += chunk;
    }
  }
} audioR;

// =====================================================================================
// SECTION L: FILE PLAYER (LittleFS) - bounded array thay vector
// =====================================================================================

#define MAX_MUSIC_FILES 32

class FilePlayer {
public:
  // Cố định thay vì std::vector để tránh heap fragmentation
  char list[MAX_MUSIC_FILES][64];
  uint8_t count = 0;
  int8_t idx = -1;
  bool playing = false, paused = false;
  File file;
  // FIX C8: buffer thành member tránh stack overflow
  int16_t fileBuf[AUDIO_CHUNK_SAMPLES * 2];

  void begin() {
    if (LittleFS.begin(true)) {
      if (!LittleFS.exists("/music")) LittleFS.mkdir("/music");
      scan();
    }
  }

  void scan() {
    count = 0;
    File dir = LittleFS.open("/music");
    if (!dir) return;
    File f = dir.openNextFile();
    while (f && count < MAX_MUSIC_FILES) {
      const char* n = f.name();
      size_t l = strlen(n);
      if (l > 4 && (strstr(n, ".wav") || strstr(n, ".mp3"))) {
        snprintf(list[count], sizeof(list[count]), "/music/%s", n);
        count++;
      }
      f = dir.openNextFile();
    }
  }

  void playIndex(int8_t i) {
    if (!count) return;
    idx = (i < 0) ? (count - 1) : (i % count);
    if (file) file.close();
    file = LittleFS.open(list[idx], "r");
    if (file) {
      if (strstr(list[idx], ".wav")) file.seek(44);
      audioR.setSource(SRC_FILE);
      playing = true; paused = false;
    }
  }

  void pause()  { paused = true;  audioR.setAmp(false); }
  void resume() { paused = false; audioR.setAmp(true); }
  void stop()   { playing = false; if (file) file.close(); audioR.setSource(SRC_NONE); }
  void next()   { if (count) playIndex(idx + 1); }
  void prev()   { if (count) playIndex(idx - 1); }

  void loop() {
    if (!playing || paused || !file) return;
    size_t got = file.read((uint8_t*)fileBuf, sizeof(fileBuf));
    if (got > 0) audioR.writePcm(fileBuf, got / 4);
    else next();
  }
} fileP;

// =====================================================================================
// SECTION M: DUAL IR SENSOR
// =====================================================================================

class DualIr {
public:
  bool s1 = HIGH, s2 = HIGH;
  unsigned long t1Down = 0, t2Down = 0, lastChange = 0, lastRelease = 0;
  uint8_t tapCount = 0;
  IrEvent pending = IR_NONE;
  bool isActive = false;

  void begin() {
    pinMode(PIN_IR_PRIMARY, INPUT_PULLUP);
    pinMode(PIN_IR_SECONDARY, INPUT_PULLUP);
    s1 = digitalRead(PIN_IR_PRIMARY);
    s2 = digitalRead(PIN_IR_SECONDARY);
  }

  bool presence() const { return (s1 == IR_ACTIVE_LEVEL) || (s2 == IR_ACTIVE_LEVEL); }

  void update() {
    unsigned long now = millis();
    bool n1 = digitalRead(PIN_IR_PRIMARY);
    bool n2 = digitalRead(PIN_IR_SECONDARY);

    if (n1 != s1 && (now - lastChange) > IR_DEBOUNCE_MS) {
      lastChange = now; s1 = n1;
      if (n1 == IR_ACTIVE_LEVEL) {
        t1Down = now; isActive = true;
        // Fix C28: don't call display.notifyPresence() here (forward decl problem).
        // Brain task will call it when IR_TAP or any presence event arrives.
      } else {
        isActive = false;
        unsigned long dur = now - t1Down;
        if (dur < IR_TAP_MAX_MS) { tapCount++; lastRelease = now; }
        else if (dur > IR_LONG_HOLD_MS) pending = IR_HOLD;
      }
    }

    if (n2 != s2 && (now - lastChange) > IR_DEBOUNCE_MS) {
      lastChange = now; s2 = n2;
      if (n2 == IR_ACTIVE_LEVEL) t2Down = now;
    }

    if (s1 == IR_ACTIVE_LEVEL && s2 == IR_ACTIVE_LEVEL && t1Down && t2Down) {
      long diff = (long)t2Down - (long)t1Down;
      if (diff > 50 && diff < 450) { pending = IR_SWIPE_RIGHT; t1Down = t2Down = 0; }
      else if (diff < -50 && diff > -450) { pending = IR_SWIPE_LEFT; t1Down = t2Down = 0; }
    }

    if (tapCount > 0 && s1 != IR_ACTIVE_LEVEL && (now - lastRelease) > 350) {
      if      (tapCount == 1) pending = IR_TAP;
      else if (tapCount == 2) pending = IR_DOUBLE;
      else if (tapCount >= 3) pending = IR_TRIPLE;
      tapCount = 0;
    }
  }

  IrEvent consume() { IrEvent e = pending; pending = IR_NONE; return e; }
} ir;

// =====================================================================================
// SECTION N: DISPLAY MANAGER (clock + eyes + visualizer)
// =====================================================================================

class DisplayManager {
public:
  DisplayMode mode = DISP_CLOCK;
  VizStyle viz = VIZ_BEAT_EYES;
  unsigned long lastFrame = 0;
  float fps = 0;
  uint16_t frameCount = 0;
  unsigned long fpsTimer = 0;
  // FIX C24: double buffer tránh race condition giữa taskUsb (ghi) và taskDisplay (đọc)
  uint8_t streamBuf[2][1024];
  volatile uint8_t streamWriteIdx = 0;  // index buffer producer đang ghi
  volatile uint8_t streamReadIdx  = 1;  // index buffer consumer đang đọc (luôn khác write)
  volatile bool streamNew = false;
  unsigned long streamLastMs = 0;
  unsigned long lastPresence = 0;
  unsigned long bootMs = 0;
  float tempC = 28.5f;
  float rawChipTemp = 35.0f;
  bool  chipOverheat = false;
  unsigned long lastOverheatLog = 0;
  uint8_t trend[7] = {8, 12, 6, 14, 10, 16, 9};
  unsigned long lastTrendMs = 0;

  // He thong dong ho thuc phan cung (POSIX RTC)
  int clockYear  = 2026;
  int clockMonth = 10;
  int clockDay   = 9;
  int clockHour  = 20;
  int clockMin   = 0;
  int clockSec   = 0;
  int clockWday  = 5;

  void begin() {
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
      // Khoi phuc ve chan mac dinh de nguoi dung cam day lai
      Wire.end();
      Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
      Wire.setClock(100000);
      LOG_PRINTLN(F(" [OLED] !!! CANH BAO: KHONG TIM THAY OLED SSD1306 TREN CAC CHAN THU NGHIEM!"));
      LOG_PRINTLN(F(" [OLED] HUONG DAN NOI DAY CHUAN:"));
      LOG_PRINTLN(F("   1. VCC cua OLED    -> Chan 3V3 (3.3V) tren bo ESP32-S3"));
      LOG_PRINTLN(F("   2. GND cua OLED    -> Chan GND tren bo ESP32-S3"));
      LOG_PRINTLN(F("   3. SDA cua OLED    -> Chan GPIO 8 (IO8)"));
      LOG_PRINTLN(F("   4. SCL/SCK cua OLED -> Chan GPIO 9 (IO9)"));
      LOG_PRINTLN(F("   (Luu y: kiem tra tiep xuc day breadboard/day Dupont co bi long khong)"));
    }

    uint8_t targetAddr = (detectedAddr != 0) ? detectedAddr : OLED_ADDR;
    LOG_PRINTF(" [OLED] Bat dau khoi tao Adafruit SSD1306 tai 0x%02X...\n", targetAddr);

    // QUAN TRONG: Tham so periphBegin = false de Adafruit khong ghi de Wire.begin() mac dinh cua board!
    bool oledOk = oled.begin(SSD1306_SWITCHCAPVCC, targetAddr, false, false);
    LOG_PRINTF(" [OLED] Ket qua oled.begin -> %s\n", oledOk ? "OK (HOAT DONG TOT)" : "FAIL (THAT BAI)");

    if (oledOk) {
      oled.cp437(true); // Kich hoat bo ky tu CP437 cho ky hieu do C (degree)
      // Hien thi Splash chao mung ngay lap tuc de xac nhan phan cung OLED
      oled.clearDisplay();
      oled.drawRoundRect(0, 0, SCREEN_W, SCREEN_H, 6, SSD1306_WHITE);
      oled.setTextSize(1);
      oled.setTextColor(SSD1306_WHITE);
      oled.setCursor(20, 16);
      oled.print(F("MIU EILIK ROBOT"));
      oled.setCursor(22, 30);
      oled.printf("OLED 1306 [0x%02X]", targetAddr);
      oled.setCursor(30, 44);
      oled.print(F("KHOI DONG..."));
      oled.display();
      delay(800);
    }

    // Khoi tao RoboEyes
    roboEyes.begin(SCREEN_W, SCREEN_H, 30);
    roboEyes.setAutoblinker(ON, 3, 2);
    roboEyes.setIdleMode(ON, 2, 2);
    roboEyes.setMood(DEFAULT);
    roboEyes.setWidth(36, 36);
    roboEyes.setHeight(36, 36);
    roboEyes.open(); // BAT BUOC: Mo mat ngay lap tuc (mac dinh thu vien bat dau voi mat nham)
    roboEyes.eyeLheightCurrent = 36;
    roboEyes.eyeRheightCurrent = 36;

    bootMs = millis();
    fpsTimer = millis();
    lastPresence = millis();
    initClock();
    mode = DISP_CLOCK; // Bat dau o che do dong ho thoi gian thuc

    if (oledOk) {
      drawClock();
    }
    LOG_PRINTLN(F("========================================\n"));
  }

  static bool isLeap(int y) {
    return ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
  }

  static int daysInMonth(int y, int m) {
    const int dim[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && isLeap(y)) return 29;
    if (m >= 1 && m <= 12) return dim[m - 1];
    return 30;
  }

  void initClock() {
    setenv("TZ", "UTC0", 1);
    tzset();

    // Nếu ESP32 khởi động lại (soft reset) mà RTC đã có giờ hợp lệ (> năm 2025) thì giữ nguyên
    time_t cur = time(nullptr);
    if (cur > 1735689600) {
      updateClock();
      LOG_PRINTF(" [CLOCK] Giu nguyen gio RTC phan cung: %02d:%02d:%02d - Ngay %02d/%02d/%04d (Thu: %d)\n",
                 clockHour, clockMin, clockSec, clockDay, clockMonth, clockYear, clockWday);
      return;
    }

    const char* ds = __DATE__;
    const char* ts = __TIME__;
    char mStr[4] = {0};
    int yr = 2026, mo = 10, md = 9, h = 20, mn = 0, sc = 0;
    if (sscanf(ds, "%3s %d %d", mStr, &md, &yr) == 3) {
      static const char PROGMEM months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
      char* p = strstr_P(months, mStr);
      if (p) mo = ((p - months) / 3) + 1;
    }
    sscanf(ts, "%d:%d:%d", &h, &mn, &sc);

    struct tm t = {};
    t.tm_year = yr - 1900;
    t.tm_mon  = mo - 1;
    t.tm_mday = md;
    t.tm_hour = h;
    t.tm_min  = mn;
    t.tm_sec  = sc;
    t.tm_isdst = -1;
    time_t epoch = mktime(&t);
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);

    updateClock();
    LOG_PRINTF(" [CLOCK] Khoi tao RTC phan cung: %02d:%02d:%02d - Ngay %02d/%02d/%04d (Thu: %d)\n",
               clockHour, clockMin, clockSec, clockDay, clockMonth, clockYear, clockWday);
  }

  void setTime(int h, int m, int s) {
    time_t nowEpoch = time(nullptr);
    struct tm ti;
    localtime_r(&nowEpoch, &ti);
    ti.tm_hour = constrain(h, 0, 23);
    ti.tm_min  = constrain(m, 0, 59);
    ti.tm_sec  = constrain(s, 0, 59);
    ti.tm_isdst = -1;
    time_t epoch = mktime(&ti);
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    updateClock();
    LOG_PRINTF(" [CLOCK] Da cap nhat gio moi: %02d:%02d:%02d\n", clockHour, clockMin, clockSec);
  }

  void setDate(int d, int m, int y) {
    time_t nowEpoch = time(nullptr);
    struct tm ti;
    localtime_r(&nowEpoch, &ti);
    ti.tm_year = constrain(y, 2020, 2099) - 1900;
    ti.tm_mon  = constrain(m, 1, 12) - 1;
    ti.tm_mday = constrain(d, 1, daysInMonth(y, m));
    ti.tm_isdst = -1;
    time_t epoch = mktime(&ti);
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    updateClock();
    LOG_PRINTF(" [CLOCK] Da cap nhat ngay moi: %02d/%02d/%04d\n", clockDay, clockMonth, clockYear);
  }

  void updateClock() {
    time_t nowEpoch = time(nullptr);
    struct tm ti;
    localtime_r(&nowEpoch, &ti);
    clockYear  = ti.tm_year + 1900;
    clockMonth = ti.tm_mon + 1;
    clockDay   = ti.tm_mday;
    clockHour  = ti.tm_hour;
    clockMin   = ti.tm_min;
    clockSec   = ti.tm_sec;
    clockWday  = ti.tm_wday;
  }

  void notifyPresence() {
    lastPresence = millis();
    if (mode == DISP_CLOCK) {
      mode = DISP_EYES;
      roboEyes.setMood(DEFAULT);
    }
  }

  // ============ GIAO DIEN DONG HO GOC NGUYEN BAN CUA ROBOT MIU EILIK ============
  void drawSun(int16_t x, int16_t y) {
    oled.drawCircle(x, y, 4, SSD1306_WHITE);
    oled.drawFastHLine(x - 7, y, 3, SSD1306_WHITE);
    oled.drawFastHLine(x + 5, y, 3, SSD1306_WHITE);
    oled.drawFastVLine(x, y - 7, 3, SSD1306_WHITE);
    oled.drawFastVLine(x, y + 5, 3, SSD1306_WHITE);
  }

  void drawCloud(int16_t x, int16_t y) {
    oled.fillCircle(x + 5,  y + 6, 4, SSD1306_WHITE);
    oled.fillCircle(x + 11, y + 4, 5, SSD1306_WHITE);
    oled.fillCircle(x + 16, y + 7, 3, SSD1306_WHITE);
    oled.fillRect(x + 4, y + 6, 13, 5, SSD1306_WHITE);
  }

  void drawBars(int16_t sx, int16_t baseY) {
    for (int i = 0; i < 7; i++) {
      int bx = sx + i * 6;
      int h = constrain(trend[i], 2, 18);
      for (int y = 0; y < h; y += 2) {
        oled.drawFastHLine(bx, baseY - y, 4, SSD1306_WHITE);
      }
    }
  }

  void drawDroplet(int16_t x, int16_t y) {
    oled.drawPixel(x + 2, y, SSD1306_WHITE);
    oled.drawLine(x + 1, y + 1, x + 3, y + 1, SSD1306_WHITE);
    oled.drawLine(x, y + 2, x + 4, y + 2, SSD1306_WHITE);
    oled.drawLine(x, y + 3, x + 4, y + 3, SSD1306_WHITE);
    oled.drawLine(x + 1, y + 4, x + 3, y + 4, SSD1306_WHITE);
  }

  void drawThermometer(int16_t x, int16_t y) {
    oled.drawLine(x + 1, y, x + 1, y + 4, SSD1306_WHITE);
    oled.drawPixel(x, y + 1, SSD1306_WHITE);
    oled.drawPixel(x + 2, y + 1, SSD1306_WHITE);
    oled.fillCircle(x + 1, y + 6, 2, SSD1306_WHITE);
  }

  void drawClock() {
    oled.clearDisplay();
    updateClock();

    unsigned long now = millis();
    if (now - lastTrendMs >= 2500) {
      lastTrendMs = now;
      for (int i = 0; i < 6; i++) trend[i] = trend[i + 1];
      int nxt = 6 + (int)(sinf((float)now * 0.0015f) * 4.0f) + (random(0, 5));
      trend[6] = constrain(nxt, 2, 18);
    }

    // Đọc cảm biến nhiệt ESP32-S3
    float rawT = temperatureRead();
    rawChipTemp = rawT;
    if (rawT >= 70.0f) {
      chipOverheat = true;
    } else if (rawT < 65.0f) {
      chipOverheat = false;
    }

    if (rawT >= 20.0f && rawT <= 85.0f) {
      float estimatedRoomT = rawT - 28.5f;
      estimatedRoomT = constrain(estimatedRoomT, 22.0f, 38.0f);
      tempC = tempC * 0.92f + estimatedRoomT * 0.08f;
    }

    // 1. Icon Mặt trời + Đám mây (Góc trên bên trái)
    drawSun(11, 11);
    drawCloud(22, 11);

    // 2. Giờ phút to Size 2 ở giữa
    oled.setTextSize(2);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(44, 2);
    bool blink = (now / 500) & 1;
    char tBuf[8];
    snprintf(tBuf, sizeof(tBuf), "%02d%c%02d", clockHour, blink ? ':' : ' ', clockMin);
    oled.print(tBuf);

    // 3. Giây Size 1 ở góc trên
    oled.setTextSize(1);
    oled.setCursor(106, 3);
    oled.printf(":%02d", clockSec);

    // 4. Hàng dưới nửa trên: Thứ, Ngày/Tháng, IDLE
    static const char PROGMEM DAYS[] = "CN\0T2\0T3\0T4\0T5\0T6\0T7";
    char dayBuf[4];
    strncpy_P(dayBuf, &DAYS[clockWday * 3], 3);
    dayBuf[3] = '\0';
    oled.drawRoundRect(44, 19, 24, 9, 2, SSD1306_WHITE);
    oled.setCursor(48, 20);
    oled.print(dayBuf);

    oled.setCursor(72, 20);
    oled.printf("%02d/%02d", clockDay, clockMonth);

    oled.setCursor(106, 20);
    oled.print(F("IDLE"));

    // 5. Đường kẻ ngang phân cách nửa trên và nửa dưới (Y = 30)
    oled.drawLine(0, 30, SCREEN_W - 1, 30, SSD1306_WHITE);

    // 6. Nửa dưới - Trái: TREND sóng
    oled.setCursor(2, 33);
    oled.print(F("TREND"));
    drawBars(2, 62);

    // 7. Đường kẻ dọc phân cách (X = 48)
    oled.drawLine(48, 32, 48, 62, SSD1306_WHITE);

    // 8. Nửa dưới - Phải: Độ ẩm, Nhiệt độ, Trạng thái hồng ngoại / Cảnh báo
    drawDroplet(52, 34);
    oled.setCursor(60, 34);
    oled.printf("%d%%", 60);

    drawThermometer(52, 45);
    oled.setCursor(60, 45);
    oled.printf("%.1fC", tempC);

    oled.setCursor(52, 55);
    if (chipOverheat) {
      bool oblink = (now / 350) & 1;
      oled.print(oblink ? F("!CHIP HOT!") : F("          "));
    } else if (ir.presence()) {
      oled.print(F("IR: CO VAT"));
    } else {
      oled.print(F("IR: SAN SANG"));
    }

    oled.display();
  }

  void drawEyes() {
    float rawT = temperatureRead();
    rawChipTemp = rawT;
    if (rawT >= 70.0f) {
      chipOverheat = true;
      roboEyes.setSweat(ON);
    } else if (rawT < 65.0f) {
      chipOverheat = false;
      roboEyes.setSweat(OFF);
    }

    if (emotion.hasPending) {
      roboEyes.setMood(emotion.pendingMood);
      if (emotion.pendingLaugh)   roboEyes.anim_laugh();
      if (emotion.pendingConfused) roboEyes.anim_confused();
      emotion.hasPending = false;
    }
    roboEyes.drawEyes();
  }

  void drawVisual() {
    AudioMetrics mt; audio.get(mt);
    if (viz == VIZ_BEAT_EYES) {
      byte sz = 34 + (byte)(mt.bands[0] * 20.0f);
      roboEyes.setWidth(sz, sz);
      roboEyes.setHeight(sz, sz);
      roboEyes.setMood(HAPPY);
      roboEyes.drawEyes();
    } else if (viz == VIZ_WAVE_EYES) {
      oled.clearDisplay();
      oled.drawRoundRect(14, 14, 44, 36, 8, SSD1306_WHITE);
      oled.drawRoundRect(70, 14, 44, 36, 8, SSD1306_WHITE);
      float phase = millis() * 0.02f;
        for (int x = 18; x < 54; x++) {
        int y = 32 + (int)(sinf((x - 18) * 0.35f + phase) * mt.rmsL * 16.0f);
        oled.drawPixel(x, constrain(y, 18, 46), SSD1306_WHITE);
        }
        for (int x = 74; x < 110; x++) {
        int y = 32 + (int)(sinf((x - 74) * 0.35f + phase) * mt.rmsR * 16.0f);
        oled.drawPixel(x, constrain(y, 18, 46), SSD1306_WHITE);
        }
      oled.display();
      } else {
      oled.clearDisplay();
      oled.drawRoundRect(14, 14, 44, 36, 8, SSD1306_WHITE);
      oled.drawRoundRect(70, 14, 44, 36, 8, SSD1306_WHITE);
        for (int b = 0; b < 4; b++) {
        int h = (int)(mt.bands[b] * 28.0f);
        oled.fillRect(18 + b * 9, 46 - h, 6, h, SSD1306_WHITE);
        }
        for (int b = 4; b < 8; b++) {
        int h = (int)(mt.bands[b] * 28.0f);
        oled.fillRect(74 + (b - 4) * 9, 46 - h, 6, h, SSD1306_WHITE);
      }
      oled.display();
    }
  }

  void drawStream() {
    if (streamNew) {
      oled.clearDisplay();
      // FIX C24: đọc từ buffer ReadIdx (cố định, không bị producer ghi đè)
      oled.drawBitmap(0, 0, streamBuf[streamReadIdx], SCREEN_W, SCREEN_H, SSD1306_WHITE);
      oled.display();
      streamNew = false;
    }
  }

  void setStreamFrame(const uint8_t* bmp) {
    if (!bmp) return;
    // FIX C24: ghi vào WriteIdx, swap read/write nguyên tử (1-byte atomic)
    memcpy(streamBuf[streamWriteIdx], bmp, 1024);
    // Swap an toàn vì 2 core cùng chỉ đọc/ghi 1 byte
    uint8_t newRead = streamWriteIdx;
    streamWriteIdx = streamReadIdx;
    streamReadIdx  = newRead;
    streamNew = true; streamLastMs = millis();
  }

  void update() {
    unsigned long now = millis();
    unsigned long dt = (lastFrame > 0) ? (now - lastFrame) : 33;
    lastFrame = now;

    frameCount++;
    if (now - fpsTimer >= 1000) {
      // FIX M4: FPS = 1000ms / delta trung bình (chính xác hơn counter snapshot)
      fps = (fps > 0) ? fps * 0.7f + (1000.0f / (float)dt) * 0.3f : (1000.0f / (float)dt);
      frameCount = 0; fpsTimer = now;
    }

    // Luon cap nhat gio noi tai moi frame display
    updateClock();

    // Kiem tra tin hieu mach hong ngoai:
    // Neu co vat can hoac tay nguoi truoc mat -> lap tuc danh thuc robot sang DISP_EYES
    if (ir.presence()) {
      lastPresence = now;
      if (mode == DISP_CLOCK) {
        mode = DISP_EYES;
        roboEyes.setMood(DEFAULT);
      }
    }

    if (mode == DISP_STREAM && (now - streamLastMs) > 2000) mode = DISP_CLOCK;

    bool music = audioR.isMusic();
    if (music && (mode == DISP_EYES || mode == DISP_CLOCK)) mode = DISP_VISUAL;
    else if (!music && mode == DISP_VISUAL) {
      mode = DISP_EYES;
      roboEyes.setWidth(36, 36); roboEyes.setHeight(36, 36);
    }

    // Yeu cau 1: CHE DO DONG HO CHI HIEN THI KHI ROBOT RANH VA KO NHAN TIN HIEU HONG NGOAI
    // Dieu kien vao DISP_CLOCK:
    // - Khong co nhac (!music)
    // - Dang o DISP_EYES
    // - KHONG CO VAT CAN / KHONG CO TIN HIEU HONG NGOAI (!ir.presence())
    // - Da qua thoi gian ranh roi (now - lastPresence >= IR_PRESENCE_TIMEOUT_MS)
    if (!music && mode == DISP_EYES && !ir.presence() && (now - lastPresence) >= IR_PRESENCE_TIMEOUT_MS) {
      mode = DISP_CLOCK;
    }

    switch (mode) {
      case DISP_CLOCK:  drawClock(); break;
      case DISP_EYES:   drawEyes(); break;
      case DISP_VISUAL: drawVisual(); break;
      case DISP_STREAM: drawStream(); break;
    }
  }

  const char* modeStr() const {
    switch (mode) {
      case DISP_CLOCK:  return PSTR("CLOCK");
      case DISP_EYES:   return PSTR("EYES");
      case DISP_VISUAL: return PSTR("VISUAL");
      case DISP_STREAM: return PSTR("STREAM");
    }
    return PSTR("?");
  }
} display;

// =====================================================================================
// SECTION P: AUTONOMY (placed BEFORE UsbLink so UsbLink JSON handler can call setMode)
// =====================================================================================

class Autonomy {
public:
  AutoMode mode = AUTO_CURIOUS;
  unsigned long manualUntil = 0, lastAction = 0;
  uint32_t lastBeat = 0;
  uint16_t interval = 4000;

  void notifyManual() {
    manualUntil = millis() + 8000;
    display.notifyPresence();
  }
  bool isManual() const { return (int32_t)(millis() - manualUntil) < 0; }

  void setMode(AutoMode m) {
    mode = m; lastAction = millis();
    display.notifyPresence();
    if      (m == AUTO_SLEEP)   emotion.setEmotion(EMO_SLEEPY);
    else if (m == AUTO_CURIOUS) emotion.setEmotion(EMO_CURIOUS);
    else if (m == AUTO_CALM)    emotion.setEmotion(EMO_NEUTRAL);
    else if (m == AUTO_PLAYFUL) emotion.setEmotion(EMO_HAPPY);
  }

  void update() {
    if (isManual()) return;

    if (audioR.isMusic()) {
      AudioMetrics m; audio.get(m);
      if (m.beatCount != lastBeat) {
        lastBeat = m.beatCount;
        head.setTarget(head.pan.target - head.pan.offset, 98.0f, 90.0f);
      } else if (!head.isMoving()) {
        head.setTarget(head.pan.target - head.pan.offset, 90.0f, 60.0f);
      }
      return;
    }

    if (gesture.playing || mode == AUTO_SLEEP) return;

    if (millis() - lastAction >= interval) {
      lastAction = millis();
      interval = random(4000, 8000);
      switch (mode) {
        case AUTO_CURIOUS:
          if (random(2)) head.setTarget(random(70, 110), random(80, 100), 80.0f);
          else gesture.play(random(2) ? GES_TILT_LEFT : GES_TILT_RIGHT);
          break;
        case AUTO_PLAYFUL:
          gesture.play(GES_BOUNCE_HAPPY);
          break;
        case AUTO_CALM:
          head.setTarget(PAN_CENTER_DEG + random(-15, 16), TILT_CENTER_DEG + random(-3, 4), 50.0f);
          break;
        default: break;
      }
    }
  }
} autonomy;

// (Old SECTION P removed - now above)

// =====================================================================================
// SECTION O: USB BINARY PROTOCOL + JSON (hash-dispatched)
// =====================================================================================

class UsbLink {
public:
  enum State : uint8_t { S_SYNC1, S_SYNC2, S_TYPE, S_LENH, S_LENL, S_PAYLOAD, S_CHKSUM };
  State state = S_SYNC1;
  uint8_t pktType = 0;
  uint16_t pktLen = 0, rxCount = 0;
  uint8_t payload[1050];
  uint8_t xorSum = 0;

  void update() {
    while (Serial.available()) {
      uint8_t b = Serial.read();
      switch (state) {
        case S_SYNC1:
          if (b == USB_SYNC_1) state = S_SYNC2;
          break;
        case S_SYNC2:
          if (b == USB_SYNC_2) { state = S_TYPE; xorSum = 0; }
          else state = S_SYNC1;
          break;
        case S_TYPE:
          pktType = b; xorSum ^= b; state = S_LENH;
          break;
        case S_LENH:
          pktLen = ((uint16_t)b) << 8; xorSum ^= b; state = S_LENL;
          break;
        case S_LENL:
          pktLen |= b; xorSum ^= b; rxCount = 0;
          state = (pktLen > 1040) ? S_SYNC1 : (pktLen == 0 ? S_CHKSUM : S_PAYLOAD);
          break;
        case S_PAYLOAD:
          payload[rxCount++] = b; xorSum ^= b;
          if (rxCount >= pktLen) state = S_CHKSUM;
          break;
        case S_CHKSUM:
          if (b == xorSum) handle();
          state = S_SYNC1;
          break;
      }
    }
  }

  // OPTIMIZED: dùng hash dispatch thay strcmp
  void processJson(const char* json, size_t len) {
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, json, len) != DeserializationError::Ok) return;

    for (auto kv : doc.as<JsonObject>()) {
      uint32_t k = hashKey(kv.key().c_str());
      switch (k) {
        case CmdHash::EMOTION:
          if (kv.value().is<const char*>()) {
            emotion.setEmotion(EmotionEngine::fromHash(hashKey(kv.value().as<const char*>())));
          } else if (kv.value().is<int>()) {
            emotion.setEmotion((Emotion)constrain(kv.value().as<int>(), 0, (int)EMOTION_COUNT - 1));
          }
          break;
        case CmdHash::GESTURE:
          if (kv.value().is<int>()) gesture.play((Gesture)constrain(kv.value().as<int>(), 0, (int)GESTURE_COUNT));
          break;
        case CmdHash::PAN:
          if (kv.value().is<float>()) {
            float t = kv.value().as<float>();
            head.setTarget(t, head.tilt.target - head.tilt.offset);
          }
          break;
        case CmdHash::TILT:
          if (kv.value().is<float>()) {
            float t = kv.value().as<float>();
            head.setTarget(head.pan.target - head.pan.offset, t);
          }
          break;
        case CmdHash::CENTER:
          if (kv.value().as<bool>()) head.center();
          break;
        case CmdHash::CALIB_P:
        case CmdHash::CALIB_T: {
          int8_t p = doc["calib_pan"] | 0;
          int8_t to = doc["calib_tilt"] | 0;
          head.saveCalib(p, to);
          break;
        }
        case CmdHash::SOURCE:
          audioR.setSource((AudioSource)constrain(kv.value().as<int>(), 0, 3));
          break;
        case CmdHash::VOL:
          audioR.setVol(constrain(kv.value().as<int>(), 0, 100));
          break;
        case CmdHash::PLAY:  fileP.playIndex(kv.value().as<int>()); break;
        case CmdHash::PAUSE: fileP.pause(); break;
        case CmdHash::NEXT:  fileP.next();  break;
        case CmdHash::PREV:  fileP.prev();  break;
        case CmdHash::MODE:
          if (kv.value().is<const char*>()) {
            uint32_t mh = hashKey(kv.value().as<const char*>());
            switch (mh) {
              case ModeHash::MODE_SLEEP:   autonomy.setMode(AUTO_SLEEP); break;
              case ModeHash::MODE_CALM:    autonomy.setMode(AUTO_CALM); break;
              case ModeHash::MODE_CURIOUS: autonomy.setMode(AUTO_CURIOUS); break;
              case ModeHash::MODE_PLAYFUL: autonomy.setMode(AUTO_PLAYFUL); break;
            }
          }
          break;
      }
    }
  }

  void handle() {
    display.notifyPresence();
    if (pktType == PKT_AUDIO_PCM && pktLen >= 4) {
      audioR.setSource(SRC_USB_PC);
      audioR.writePcm((const int16_t*)payload, pktLen / 4);
    } else if (pktType == PKT_OLED_BITMAP && pktLen == 1024) {
      display.mode = DISP_STREAM;
      display.setStreamFrame(payload);
    } else if (pktType == PKT_CMD_JSON && pktLen > 0 && pktLen < sizeof(payload)) {
      // FIX B4: bounds check + null-terminate an toàn
      payload[pktLen] = '\0';
      processJson((const char*)payload, pktLen);
    } else if (pktType == PKT_TELEMETRY_REQ) {
      uint8_t res[11] = {
        USB_SYNC_1, USB_SYNC_2, 0x84, 0x00, 0x06,
        (uint8_t)roundf(head.pan.current), (uint8_t)roundf(head.tilt.current),
        (uint8_t)roundf(display.fps), (uint8_t)audioR.src,
        (uint8_t)(gUnderrunCount & 0xFF), 0
      };
      res[10] = res[5] ^ res[6] ^ res[7] ^ res[8] ^ res[9];
      Serial.write(res, 11);
    }
  }
} usb;

// (Autonomy moved BEFORE UsbLink - see above)

// =====================================================================================
// SECTION Q: FREERTOS TASKS (2 cores)
// =====================================================================================

// CORE 0:
void taskAudio(void*) {
  while (true) {
    fileP.loop();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void taskUsb(void*) {
  while (true) {
    usb.update();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// CORE 1:
void taskServo(void*) {
  TickType_t last = xTaskGetTickCount();
  uint32_t lastMs = millis();
  while (true) {
    // FIX C18: dt thực thay vì hardcoded 20ms (tránh servo chạy nhanh khi task bị stall)
    uint32_t nowMs = millis();
    float dt = (nowMs - lastMs) / 1000.0f;
    if (dt > 0.1f) dt = 0.1f;  // cap nếu task bị stall (ví dụ: 100ms)
    lastMs = nowMs;
    head.update(dt);
    vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
  }
}

void taskDisplay(void*) {
  TickType_t last = xTaskGetTickCount();
  while (true) {
    display.update();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(33));  // 30 FPS
  }
}

void taskBrain(void*) {
  TickType_t last = xTaskGetTickCount();
  while (true) {
    ir.update();
    // Yeu cau 1: Ngay khi co vat can truoc mat (ir.presence()) -> lap tuc danh thuc robot sang DISP_EYES
    if (ir.presence()) {
      display.notifyPresence();
    }
    IrEvent ev = ir.consume();
    if (ev != IR_NONE) {
      autonomy.notifyManual();
      display.notifyPresence();
      switch (ev) {
        case IR_TAP:    emotion.setEmotion(EMO_HAPPY); break;
        case IR_DOUBLE: {
          uint8_t next = (autonomy.mode + 1) % 4;
          autonomy.setMode((AutoMode)next);
          break;
        }
        case IR_TRIPLE: gesture.play(GES_NOD_YES); break;
        case IR_HOLD:
          if (fileP.playing) { if (fileP.paused) fileP.resume(); else fileP.pause(); }
          else emotion.setEmotion(EMO_SLEEPY);
          break;
        case IR_SWIPE_RIGHT: fileP.next();  emotion.setEmotion(EMO_HAPPY);   break;
        case IR_SWIPE_LEFT:  fileP.prev();  emotion.setEmotion(EMO_CURIOUS); break;
        default: break;
      }
    }
    gesture.update();
    autonomy.update();
    vTaskDelayUntil(&last, pdMS_TO_TICKS(20));
  }
}

// =====================================================================================
// SECTION R: SETUP & LOOP
// =====================================================================================

void setup() {
  Serial0.begin(115200);
  Serial.begin(115200);
  delay(300);

  LOG_PRINTLN(F("\n====================================================="));
  LOG_PRINTLN(F(" MIU EILIK ESP32-S3 - OPTIMIZED v6.15                "));
  LOG_PRINTLN(F(" PROGMEM | FNV-1a | Streaming I2S | 2-core RTOS     "));
  LOG_PRINTLN(F(" Chuc nang moi: Sad/Cry/Wink/Shock | Dog Tilt Servo "));
  LOG_PRINTLN(F(" IR Smart Idle Wakeup | ESP32-S3 Temp Protection    "));
  LOG_PRINTLN(F("====================================================="));

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);

  gStateMutex = xSemaphoreCreateMutex();
  audio.begin();
  head.begin();
  display.begin();
  audioR.begin();
  fileP.begin();
  ir.begin();

  xTaskCreatePinnedToCore(taskAudio,   "Audio",   4096, NULL, 5, NULL, 0);
  xTaskCreatePinnedToCore(taskUsb,     "USB",     4096, NULL, 4, NULL, 0);
  xTaskCreatePinnedToCore(taskServo,   "Servo",   4096, NULL, 5, NULL, 1);
  xTaskCreatePinnedToCore(taskDisplay, "Display", 6144, NULL, 4, NULL, 1);
  xTaskCreatePinnedToCore(taskBrain,   "Brain",   4096, NULL, 3, NULL, 1);

  LOG_PRINTLN(F("[OK] 5 tasks started on 2 cores"));
}

void checkSerialCommands() {
  Stream* ports[] = { &Serial, &Serial0 };
  for (Stream* p : ports) {
    while (p->available()) {
      String line = p->readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;

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
      } else if (line.startsWith("EMO=") || line.startsWith("EMO ")) {
        String em = line.substring(line.indexOf('=') >= 0 ? line.indexOf('=') + 1 : 4);
        em.toUpperCase();
        em.trim();
        display.notifyPresence();
        if      (em == "HAPPY")   emotion.setEmotion(EMO_HAPPY);
        else if (em == "SAD")     emotion.setEmotion(EMO_SAD);
        else if (em == "CRY" || em == "CRYING") emotion.setEmotion(EMO_CRYING);
        else if (em == "WINK")    emotion.setEmotion(EMO_WINK);
        else if (em == "SHOCK" || em == "SHOCKED") emotion.setEmotion(EMO_SHOCKED);
        else if (em == "ANGRY")   emotion.setEmotion(EMO_ANGRY);
        else if (em == "SLEEP" || em == "SLEEPY") emotion.setEmotion(EMO_SLEEPY);
        else if (em == "CURIOUS") emotion.setEmotion(EMO_CURIOUS);
        else                      emotion.setEmotion(EMO_NEUTRAL);
        LOG_PRINTF("[OK] Emotion set -> %s\n", em.c_str());
      } else if (line.startsWith("GES=") || line.startsWith("GES ")) {
        String ge = line.substring(line.indexOf('=') >= 0 ? line.indexOf('=') + 1 : 4);
        ge.toUpperCase();
        ge.trim();
        display.notifyPresence();
        if      (ge == "NOD")    gesture.play(GES_NOD_YES);
        else if (ge == "SHAKE")  gesture.play(GES_SHAKE_NO);
        else if (ge == "TILT_L" || ge == "CUN_L") gesture.play(GES_TILT_LEFT);
        else if (ge == "TILT_R" || ge == "CUN_R") gesture.play(GES_TILT_RIGHT);
        else if (ge == "LEFT")   gesture.play(GES_LOOK_LEFT);
        else if (ge == "RIGHT")  gesture.play(GES_LOOK_RIGHT);
        else if (ge == "UP")     gesture.play(GES_LOOK_UP);
        else if (ge == "BOW" || ge == "SAD") gesture.play(GES_BOW_SAD);
        else if (ge == "BOUNCE") gesture.play(GES_BOUNCE_HAPPY);
        else if (ge == "SLEEP")  gesture.play(GES_SLEEP_DROOP);
        else if (ge == "WAKE")   gesture.play(GES_WAKE_STRETCH);
        LOG_PRINTF("[OK] Gesture play -> %s\n", ge.c_str());
      } else if (line.startsWith("PAN=") || line.startsWith("PAN ")) {
        float p = line.substring(line.indexOf('=') >= 0 ? line.indexOf('=') + 1 : 4).toFloat();
        head.attach();
        head.pan.setTarget(p, 120.0f);
        LOG_PRINTF("[SERVO TEST] PAN setTarget = %.1f deg (Tilt giu nguyen)\n", p);
      } else if (line.startsWith("TILT=") || line.startsWith("TILT ")) {
        float t = line.substring(line.indexOf('=') >= 0 ? line.indexOf('=') + 1 : 5).toFloat();
        head.attach();
        head.tilt.setTarget(t, 120.0f);
        LOG_PRINTF("[SERVO TEST] TILT setTarget = %.1f deg (Pan giu nguyen)\n", t);
      } else if (line.equalsIgnoreCase("CENTER")) {
        head.center();
        LOG_PRINTLN(F("[SERVO TEST] Ca 2 servo ve goc tam 90.0 deg"));
      } else if (line.startsWith("SYNC=") || line.startsWith("SYNC ")) {
        unsigned long ep = strtoul(line.c_str() + 5, NULL, 10);
        if (ep > 1700000000) {
          struct timeval tv = { .tv_sec = (time_t)ep, .tv_usec = 0 };
          settimeofday(&tv, NULL);
          display.updateClock();
          LOG_PRINTF("[OK] Da DONG BO GIO TU MAY TINH (Epoch=%lu) -> %02d:%02d:%02d - %02d/%02d/%04d\n",
                     ep, display.clockHour, display.clockMin, display.clockSec, display.clockDay, display.clockMonth, display.clockYear);
        }
      } else if (line.equalsIgnoreCase("IR") || line.equalsIgnoreCase("TEST_IR")) {
        bool p1 = digitalRead(PIN_IR_PRIMARY);
        bool p2 = digitalRead(PIN_IR_SECONDARY);
        LOG_PRINTF("[IR TEST] Primary(IO%d)=%s | Secondary(IO%d)=%s | Presence=%s\n",
                   PIN_IR_PRIMARY, (p1 == IR_ACTIVE_LEVEL) ? "ACTIVE" : "IDLE",
                   PIN_IR_SECONDARY, (p2 == IR_ACTIVE_LEVEL) ? "ACTIVE" : "IDLE",
                   ir.presence() ? "YES (CO VAT CAN)" : "NO (RANH ROI)");
      } else if (line.equalsIgnoreCase("CHIP") || line.equalsIgnoreCase("TEMP")) {
        float ct = temperatureRead();
        LOG_PRINTF("[CHIP TEST] ESP32-S3 Temp = %.1f C | Canh bao qua nhiet = %s\n",
                   ct, (ct >= 70.0f) ? "CO (!CANH BAO!)" : "BINH THUONG");
      }
    }
  }
}

unsigned long lastTelem = 0;

void loop() {
  checkSerialCommands();
  unsigned long now = millis();

  // Canh bao qua nhiet chip chu ky moi 3s neu chip >= 70C
  if (display.chipOverheat && (now - display.lastOverheatLog >= 3000)) {
    display.lastOverheatLog = now;
    LOG_PRINTF(" [CANH BAO] CHIP ESP32-S3 QUA NHIET: %.1f C! Hay kiem tra nguon cap va tan nhiet!\n", display.rawChipTemp);
  }

  if (now - lastTelem >= 10000) {
    lastTelem = now;
    LOG_PRINTF("[TELEM] up=%lus fps=%.0f heap=%u psram=%u und=%u pan=%.1f tilt=%.1f chipT=%.1fC mode=%s\n",
      now / 1000, display.fps, ESP.getFreeHeap(), ESP.getFreePsram(),
      gUnderrunCount, head.pan.current, head.tilt.current, display.rawChipTemp, display.modeStr());
  }
  delay(50);
}
