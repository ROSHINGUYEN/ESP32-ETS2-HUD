# MIU EILIK - ESP32-S3 OLED TELEMETRY COMPANION & ROBOT EYES

Hệ thống **Taplo hiển thị Telemetry xe tải Châu Âu (Euro Truck Simulator 2 / ATS)** và **Robot biểu cảm (RoboEyes)** chạy trên vi điều khiển **ESP32-S3-WROOM-1 (N16R8)** với màn hình OLED 128x64.

Dự án tích hợp cơ chế **Tự động chạy ngầm cùng Windows (0-Touch)**, kéo luồng trực tiếp từ Windows Shared Memory với độ trễ siêu thấp (<2ms) qua chuẩn mở SCS SDK / Open Telemetry API, và **Cổng MCP Server (Model Context Protocol)** kết nối trực tiếp với AI IDE (Antigravity, Cursor, Claude).

---

## 📸 Tổng Quan Tính Năng Chính

```
  ┌─────────────────────────────────────────────────────────────┐
  │  [!] 80     D12     14:25   │  Biển báo tốc độ 80 (chớp khi quá tốc)
  │  ─────────────────────────  │  Hộp số D12 | Giờ game 14:25
  │          85 km/h            │  Tốc độ số to nét Font 3 (21x16 px)
  │  ■■■■■■■■■■■■■■■■ [1250]    │  RPM có dải xanh kinh tế (1000-1500)
  │  F: 92% (778L)  T:2%  C:0%  │  Xăng % & Lít | Độ bền xe & Hàng hóa
  │  < ETA:140km >  WIP  RET:2  │  Xi-nhan | Cự ly | Gạt mưa | Retarder
  └─────────────────────────────────────────────────────────────┘
```

### 1. 🚛 Full Taplo Euro Truck Simulator 2 (ETS2 / ATS)
- **Tốc độ xe to bản (Font 3)**: Chữ số to 21x16 pixel, căn giữa hoàn hảo, hiển thị rõ nét từ xa.
- **Biển báo tốc độ Châu Âu (`[80]`, `[90]`)**: Tự động nhận diện biển báo trên cung đường. Chớp nháy tương phản ngược (chữ trắng nền đen) khi chạy quá tốc độ để cảnh báo tài xế.
- **Hộp số thông minh**: Tự động hiển thị `D1`..`D12`, `N` (Số Mo), `R1`, `R2` (Số lùi).
- **Vòng tua máy RPM & Dải tua kinh tế**: Thanh Bar Graph RPM mượt mà kèm vạch chấm căn chuẩn dải tua xanh kinh tế (`1000 - 1500 RPM`) theo chuẩn các dòng xe Scania, Volvo, MAN.
- **Nhiên liệu chi tiết**: Hiển thị đồng thời **% bình xăng** và **Số lít thực tế** (ví dụ `92% (778L)`). Tự động chớp cảnh báo khi xăng dưới 15%.
- **Độ bền xe & Rơ-moóc**:
  - `T:%` (Truck Wear): Tỷ lệ hao mòn tổng hợp đầu kéo (Động cơ, hộp số, cabin, khung gầm, bánh xe).
  - `C:%` (Cargo/Trailer Wear): Tỷ lệ hao mòn hàng hóa và rơ-moóc (`--` nếu chưa nối rơ-moóc).
- **Hành trình & Điều hướng GPS**: Hiển thị cự ly còn lại tới đích đến (ví dụ `ETA: 140km`).
- **Đèn & Hệ thống phụ trợ**: Xi-nhan trái/phải (`<` / `>`), Đèn pha chiếu gần/xa, Đèn nóc xe (Beacon), Gạt mưa (`WIP`), Phanh tay (`PARK`), Phanh khí nén (`AIR`), Phanh máy (`MBRK`), Phanh phụ Retarder đa cấp (`RET: 1..5`), Ga tự động (`CRUISE 80k`).
- **Cockpit Toast Popups (Thông báo buồng lái thời gian thực)**:
  - Cảnh báo bị phạt tốc độ: `PHAT TOC DO: -550 EUR`
  - Trạm thu phí cao tốc: `THU PHI: -45 EUR`
  - Tai nạn va chạm: `VA CHAM: -400 EUR`
  - Thao tác buồng lái: `RETARDER: BAC 2`, `GAT MUA: BAT`, `CRUISE: 85 km/h`...

