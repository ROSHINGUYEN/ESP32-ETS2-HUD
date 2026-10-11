# ROSHI MIU EILIK — KIẾN TRÚC & LỘ TRÌNH PHÁT TRIỂN (ROADMAP)

> **Dự án:** Robot để bàn thông minh ROSHI Miu Eilik (ESP32-S3) kết hợp PC Desktop Companion  
> **Cập nhật:** 2026-10-11  
> **Định hướng cốt lõi:** Tinh giản tối đa — Siêu ổn định — Màn hình biểu cảm OLED + HUD Game Dashboard (ETS2 / Sim Racing) — Desktop Companion PC tốc độ cao.

---

## 1. TỔNG QUAN ĐỊNH HƯỚNG TÁI CẤU TRÚC

Sau quá trình thử nghiệm thực tế, hệ thống được tái cấu trúc toàn diện theo các nguyên tắc:
1. **Loại bỏ Web Server & Web UI nội bộ trên ESP32:**
   - *Lý do kỹ thuật:* Web Server nhúng (Soft-AP, Captive Portal, DNS Server, chuỗi HTML 60KB trong PROGMEM) tiêu tốn hơn 300KB Flash, 40KB RAM, tạo luồng xử lý nặng nề trên CPU và làm chậm thời gian khởi động (boot).
   - *Giải pháp mới:* Toàn bộ việc điều khiển, cấu hình và giám sát được chuyển sang **Desktop Miu Companion** trên PC (qua USB Serial hoặc App máy tính) và **WiFi REST Client độc lập**. ESP32 khởi động tức thì trong **< 100ms**.
2. **Loại bỏ hoàn toàn cơ chế Servo phần cứng:**
   - *Lý do kỹ thuật:* Động cơ SG90 tiêu thụ dòng tức thời cao gây sụt áp nguồn cấp, kích hoạt lỗi `rst:0xf (BROWNOUT_RST)` làm reset hệ thống. Cơ cấu 360° cơ khí không ổn định.
   - *Giải pháp mới:* Ngắt hoàn toàn timer LEDC PWM, không xuất xung ra GPIO 11/12/13. Các hàm góc chỉ lưu biến tọa độ ảo trong RAM để giữ nguyên tương thích giao thức điều khiển biểu cảm.
3. **Trọng tâm sản phẩm mới:**
   - **Màn hình OLED SSD1306 (128x64):** Mắt robot RoboEyes sinh động, 42 animation Desk-Emoji, Đồng hồ số ngày giờ, và **Màn hình Game Dashboard ETS2/Sim Racing** (đồng hồ tốc độ số lớn, cấp số, xi-nhan, cruise control, cột xăng đồ họa, cảnh báo giới hạn tốc độ).
   - **Âm thanh I2S & Đèn LED Neopixel:** Hệ thống âm thanh sống động và LED đổi màu theo trạng thái.
   - **Desktop Miu Companion Tool:** Kết nối USB Serial tốc độ cao 20–30 FPS đồng bộ trực tiếp từ Telemetry Server trên PC.

---

## 2. KẾ HOẠCH DỌN DẸP & PHÂN LOẠI FILE SOURCE

### A. Nhóm file XÓA BỎ HOÀN TOÀN (File rác, test cũ, web thừa)
| Đường dẫn | Phân loại | Lý do xóa |
| :--- | :--- | :--- |
| `web_ui/` *(toàn bộ thư mục)* | Web UI | Đã chuyển toàn bộ điều khiển sang PC Desktop Companion |
| `miu_eilik/web_ui_html.h` | Web Header | File mã hóa HTML PROGMEM nặng 60KB, không còn dùng |
| `miu_eilik/web_server_controller.h` | Web Server | Xóa bỏ class WebController và các route HTTP |
| `miu_eilik/web_server_controller.cpp` | Web Server | Xóa bỏ logic SoftAP, DNS và WebServer nhúng |
| `miu_eilik/servo_pulse_policy.h` | Servo Policy | Không còn dùng phần cứng PWM servo |
| `CHAY_KIEM_TRA_SERVO.bat` | Script Test | Script test servo cũ |
| `SERVO_DIAGNOSTIC_REPORT.md` | Báo cáo cũ | Báo cáo chẩn đoán servo cũ |
| `oled_ui_preview.html` | HTML Preview | File preview giao diện web tĩnh |
| `tools/oled_diag.ino` | Firmware Test | File chẩn đoán màn hình 66KB cũ |
| `tools/oled_test/` *(thư mục)* | Firmware Test | File test OLED độc lập cũ |
| `tools/scan_servo.py` | Python Tool | Tool quét xung servo cũ |
| `tools/servo_diag_tool.py` | Python Tool | Tool GUI chẩn đoán servo cũ |
| `patches/` & `apply_patches.py` | Patch Library | Patch sửa lỗi ESP32Servo (thư viện không còn sử dụng) |

