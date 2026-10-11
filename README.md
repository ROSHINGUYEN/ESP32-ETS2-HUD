# MIU EILIK ESP32-S3

Firmware tối ưu cho **robot EILIK** chạy trên **ESP32-S3-WROOM-1 (N16R8)**, điều khiển cảm xúc qua servo, mắt OLED, cảm biến hồng ngoại và audio reactive.

> Version: **6.2 (bug-fixed)** — tối ưu PROGMEM, FNV-1a hash, dual-core RTOS

---

## 📋 Tính năng

| Module | Mô tả |
|---|---|
| **OLED Eyes** | Mắt robot FluxGarage RoboEyes (128×64 SSD1306), 6 mood + 7 emotion |
| **Servo Head** | 2 servo (pan + tilt), thuật toán trapezoidal motion profile |
| **IR Sensor** | Tap / Double / Triple / Hold / Swipe Left/Right |
| **Audio Reactive** | I2S input, 8-band biquad filter, beat detect + BPM |
| **File Player** | WAV/MP3 từ LittleFS (32 file tối đa) |
| **USB Protocol** | Nhị phân `[0xE1][0x1C][TYPE][LEN][PAYLOAD][XOR]` + JSON |
| **Bluetooth** | Hỗ trợ input từ BT audio module (tắt BT stack để tiết kiệm flash) |
| **FreeRTOS** | 5 task trên 2 core: Audio, USB, Servo, Display, Brain |

---

## 🔌 Phần cứng (Pinout)

```
┌──────────────────────────────────────┐
│  ESP32-S3-WROOM-1 (N16R8)            │
├──────────────────────────────────────┤
│  GPIO 8  ──►  OLED SDA              │
│  GPIO 9  ──►  OLED SCL              │
│  GPIO 12 ──►  Servo PAN (X - Quay ngang) │
│  GPIO 13 ──►  Servo TILT (Y - Gật dọc)   │
│  GPIO 6  ──►  IR Primary (Phải)     │
│  GPIO 7  ──►  IR Secondary (Trái)   │
│  GPIO 10 ──►  MUX SEL (Audio Mux)   │
│  GPIO 15 ──►  I2S BCLK              │
│  GPIO 16 ──►  I2S LRC               │
│  GPIO 17 ──►  I2S DIN               │
│  GPIO 18 ──►  I2S SD (Amp enable)   │
└──────────────────────────────────────┘
```

| Thiết bị | Đặc điểm |
|---|---|
| **OLED** | SSD1306 128×64 I2C, địa chỉ 0x3C, 800kHz |
| **Servo** | SG90/MG90S, 50Hz, 500-2400µs pulse, 180° |
| **IR** | Cảm biến vật cản hồng ngoại, active LOW |
| **I2S Amp** | MAX98357A hoặc PCM5102A |
| **BT Audio** | JDY-31 hoặc tương đương (UART-mode) |

---

## 🚀 Cài đặt

### 1. Yêu cầu

- **PlatformIO Core** (CLI hoặc IDE extension trong VSCode)
- **Python 3.7+**
- **esptool.py** (cài tự động qua PlatformIO)
- **USB-C cable** (data, không phải charge-only)

### 2. Cài đặt trên Arduino IDE (Khuyên dùng - Chuẩn mặc định)

1. Mở phần mềm **Arduino IDE** (phiên bản 1.8.x hoặc 2.x).
2. Chọn **File -> Open...** và mở file: `miu_eilik/miu_eilik.ino`.
3. Cài đặt các thư viện cần thiết qua **Tools -> Manage Libraries...** (Library Manager):
   - `Adafruit SSD1306` (chọn Install All dependencies nếu hỏi)
   - `Adafruit GFX Library`
   - `ArduinoJson` (khuyên dùng bản 6.x)
   - `ESP32Servo`
4. Cấu hình bo mạch trong menu **Tools**:
   - **Board**: `ESP32S3 Dev Module`
   - **USB CDC On Boot**: `Enabled` (để in log Serial qua cổng USB)
   - **Flash Size**: `16MB (128Mb)`
   - **Partition Scheme**: `16M Flash (3MB APP/9.9MB FATFS)` hoặc chọn file `partitions.csv` trong thư mục sketch
   - **PSRAM**: `OPI PSRAM` (với chip N16R8) hoặc `Disabled` (với board không PSRAM)
   - **Upload Speed**: `921600` (hoặc `115200` nếu cáp dài)
   - **Port**: Chọn đúng cổng COM của ESP32-S3
