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
//   RoboEyes.h defines DEFAULT, TIRED, ANGRY, HAPPY at top level.
//   Some Arduino/Adafruit headers may also define these -> redefine warning/error.
//   Solution: undefine them before including, then redefine our own values.
#undef DEFAULT
#undef TIRED
#undef ANGRY
#undef HAPPY
#include "FluxGarage_RoboEyes.h"
// Bring back RoboEy's values (we use them via names below)
#define DEFAULT 0
#define TIRED   1
#define ANGRY   2
#define HAPPY   3

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
#define EMOTION_COUNT           7
#define GESTURE_COUNT           11
#define AUDIO_SRC_COUNT         4

// =====================================================================================
// SECTION A2: ENUMS
// =====================================================================================

enum AudioSource : uint8_t { SRC_NONE = 0, SRC_FILE, SRC_USB_PC, SRC_BT_EXT };
enum VizStyle    : uint8_t { VIZ_BEAT_EYES = 0, VIZ_WAVE_EYES, VIZ_BARS_EYES };
enum DisplayMode : uint8_t { DISP_CLOCK = 0, DISP_EYES = 1, DISP_VISUAL = 2, DISP_STREAM = 3 };
enum Emotion     : uint8_t { EMO_NEUTRAL, EMO_HAPPY, EMO_ANGRY, EMO_SLEEPY, EMO_CURIOUS, EMO_CONFUSED, EMO_WAKE_UP };
enum Gesture     : uint8_t {
  GES_NONE,
  GES_NOD_YES,        // Gật đầu (có sẵn)
  GES_SHAKE_NO,       // Lắc đầu (có sẵn)
  GES_TILT_CURIOUS,   // Nghiêng đầu tò mò (có sẵn)
  GES_SLEEP_DROOP,    // Buồn ngủ (có sẵn)
  GES_WAKE_STRETCH,   // Vươn vai khi thức (có sẵn)
  GES_BOUNCE_HAPPY,   // Nhún nhảy vui (có sẵn)
  GES_ANGRY_SNAP,     // Cáu giận (có sẵn)
  // -------- Them moi tu desk-emoji-v3 --------
  GES_HEAD_LOOK_LEFT,   // Quay đầu sang trái (mượt)
  GES_HEAD_LOOK_RIGHT,  // Quay đầu sang phải (mượt)
  GES_HEAD_LOOK_UP,     // Ngước lên
  GES_HEAD_LOOK_DOWN    // Cúi xuống
};
enum IrEvent     : uint8_t { IR_NONE, IR_TAP, IR_DOUBLE, IR_TRIPLE, IR_HOLD, IR_SWIPE_RIGHT, IR_SWIPE_LEFT };
enum AutoMode    : uint8_t { AUTO_SLEEP, AUTO_CALM, AUTO_CURIOUS, AUTO_PLAYFUL };

// Forward declarations: Desk-Emoji action and servo functions
void head_move(int x_offset = 0, int y_offset = 0, int servo_delay = 10);
void head_center(int servo_delay = 10);
void head_right(int offset = 20);
void head_left(int offset = 20);
void head_down(int offset = 15);
void head_up(int offset = 15);
void head_nod(int servo_delay = 3);
void head_shake(int servo_delay = 3);
void head_roll_left(int servo_delay = 10);
void head_roll_right(int servo_delay = 10);

void eye_blink();
void eye_happy();
void eye_sad();
void eye_anger();
void eye_surprise();
void eye_right();
void eye_left();
void eye_center();

void executeCommand(String cmd);
void sequent_act(String actions);
void random_act();
void speech_act();
void async_sequent_act(String actions);
void async_random_act();
void async_speech_act();
void processSerialLine(String line);

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

  // -------- Thêm mới từ desk-emoji-v3: 4 hướng nhìn (dùng setTarget trực tiếp) --------
  // Smooth hơn vì ServoAxis đã có trapezoidal motion (giatoc + giamtoc)
  // Pan range: 40..140 (X), 65..115 (Y)
  void lookLeft()  { setTarget(60,  90, 180.0f); }   // Quay trai 30 do
  void lookRight() { setTarget(120, 90, 180.0f); }   // Quay phai 30 do
  void lookUp()    { setTarget(90,  70, 180.0f); }   // Nguoc len 20 do
  void lookDown()  { setTarget(90,  110, 180.0f); }  // Cui xuong 20 do

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

static const Keyframe PROGMEM TRACK_NOD[]    = { {90,106,160},{90,82,160},{90,104,140},{90,86,140},{90,90,180} };
static const Keyframe PROGMEM TRACK_SHAKE[]   = { {115,90,150},{65,90,200},{110,90,180},{70,90,180},{90,90,180} };
static const Keyframe PROGMEM TRACK_CURIOUS[] = { {106,80,350},{106,80,800},{90,90,350} };
static const Keyframe PROGMEM TRACK_SLEEP[]   = { {90,96,400},{90,106,600},{90,114,800} };
static const Keyframe PROGMEM TRACK_WAKE[]    = { {80,105,350},{90,76,500},{100,76,250},{90,90,350} };
static const Keyframe PROGMEM TRACK_BOUNCE[]  = { {90,102,110},{90,86,120},{90,102,110},{90,90,150} };
static const Keyframe PROGMEM TRACK_ANGRY[]   = { {90,114,80},{70,114,100},{110,114,100},{90,90,200} };

// -------- Thêm mới từ desk-emoji-v3 (chuyển động nét mượt với keyframe) --------
// 4 hướng nhìn cơ bản: trái/phải/lên/xuống (dùng 2 giai đoạn: tới -> về)
// Pan range của miu_eilik: 40..140 (X), 65..115 (Y)
static const Keyframe PROGMEM TRACK_LOOK_LEFT[]  = { {60, 90, 220}, {90, 90, 250} };   // 60 = 90 - 30
static const Keyframe PROGMEM TRACK_LOOK_RIGHT[] = { {120, 90, 220}, {90, 90, 250} };  // 120 = 90 + 30
static const Keyframe PROGMEM TRACK_LOOK_UP[]    = { {90, 70, 250}, {90, 90, 250} };   // 70 = 90 - 20
static const Keyframe PROGMEM TRACK_LOOK_DOWN[]  = { {90, 110, 250}, {90, 90, 250} };  // 110 = 90 + 20

