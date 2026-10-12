# MIU-ETS2 — KIẾN TRÚC & LỘ TRÌNH PHÁT TRIỂN (ROADMAP)

> **Dự án:** Hệ thống Taplo Hiển Thị & Render Dữ Liệu Game ETS2/ATS (ESP32-S3) kết hợp PC Desktop Companion  
> **Cập nhật:** 2026-10-12  
> **Định hướng cốt lõi:** Tinh giản tối đa — Siêu ổn định — Màn hình biểu cảm OLED + HUD Game Dashboard (ETS2 / Sim Racing) — Desktop Companion PC tốc độ cao.

---

## 1. TỔNG QUAN KIẾN TRÚC & NGUYÊN TẮC THIẾT KẾ

Hệ thống được thiết kế theo mô hình **Zero-Allocation & Hybrid Display**:
1. **Zero-Allocation & Chống Phân Mảnh Heap (Anti-Heap-Fragmentation):**
   - Loại bỏ hoàn toàn lớp `String` động trong các vòng lặp truyền thông nối tiếp 25–30 FPS.
   - Chuyển toàn bộ bộ đệm giải mã JSON (`StaticJsonDocument<2048>`) sang vùng nhớ DRAM tĩnh (`static`), giải phóng hoàn toàn 2KB bộ nhớ ngăn xếp (Stack) của task FreeRTOS `taskUsb`, triệt tiêu nguy cơ tràn Stack (Stack Overflow) và sụp nguồn.
2. **Cơ chế Bảo vệ Khởi động Độc lập (Boot Isolation & Protection):**
   - Áp dụng cửa sổ bảo vệ 2.5 giây (`bootProtectionUntilMs`) ngay khi khởi động.
   - Ngăn chặn hoàn toàn hiện tượng daemon PC gửi gói telemetry làm đè/cướp màn hình boot logo robot khi vừa cấp nguồn.
   - Chuyển cảnh thông minh: Chỉ hiển thị Splash xe container Scania khi game thực sự đang tải (`loading == true` hoặc `loadPct < 100`). Nếu đã vào buồng lái, hệ thống nhảy thẳng vào Cockpit Dashboard mượt mà.
3. **Màn hình OLED SSD1306 (128x64) Chế độ Kép:**
   - **Chế độ Robot:** Biểu cảm mắt RoboEyes sinh động, Desk-Emoji, Đồng hồ ngày giờ số lớn, quang phổ âm thanh.
   - **Chế độ Cockpit Dashboard (ETS2 / Sim Racing):** Thiết kế 3 cụm Route Advisor đối xứng, Tốc độ số lớn, Cấp số thực tế (R2, R1, N, D1..D12), Icon cảnh báo chuẩn ISO (Kèn, Gạt nước 3 cấp tốc độ, Đèn chiếu sáng, Đèn pha, Lỗi động cơ), Cột thanh xăng, và Thanh tiến độ tải hàng Scania tỉ lệ chuẩn.
4. **Loại bỏ Web Server & Cơ cấu Servo phần cứng:**
   - Web Server nhúng cũ đã xóa hoàn toàn, tiết kiệm 300KB Flash và 40KB RAM.
   - Ngắt hoàn toàn xung PWM servo, loại bỏ hiện tượng sụt áp nguồn tức thời gây brownout reset.

---

## 2. BẢNG PHÂN BỔ BỘ NHỚ & TÀI NGUYÊN ESP32-S3

| Tài nguyên | Dung lượng sử dụng | Tổng dung lượng phần cứng | Tỷ lệ sử dụng | Đánh giá an toàn |
| :--- | :--- | :--- | :--- | :--- |
| **SRAM (RAM tĩnh)** | 28.2 KB (28,224 bytes) | 320 KB | **8.6%** | Vùng an toàn tuyệt đối (> 290KB trống) |
| **Flash ROM** | 394.6 KB (394,673 bytes) | 4.7 MB (App partition) | **8.4%** | Cực kỳ nhẹ, nạp chỉ mất 11 giây |
| **FreeRTOS Stack** | Tiết kiệm thêm 2048 bytes | 6144 bytes / task | Giảm 33% tải Stack | Triệt tiêu Stack Overflow |
| **Heap Allocation** | 0 bytes / frame (Zero-alloc) | Bộ nhớ động | 0% phân mảnh | Chạy 24/7 không crash |

---

## 3. CẤU TRÚC THƯ MỤC CHUẨN DỰ ÁN