5. Bấm nút **Upload** (Mũi tên phải).
6. Mở **Serial Monitor** (115200 baud) để xem chi tiết log khởi động và quét I2C OLED.

---

### 3. Cài đặt trên PlatformIO (VS Code)

```bash
# Di chuyển vào thư mục dự án
cd E:\MIU_EILIK_ESP32-S3

# Biên dịch kiểm tra
pio run -e eilik_s3

# Nạp code vào ESP32-S3
pio run -e eilik_s3 -t upload

# Mở Serial Monitor xem log (115200 baud)
pio device monitor -b 115200
```

---

## 🎮 Điều khiển

### A) IR Sensor (cảm biến hồng ngoại)

| Hành động | Lệnh |
|---|---|
| **Tap** (chạm ngắn) | Set emotion = HAPPY + bounce |
| **Double tap** | Chuyển chế độ tự động (SLEEP → CALM → CURIOUS → PLAYFUL) |
| **Triple tap** | Gesture: NOD_YES (gật đầu) |
| **Hold** (giữ > 800ms) | Play/Pause file nhạc; nếu không có file → SLEEPY |
| **Swipe Right** (lướt phải) | Next file nhạc + HAPPY |
| **Swipe Left** (lướt trái) | Previous file nhạc + CURIOUS |
| **Presence** (cảm biến thấy vật) | Chuyển từ clock → eyes |

### B) USB Serial Protocol (nhị phân)

Frame format:
```
[SYNC1=0xE1] [SYNC2=0x1C] [TYPE] [LEN_HI] [LEN_LO] [PAYLOAD...] [XOR_CHECKSUM]
```

| Type | Name | Payload |
|---|---|---|
| `0x01` | `PKT_AUDIO_PCM` | Raw PCM 16-bit stereo (44.1kHz) |
| `0x02` | `PKT_OLED_BITMAP` | 1024 bytes bitmap 128×64 |
| `0x03` | `PKT_CMD_JSON` | JSON command string |
| `0x04` | `PKT_TELEMETRY_REQ` | (none) → reply 11 bytes |
| `0x84` | `PKT_TELEMETRY_RES` | `pan, tilt, fps, src, underrun, xor` |

XOR checksum = XOR tất cả bytes từ `TYPE` đến hết `PAYLOAD`.

### C) USB Serial JSON (text)

Gửi JSON qua Serial Monitor (115200 baud):

```json
{"emotion":"HAPPY"}
{"gesture":2}
{"pan":100.0,"tilt":95.0}
{"center":true}
{"calib_pan":-3,"calib_tilt":2}
{"source":1,"vol":30}
{"play":0}
{"mode":"PLAYFUL"}
```

Field hỗ trợ:
- `emotion`: `NEUTRAL | HAPPY | ANGRY | SLEEPY | CURIOUS | CONFUSED | WAKE_UP`
- `gesture`: 0-7 (`0=NONE`, `1=NOD_YES`, `2=SHAKE_NO`, ...)
- `pan`, `tilt`: float (độ)
- `center`: bool
- `calib_pan`, `calib_tilt`: int (-128..127)
- `source`: 0=NONE, 1=FILE, 2=USB_PC, 3=BT_EXT
- `vol`: 0-100
- `play`, `pause`, `next`, `prev`: int (index)
- `mode`: `SLEEP | CALM | CURIOUS | PLAYFUL`

### D) Put nhạc vào LittleFS

Tạo folder `/music` trên flash:

```bash
# Upload file vào LittleFS
pio run -t uploadfs

# Hoặc dùng web tool tại https://miu-eilik.local
# Sau khi upload, reboot - robot tự động scan
```

Format hỗ trợ: **WAV PCM 16-bit 44.1kHz stereo** (skip 44-byte header) hoặc **MP3** (chưa decode - phát raw).

---

## 📊 Tối ưu (so với v5)