// Table các gesture (PROGMEM)
struct GestureTrack {
  const Keyframe* data;  // pointer tới PROGMEM
  uint8_t len;
};

static const GestureTrack PROGMEM GESTURE_TABLE[] = {
  { TRACK_NOD,        5 },
  { TRACK_SHAKE,      5 },
  { TRACK_CURIOUS,    3 },
  { TRACK_SLEEP,      3 },
  { TRACK_WAKE,       4 },
  { TRACK_BOUNCE,     4 },
  { TRACK_ANGRY,      4 },
  // -------- Thêm mới --------
  { TRACK_LOOK_LEFT,  2 },
  { TRACK_LOOK_RIGHT, 2 },
  { TRACK_LOOK_UP,    2 },
  { TRACK_LOOK_DOWN,  2 },
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
      default:                return EMO_NEUTRAL;
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
        pendingMood = ANGRY; gesture.breathingEnabled = false; break;
      case EMO_SLEEPY:
        pendingMood = TIRED; gesture.play(GES_SLEEP_DROOP); break;
      case EMO_CURIOUS:
        pendingMood = DEFAULT; gesture.play(GES_TILT_CURIOUS); break;
      case EMO_CONFUSED:
        pendingMood = DEFAULT; pendingConfused = true;
        gesture.play(GES_SHAKE_NO); break;
      case EMO_WAKE_UP:
        pendingMood = DEFAULT; gesture.play(GES_WAKE_STRETCH); break;
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
// HELPER: TINH EPOCH TU y/m/d/h/m/s (LOCAL TIME) - TRÁNH PHỤ THUỘC TZ + mktime()
// =====================================================================================

// Tinh so giay tu 1970-01-01 00:00:00 den thoi diem (y,m,d,h,m,s) dang LOCAL
// Thuat toan: tinh so ngay tu epoch, nhan 86400 + so giay trong ngay
static time_t tmToEpoch(int y, int mo, int d, int h, int mi, int s) {
  if (mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
  // So nam nhuan tu 1970 den y-1
  int yFromEpoch = y - 1970;
  // So ngay tu 1970-01-01 den 1970-y-1  (co tinh nhuan)
  // Cong thuc: 365*y + so nam nhuan
  int leapDays = (yFromEpoch + 1) / 4 - (yFromEpoch + 1) / 100 + (yFromEpoch + 1) / 400;
  // Hien dang o 1970-01-01, can tinh den ngay dau tien cua y
  int daysUpToYear = yFromEpoch * 365 + leapDays;
  // So ngay tu dau nam y den dau thang mo
  static const int cumDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
  int daysUpToMonth = cumDays[mo - 1];
  // Cong them 1 ngay neu y la nam nhuan va mo > 2
  if (mo > 2 && ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0))) daysUpToMonth++;
  int totalDays = daysUpToYear + daysUpToMonth + (d - 1);
  return (time_t)totalDays * 86400L + (time_t)h * 3600L + (time_t)mi * 60L + (time_t)s;
}