### B. Nhóm file GIỮ LẠI & TỐI ƯU HÓA (Mã nguồn cốt lõi)
| Đường dẫn | Vai trò | Trạng thái |
| :--- | :--- | :--- |
| `miu_eilik/miu_eilik.ino` | Entry point chính của firmware | Tinh giản loop, loại bỏ web controller, boot tức thì |
| `miu_eilik/config.h` | Cấu hình toàn hệ thống, mã lệnh, gói tin | Đã cập nhật chế độ DISP_GAME & gói PKT_GAME_TELEMETRY |
| `miu_eilik/board_config.h` | Định nghĩa chân phần cứng GPIO ESP32-S3 | Giữ nguyên sơ đồ chân chuẩn |
| `miu_eilik/display_controller.h/.cpp` | Điều khiển OLED SSD1306 128x64 | Chứa RoboEyes, Clock, Visualizer & ETS2 Game Dashboard |
| `miu_eilik/FluxGarage_RoboEyes.h` | Thư viện đồ họa mắt Eilik | Đã xử lý triệt để macro la bàn |
| `miu_eilik/ets2_client.h/.cpp` | WiFi Client kéo Telemetry trực tiếp | Hoạt động độc lập qua mạng LAN với PC IP:25555 |
| `miu_eilik/usb_protocol.h/.cpp` | Giao thức USB Serial nhị phân & JSON | Xử lý lệnh nhanh, độ trễ cực thấp 20-30 FPS |
| `miu_eilik/emoji_controller.h/.cpp` | Bộ điều phối biểu cảm, hoạt ảnh, lệnh | Giữ các lệnh điều khiển mắt, âm thanh, chế độ màn hình |
| `miu_eilik/audio_engine.h/.cpp` | Bộ xử lý âm thanh I2S DAC | Phát âm thanh hiệu ứng & stream audio |
| `miu_eilik/led.h/.cpp` | Điều khiển đèn Neopixel RGB | Hiệu ứng ánh sáng biểu cảm |
| `miu_eilik/logger.h/.cpp` | Hệ thống ghi log tuần tự | Quản lý log ra Serial |
| `miu_eilik/ntp_sync.h/.cpp` | Đồng bộ thời gian thực qua NTP | Cập nhật đồng hồ khi có WiFi |
| `miu_eilik/servo_controller.h/.cpp` | Trạng thái góc ảo (Virtual Stub) | Tinh gọn thành stub nhẹ lưu góc ảo, không tốn tài nguyên |
| `tools/miu_desktop_ets2.py` | Companion Tool chính trên PC | Tự bắt cổng COM, đọc telemetry 25555, hỗ trợ mode `--demo` |
| `API-ETS2/` | Telemetry Server ETS2 trên PC | Chứa server `Ets2Telemetry.exe` và dữ liệu test của game |
| `platformio.ini` | Cấu hình biên dịch PlatformIO | Gỡ bỏ thư viện web và servo thừa |
| `WORK_REPORT.md` | Nhật ký công việc liên tục | Duy trì cập nhật tiến độ minh bạch |

---

## 3. CẤU TRÚC THƯ MỤC DỰ KIẾN SAU DỌN DẸP