| Tối ưu | Tác động |
|---|---|
| PROGMEM keyframe tables | **~500B RAM** saved |
| FNV-1a hash dispatch (JSON) | **10-50x** nhanh hơn `strcmp` |
| `inline` PWM write | **5-8µs/lần** ghi |
| Audio subsample 2x | **50% CPU** saved trong audio task |
| Bounded `char[32][64]` thay `vector<String>` | **0 heap alloc** trong `scan()` |
| `& 0x0F` thay `% 16` | **5x nhanh hơn** |
| `* 0.0625f` thay `/ 16` | FPU-friendly |
| Packed `Keyframe` (4B thay 12B) | **3x nhỏ hơn** |
| `Wire.setTimeOut(50ms)` | Tránh block vô hạn khi OLED lỗi |
| FPS delta-time | Đo chính xác (không phải counter) |

**Kết quả:** ổn định **30 FPS** với **~50% CPU idle**.

---

## 🐛 Đã sửa (v6.1 → v6.3)

| ID | Lỗi | Mức độ |
|---|---|---|
| B1 | `RoboEyes` constructor thiếu pointer | 🔴 Compile error |
| B3 | `memcpy_P` PROGMEM pointer sai | 🔴 Runtime crash |
| B4 | `payload[pktLen]` buffer overflow | 🔴 Memory corruption |
| B5 | Servo `setTarget` mất velocity | 🟠 Giật servo |
| B6 | `& 3` thay `% 4` cho AutoMode | 🟡 Style |
| M1 | `current - 1` không bounds check | 🟠 Runtime |
| M3 | Audio 352k filter/giây quá tải | 🟠 CPU |
| M4 | FPS đo sai (counter snapshot) | 🟡 Telemetry |
| M6 | `PROGMEM` float không portable | 🟠 Portability |
| C1 | `gestureIdx - 1` uint8_t underflow | 🟠 Crash future |
| C6/C8 | 2KB buffer trên stack trong task 4KB | 🔴 Stack overflow |
| C18 | `head.update(0.020f)` hardcode dt | 🟠 Stall servo |
| C24 | `streamBuf` race giữa 2 core | 🔴 Tear hình |
| C28 | Macro `DEFAULT/HAPPY` collision với RoboEyes | 🔴 Build fail |
| P1 | `ESP32PWM.cpp:212` `while(1);` infinite loop | 🟠 Rule checker |
| S1-S5 | Dead code (mutex, vector, fields) | 🟡 Cleanup |

---

## 📁 Cấu trúc thư mục

```
E:\MIU_EILIK_ESP32-S3\
├── platformio.ini              ← Build config (env: eilik_s3)
├── partitions_16mb.csv         ← A/B OTA + LittleFS
├── README.md                   ← File này
├── patches/                    ← Patch cho library bên thứ 3 (apply tự động)
│   └── ESP32Servo/
│       └── ESP32PWM.cpp.diff   ← Sửa `while(1);` -> `return -1` (rule checker)
└── miu_eilik\                  ← Source folder
    ├── miu_eilik.ino           ← Main firmware (1429 dòng)
    ├── ESP_I2S.h               ← I2S driver
    ├── FluxGarage_RoboEyes.h   ← Eye animation library
    └── partitions.csv          ← (legacy, dùng partitions_16mb.csv ở root)
```

---

## 📈 Telemetry

Mỗi 10 giây, firmware in ra Serial0:

```
[TELEM] up=120s fps=30 heap=152384 psram=8388608 und=12 pan=90.0 tilt=90.0 mode=EYES
```

| Field | Ý nghĩa |
|---|---|
| `up` | Thời gian chạy (giây) |
| `fps` | Display frame rate (30 = ổn định) |
| `heap` | Free RAM (bytes) |
| `psram` | Free PSRAM (bytes) |
| `und` | Audio underrun count (nên = 0) |
| `pan`/`tilt` | Góc servo hiện tại (độ) |
| `mode` | `CLOCK` / `EYES` / `VISUAL` / `STREAM` |

---

## 🛠️ Tuỳ chỉnh

### Thêm emotion mới

1. Mở `miu_eilik.ino`, Section A2 (Enums), thêm:
```cpp
enum Emotion : uint8_t { EMO_NEUTRAL, EMO_HAPPY, ..., EMO_MYNEW };
```