```
MIU_EILIK_ESP32-S3/
├── .agents/                    # Bộ quy tắc kỹ thuật và kỹ năng AI IDE
├── docs/                       # Tài liệu thiết kế & quy hoạch kiến trúc
│   └── roadmap.md              # Lộ trình & kiến trúc chi tiết
├── miu_eilik/                  # Source code firmware ESP32-S3 (Siêu tinh gọn)
│   ├── miu_eilik.ino           # Vòng lặp chính, FreeRTOS tasks & Serial Zero-alloc
│   ├── config.h                # Cấu hình toàn hệ thống, mã lệnh & enums
│   ├── board_config.h          # Sơ đồ chân phần cứng GPIO ESP32-S3
│   ├── display_controller.h/.cpp # Màn hình OLED (RoboEyes + Cockpit Dashboard + Boot Protection)
│   ├── FluxGarage_RoboEyes.h   # Thư viện đồ họa mắt robot
│   ├── ets2_client.h/.cpp      # WiFi REST client dự phòng
│   ├── usb_protocol.h/.cpp     # USB Serial parser (Static DRAM JsonDocument)
│   ├── emoji_controller.h/.cpp # Bộ điều phối biểu cảm & hoạt ảnh
│   ├── audio_engine.h/.cpp     # I2S Audio DAC engine
│   ├── led.h/.cpp              # Neopixel RGB LED controller
│   ├── logger.h/.cpp           # Tiện ích ghi log tuần tự
│   ├── ntp_sync.h/.cpp         # Đồng bộ thời gian qua WiFi
│   ├── servo_controller.h/.cpp # Virtual angle stub (không cấp nguồn xung PWM)
│   └── roshi_logo.h            # Bitmap logo khởi động robot
├── scripts/                    # Scripts vận hành tự động tiện lợi cho người dùng
│   ├── CHAY_DEMO_THU_NGHIEM.bat # Chạy thử nghiệm mô phỏng Telemetry trên PC
│   ├── BAT_TU_DONG_CHAY_NGAM.bat # Bật chạy ngầm cùng Windows
│   └── TAT_TU_DONG_CHAY_NGAM.bat # Tắt chế độ tự khởi động ngầm
├── tools/                      # Bộ công cụ PC Companion & SCS SDK Telemetry
│   ├── miu_desktop_ets2.py     # Desktop Companion chính (SCS SDK SHM + Auto COM)
│   ├── mock_telemetry.json     # Dữ liệu kiểm thử độc lập
│   ├── esp32_mcp_server.py     # Cổng giao tiếp AI MCP Server cho IDE
│   └── setup_autostart.py      # Bộ cài đặt Registry tự khởi động
├── platformio.ini              # Cấu hình biên dịch PlatformIO chuẩn hóa
├── partitions_16mb.csv         # Bảng phân vùng Flash tốc độ cao
├── README.md                   # Hướng dẫn sử dụng & vận hành dự án
├── roadmap.md                  # Bản sao tiện ích tại thư mục gốc
└── WORK_REPORT.md              # Báo cáo tiến độ & lịch sử công việc
```

---

## 4. LỘ TRÌNH TRIỂN KHAI THEO GIAI ĐOẠN (MILESTONES)

### Milestone 1: Tinh giản kiến trúc & Loại bỏ rác (HOÀN THÀNH 100%)
- [x] Xóa bỏ Web Server nhúng, DNS Server và chuỗi HTML PROGMEM nặng 60KB.
- [x] Tắt phần cứng Servo PWM tránh sụt áp gây brownout reset.
- [x] Làm sạch thư viện thừa trong `platformio.ini`.
- [x] Build thành công trên môi trường PlatformIO với dung lượng giảm hơn 70%.

### Milestone 2: Cockpit Dashboard & SCS Telemetry SDK (HOÀN THÀNH 100%)
- [x] Đọc trực tiếp từ bộ nhớ chia sẻ `Local\SimTelemetrySCS` của SCS SDK chuẩn xác từng mili-giây.
- [x] Màn hình Cockpit Dashboard 3 cột Route Advisor: Tốc độ số lớn, cấp số D1..D12 / R2 / N, Cột xăng, Cruise Control, Hạn mức tốc độ.
- [x] Hệ thống Icon Header chuẩn ISO: Kèn (Horn), Gạt mưa 3 cấp (Wiper 1-2-3), Đèn cốt, Đèn pha, Cảnh báo lỗi động cơ (Check Engine).
- [x] Splash Screen xe container Scania sắc nét kèm thanh tiến trình tải hàng (% và pixel bar).
- [x] Cơ chế chuyển đổi mượt mà giữa mặt robot Eilik và Dashboard game.

### Milestone 3: Tối ưu hiệu năng, Chống tràn RAM & Bảo vệ Khởi động (HOÀN THÀNH 100%)
- [x] **Boot Protection:** Khóa 2.5 giây đầu, giữ nguyên màn hình mắt và logo khởi động của robot, không bị daemon PC cướp màn hình.
- [x] **Zero-Allocation Parsing:** Loại bỏ hoàn toàn `String` động khi đọc Serial 25–30 FPS, ngăn chặn triệt để hiện tượng phân mảnh Heap (Heap Fragmentation).
- [x] **Stack Safety:** Chuyển `StaticJsonDocument` sang DRAM tĩnh, giải phóng 2KB Stack cho FreeRTOS task, loại bỏ nguy cơ Stack Overflow.
- [x] Cải tiến nạp số lớn 2 chữ số (10, 11, 12, R2) hiển thị cân đối, không bị cắt ngắn thành 1 chữ số.
- [x] Nạp firmware và nghiệm thu ổn định qua cổng COM5.

### Milestone 4: PC Companion GUI & Trình quản lý Đa Game (ĐANG TRIỂN KHAI)
- [ ] Xây dựng giao diện Desktop Companion trực quan bằng PyQt / Tkinter:
  - Tự động nhận diện thiết bị và chọn cổng COM khi cắm USB.
  - Màn hình mô phỏng ảo Dashboard thời gian thực trên PC.
  - Cho phép tùy biến biểu cảm mắt, tùy chỉnh mức sáng OLED, âm lượng và kiểm tra kết nối.
  - Chuyển đổi linh hoạt giữa các game: Euro Truck Simulator 2, American Truck Simulator, Forza Horizon, Assetto Corsa.

### Milestone 5: Hiệu ứng Đa giác quan (Âm thanh I2S & LED Neopixel)
- [ ] Âm thanh cảnh báo qua loa robot: Tiếng "tách-tách" xi-nhan, tiếng bíp quá tốc độ hoặc cảnh báo sắp cạn nhiên liệu.
- [ ] Đèn LED RGB Neopixel đổi màu theo dải vòng tua động cơ (Shift Light) và cảnh báo nguy hiểm khi va chạm.