### 2. 🛡️ Chuẩn Open Telemetry Độc Lập & Hợp Pháp (100% An Toàn Trên GitHub)
- **Tách biệt hoàn toàn mã nguồn khỏi binary độc quyền**: Không nhúng bất kỳ file `.exe`, `.dll` hay file bản quyền của game/bên thứ ba vào repository Git, tránh vi phạm bản quyền khi phát hành mã nguồn mở.
- **Thư viện mở `tools/ets2_open_api.py`**:
  - Hỗ trợ trực tiếp chuẩn chính thống **SCS SDK Memory-Mapped File (`Local\SCSTelemetry`)** và kênh **Shared Memory Funbit (`Local\Ets2TelemetryServer`)**.
  - Đọc cấp số trực tiếp từ biến `dashboardGear` của SDK, bảo đảm cấp số đồng bộ 100% với màn hình taplo buồng lái xe tải (D1..D12).
  - Tích hợp bộ tạo giả lập độc lập `tools/mock_telemetry.json` để kiểm thử và chạy demo 30 FPS ngay cả khi không có game hoặc khi chạy trên môi trường CI/CD.

### 3. 🤖 Chế Độ Robot Biểu Cảm (RoboEyes Emotion)
- Khi không bật game, màn hình tự động hiển thị mắt robot động **RoboEyes** (FluxGarage) tự nhiên: chớp mắt ngẫu nhiên, đổi tâm trạng, liếc nhìn xung quanh.
- **Tự động chuyển cảnh thông minh**:
  - Khởi động ETS2 -> Robot hiện **Loading Splash** (Logo SCS & Xe tải pixel 2.2s) -> Tự động chuyển sang **HUD Taplo ETS2**.
  - Thoát game -> Robot tự động chuyển về **Mắt robot RoboEyes**.

### 4. ⚡ Tự Động Chạy Cùng Windows (0-Touch Boot)
- Không cần mở thủ công mỗi lần bật máy: Phần mềm cầu nối (`miu_desktop_ets2.py`) tự động khởi chạy tàng hình cùng Windows qua VBScript (0% CPU, không chiếm cửa sổ CMD).
- Tự động phát hiện và kết nối lại ESP32-S3 khi cắm/rút cáp USB (Auto COM Port Reconnect).
- Đọc trực tiếp **Windows Shared Memory (`Local\Ets2TelemetryServer`)**: Không cần cài đặt server Node.js, không cần cấu hình IP/Port mạng, độ trễ cực thấp < 2ms!

### 5. 🧠 Cổng MCP Server Tích Hợp (Model Context Protocol)
- Kết nối trực tiếp với các AI Coding IDE thế hệ mới (**Antigravity**, **Cursor**, **Claude Desktop**).
- Cho phép AI đọc log thời gian thực, giám sát telemetry xe và gửi lệnh điều khiển robot trực tiếp thông qua JSON-RPC stdio.

---

## 🔌 Sơ Đồ Phần Cứng (Hardware Pinout)

Bo mạch sử dụng: **ESP32-S3-WROOM-1 (N16R8: 16MB Flash, 8MB Octal PSRAM)** kết hợp màn hình **OLED SSD1306 0.96" hoặc SH1106 1.3" (I2C)**.

```
┌────────────────────────────────────────────────────────┐
│               ESP32-S3-WROOM-1 (N16R8)                 │
├────────────────────────────────────────────────────────┤
│  GPIO 8  (D8)  ──►  OLED SDA  (Dữ liệu I2C)           │
│  GPIO 9  (D9)  ──►  OLED SCL  (Xung I2C Clock)         │
│  3V3           ──►  OLED VCC  (Nguồn 3.3V)             │
│  GND           ──►  OLED GND  (Mass)                   │
│                                                        │
│  * Firmware có tính năng Smart Auto-Scan:             │
│    Tự động quét và kích hoạt 8 cặp chân I2C thay thế   │
│    (GPIO 8/9, GPIO 1/2, GPIO 41/42, GPIO 47/48)        │
│    nếu người dùng cắm sai cổng.                        │
└────────────────────────────────────────────────────────┘
```

---

## 🚀 Hướng Dẫn Vận Hành & Khởi Động Tự Động

