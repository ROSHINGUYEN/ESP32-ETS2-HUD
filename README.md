# MIU EILIK - ESP32-S3 DESKTOP COMPANION ROBOT

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-red.svg)](https://www.espressif.com/)
[![Framework: Arduino ESP32](https://img.shields.io/badge/Framework-Arduino_ESP32_v3.x-blue.svg)](https://github.com/espressif/arduino-esp32)
[![RTOS: FreeRTOS](https://img.shields.io/badge/RTOS-FreeRTOS_Dual--Core-green.svg)](https://freertos.org/)
[![UI: Glassmorphism Web](https://img.shields.io/badge/UI-Builtin_Web_App-cyan.svg)](#)

---

## 1. Giới thiệu dự án (Introduction)

**MIU EILIK** là dự án robot để bàn tương tác thông minh được thiết kế và tối ưu hóa toàn diện trên nền tảng vi điều khiển cao cấp **ESP32-S3 (Dual-Core Xtensa LX7)**. Lấy cảm hứng từ robot biểu cảm để bàn Eilik, dự án mang đến một người bạn đồng hành sống động trên bàn làm việc với khả năng biểu lộ cảm xúc phong phú qua màn hình OLED, chuyển động cổ và đầu uyển chuyển, phản hồi xúc giác qua cảm biến hồng ngoại, cùng hệ thống âm thanh I2S đồng bộ vũ điệu theo điệu nhạc thời gian thực.

Robot được thiết kế theo tư duy **All-in-One Firmware** độc lập, tích hợp sẵn toàn bộ giao diện điều khiển Web App giao diện kính mờ (Glassmorphism Neon) trực tiếp trong bộ nhớ Flash, cho phép người dùng kết nối và tinh chỉnh mọi thông số của robot từ smartphone hoặc máy tính mà không cần cài đặt ứng dụng phụ trợ hay kết nối internet bên ngoài.

---

## 2. Chức năng & Tính năng nổi bật (Features & Capabilities)

### 2.1. Hệ thống Biểu cảm & Mắt Động cơ học (RoboEyes & Audio Reactive OLED)
* **10+ Trạng thái cảm xúc phong phú:** Vui vẻ (Happy), Nháy mắt (Wink), Giận dữ (Angry), Mệt mỏi (Tired), Ngơ ngác (Curious), Buồn bã (Sad), Say sưa (Dizzy),...
* **Tốc độ quét 30 FPS mượt mà:** Khử hiện tượng giật xé hình ảnh nhờ tối ưu hóa bộ đệm màn hình và điều phối tác vụ hiển thị chuyên biệt trên Core 1.
* **3 Chế độ Mắt Phổ Âm Thanh (Audio Visualizer):**
  * **Beat Eyes:** Kích thước đồng tử tự động co giãn và chớp nháy đồng pha theo từng nhịp Bass trống.
  * **Wave Eyes:** Các đường gợn sóng âm thanh dao động liên tục quanh viền mắt robot.
  * **Bars Equalizer:** Biểu đồ cột phổ âm thanh 8 dải tần số hiển thị trực tiếp trên màn hình OLED.

### 2.2. Động học Đầu & Cổ 2 Trục (Pan/Tilt Motion Engine)
* **Nội suy chuyển động Smoothstep:** Điều khiển 2 động cơ servo (Trục xoay cổ Pan và Trục gật đầu Tilt) với thuật toán tăng giảm tốc mượt mà, triệt tiêu quán tính và rung lắc cơ khí.
* **Bảo vệ phần cứng thông minh:**
  * Giới hạn góc mềm (Soft Limits) ngăn ngừa kẹt cơ khí hoặc va chạm khung vỏ.
  * Lưu trữ điểm cân chỉnh góc (Calibration Offset) vĩnh viễn vào bộ nhớ không bay hơi NVS Flash.
  * Tính năng **Auto-Detach sau 1.5 giây:** Tự động ngắt xung kích hoạt khi robot dừng yên vị trí, chống sinh nhiệt quá mức và bảo vệ tuổi thọ động cơ servo.
* **Cử chỉ đầu & Vũ điệu theo nhạc (Head Dancing):**
  * Hỗ trợ nạp các chuỗi cử chỉ theo khung hình động (Keyframe Gestures): gật đầu chào, lắc đầu từ chối, nghiêng đầu thắc mắc.
  * Tự động nhún nhảy, đảo đầu theo nhịp phách (BPM) khi phát hiện bài hát đang phát.

### 2.3. Âm thanh Kỹ thuật số & Bộ Phân tích Nhạc Thông minh (I2S Audio & AudioAnalyzer)
* **Âm thanh số I2S qua DAC MAX98357A:** Truyền tải âm thanh chuẩn stereo/mono trung thực, loại bỏ hoàn toàn nhiễu analog từ nguồn nuôi.
* **Chống nổ loa (Anti-Pop Mute Circuit):** Tự động điều khiển chân SD và áp dụng thuật toán Fade-in / Fade-out âm lượng mềm khi bắt đầu và kết thúc phát âm thanh.
* **Bộ phân tích phổ âm thanh thời gian thực (AudioAnalyzer):**
  * Tốc độ xử lý dưới 1 mili-giây, phân tách 8 băng tần âm thanh (từ Bass 60Hz đến Treble 16kHz).
  * Thuật toán phát hiện xung nhịp (Beat Detection) và ước tính nhịp độ bài hát (BPM Tracking) để kích hoạt vũ điệu đồng bộ.
* **Phát âm thanh hiệu ứng (SFX) & Nhạc:** Tự động giải mã và phát các đoạn hiệu ứng âm thanh sống động hoặc file nhạc lưu trong bộ nhớ.

### 2.4. Cảm biến Tương tác Cử chỉ Hồng ngoại (Dual IR Touch Sensors)
* **Nhận diện thao tác đa dạng:**
  * Chạm nhanh đơn (Single Tap)
  * Chạm đúp (Double Tap)
  * Chạm ba lần (Triple Tap)
  * Chạm giữ lâu (Long Press)
  * Vẫy tay chuyển hướng (Swipe Gesture)
* **Xử lý theo ngữ cảnh thông minh (Context-Aware):**
  * Khi ở trạng thái bình thường: Đổi cảm xúc, làm robot bất ngờ, cười toe toét hoặc hờn dỗi.
  * Khi đang phát nhạc: Trở thành cụm điều khiển media cảm ứng (Next bài, Pause/Play, Tăng/Giảm âm lượng).

### 2.5. Giao diện Web App điều khiển tích hợp (Built-in Web Controller)
* **Phát Wi-Fi SoftAP độc lập:** Tạo điểm truy cập Wi-Fi mã hóa WPA2 tiện dụng, không cần phụ thuộc vào bộ phát Wi-Fi gia đình.
* **Cơ chế Captive Portal thông minh:** Tự động bật trang điều khiển ngay khi điện thoại hoặc laptop kết nối vào mạng Wi-Fi của Robot.
* **Phong cách Thiết kế Hiện đại (Cyberpunk / Glassmorphism):**
  * Tông màu tối (Dark Mode) kết hợp hiệu ứng kính mờ và viền Neon tinh tế.
  * Bảng điều khiển cảm xúc trực quan bằng một chạm.
  * Cần điều khiển góc xoay Servo (Pan / Tilt) phản hồi tức thì.
  * Trình phát nhạc mini (Play, Pause, Next, Volume Slider).
  * Công cụ cân chỉnh Offset Servo và giám sát thông số hệ thống thời gian thực.
* **Nhúng hoàn toàn trong Firmware (PROGMEM):** Không cần nạp tệp giao diện riêng biệt lên hệ thống tệp, nạp firmware là sẵn sàng hoạt động ngay lập tức.

### 2.6. Cổng Giao tiếp PC Bridge & Web Serial Tốc độ cao
* **Native USB CDC High-Speed:** Giao tiếp trực tiếp qua cổng USB phần cứng của ESP32-S3 với giao thức nhị phân nhận diện khung dữ liệu chuẩn xác `[0xE1 0x1C]`.
* **Truyền tải hình ảnh PC lên màn hình:** Chuyển đổi và dither hình ảnh hoặc video từ máy tính với độ phân giải 128x64 lên màn hình OLED của Robot theo thời gian thực.
* **Điều khiển từ xa qua Serial JSON:** Cho phép PC gửi lệnh điều khiển trạng thái cảm xúc, góc quay và bài hát.
* **Công cụ Web Serial:** Hỗ trợ kết nối và điều khiển trực tiếp robot từ trình duyệt máy tính (Google Chrome, Microsoft Edge) mà không cần cài đặt phần mềm bổ sung.

### 2.7. Kiến trúc Đa nhiệm Phân luồng FreeRTOS (Dual-Core Multitasking)
* **Core 0:** Đảm nhiệm tác vụ mạng Wi-Fi, máy chủ WebServer, xử lý luồng âm thanh I2S và giải mã phổ âm thanh.
* **Core 1:** Đảm nhiệm chuyển động động học Servo nội suy, vẽ giao diện màn hình OLED 30 FPS và quét tín hiệu cảm biến hồng ngoại.
* Nhờ chia tách nhân xử lý, các chuyển động và biểu cảm của robot luôn đạt độ mượt tuyệt đối, không bị trễ hay giật ngay cả khi đang truyền phát dữ liệu mạng nặng.

---

## 3. Ngôn ngữ Lập trình (Programming Languages)

Dự án được xây dựng và kết hợp hài hòa giữa các ngôn ngữ lập trình hiện đại nhằm tối ưu hiệu năng nhúng và trải nghiệm người dùng:

* **C++ (Arduino / ESP-IDF FreeRTOS):** Ngôn ngữ chính cấu thành toàn bộ Firmware của Robot, tối ưu hóa xử lý thời gian thực, quản lý bộ nhớ đệm, điều khiển ngoại vi và định tuyến luồng FreeRTOS.
* **HTML5:** Cấu trúc giao diện Web điều khiển nhúng trong bộ nhớ vi điều khiển.
* **CSS3 (Vanilla Glassmorphism):** Phong cách thiết kế giao diện kính mờ với hiệu ứng Neon, chuyển động mượt mà và tương thích hoàn hảo trên thiết bị di động.
* **JavaScript (ES6+):** Xử lý logic phía máy khách trên trình duyệt Web và giao tiếp Serial Web API.
* **Python (3.10+):** Ngôn ngữ xây dựng công cụ cầu nối PC Bridge, xử lý thuật toán dither hình ảnh Floyd-Steinberg và truyền dữ liệu nhị phân tốc độ cao qua cổng USB CDC.

---

## 4. Thư viện & Phụ thuộc (Libraries & Dependencies)

### 4.1. Dành cho Firmware ESP32-S3 (Arduino IDE / PlatformIO)
* **Thư viện điều khiển màn hình & Đồ họa:**
  * `Adafruit_GFX` (v1.11.x trở lên) - Bộ công cụ vẽ đồ họa 2D cơ bản.
  * `Adafruit_SSD1306` (v2.5.x trở lên) - Driver giao tiếp I2C cho màn hình OLED 128x64.
  * `FluxGarage_RoboEyes` - Thư viện tạo hình biểu cảm mắt robot hình học.
* **Thư viện điều khiển động cơ:**
  * `ESP32Servo` (v3.x trở lên) - Hỗ trợ phát xung PWM mượt mà dựa trên phần tử phần cứng MCPWM/LEDC của ESP32-S3.
* **Thư viện Âm thanh & Xử lý số:**
  * `Audio` (ESP32-audioI2S v3.x trở lên) - Giải mã âm thanh MP3/WAV và xuất luồng số I2S.
* **Thư viện Hệ thống & Mạng (Tích hợp sẵn trong ESP32 Arduino Core):**
  * `WiFi` & `WebServer` - Quản lý kết nối mạng và phục vụ yêu cầu HTTP.
  * `DNSServer` - Phục vụ định tuyến tên miền Captive Portal.
  * `Preferences` - Giao tiếp bộ nhớ Flash NVS lưu trữ thông số cân chỉnh góc.
  * `LittleFS` - Hệ thống tệp bộ nhớ Flash lưu trữ âm thanh SFX và nhạc.
  * `freertos/FreeRTOS.h` & `freertos/task.h` - Nhân điều phối đa nhiệm thời gian thực.

### 4.2. Dành cho Công cụ Máy tính (PC Tools)
* **Python (Cài đặt qua requirements):**
  * `pyserial` (>= 3.5) - Giao tiếp cổng nối tiếp ảo USB Native CDC.
  * `Pillow` (>= 10.0.0) - Đọc, căn chỉnh kích thước và xử lý dither điểm ảnh 1-bit.
  * `numpy` (>= 1.24.0) - Xử lý tính toán ma trận điểm ảnh tốc độ cao.

---

## 5. Sơ đồ Đấu nối Phần cứng (Hardware Pinout & Specifications)

| Khối Phần cứng | Linh kiện | Chân ESP32-S3 | Chức năng chi tiết |
| :--- | :--- | :--- | :--- |
| **Màn hình hiển thị** | OLED SSD1306 0.96" | **GPIO 8** | I2C SDA (Truyền dữ liệu màn hình) |
| | | **GPIO 9** | I2C SCL (Xung nhịp I2C) |
| **Động cơ Servo** | SG90 / MG90S (Trục Cổ Pan) | **GPIO 4** | PWM Control Pan Servo (0° - 180°) |
| | SG90 / MG90S (Trục Đầu Tilt) | **GPIO 5** | PWM Control Tilt Servo (0° - 180°) |
| **Âm thanh I2S** | Mạch giải mã DAC MAX98357A | **GPIO 15** | BCLK (Bit Clock) |
| | | **GPIO 16** | LRC / WSEL (Left/Right Channel Clock) |
| | | **GPIO 17** | DIN (Dữ liệu âm thanh số I2S) |
| | | **GPIO 18** | SD_MODE (Bật/Tắt chống nổ loa) |
| **Cảm biến Xúc giác**| Cảm biến Hồng ngoại LM393 #1 | **GPIO 6** | Digital In (Cảm biến đỉnh đầu / trán) |
| | Cảm biến Hồng ngoại LM393 #2 | **GPIO 7** | Digital In (Cảm biến cằm / bên hông) |
| **Cổng Nạp & Giao tiếp**| Native USB CDC | **GPIO 19 / 20** | USB D- và USB D+ (Giao tiếp PC Bridge) |
| | Debug UART0 | **GPIO 43 / 44** | TX0 / RX0 (In log theo dõi sức khỏe chip) |

---

## 6. Hướng dẫn Nạp Firmware & Sử dụng (Setup & Usage)

1. **Chuẩn bị môi trường nạp:**
   * Mở Arduino IDE (phiên bản 2.x khuyến nghị).
   * Cài đặt gói bo mạch **esp32 by Espressif Systems** (phiên bản 3.0.x trở lên).
   * Cài đặt các thư viện cần thiết theo danh sách phụ thuộc.
2. **Cấu hình thông số Bo mạch trong Arduino IDE:**
   * **Board:** `ESP32S3 Dev Module`
   * **USB CDC On Boot:** `Enabled`
   * **Flash Size:** `16MB (128Mb)`
   * **Partition Scheme:** `16M Flash (3MB APP/9.9MB LittleFS)`
   * **PSRAM:** `OPI PSRAM` (nếu dùng module N16R8)
   * **Upload Speed:** `921600`
3. **Nạp Firmware:**
   * Kết nối cáp Type-C vào cổng USB của ESP32-S3.
   * Nhấn nút **Upload** trên Arduino IDE.
4. **Vận hành & Trải nghiệm:**
   * Sau khi khởi động, robot sẽ tự động căn chỉnh góc và mở mắt chào đón.
   * Dùng điện thoại tìm kiếm Wi-Fi có tên **`EILIK_ROBOT`** (Mật khẩu mặc định: `12345678`).
   * Trang điều khiển sẽ tự động hiển thị (hoặc truy cập trình duyệt tại địa chỉ `http://192.168.4.1`).

---

## 7. Giấy tờ, Bản quyền & Tuyên bố Pháp lý (License & Legal)

### 7.1. Giấy phép Mã nguồn (License)
Dự án được phát hành theo giấy phép mã nguồn mở **MIT License**. Bạn được tự do nghiên cứu, chỉnh sửa, phân phối và sử dụng cho mục đích cá nhân hoặc thương mại với điều kiện giữ nguyên thông tin bản quyền gốc.

### 7.2. Tác giả & Đóng góp (Author & Acknowledgments)
* **Tác giả:** ROSHINGUYEN ([@ROSHINGUYEN](https://github.com/ROSHINGUYEN))
* **Ý tưởng thiết kế:** Lấy cảm hứng từ cộng đồng yêu thích robot để bàn thông minh và robot tương tác Eilik (Energize Lab).
* **Đóng góp cộng đồng:** Cảm ơn các dự án mã nguồn mở tuyệt vời: Espressif ESP32 Arduino Core, Adafruit GFX, FluxGarage RoboEyes, và ESP32-audioI2S.

### 7.3. Tuyên bố Miễn trừ Trách nhiệm (Legal Disclaimer)
* Đây là dự án học tập, nghiên cứu kỹ thuật nhúng và phát triển phần cứng cộng đồng (DIY Open Source Project).
* Dự án không trực thuộc, không được tài trợ và không có liên kết thương mại chính thức với hãng sản xuất robot Eilik ban đầu.
* Người sử dụng tự chịu trách nhiệm về an toàn phần cứng, nguồn cấp điện và kết nối cơ khí trong quá trình tự lắp ráp.