```
MIU_EILIK_ESP32-S3/
├── .agents/                    # Quy tắc và kỹ năng AI
├── API-ETS2/
│   └── server/                 # ETS2 Telemetry Server cho PC (port 25555)
├── miu_eilik/                  # Source code firmware ESP32-S3 (Siêu tinh gọn)
│   ├── miu_eilik.ino           # Main setup & FreeRTOS loop
│   ├── config.h                # System configuration & enums
│   ├── board_config.h          # Hardware pinout (I2C, I2S, RGB)
│   ├── display_controller.h/.cpp # OLED 128x64 (RoboEyes + ETS2 HUD)
│   ├── FluxGarage_RoboEyes.h   # Thư viện RoboEyes
│   ├── ets2_client.h/.cpp      # WiFi REST client kết nối PC
│   ├── usb_protocol.h/.cpp     # USB Serial protocol (JSON + Binary)
│   ├── emoji_controller.h/.cpp # Emotion & command execution
│   ├── audio_engine.h/.cpp     # I2S Audio engine
│   ├── led.h/.cpp              # Neopixel RGB LED controller
│   ├── logger.h/.cpp           # Serial logging utility
│   ├── ntp_sync.h/.cpp         # Network time protocol sync
│   ├── servo_controller.h/.cpp # Virtual angle stub (no hardware PWM)
│   └── roshi_logo.h            # Bitmap logo khởi động
├── tools/
│   └── miu_desktop_ets2.py     # Desktop Companion chính (USB Serial & Demo Sim)
├── platformio.ini              # Build config tối ưu (loại bỏ thư viện thừa)
├── partitions_16mb.csv         # Phân vùng flash tối ưu
├── README.md                   # Hướng dẫn sử dụng dự án
├── roadmap.md                  # Tài liệu quy hoạch kiến trúc (File này)
└── WORK_REPORT.md              # Báo cáo tiến độ liên tục
```

---

## 4. LỘ TRÌNH TRIỂN KHAI (MILESTONES)

### Milestone 1: Dọn dẹp & Tinh giản (Hoàn thành ngay)
- [x] Xây dựng Roadmap quy hoạch toàn bộ file source.
- [ ] Xóa bỏ toàn bộ các file rác, file test không dùng và thư mục `web_ui/`.
- [ ] Gỡ bỏ `web_server_controller` và `web_ui_html.h` khỏi firmware `miu_eilik`.
- [ ] Dọn dẹp `platformio.ini` (bỏ `ESP32Servo`, `WebServer`, `DNSServer`, `apply_patches.py`).
- [ ] Thu gọn `servo_controller` thành virtual stub nhẹ.
- [ ] Build kiểm chứng PlatformIO thành công 100% với dung lượng Flash và RAM giảm sâu.

### Milestone 2: Tối ưu hóa hiệu năng & Boot Time
- Đảm bảo thời gian khởi động từ lúc cắm nguồn đến khi hiển thị mắt robot đạt **< 100ms**.
- Đảm bảo tốc độ vẽ OLED đạt 30+ FPS ổn định, mượt mà không rung giật.
- Tự động nhận diện kết nối USB Serial từ PC; khi PC gửi telemetry thì chuyển tức thì sang Game Dashboard.

### Milestone 3: Nâng cấp Miu Desktop Companion (GUI trên PC)
- Phát triển giao diện Desktop app trực quan trên PC (dùng Python Tkinter/PyQt hoặc Electron):
  - Tự động dò tìm cổng COM của Robot khi cắm vào máy tính.
  - Hiển thị HUD mô phỏng trên màn hình PC song song với OLED.
  - Cho phép tùy biến biểu cảm mắt, âm thanh và bài hát phát ra loa robot trực tiếp từ PC.
  - Quản lý profile game: Euro Truck Simulator 2, American Truck Simulator, Forza Horizon, Assetto Corsa.

### Milestone 4: Mở rộng tính năng Tương tác & Âm thanh
- Bổ sung hiệu ứng âm thanh cảnh báo khi quá tốc độ, sắp hết xăng hoặc bật xi-nhan (đồng bộ từ loa robot).
- Hiệu ứng LED RGB Neopixel đổi màu theo vòng tua RPM và trạng thái xe.