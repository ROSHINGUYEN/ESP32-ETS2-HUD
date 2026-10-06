# MIU EILIK — ESP32-S3 Desktop Companion Robot v4

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-red.svg)](https://www.espressif.com/)
[![Framework: Arduino ESP32 v3](https://img.shields.io/badge/Framework-Arduino_ESP32_v3-blue.svg)](https://github.com/espressif/arduino-esp32)
[![RTOS: FreeRTOS Dual-Core](https://img.shields.io/badge/RTOS-FreeRTOS_Dual--Core-green.svg)](https://freertos.org/)
[![UI: SVG + Glassmorphism Web](https://img.shields.io/badge/UI-SVG%2BGlass-cyan.svg)]()

Phiên bản **v4.0.0-clean** — Tối ưu hóa toàn diện, loại bỏ toàn bộ code audio, robot vẽ hoàn toàn bằng HTML+CSS+SVG, joystick điều khiển trực tiếp 2 DC Motor qua H-Bridge.

---

## 1. Cấu trúc dự án

```
MIU_EILIK_ESP32-S3/
├── miu_eilik/                     # Firmware robot full chức năng chuẩn Arduino IDE
│   ├── miu_eilik.ino             # Sketch chính (18 Sections kỹ thuật hoàn chỉnh)
│   ├── FluxGarage_RoboEyes.h     # Thư viện mắt OLED biểu cảm RoboEyes
│   ├── ESP_I2S.h                 # Driver âm thanh I2S MAX98357A (ESP-IDF v4/v5)
│   └── partitions.csv            # Bảng phân vùng Flash 16MB
├── web/                          # Bộ công cụ Web Flasher & Server trực quan
│   ├── index.html                # Giao diện Web Flasher + Robot OLED giả lập + Serial Console
│   ├── server.py                 # Local server Python
│   ├── start_web.bat             # File khởi động nhanh web server
│   └── assets/                   # Ảnh mẫu tham chiếu
├── pc_tools/                     # Công cụ hỗ trợ trên máy tính
│   └── miu_web_flasher.html
├── start_web.bat                 # Shortcut khởi động web server tại localhost:8080
├── README.md                     # Tài liệu hướng dẫn dự án
└── roadmap.md                    # Lộ trình phát triển
```

---

## 2. Phần cứng

| Thành phần        | Chân GPIO     | Ghi chú                          |
|-------------------|---------------|----------------------------------|
| OLED SSD1306 SDA  | 8             | I2C 800 kHz                      |
| OLED SSD1306 SCL  | 9             |                                  |
| OLED địa chỉ      | 0x3C          |                                  |
| Motor PAN IN1     | 4             | TB6612FNG AI1 (cổ trái/phải)     |
| Motor PAN IN2     | 5             | TB6612FNG AI2                    |
| Motor PAN PWM     | 6             | TB6612FNG PWMA, 25 kHz, 8-bit    |
| Motor TILT IN1    | 15            | TB6612FNG BI1 (đầu lên/xuống)   |
| Motor TILT IN2    | 16            | TB6612FNG BI2                    |
| Motor TILT PWM    | 17            | TB6612FNG PWMB                   |

**Cấu hình motor:**
- PWM 25 kHz, 8-bit (0–255)
- Deadzone 24 (ngưỡng tối thiểu để SG90 DC quay)
- Tốc độ mặc định 140/255
- Auto-stop sau 600 ms không nhận lệnh

**Góc giới hạn:**
- PAN: 40° – 140° (center 90°)
- TILT: 70° – 115° (center 90°)

---

## 3. Tính năng chính

### 3.1. Điều khiển Joystick trực tiếp (Web)
- **2 joystick analog** độc lập:
  - Joystick trái → xoay **cổ** (PAN) trái/phải
  - Joystick phải → gật **đầu** (TILT) lên/xuống
- Tần số gửi lệnh 30 Hz, có dead-zone 8% chống nhiễu
- Nút **STOP** dừng khẩn cấp, **CENTER** đưa về giữa

### 3.2. AI LITE — Tự hành phản ứng (không cần model)
State-machine đơn giản, tiêu tốn 0 byte mô hình:
- **Mood (0–1):** tăng khi user tương tác, giảm từ từ khi bỏ rơi
- **Personality:** 3 cá tính
  - 🫣 **Shy** — ít chuyển động, tò mò ngắn
  - 😄 **Playful** — gesture nhiều, năng động
  - 😎 **Chill** — thở đều, chậm rãi
- **Reaction:** khi user thả joystick, robot tự "nghiêng đầu tò mò" sau 1.2s
- **Trọng số hành vi** thay đổi theo mood

### 3.3. 6 Cử chỉ mẫu
- Gật đầu (Nod Yes) / Lắc đầu (Shake No)
- Nghiêng đầu (Curious) / Buồn ngủ (Sleep)
- Vươn vai (Wake) / Nhún vui (Bounce Happy)

### 3.4. 4 Chế độ tự hành
- 💤 Sleep / 🌤 Calm / 🤔 Curious / 🎉 Playful / 🧠 **AI LITE**

### 3.5. Mắt OLED (RoboEyes)
- Tự chớp, tự nhìn lang thang (idle mode)
- 30 FPS trên Core 1

### 3.6. OTA qua Web
- **URL OTA:** nhập link `.bin` → ESP tự tải về và flash
- **Drag-drop:** kéo thả file `.bin` trực tiếp vào web
- Tiến trình real-time, tự reboot sau khi xong

---

## 4. Kiến trúc phần mềm

### 4.1. Phân bổ FreeRTOS 2 core

| Core 0 (WiFi/Network)        | Core 1 (Realtime/Motion)        |
|------------------------------|----------------------------------|
| WebServer + DNS              | OLED display 30 FPS              |
| OTA HTTP download task       | Brain (AI + gesture + autonomy)  |
|                              | Motor kinematics 50 Hz           |

### 4.2. Web API (JSON, GET)

| Endpoint                  | Mô tả                                     |
|---------------------------|-------------------------------------------|
| `GET /`                   | Trang index.html (LittleFS)                |
| `GET /style.css`          | CSS (LittleFS)                             |
| `GET /app.js`             | JS (LittleFS)                              |
| `GET /api/joystick?axis=0&v=50` | Joystick analog: axis 0=PAN, 1=TILT; v=-100..100 |
| `GET /api/stop`           | Dừng khẩn cấp                            |
| `GET /api/center`         | Đưa cổ + đầu về giữa                      |
| `GET /api/nudge?dpan=&dtilt=` | Nudge tương đối                          |
| `GET /api/emotion?id=`    | Đặt cảm xúc                               |
| `GET /api/gesture?id=`    | Chạy cử chỉ                               |
| `GET /api/mode?id=`       | Đổi chế độ tự hành                        |
| `GET /api/ai?act=shy\|playful\|chill` | Đổi cá tính AI                 |
| `GET /api/state`          | Trạng thái realtime (góc, fps, mood, AI)  |
| `GET /api/info`           | Thông tin firmware/IP/heap                 |
| `GET /api/ota/start?url=` | Bắt đầu tải firmware từ URL                |
| `GET /api/ota/progress`   | Tiến trình OTA (polling)                   |
| `POST /api/ota/upload`    | Upload file .bin (kéo-thả)                |

### 4.3. Lưu trữ
- **LittleFS** ~9.7 MB: chứa `index.html`, `style.css`, `app.js` (web UI)
- **Preferences** (NVS): lưu calibration offset PAN/TILT

---

## 5. Cài đặt & Build

### Yêu cầu
- [PlatformIO Core](https://platformio.org/) (CLI) hoặc PlatformIO IDE (VSCode)
- ESP32-S3 DevKitC-1 (N16R8 hoặc tương đương, 16MB flash)
- Cổng USB-C kết nối với PC

### Biên dịch & Nạp

```bash
cd firmware/

# Biên dịch firmware
pio run

# Nạp firmware qua USB
pio run --target upload

# Nạp LittleFS (web UI) - chạy lần đầu và mỗi khi đổi web
pio run --target uploadfs

# Nạp cả 2 cùng lúc
pio run --target upload --target uploadfs

# Mở Serial Monitor (115200 baud)
pio device monitor
```

### Nạp qua Web Browser (không cần cài driver)
Mở `pc_tools/miu_web_flasher.html` trong Chrome/Edge, kết nối USB.

---

## 6. Sử dụng

1. **Nạp firmware + LittleFS** lần đầu.
2. ESP phát WiFi AP: `EILIK_ROBOT` / `12345678`
3. Kết nối điện thoại/laptop vào AP này.
4. Mở trình duyệt, vào `http://192.168.4.1`
5. Sử dụng 2 joystick:
   - **Joystick trái** → xoay cổ
   - **Joystick phải** → gật đầu
6. Tab **AI** → bật **🧠 AI LITE** để robot tự phản ứng
7. Tab **OTA** → dán URL firmware mới, nhấn **TẢI VỀ & NẠP**

---

## 7. Phát triển tiếp

- [ ] Thêm cảm biến khoảng cách (HC-SR04) để né vật cản
- [ ] Thêm pin LiPo + đo điện áp (ADC)
- [ ] Thêm MQTT bridge để điều khiển từ xa qua internet
- [ ] Web UI: thêm chỉnh calibration trực tiếp
- [ ] AI nâng cao: nhận diện giọng nói offline (ESP-SR)

Xem chi tiết trong [roadmap.md](roadmap.md).