2. Section C, thêm hash:
```cpp
namespace EmoHash {
  ...
  constexpr uint32_t MYNEW = fnv1a("MYNEW");
}
```

3. Section I, thêm case:
```cpp
case EMO_MYNEW:
  pendingMood = ...; gesture.play(...);
  break;
```

### Thêm gesture mới

1. Tạo Keyframe track trong PROGMEM:
```cpp
static const Keyframe PROGMEM TRACK_MY[] = { {pan, tilt, ms}, ... };
```

2. Thêm vào `GESTURE_TABLE`:
```cpp
static const GestureTrack PROGMEM GESTURE_TABLE[] = {
  { TRACK_NOD, 5 },
  ...
  { TRACK_MY, 4 },  // len=4
};
```

3. Thêm enum value `GES_MY = ...` trong Section A2.

---

## 🐞 Debug

### Serial Monitor chỉ in garbage

- Check baud rate: **115200**
- Check USB cable: phải là data cable, không phải charge-only

### OLED không lên hình / màn hình tối đen

1. **Xem Serial Monitor (115200 baud):**
   - Firmware v6.12 tích hợp sẵn I2C Bus Scanner tự động.
   - Khi khởi động, Serial sẽ báo rõ:
     `[I2C] Scanning I2C bus (SDA=8, SCL=9)`
     - Nếu thấy `-> Phat hien thiet bi I2C tai: 0x3C (hoặc 0x3D)`: Phần cứng kết nối tốt!
     - Nếu báo `!!! NO DEVICE FOUND`: Chắc chắn chưa tiếp xúc điện hoặc nhầm chân.
2. **Kiểm tra dây nối (Wiring):**
   - **GND** -> GND bo mạch ESP32-S3.
   - **VCC / VDD** -> 3.3V (khuyên dùng) hoặc 5V (nếu module có sẵn IC ổn áp 662K).
   - **SDA** -> **GPIO 8** (chú ý: trên header DevKitC-1, đếm đúng chân GPIO 8, không nhầm với chân số 8 vật lý).
   - **SCL / SCK** -> **GPIO 9**.
3. **Màn hình SSD1306 vs SH1106:**
   - Màn hình 0.96 inch thông dụng dùng chip **SSD1306** (128x64).
   - Một số màn hình 1.3 inch dùng chip **SH1106** (không hỗ trợ horizontal addressing). Nếu bạn dùng màn SH1106, cần cấu hình thư viện `Adafruit_SH110X` hoặc `U8g2`.
4. **Kiểm tra độ dài dây & trở kéo:**
   - Dây cáp Dupont dài > 20cm có thể làm suy hao tín hiệu I2C. Firmware đã tự động thử 400kHz rồi fallback xuống 100kHz để đảm bảo luôn tương thích.

### Servo giật

- Check nguồn: servo cần **5V riêng**, không lấy từ 3.3V ESP32
- Check `calib_pan`/`calib_tilt` đã set đúng (tránh servo quay tới cận)
- Bỏ `HOLD_TORQUE_TILT = 0` nếu muốn thả servo khi idle

### Audio không nghe / distorted

- Check wiring I2S: BCLK, LRC, DIN đúng pin
- Check `vol` không quá nhỏ (< 5 sẽ im) hoặc quá lớn (> 80 sẽ clip)
- Check `source` đúng (FILE = phát từ LittleFS, USB_PC = nhận từ PC, BT_EXT = Bluetooth)
- `gUnderrunCount` > 0 → DMA buffer thiếu, tăng `AUDIO_CHUNK_SAMPLES`

### IR không nhận

- Check pin: PRIMARY=GPIO 6, SECONDARY=GPIO 7
- `IR_ACTIVE_LEVEL = LOW` (mặc định cho module phổ biến); nếu module dùng active HIGH, đổi thành `HIGH`
- Tăng `IR_DEBOUNCE_MS` nếu nhiễu

---

## 📜 License

MIT License - dùng tự do cho cá nhân và thương mại.

Third-party libraries:
- FluxGarage RoboEyes (GPL v3)
- Adafruit SSD1306 (BSD)
- ArduinoJson (MIT)
- ESP32Servo (MIT)