### Cách 1: Kích Hoạt Tự Động Chạy Cùng Windows (Khuyên Dùng)
Chỉ cần bấm 1 lần duy nhất, máy tính sẽ tự động chạy ngầm vĩnh viễn:
1. Nhấp đúp vào file: **`scripts/BAT_TU_DONG_CHAY_NGAM.bat`**
2. Cửa sổ báo `[OK] Da cai dat vao Windows Startup thanh cong!`
3. Xong! Từ nay mỗi khi bật máy tính Windows:
   - Phần mềm cầu nối tự động chạy ngầm hoàn toàn.
   - Khi bạn mở ETS2 và lái xe, màn hình robot tự nhảy Taplo HUD đầy đủ thông số.
   - Khi bạn tắt game, màn hình quay về mắt robot.

### Cách 2: Tắt Chế Độ Tự Động Chạy Ngầm
Nếu muốn tắt hoặc gỡ bỏ khỏi Windows Startup:
- Nhấp đúp vào file: **`scripts/TAT_TU_DONG_CHAY_NGAM.bat`**

### Cách 3: Chạy Thủ Công Có Cửa Sổ Console (Để Xem Dashboard Trên Màn Hình Máy Tính)
- Nhấp đúp vào: **`scripts/CHAY_DASHBOARD_ETS2.bat`**
- Terminal sẽ hiển thị thanh dashboard ASCII thời gian thực:
  ```
  |   D12   | SPD:  85 km/h [80] ! OVER | RPM: 1250 | FUEL: 92% (778L) | TRK: 2% | CRG: 0% | ETA: 140km | WIP RET:2 | TX: 30.0 FPS
  ```

---

## 🛠️ Hướng Dẫn Nạp Firmware ESP32-S3

### 1. Nạp Bằng PlatformIO (Khuyên Dùng)
Dự án đã được cấu hình tối ưu sẵn trong `platformio.ini`:
```bash
# Di chuyển vào thư mục dự án
cd E:\MIU_EILIK_ESP32-S3

# Biên dịch kiểm tra
pio run -e eilik_s3

# Nạp firmware vào bo mạch qua cổng COM (tự động nhận diện COM5)
pio run -e eilik_s3 -t upload

# Mở Serial Monitor xem log khởi động (115200 baud)
pio device monitor -b 115200
```

### 2. Thông Số Tài Nguyên Sau Biên Dịch
- **Flash ROM**: ~394 KB / 4.6 MB APP (**8.4%**) - Siêu nhẹ, độ ổn định tuyệt đối.
- **RAM SRAM**: ~25.2 KB / 327 KB (**7.7%**) - Không giật lag, không tràn bộ nhớ.
- **FreeRTOS**: Phân luồng độc lập 2 nhân (Core 0 nhận USB Serial 115200 baud, Core 1 render màn hình 30 FPS).

---

## 🧠 Cổng MCP Server Cho AI IDE (Model Context Protocol)

Dự án tích hợp sẵn server MCP tại `tools/esp32_mcp_server.py`. Server này tuân thủ chuẩn JSON-RPC 2.0 stdio của Model Context Protocol.

### 1. Cấu Hình Tích Hợp Vào IDE
Cấu hình trong file `mcp_config.json` (Antigravity / Claude Desktop / Cursor):
```json
{
  "mcpServers": {
    "esp32-eilik": {
      "command": "python",
      "args": [
        "tools/esp32_mcp_server.py"
      ]
    }
  }
}
```

### 2. Danh Sách Tools Của MCP Server

| Tên Tool | Chức Năng |
|---|---|
| `esp32_get_status` | Kiểm tra trạng thái daemon, cổng COM5, kết nối game ETS2, FPS và phần cứng. |
| `esp32_get_telemetry` | Đọc toàn bộ luồng telemetry thời gian thực: tốc độ, số, RPM, xăng lít & %, độ bền, GPS, đèn... |
| `esp32_get_logs` | Lấy các dòng log mới nhất từ `tools/daemon.log` (theo dõi sự kiện phạt tiền, thu phí, va chạm). |
| `esp32_send_command` | Gửi lệnh chuyển chế độ trực tiếp tới ESP32 (`MODE=GAME`, `MODE=EYES`, `MODE=CLOCK`, `MODE=OFF`). |
| `esp32_trigger_event` | Bắn popup thông báo tùy chỉnh tức thời lên màn hình OLED của robot. |