// Tinh y/m/d/h/m/s tu epoch (epoch la LOCAL time)
static void epochToTm(time_t epoch, int& y, int& mo, int& d, int& h, int& mi, int& s) {
  // Tinh gio trong ngay
  int secOfDay = (int)(epoch % 86400L);
  if (secOfDay < 0) secOfDay += 86400L;  // phong TH am
  h = secOfDay / 3600;
  mi = (secOfDay % 3600) / 60;
  s = secOfDay % 60;

  // Tinh ngay
  long days = (long)(epoch / 86400L);
  if ((long)epoch < 0 && (long)epoch % 86400L != 0) days--;  // xu ly am chinh xac

  // Thuat toan "date from ordinal" tu "Howard Hinnant" - nhanh va chinh xac
  // Dich sang epoch days: 1970-01-01 = 0
  days += 719468;  // shift tu 0000-03-01
  long era = (days >= 0 ? days : days - 146096) / 146097;
  long doe = days - era * 146097;             // [0, 146096]
  long yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;  // [0, 399]
  y = (int)(yoe) + (int)(era) * 400;
  long doy = doe - (365*yoe + yoe/4 - yoe/100);  // [0, 365]
  long mp = (5*doy + 2) / 153;                  // [0, 11]
  d = (int)(doy - (153*mp + 2)/5 + 1);          // [1, 31]
  mo = (int)(mp < 10 ? mp + 3 : mp - 9);        // [1, 12]
  if (mo <= 2) y += 1;
}

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
  float tempC = 27.5f;

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
    // POSIX TZ: "ICT-7" nghia la Indochina Time (UTC+7, VN khong co DST)
    setenv("TZ", "ICT-7", 1);
    tzset();

    const char* ds = __DATE__;
    const char* ts = __TIME__;
    char mStr[4] = {0};
    int yr = 2026, mo = 10, md = 10, h = 6, mn = 0, sc = 0;
    if (sscanf(ds, "%3s %d %d", mStr, &md, &yr) == 3) {
      static const char PROGMEM months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
      char* p = strstr_P(months, mStr);
      if (p) mo = ((p - months) / 3) + 1;
    }
    sscanf(ts, "%d:%d:%d", &h, &mn, &sc);

    // Thoi gian compile tren PC la gio LOCAL Vietnam (UTC+7)
    // Quy doi ve UTC epoch: tru di 7 gio (25200s)
    time_t compile_local = tmToEpoch(yr, mo, md, h, mn, sc);
    time_t compile_utc   = compile_local - (7 * 3600);

    time_t cur = time(nullptr);
    // Neu RTC hien tai cu hon gio compile (vd: sau khi flash code moi hoac mat nguon)
    // thi bat buoc cap nhat theo compile_utc
    if (cur < compile_utc || cur < 1735689600) {
      struct timeval tv = { .tv_sec = compile_utc, .tv_usec = 0 };
      settimeofday(&tv, NULL);
      LOG_PRINTF(" [CLOCK] Khoi tao RTC tu gio compile (%s %s UTC+7): ", ds, ts);
    } else {
      LOG_PRINTF(" [CLOCK] Giu nguyen gio RTC dang chay: ");
    }

    updateClock();
    LOG_PRINTF("%02d:%02d:%02d - Ngay %02d/%02d/%04d (Thu %d)\n",
               clockHour, clockMin, clockSec, clockDay, clockMonth, clockYear, clockWday);
  }

  void setTime(int h, int m, int s) {
    // Nguoi dung truyen gio LOCAL (0-23, 0-59, 0-59)
    time_t nowUtc = time(nullptr);
    struct tm ti;
    localtime_r(&nowUtc, &ti);
    // Giu nguyen ngay thang nam LOCAL, thay the gio/phut/giay
    time_t localEpoch = tmToEpoch(ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
                                  constrain(h, 0, 23),
                                  constrain(m, 0, 59),
                                  constrain(s, 0, 59));
    time_t newUtc = localEpoch - (7 * 3600);
    struct timeval tv = { .tv_sec = newUtc, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    updateClock();
    LOG_PRINTF(" [CLOCK] Da cap nhat gio moi: %02d:%02d:%02d\n", clockHour, clockMin, clockSec);
  }

  void setDate(int d, int m, int y) {
    // Nguoi dung truyen ngay LOCAL
    time_t nowUtc = time(nullptr);
    struct tm ti;
    localtime_r(&nowUtc, &ti);
    int newY = constrain(y, 2020, 2099);
    int newM = constrain(m, 1, 12);
    int newD = constrain(d, 1, daysInMonth(newY, newM));
    time_t localEpoch = tmToEpoch(newY, newM, newD, ti.tm_hour, ti.tm_min, ti.tm_sec);
    time_t newUtc = localEpoch - (7 * 3600);
    struct timeval tv = { .tv_sec = newUtc, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    updateClock();
    LOG_PRINTF(" [CLOCK] Da cap nhat ngay moi: %02d/%02d/%04d\n", clockDay, clockMonth, clockYear);
  }

  void updateClock() {
    time_t nowUtc = time(nullptr);
    struct tm ti;
    localtime_r(&nowUtc, &ti);
    clockYear  = ti.tm_year + 1900;
    clockMonth = ti.tm_mon + 1;
    clockDay   = ti.tm_mday;
    clockHour  = ti.tm_hour;
    clockMin   = ti.tm_min;
    clockSec   = ti.tm_sec;
    clockWday  = ti.tm_wday; // 0=CN, 1=T2, 2=T3, ..., 6=T7
  }

  void notifyPresence() {
    lastPresence = millis();
    if (mode == DISP_CLOCK) {
      mode = DISP_EYES;
      roboEyes.setMood(DEFAULT);
    }
  }

  // ============ WEATHER & DAY/NIGHT ICONS (PIXEL PERFECT) ============
  void drawSun(int16_t cx, int16_t cy) {
    oled.fillCircle(cx, cy, 4, SSD1306_WHITE);
    oled.drawFastVLine(cx, cy - 7, 2, SSD1306_WHITE);
    oled.drawFastVLine(cx, cy + 6, 2, SSD1306_WHITE);
    oled.drawFastHLine(cx - 7, cy, 2, SSD1306_WHITE);
    oled.drawFastHLine(cx + 6, cy, 2, SSD1306_WHITE);
    oled.drawPixel(cx - 4, cy - 4, SSD1306_WHITE);
    oled.drawPixel(cx + 4, cy - 4, SSD1306_WHITE);
    oled.drawPixel(cx - 4, cy + 4, SSD1306_WHITE);
    oled.drawPixel(cx + 4, cy + 4, SSD1306_WHITE);
  }

  void drawMoon(int16_t cx, int16_t cy) {
    oled.fillCircle(cx, cy, 5, SSD1306_WHITE);
    oled.fillCircle(cx + 2, cy - 2, 4, SSD1306_BLACK);
  }

  void drawStars(int16_t x, int16_t y) {
    bool blink = (millis() / 500) & 1;
    oled.drawPixel(x, y, SSD1306_WHITE);
    if (blink) {
      oled.drawPixel(x - 1, y, SSD1306_WHITE);
      oled.drawPixel(x + 1, y, SSD1306_WHITE);
      oled.drawPixel(x, y - 1, SSD1306_WHITE);
      oled.drawPixel(x, y + 1, SSD1306_WHITE);
    }
    oled.drawPixel(x + 5, y + 5, blink ? SSD1306_WHITE : SSD1306_BLACK);
  }

  void drawThermometer(int16_t x, int16_t y) {
    oled.drawFastVLine(x + 1, y, 5, SSD1306_WHITE);
    oled.drawPixel(x, y + 1, SSD1306_WHITE);
    oled.drawPixel(x + 2, y + 1, SSD1306_WHITE);
    oled.fillCircle(x + 1, y + 6, 2, SSD1306_WHITE);
  }

  // ============ MAU 1: SMART CARD LAYOUT (CHUAN PIXEL 128x64) ============
  void drawClock() {
    oled.clearDisplay();
    // KHONG goi updateClock() o day (da goi trong DisplayManager::update() truoc do)
    // Tranh goi trung lap, tiet kiem CPU ~30us/frame

    bool isNight = (clockHour >= 18 || clockHour < 6);

    // 1. HEADER (Thứ + Ngày/Tháng/Năm + Biểu tượng Ngày/Đêm)
    static const char PROGMEM DAYS[] = "CN\0T2\0T3\0T4\0T5\0T6\0T7";
    char dayBuf[4];
    strncpy_P(dayBuf, &DAYS[clockWday * 3], 3);
    dayBuf[3] = '\0';

    oled.drawRoundRect(2, 1, 20, 10, 2, SSD1306_WHITE);
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(6, 2);
    oled.print(dayBuf);

    oled.setCursor(28, 2);
    oled.printf("%02d/%02d/%04d", clockDay, clockMonth, clockYear);

    if (!isNight) {
      drawSun(116, 6);
    } else {
      drawMoon(118, 5);
      drawStars(104, 3);
    }

    // Đường kẻ phân cách Header
    oled.drawFastHLine(0, 13, SCREEN_W, SSD1306_WHITE);

    // 2. PHẦN GIỮA: GIỜ SỐ TO SIZE 3 & GIÂY
    oled.setTextSize(3);
    oled.setCursor(12, 18);
    bool blink = (millis() / 500) & 1;
    oled.printf("%02d%c%02d", clockHour, blink ? ':' : ' ', clockMin);

    // Giây ở góc trên
    oled.setTextSize(1);
    oled.setCursor(105, 19);
    oled.printf(":%02d", clockSec);

    // Badge trạng thái nhỏ gọn
    oled.drawRoundRect(103, 31, 22, 11, 2, SSD1306_WHITE);
    oled.setCursor(106, 33);
    oled.print(isNight ? F("Zz") : F("OK"));

    // Đường kẻ phân cách Footer
    oled.drawFastHLine(0, 46, SCREEN_W, SSD1306_WHITE);

    // 3. FOOTER: NHIỆT KẾ & TRẠNG THÁI
    // Đọc cảm biến nhiệt ESP32-S3 (trừ đi nhiệt độ tỏa ra từ CPU)
    float rawT = temperatureRead();
    if (rawT >= 20.0f && rawT <= 85.0f) {
      float estimatedRoomT = rawT - 28.5f;
      estimatedRoomT = constrain(estimatedRoomT, 22.0f, 38.0f);
      tempC = tempC * 0.92f + estimatedRoomT * 0.08f;
    }

    drawThermometer(4, 50);
    oled.setTextSize(1);
    oled.setCursor(14, 52);
    oled.printf("%.1f%cC", tempC, (char)247);

    oled.setCursor(56, 52);
    if (ir.isActive) {
      oled.print(F("| VAT CAN"));
    } else {
      oled.print(isNight ? F("| BAN DEM") : F("| BAN NGAY"));
    }

    oled.display();
  }

  void drawEyes() {
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

    // Cap nhat gio noi tai 1 LAN MOI FRAME (30Hz) - du cho dong ho hien thi muot
    updateClock();

    if (mode == DISP_STREAM && (now - streamLastMs) > 2000) mode = DISP_CLOCK;

    bool music = audioR.isMusic();
    if (music && (mode == DISP_EYES || mode == DISP_CLOCK)) mode = DISP_VISUAL;
    else if (!music && mode == DISP_VISUAL) {
      mode = DISP_EYES;
      roboEyes.setWidth(36, 36); roboEyes.setHeight(36, 36);
    }

    if (!music && mode == DISP_EYES && (now - lastPresence) >= IR_PRESENCE_TIMEOUT_MS) {
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

static volatile bool s_actionRunning = false;

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

    if (gesture.playing || mode == AUTO_SLEEP || s_actionRunning) return;

    if (millis() - lastAction >= interval) {
      lastAction = millis();
      interval = random(4000, 8000);
      switch (mode) {
        case AUTO_CURIOUS:
          if (random(2)) async_speech_act();
          else gesture.play(GES_TILT_CURIOUS);
          break;
        case AUTO_PLAYFUL:
          if (random(2)) async_random_act();
          else gesture.play(GES_BOUNCE_HAPPY);
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

  char asciiBuf[128];
  uint8_t asciiLen = 0;

  void update() {
    while (Serial.available()) {
      uint8_t b = Serial.read();

      if (state == S_SYNC1 && b != USB_SYNC_1) {
        // Xu ly ASCII serial text (ACT=..., TIME=..., v.v.) qua USB CDC
        if (b == '\n') {
          asciiBuf[asciiLen] = '\0';
          processSerialLine(String(asciiBuf));
          asciiLen = 0;
        } else if (b != '\r') {
          if (asciiLen < sizeof(asciiBuf) - 1) {
            asciiBuf[asciiLen++] = (char)b;
          }
        }
        continue;
      }

      switch (state) {
        case S_SYNC1:
          if (b == USB_SYNC_1) state = S_SYNC2;
          break;
        case S_SYNC2:
          if (b == USB_SYNC_2) { state = S_TYPE; xorSum = 0; }
          else {
            state = S_SYNC1;
            if (b != USB_SYNC_1 && asciiLen < sizeof(asciiBuf) - 1) {
              asciiBuf[asciiLen++] = (char)b;
            }
          }
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
            emotion.setEmotion((Emotion)constrain(kv.value().as<int>(), 0, 6));
          }
          break;
        case CmdHash::GESTURE:
          if (kv.value().is<int>()) gesture.play((Gesture)constrain(kv.value().as<int>(), 0, 7));
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

    if (doc["actions"].is<JsonArray>()) {
      String combined = "";
      JsonArray arr = doc["actions"].as<JsonArray>();
      for (JsonVariant v : arr) {
        if (combined.length() > 0) combined += ",";
        combined += v.as<String>();
      }
      async_sequent_act(combined);
    } else if (doc["act"].is<const char*>()) {
      async_sequent_act(doc["act"].as<const char*>());
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

// =====================================================================================
// SECTION P2: DESK-EMOJI ACT & SERVO SYSTEM (Ported from desk-emoji-v3)
// =====================================================================================

// ---- 1. Head Motion Functions ----
void head_move(int x_offset, int y_offset, int servo_delay) {
  // Pan (X): (+) sang phai, (-) sang trai. Clamp an toan [PAN_MIN_DEG, PAN_MAX_DEG] (40..140)
  // Tilt (Y): (+) cui xuong, (-) ngua len. Clamp an toan [TILT_MIN_DEG, TILT_MAX_DEG] (65..115)
  float toPan  = constrain(head.pan.target  + (float)x_offset, PAN_MIN_DEG,  PAN_MAX_DEG);
  float toTilt = constrain(head.tilt.target + (float)y_offset, TILT_MIN_DEG, TILT_MAX_DEG);
  
  float spd = SERVO_MAX_SPEED_DPS;
  if (servo_delay > 0) {
    spd = constrain(1000.0f / (float)(servo_delay * 2), SERVO_MIN_SPEED_DPS, SERVO_MAX_SPEED_DPS);
  }
  
  head.setTarget(toPan, toTilt, spd);
  
  unsigned long t0 = millis();
  while (head.isMoving() && (millis() - t0 < 3000)) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void head_center(int servo_delay) {
  float spd = (servo_delay > 0) ? constrain(1000.0f / (float)(servo_delay * 2), SERVO_MIN_SPEED_DPS, SERVO_MAX_SPEED_DPS) : 120.0f;
  head.setTarget(PAN_CENTER_DEG, TILT_CENTER_DEG, spd);
  unsigned long t0 = millis();
  while (head.isMoving() && (millis() - t0 < 3000)) {
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void head_right(int offset) { head_move(offset, 0); }
void head_left(int offset)  { head_move(-offset, 0); }
void head_down(int offset)  { head_move(0, offset); }
void head_up(int offset)    { head_move(0, -offset); }

void head_nod(int servo_delay) {
  for (int i = 0; i < 2; i++) {
    head_move(0, 12, servo_delay);
    vTaskDelay(pdMS_TO_TICKS(40));
    head_move(0, -12, servo_delay);
    vTaskDelay(pdMS_TO_TICKS(40));
  }
}

void head_shake(int servo_delay) {
  head_move(-15, 0, servo_delay);
  vTaskDelay(pdMS_TO_TICKS(40));
  head_move(30, 0, servo_delay);
  vTaskDelay(pdMS_TO_TICKS(40));
  head_move(-30, 0, servo_delay);
  vTaskDelay(pdMS_TO_TICKS(40));
  head_move(30, 0, servo_delay);
  vTaskDelay(pdMS_TO_TICKS(40));
  head_move(-15, 0, servo_delay);
  vTaskDelay(pdMS_TO_TICKS(40));
}

void head_roll_left(int servo_delay) {
  head_center(servo_delay);
  head_down(10);
  head_move(-15, -10, servo_delay);
  head_move(15, -10, servo_delay);
  head_move(15, 10, servo_delay);
  head_move(-15, 10, servo_delay);
  head_center(servo_delay);
}

void head_roll_right(int servo_delay) {
  head_center(servo_delay);
  head_down(10);
  head_move(15, -10, servo_delay);
  head_move(-15, -10, servo_delay);
  head_move(-15, 10, servo_delay);
  head_move(15, 10, servo_delay);
  head_center(servo_delay);
}

// ---- 2. Eye Expression Functions ----
void eye_blink() {
  display.notifyPresence();
  roboEyes.blink();
}

void eye_happy() {
  display.notifyPresence();
  roboEyes.setMood(HAPPY);
  roboEyes.anim_laugh();
}

void eye_sad() {
  display.notifyPresence();
  roboEyes.setMood(TIRED);
}

void eye_anger() {
  display.notifyPresence();
  roboEyes.setMood(ANGRY);
}

void eye_surprise() {
  display.notifyPresence();
  roboEyes.setMood(DEFAULT);
  roboEyes.anim_confused();
}

void eye_right() {
  display.notifyPresence();
  roboEyes.setPosition(E);
}

void eye_left() {
  display.notifyPresence();
  roboEyes.setPosition(W);
}

void eye_center() {
  display.notifyPresence();
  roboEyes.setPosition(DEFAULT);
}

// ---- 3. Action Parser & Dispatcher ----
void executeCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd == "eye_blink") {
    eye_blink();
    vTaskDelay(pdMS_TO_TICKS(100));
  } else if (cmd == "eye_happy" || cmd == "happy") {
    eye_happy();
    vTaskDelay(pdMS_TO_TICKS(150));
  } else if (cmd == "eye_sad" || cmd == "sad") {
    eye_sad();
    vTaskDelay(pdMS_TO_TICKS(150));
  } else if (cmd == "eye_anger" || cmd == "eye_angry" || cmd == "angry") {
    eye_anger();
    vTaskDelay(pdMS_TO_TICKS(150));
  } else if (cmd == "eye_surprise" || cmd == "surprise") {
    eye_surprise();
    vTaskDelay(pdMS_TO_TICKS(150));
  } else if (cmd == "eye_right") {
    eye_right();
    vTaskDelay(pdMS_TO_TICKS(100));
  } else if (cmd == "eye_left") {
    eye_left();
    vTaskDelay(pdMS_TO_TICKS(100));
  } else if (cmd == "eye_center") {
    eye_center();
    vTaskDelay(pdMS_TO_TICKS(100));
  } else if (cmd == "head_left" || cmd == "left") {
    head_left(20);
    vTaskDelay(pdMS_TO_TICKS(100));
  } else if (cmd == "head_right" || cmd == "right") {
    head_right(20);
    vTaskDelay(pdMS_TO_TICKS(100));
  } else if (cmd == "head_up" || cmd == "up") {
    head_up(15);
    vTaskDelay(pdMS_TO_TICKS(100));
  } else if (cmd == "head_down" || cmd == "down") {
    head_down(15);
    vTaskDelay(pdMS_TO_TICKS(100));
  } else if (cmd == "head_center" || cmd == "center") {
    head_center();
    eye_center();
    vTaskDelay(pdMS_TO_TICKS(100));
  } else if (cmd == "head_nod" || cmd == "nod") {
    head_nod(3);
    vTaskDelay(pdMS_TO_TICKS(100));
  } else if (cmd == "head_shake" || cmd == "shake") {
    head_shake(3);
    vTaskDelay(pdMS_TO_TICKS(100));
  } else if (cmd == "head_roll_left" || cmd == "roll_left") {
    head_roll_left(10);
    vTaskDelay(pdMS_TO_TICKS(100));
  } else if (cmd == "head_roll_right" || cmd == "roll_right") {
    head_roll_right(10);
    vTaskDelay(pdMS_TO_TICKS(100));
  } else if (cmd == "speech") {
    speech_act();
  } else if (cmd == "random") {
    random_act();
  } else if (cmd == "delay") {
    vTaskDelay(pdMS_TO_TICKS(1000));
  } else if (cmd == "disp_clock") {
    display.mode = DISP_CLOCK;
  } else if (cmd == "disp_eyes") {
    display.mode = DISP_EYES;
    roboEyes.setMood(DEFAULT);
  } else if (cmd == "reboot" || cmd == "restart") {
    ESP.restart();
  } else {
    LOG_PRINTF("[ACT] Unknown command: %s\n", cmd.c_str());
    return;
  }
  LOG_PRINTF("[ACT] Done: %s\n", cmd.c_str());
}

void sequent_act(String actions) {
  if (actions.isEmpty()) return;
  int startIndex = 0;
  int commaIndex;

  while (startIndex < actions.length()) {
    commaIndex = actions.indexOf(',', startIndex);
    String cmd;
    if (commaIndex != -1) {
      cmd = actions.substring(startIndex, commaIndex);
      startIndex = commaIndex + 1;
    } else {
      cmd = actions.substring(startIndex);
      startIndex = actions.length();
    }
    cmd.trim();
    if (cmd.length() > 0) {
      executeCommand(cmd);
    }
  }
}

void random_act() {
  int random_num = random(100);
  int x = random(-20, 21);
  int y = random(-8, 12);

  if (random_num < 15) {
    eye_happy();
    vTaskDelay(pdMS_TO_TICKS(400));
  } else if (random_num > 75) {
    head_move(x, y, 10);
    if (x > 0) eye_right(); else eye_left();
    vTaskDelay(pdMS_TO_TICKS(350));
    head_center();
    eye_center();
  } else {
    eye_blink();
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

void speech_act() {
  int random_num = random(100);
  int x = random(-20, 21);
  int y = random(-8, 12);

  if (random_num > 70) {
    eye_happy();
    if (random_num > 90) {
      head_shake();
    } else {
      head_nod();
    }
    head_center();
    eye_blink();
  } else if (random_num > 40) {
    head_move(x, y, 10);
    if (x > 0) eye_right(); else eye_left();
    vTaskDelay(pdMS_TO_TICKS(250));
    head_center();
    eye_center();
  } else if (random_num > 20) {
    eye_happy();
    if (random_num > 10) {
      head_roll_left();
    } else {
      head_roll_right();
    }
    head_center();
    eye_blink();
  } else {
    eye_blink();
  }
}

// ---- 4. FreeRTOS Async Tasks ----
void async_sequent_act(String actions) {
  if (s_actionRunning) {
    LOG_PRINTLN(F("[ACT] Robot dang ban thuc hien hanh dong!"));
    return;
  }
  xTaskCreate(
    [](void* parameter) {
      s_actionRunning = true;
      String* actions = (String*)parameter;
      sequent_act(*actions);
      delete actions;
      s_actionRunning = false;
      vTaskDelete(NULL);
    },
    "SeqActTask", 4096, new String(actions), 2, NULL
  );
}

void async_random_act() {
  if (s_actionRunning) return;
  xTaskCreate(
    [](void* parameter) {
      s_actionRunning = true;
      randomSeed(esp_random());
      random_act();
      s_actionRunning = false;
      vTaskDelete(NULL);
    },
    "RandActTask", 4096, NULL, 2, NULL
  );
}

void async_speech_act() {
  if (s_actionRunning) return;
  xTaskCreate(
    [](void* parameter) {
      s_actionRunning = true;
      randomSeed(esp_random());
      speech_act();
      s_actionRunning = false;
      vTaskDelete(NULL);
    },
    "SpeechActTask", 4096, NULL, 2, NULL
  );
}

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
    IrEvent ev = ir.consume();
    if (ev != IR_NONE) {
      autonomy.notifyManual();
      display.notifyPresence();  // Move from DualIr -> Brain (fix forward decl)
      switch (ev) {
        case IR_TAP:    emotion.setEmotion(EMO_HAPPY); break;
        case IR_DOUBLE: {
          // FIX B6: modulo thay bitwise AND (an toàn nếu AutoMode count thay đổi)
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
// SECTION S: NTP TIME SYNC (WiFi + SNTP)
// =====================================================================================
// - Ket noi WiFi khi boot (neu co config) de dong bo gio tu NTP server
// - Tu dong retry moi 30 phut neu that bai
// - Co the bat/tat qua lenh Serial: NTP=ON, NTP=OFF, NTPSET=SSID,PASS
// - KHONG bat WiFi mac dinh (tiet kiem dien) - chi bat khi NTP=ON
// - Muc tieu: giu gio chinh xac ~vai giay, khong bi loi clock drift
// =====================================================================================

#include <WiFi.h>
#include <esp_sntp.h>

class NtpSync {
public:
  // Cai dat mac dinh (co the thay qua Preferences hoac lenh Serial NTPSET=)
  const char* ssid     = "YOUR_WIFI_SSID";
  const char* pass     = "YOUR_WIFI_PASS";
  const char* ntp1     = "pool.ntp.org";     // Server NTP chinh
  const char* ntp2     = "time.nist.gov";    // Server NTP phu
  const long  gmtOffset_sec = 7 * 3600;       // UTC+7 (Indochina)
  const int   daylightOffset_sec = 0;         // VN khong co DST

  bool enabled = false;     // bat/tat NTP qua lenh Serial
  bool synced = false;      // da sync thanh cong it nhat 1 lan
  unsigned long lastSyncMs = 0;     // ms cua lan sync thanh cong
  unsigned long lastAttemptMs = 0;  // ms cua lan thu connect
  int8_t wifiState = -1;    // -1: chua bat, 0: dang bat/tat, 1: connected
  uint8_t failCount = 0;

  // Khoi dong: doc config tu Preferences (neu co), ket noi neu enabled
  void begin() {
    prefs.begin(NVS_NAMESPACE, true);
    enabled = prefs.getBool("ntp_on", false);
    String s = prefs.getString("ntp_ssid", "");
    String p = prefs.getString("ntp_pass", "");
    if (s.length() > 0) ssid = strdup(s.c_str());
    if (p.length() > 0) pass = strdup(p.c_str());
    prefs.end();
    if (enabled) {
      LOG_PRINTF("[NTP] Khoi dong che do NTP (SSID: %s)\n", ssid);
      startConnect();
    } else {
      LOG_PRINTLN(F("[NTP] NTP tat mac dinh. Gui 'NTP=ON' de bat."));
    }
  }

  // Voi cac command tu Serial
  void setEnabled(bool en) {
    if (en == enabled) return;
    enabled = en;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putBool("ntp_on", en);
    prefs.end();
    if (en) startConnect();
    else stopWifi();
    LOG_PRINTF("[NTP] NTP %s\n", en ? "BAT" : "TAT");
  }

  void setCredentials(const char* newSsid, const char* newPass) {
    if (newSsid) ssid = strdup(newSsid);
    if (newPass) pass = strdup(newPass);
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("ntp_ssid", newSsid ? newSsid : "");
    prefs.putString("ntp_pass", newPass ? newPass : "");
    prefs.end();
    LOG_PRINTF("[NTP] Luu WiFi credentials: %s\n", ssid);
    if (enabled) {
      stopWifi();
      startConnect();
    }
  }

  // Vong lap (goi tu loop()): retry connect, retry sync
  void update() {
    if (!enabled) return;
    unsigned long now = millis();
    // Neu da sync roi, retry moi 30 phut
    if (synced && (now - lastSyncMs) < 1800000UL) return;
    // Neu chua den luot retry (tranh spam)
    if (now - lastAttemptMs < 15000UL) return;
    lastAttemptMs = now;

    // Buoc 1: ket noi WiFi neu chua
    if (WiFi.status() != WL_CONNECTED) {
      if (wifiState != 1) {
        startConnect();
        return;
      }
      // Dang connect, cho 15s
      if (now - lastAttemptMs < 15000UL) return;
    }

    // Buoc 2: SNTP sync (SNTP tu dong sync khi WiFi connected, kiem tra status)
    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
      // Lay thoi gian moi nhat tu he thong
      time_t now = time(nullptr);
      if (now > 1735689600) {  // > 2025-01-01 -> valid
        struct timeval tv = { .tv_sec = now, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        lastSyncMs = millis();
        synced = true;
        failCount = 0;
        display.updateClock();  // cap nhat ngay
        LOG_PRINTF("[NTP] Sync thanh cong! Epoch=%lu\n", (unsigned long)now);
        // NTP xong roi co the tat WiFi de tiet kiem dien
        // (nhung giu connection de sync lai sau 30 phut)
      }
    } else if (sntp_get_sync_status() == SNTP_SYNC_STATUS_IN_PROGRESS) {
      LOG_PRINTLN(F("[NTP] Sync dang tien hanh..."));
    }
  }

  // Lay trang thai (cho Serial/USB debug)
  void printStatus() {
    LOG_PRINTF("[NTP] enabled=%d synced=%d wifiState=%d failCount=%d\n",
               enabled, synced, wifiState, failCount);
    if (synced) {
      unsigned long agoMs = millis() - lastSyncMs;
      LOG_PRINTF("[NTP] Last sync: %lu phut truoc\n", agoMs / 60000);
    }
  }

private:
  Preferences prefs;

  void startConnect() {
    LOG_PRINTF("[NTP] Dang ket noi WiFi: %s ...\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    wifiState = 0;  // dang bat
    // Cho WiFi connected (khoa 5s de khong lam loop() bi treo)
    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 5000) {
      delay(100);
    }
    if (WiFi.status() == WL_CONNECTED) {
      wifiState = 1;
      LOG_PRINTF("[NTP] WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
      // Cau hinh SNTP
      sntp_setoperatingmode(SNTP_OPMODE_POLL);
      sntp_setservername(0, (char*)ntp1);
      sntp_setservername(1, (char*)ntp2);
      sntp_set_time_sync_notification_cb(nullptr);
      sntp_init();
      LOG_PRINTLN(F("[NTP] SNTP started, doi sync..."));
    } else {
      wifiState = -1;
      failCount++;
      LOG_PRINTF("[NTP] WiFi connect that bai (lan %d)\n", failCount);
      if (failCount >= 3) {
        LOG_PRINTLN(F("[NTP] 3 lan fail -> tam dung 5 phut"));
        failCount = 0;
        lastAttemptMs = millis() + 300000UL;  // delay 5 phut
      }
    }
  }

  void stopWifi() {
    sntp_stop();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiState = -1;
    synced = false;
    LOG_PRINTLN(F("[NTP] WiFi da tat"));
  }
} ntp;

// =====================================================================================
// SECTION R: SETUP & LOOP
// =====================================================================================

void setup() {
  Serial0.begin(115200);
  Serial.begin(115200);
  delay(300);

  LOG_PRINTLN(F("\n====================================================="));
  LOG_PRINTLN(F(" MIU EILIK ESP32-S3 - OPTIMIZED v6.12 (OLED Fix)   "));
  LOG_PRINTLN(F(" PROGMEM | FNV-1a | Streaming I2S | 2-core RTOS     "));
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
  display.initClock();  // Khoi tao gia tri clock ban dau tu __DATE__/__TIME__
  ntp.begin();          // Khoi tao NTP (chi connect WiFi neu ntp_on=true trong NVS)

  xTaskCreatePinnedToCore(taskAudio,   "Audio",   4096, NULL, 5, NULL, 0);
  xTaskCreatePinnedToCore(taskUsb,     "USB",     4096, NULL, 4, NULL, 0);
  xTaskCreatePinnedToCore(taskServo,   "Servo",   4096, NULL, 5, NULL, 1);
  xTaskCreatePinnedToCore(taskDisplay, "Display", 6144, NULL, 4, NULL, 1);
  xTaskCreatePinnedToCore(taskBrain,   "Brain",   4096, NULL, 3, NULL, 1);

  LOG_PRINTLN(F("[OK] 5 tasks started on 2 cores"));
}

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
  } else if (line.startsWith("NTP=ON") || line.startsWith("NTP=1")) {
    ntp.setEnabled(true);
  } else if (line.startsWith("NTP=OFF") || line.startsWith("NTP=0")) {
    ntp.setEnabled(false);
  } else if (line.startsWith("NTP?")) {
    ntp.printStatus();
  } else if (line.startsWith("NTPSET=")) {
    // NTPSET=MySSID,MyPassword
    String creds = line.substring(7);
    int commaIdx = creds.indexOf(',');
    if (commaIdx > 0) {
      String ssid = creds.substring(0, commaIdx);
      String pass = creds.substring(commaIdx + 1);
      ntp.setCredentials(ssid.c_str(), pass.c_str());
    } else {
      LOG_PRINTLN(F("[NTP] Sai format. Su dung: NTPSET=SSID,PASS"));
    }
  } else if (line.startsWith("ACT=") || line.startsWith("ACT ")) {
    String actStr = line.substring(4);
    actStr.trim();
    LOG_PRINTF("[ACT] Nhan lenh hanh dong: %s\n", actStr.c_str());
    async_sequent_act(actStr);
  } else if (line.startsWith("{") && line.indexOf("\"actions\"") >= 0) {
    // Ho tro format JSON cua desk-emoji: {"actions":["head_nod","eye_happy"]}
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, line);
    if (!err && doc["actions"].is<JsonArray>()) {
      String combined = "";
      JsonArray arr = doc["actions"].as<JsonArray>();
      for (JsonVariant v : arr) {
        if (combined.length() > 0) combined += ",";
        combined += v.as<String>();
      }
      LOG_PRINTF("[ACT JSON] %s\n", combined.c_str());
      async_sequent_act(combined);
    }
  } else if (line.startsWith("adjust_x ") || line.startsWith("adjust_x=")) {
    int val = line.substring(9).toInt();
    head.pan.offset += val;
    head.saveCalib((int8_t)head.pan.offset, (int8_t)head.tilt.offset);
    LOG_PRINTF("[CALIB] Pan offset=%d, Tilt offset=%d\n", (int)head.pan.offset, (int)head.tilt.offset);
  } else if (line.startsWith("adjust_y ") || line.startsWith("adjust_y=")) {
    int val = line.substring(9).toInt();
    head.tilt.offset += val;
    head.saveCalib((int8_t)head.pan.offset, (int8_t)head.tilt.offset);
    LOG_PRINTF("[CALIB] Pan offset=%d, Tilt offset=%d\n", (int)head.pan.offset, (int)head.tilt.offset);
  } else if (line.startsWith("CALIB=")) {
    int p = 0, t = 0;
    if (sscanf(line.c_str() + 6, "%d,%d", &p, &t) == 2) {
      head.saveCalib((int8_t)p, (int8_t)t);
      LOG_PRINTF("[CALIB] Set Pan offset=%d, Tilt offset=%d\n", p, t);
    }
  } else if (line == "ACT?") {
    LOG_PRINTLN(F("[ACT] Danh sach lenh san sang:"));
    LOG_PRINTLN(F("  Motion: head_nod, head_shake, head_roll_left, head_roll_right"));
    LOG_PRINTLN(F("          head_left, head_right, head_up, head_down, head_center"));
    LOG_PRINTLN(F("  Eyes:   eye_happy, eye_sad, eye_anger, eye_surprise, eye_blink"));
    LOG_PRINTLN(F("          eye_left, eye_right, eye_center"));
    LOG_PRINTLN(F("  Macro:  speech, random, delay, disp_clock, disp_eyes, reboot"));
    LOG_PRINTLN(F("  Chuoi:  ACT=head_nod,eye_happy,delay,head_roll_left"));
  } else if (line.startsWith("head_") || line.startsWith("eye_") ||
             line == "nod" || line == "shake" || line == "roll_left" ||
             line == "roll_right" || line == "speech" || line == "random" ||
             line == "happy" || line == "sad" || line == "center") {
    LOG_PRINTF("[ACT DIRECT] %s\n", line.c_str());
    async_sequent_act(line);
  }
}

void checkSerialCommands() {
  Stream* ports[] = { &Serial, &Serial0 };
  for (Stream* p : ports) {
    while (p->available()) {
      String line = p->readStringUntil('\n');
      processSerialLine(line);
    }
  }
}


unsigned long lastTelem = 0;
unsigned long lastNtpUpdate = 0;

void loop() {
  checkSerialCommands();
  unsigned long now = millis();
  // NTP update moi 15s (co check thong minh ben trong)
  if (now - lastNtpUpdate >= 15000UL) {
    lastNtpUpdate = now;
    ntp.update();
  }
  if (now - lastTelem >= 10000) {
    lastTelem = now;
    LOG_PRINTF("[TELEM] up=%lus fps=%.0f heap=%u psram=%u und=%u pan=%.1f tilt=%.1f mode=%s\n",
      now / 1000, display.fps, ESP.getFreeHeap(), ESP.getFreePsram(),
      gUnderrunCount, head.pan.current, head.tilt.current, display.modeStr());
  }
  delay(50);
}