### 3. Kiểm Tra MCP Server Qua Dòng Lệnh
```bash
python tools/esp32_mcp_server.py --test
```

---

## 📁 Cấu Trúc Thư Mục Dự Án

```
E:\MIU_EILIK_ESP32-S3\
├── README.md                     ← Tài liệu hướng dẫn toàn diện dự án
├── platformio.ini                ← Cấu hình build PlatformIO cho ESP32-S3 (N16R8)
├── partitions_16mb.csv           ← Bảng phân vùng Flash 16MB
│
├── scripts\                      ← Bộ script tiện ích Windows
│   ├── BAT_TU_DONG_CHAY_NGAM.bat ← Kích hoạt tự động chạy ngầm cùng Windows boot
│   ├── TAT_TU_DONG_CHAY_NGAM.bat ← Dừng và gỡ bỏ chế độ chạy ngầm
│   ├── CHAY_DASHBOARD_ETS2.bat   ← Chạy companion thủ công có console theo dõi
│   └── CHAY_DEMO_THU_NGHIEM.bat  ← Chạy mô phỏng demo HUD không cần game
│
├── miu_eilik\                    ← Mã nguồn C++ Firmware ESP32-S3
│   ├── miu_eilik.ino             ← Setup FreeRTOS 2 nhân & vòng lặp chính
│   ├── config.h                  ← Cấu hình thông số hệ thống
│   ├── board_config.h            ← Định nghĩa chân I2C (SDA=8, SCL=9)
│   ├── display_controller.h/.cpp ← Full Taplo ETS2, Fonts, RPM Bar, Biển báo, Icons
│   ├── usb_protocol.h/.cpp       ← Bộ giải mã giao thức JSON từ PC (115200 baud)
│   ├── emoji_controller.h/.cpp   ← Quản lý mắt robot RoboEyes
│   └── FluxGarage_RoboEyes.h     ← Thư viện đồ họa mắt robot chuyển động
│
├── tools\                        ← Bộ công cụ Python Companion & MCP Server
│   ├── miu_desktop_ets2.py       ← Cầu nối Telemetry Windows Shared Memory -> ESP32
│   ├── ets2_open_api.py          ← Module Open Telemetry chuẩn SCS SDK độc lập
│   ├── mock_telemetry.json       ← Dữ liệu telemetry mô phỏng kiểm thử độc lập
│   ├── esp32_mcp_server.py       ← Server MCP JSON-RPC 2.0 stdio cho AI IDE
│   ├── check_shm.py              ← Công cụ kiểm tra Shared Memory nhanh
│   └── setup_autostart.py        ← Cài đặt tự khởi động registry Windows
│
└── docs\                         ← Tài liệu kiến trúc & lộ trình
    └── roadmap.md                ← Lộ trình phát triển và quy hoạch tính năng
```

---

## ❓ Câu Hỏi Thường Gặp (Troubleshooting)

1. **Tại sao tôi vào game ETS2 mà màn hình robot vẫn ở mắt robot?**
   - Đảm bảo bạn đã cài đặt plugin Telemetry của ETS2 (file `ets2-telemetry-server.dll` trong thư mục `bin\win_x64\plugins\` của game).
   - Kiểm tra xem daemon đang chạy hay chưa bằng lệnh: `python tools/esp32_mcp_server.py --test`. Nếu daemon đang tắt, chỉ cần nhấp đúp vào `scripts/BAT_TU_DONG_CHAY_NGAM.bat`.

2. **Khi rút cáp USB cắm lại, robot có nhận lại không?**
   - Hoàn toàn tự động! Companion có cơ chế auto-reconnect mỗi 1 giây. Khi bạn cắm lại cáp USB, daemon sẽ tự động bắt lại cổng COM5 và tiếp tục truyền dữ liệu bình thường.

3. **Màn hình OLED bị tối hoặc không lên?**
   - Kiểm tra chân cắm SDA (GPIO 8) và SCL (GPIO 9).
   - Mở Serial Monitor ở 115200 baud khi khởi động để xem log bộ quét I2C tự động tìm địa chỉ `0x3C` hoặc `0x3D`.

---

**Phát triển bởi ROSHINGUYEN — Miu Eilik Telemetry Ecosystem (2026)**
