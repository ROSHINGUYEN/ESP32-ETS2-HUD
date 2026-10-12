# MIU EILIK - ESP32-S3 DIGITAL COCKPIT & TELEMETRY COMPANION

> **Hệ thống Taplo Buồng Lái Kỹ Thuật Số (Digital Cockpit)** và **Trợ Lý Biểu Cảm (RoboEyes)** dành cho **Euro Truck Simulator 2 (ETS2)** & **American Truck Simulator (ATS)** trên vi điều khiển **ESP32-S3-WROOM-1 (N16R8)** với màn hình hiển thị OLED 128x64.

---

## 🏷️ Danh Mục Game & Hệ Thống (Game Tags & Topics)

### Game & Thể Loại Mô Phỏng
`#EuroTruckSimulator2` `#AmericanTruckSimulator` `#ETS2` `#ATS` `#SCSSoftware` `#SimRacing` `#TruckSimulator` `#SCSModding` `#SimulationGames`

### Công Nghệ & Phần Cứng
`#ESP32S3` `#SSD1306` `#SH1106` `#OLED128x64` `#FreeRTOS` `#DualCore` `#DigitalCockpit` `#TelemetryDashboard` `#SimRigHardware` `#ModelContextProtocol` `#PlatformIO`

---

## 📸 Thiết Kế Giao Diện Buồng Lái (Digital Cockpit Layout)

Màn hình OLED 128x64 được thiết kế theo tỷ lệ phân chia 3 cột công thái học (Ergonomic 3-Column Layout), tối ưu hóa góc nhìn của tài xế khi gắn trên Vô-lăng hoặc Sim Rig:

```
┌─────────────────────────────────────────────────────────────┐
│  <  D  WIP [2]     14:25           R2  CC [85]  >  │  Hàng 1: Xi-nhan | Đèn | Gạt mưa | Giờ | Retarder | CC
├─────────────────────────────────────────────────────────────┤
│   [80]                 85                [ D12 ]   │  Hàng 2: Biển báo tốc độ (Chớp) | Tốc độ số to nét
│   C:70             ── km/h ──             [ P ]    │  Hàng 3: Cảnh báo camera | Vạch phân cách | Phanh tay
├─────────────────────────────────────────────────────────────┤
│  F: [■■■■■■■■□□] 78%       R: [■■■■■|■■|  ] 1350   │  Hàng 4: Thước Xăng % & Thước RPM dải kinh tế xanh
│  T: 2%        C: 0%        778L           140km    │  Hàng 5: Độ bền xe | Hàng hóa | Lít thực | Cự ly ETA
└─────────────────────────────────────────────────────────────┘
```

### Các Khối Hiển Thị Trực Quan
- **Khu vực Trung tâm (Tốc độ km/h)**: Phông số kích thước lớn (Font 3: 21x16 pixel), đường nét tương phản cao, căn giữa tuyệt đối, giúp nhận diện vận tốc tức thì.
- **Khu vực Cột phải (Hộp số buồng lái & Trạng thái phanh)**:
  - Khung hộp số bo góc (Rounded Box) hiển thị chính xác cấp số hiển thị taplo: Số tiến (`D1` .. `D12`), Số bò địa hình (`C1`, `C2` cho hộp số Scania/Volvo 14 cấp), Số Mo (`N`), Số lùi đa cấp (`R1`, `R2`).
  - Biểu tượng phanh cơ khí / phanh đỗ xe `[ P ]`.
- **Khu vực Cột trái (Biển báo & Giới hạn tốc độ Châu Âu)**:
  - Khung biển báo tròn `[80]`, `[90]`: Tự động nhận diện tốc độ quy định của cung đường hiện tại. Tự động chớp nháy tương phản ngược (chữ trắng nền đen) khi xe chạy vượt quá tốc độ quy định.
  - Vận tốc đã cài đặt cho hệ thống ga tự động (`C:70`).
- **Khu vực Thước đo Kép (Dual Bar Gauges)**:
  - **Thước Nhiên Liệu (Fuel Gauge)**: Thanh đo 10 nấc kèm tỷ lệ phần trăm và số lít dầu thực tế (`778L`). Tự động chớp nháy cảnh báo khi nhiên liệu xuống dưới 15%.
  - **Thước Vòng Tua Máy (RPM Bar)**: Mô phỏng vòng tua máy thời gian thực với vạch căn dải tua kinh tế (Green Economy Band: 1000 - 1500 RPM) của các dòng động cơ xe tải nặng.
- **Khu vực Thống kê Độ Bền & Hành Trình (Trip Advisor)**:
  - `T:%` (Truck Wear): Tỷ lệ hao mòn tổng thể đầu kéo xe (Động cơ, hộp số, cabin, khung gầm, lốp xe).
  - `C:%` (Cargo Wear): Tỷ lệ tổn hại hàng hóa và rơ-moóc.
  - `ETA`: Cự ly còn lại đến điểm giao hàng (km).

---

## 🔔 Hệ Thống Đèn Báo & Thông Báo Buồng Lái (Cockpit Indicators & Alerts)

| Ký Hiệu / Nhãn | Chức Năng | Hành Vi & Trạng Thái |
|---|---|---|
| `<` / `>` | Đèn xi-nhan Trái / Phải | Chớp nháy đồng bộ 1Hz theo rơ-le xi-nhan trong game |
| `D` | Đèn chiếu sáng | Đèn sương mù, đèn pha cốt, đèn chiếu xa |
| `WIP [1..4]` | Cần gạt nước mưa | Hiển thị trạng thái bật và cấp độ gạt mưa (Cấp 1 đến 4) |
| `HORN` | Còi xe buồng lái | Hiển thị biểu tượng khi tài xế bấm còi/kèn hơi |
| `R [1..5]` | Phanh hãm phụ (Retarder) | Hiển thị cấp độ phanh thủy lực Retarder từ nấc 1 đến nấc 5 |
| `CC [km/h]` | Hệ thống ga tự động (Cruise Control) | Hiển thị khi kích hoạt kèm vận tốc ga tự động đã lưu |
| `[ P ]` | Phanh dừng đỗ (Parking Brake) | Bật sáng khi phanh tay được kéo |
| `AIR` | Phanh khí nén khẩn cấp | Cảnh báo khi áp suất bình hơi giảm thấp nguy hiểm |
| `ENG` | Đèn kiểm tra động cơ | Bật sáng khi động cơ bị hỏng hóc hoặc chết máy |
| `TOLL: -45 EUR` | Thông báo qua trạm thu phí | Hiển thị Popup Toast tự động biến mất sau 3 giây |
| `FINE: -550 EUR`| Cảnh báo vi phạm giao thông / bắn tốc độ | Hiển thị Popup Toast tự động biến mất sau 3 giây |
| `CRASH: -400 EUR`| Cảnh báo va chạm / tai nạn | Hiển thị Popup Toast tự động biến mất sau 3 giây |

---

## 📦 Kê Khai Thư Viện Bên Thứ Ba (Third-Party Libraries & Dependencies)

Dự án cam kết minh bạch về việc sử dụng các thành phần mã nguồn mở và bộ công cụ của bên thứ ba:

### 1. Phân Hệ Nhúng Vi Điều Khiển (ESP32-S3 Firmware)

| Tên Thư Viện | Phiên Bản | Tác Giả / Tổ Chức | Giấy Phép | Vai Trò & Mục Đích Trong Hệ Thống |
|---|---|---|---|---|
| **Adafruit SSD1306** | `^2.5.7` | Adafruit Industries | BSD 3-Clause | Trình điều khiển giao tiếp phần cứng I2C cho màn hình OLED 128x64 (SSD1306 / SH1106). |
| **Adafruit GFX Library**| `^1.11.9` | Adafruit Industries | BSD 2-Clause | Thư viện lõi đồ họa: vẽ điểm, đường thẳng, khung chữ nhật bo góc, icon bitmap nhị phân. |
| **ArduinoJson** | `^6.21.4` | Benoit Blanchon | MIT | Bộ giải mã (parser) và đóng gói chuỗi JSON tốc độ cao từ kênh Serial USB CDC. |
| **FluxGarage RoboEyes** | `v1.0.0 (Customized)` | Dennis Bueschel (FluxGarage) | MIT | Hệ thống diễn hoạt mắt biểu cảm robot đa trạng thái (Vui, Buồn, Mệt, Tức giận, Ngủ, Liếc nhìn). |
| **Espressif Arduino Core** | `~6.5.0 (IDF v4.4+)` | Espressif Systems | Apache 2.0 | Bộ công cụ phát triển phần cứng ESP32-S3, trình điều khiển USB CDC OTG, I2C và NVS. |
| **FreeRTOS Kernel** | `SMP v10.x` | Amazon Web Services / FreeRTOS | MIT | Hệ điều hành đa nhiệm thời gian thực, điều phối 2 nhân vi xử lý (Core 0 nhận dữ liệu, Core 1 xuất đồ họa). |

### 2. Phân Hệ Máy Tính Cầu Nối (PC Companion & Automation)

| Tên Thư Viện | Phiên Bản | Tác Giả / Nguồn | Giấy Phép | Vai Trò & Mục Đích Trong Hệ Thống |
|---|---|---|---|---|
| **pyserial** | `>=3.5` | Chris Liechti | BSD 3-Clause | Kênh giao tiếp truyền thông nối tiếp tốc độ cao (Baud Rate 460800) giữa Windows PC và ESP32-S3. |
| **psutil** | `>=5.9.0` | Jay Loden, Giampaolo Rodola | BSD 3-Clause | Giám sát trạng thái tiến trình game (`eurotrucks2.exe` / `amtrucks.exe`) để tự động chuyển cảnh. |
| **requests** | `>=2.28.0` | Kenneth Reitz & PSF | Apache 2.0 | Giao thức HTTP REST Client dự phòng để kết nối tới cổng Telemetry Server WebApi (Port 25555). |

### 3. Phân Hệ Trích Xuất Dữ Liệu Game (Game Telemetry Engines & Plugins)

| Thành Phần | Phiên Bản | Tác Giả / Nhà Cung Cấp | Giấy Phép | Vai Trò & Mục Đích Trong Hệ Thống |
|---|---|---|---|---|
| **RenCloud SCS SDK Plugin** | `v1.1.0+` | RenCloud / nlhans | MIT | Plugin C++ (`scs-telemetry.dll`) gắn trực tiếp vào game engine SCS, tạo bộ nhớ chia sẻ `Local\SCSTelemetry`. |
| **Funbit ETS2 Telemetry Server**| `v4.0.0` | Funbit / Denilson Sá Maier | MIT | Máy chủ Telemetry (`Ets2Telemetry.exe`) cung cấp bộ nhớ chia sẻ `Local\Ets2TelemetryServer` và REST API. |
| **Microsoft OWIN / Katana** | `v3.0.0` | Microsoft Corporation | Apache 2.0 | Nền tảng máy chủ HTTP tự lưu trữ (Self-Hosted HTTP Web Server) cho Telemetry Server. |
| **Microsoft SignalR Core** | `v2.x` | Microsoft Corporation | Apache 2.0 | Kênh truyền thông thời gian thực hai chiều dựa trên công nghệ WebSocket / Long Polling. |
| **Newtonsoft.Json** | `v12.0+` | James Newton-King | MIT | Bộ tuần tự hóa và giải mã dữ liệu JSON cho nền tảng .NET Framework. |
| **log4net** | `v2.x` | Apache Software Foundation | Apache 2.0 | Phân hệ ghi vết và quản lý tệp nhật ký hệ thống của Telemetry Server. |

---

## ⚡ Kiến Trúc Luồng Dữ Liệu & Hiệu Năng (Architecture & Data Flow)

Hệ thống hoạt động theo nguyên lý kênh truyền phân tầng khép kín, triệt tiêu độ trễ hiển thị:

```
┌─────────────────────────────────┐
│ Euro Truck Simulator 2 / ATS   │
│ (SCS Software Game Engine)     │
└──────────────┬──────────────────┘
               │  Bộ nhớ dùng chung Windows (Shared Memory)
               ▼  Độ trễ < 0.5ms (Không qua lớp mạng IP)
┌─────────────────────────────────┐
│ Windows Shared Memory Block     │
│ ("Local\SCSTelemetry")          │
└──────────────┬──────────────────┘
               │  Tần số đọc: 30 Lần/giây
               ▼  Thuật toán bù trừ hộp số Crawler (C1, C2)
┌─────────────────────────────────┐
│ PC Companion Daemon             │
│ (tools/miu_desktop_ets2.py)     │
└──────────────┬──────────────────┘
               │  USB CDC Serial tốc độ cực cao: 460,800 Baud
               ▼  Gói tin JSON rút gọn tối ưu hóa băng thông
┌─────────────────────────────────┐
│ ESP32-S3 Core 0 (Task USB)      │ ──► Giải mã JSON, cập nhật Struct dữ liệu
└──────────────┬──────────────────┘
               │  Bộ đệm chia sẻ an toàn luồng FreeRTOS
               ▼
┌─────────────────────────────────┐
│ ESP32-S3 Core 1 (Task Display)  │ ──► Màn hình OLED I2C (29.3 - 30.0 FPS mượt mà)
└─────────────────────────────────┘
```

### Chỉ Số Tối Ưu Hóa Hệ Thống
- **Tốc độ Baud Rate**: **460,800 baud** (Nhanh gấp 4 lần chuẩn 115200 truyền thống, triệt tiêu hoàn toàn hiện tượng trễ nhịp nhảy số).
- **Tốc độ khung hình (Frame Rate)**: **29.3 – 30.0 FPS** ổn định liên tục.
- **Thời gian xử lý vòng lặp (Cycle Latency)**: **< 5 mili-giây**.
- **Mức tiêu hao CPU Máy tính**: **< 0.1%** nhờ cơ chế truy xuất trực tiếp con trỏ bộ nhớ dùng chung.
- **Tài nguyên vi điều khiển ESP32-S3**:
  - Dung lượng Flash: **~394 KB / 4.6 MB** (Chiếm ~8.4% phân vùng ứng dụng).
  - Dung lượng RAM: **~25.2 KB / 327 KB** (Chiếm ~7.7% SRAM nội, không rò rỉ bộ nhớ).

---

## 🔌 Sơ Đồ Đấu Nối Phần Cứng (Hardware Wiring)

Hệ thống được thiết kế tối ưu cho bo mạch **ESP32-S3-WROOM-1 (N16R8: 16MB Flash, 8MB Octal PSRAM)**:

| Chân ESP32-S3 | Chân Màn Hình OLED | Chức Năng |
|---|---|---|
| **GPIO 8 (D8)** | **SDA** | Tuyến dữ liệu I2C Data |
| **GPIO 9 (D9)** | **SCL** | Tuyến xung đồng hồ I2C Clock |
| **3V3** | **VCC** | Nguồn cấp điện áp 3.3V ổn định |
| **GND** | **GND** | Chân nối đất (Mass chung) |

*Ghi chú*: Firmware tích hợp tính năng **Auto-Scan I2C**, tự động dò tìm địa chỉ màn hình (`0x3C` hoặc `0x3D`) trên đường truyền.

---

## 🚀 Hướng Dẫn Vận Hành (Operating Guide)

### 1. Cài Đặt Trọn Gói Tự Động Chạy Cùng Windows (Khuyên Dùng)
Chỉ cần thực hiện một lần duy nhất, hệ thống sẽ tự động hóa hoàn toàn mà không cần mở lại mỗi lần bật máy:
- Nhấp đúp chuột vào tập tin: `scripts/BAT_TU_DONG_CHAY_NGAM.bat`
- Hệ thống sẽ kích hoạt tiến trình ngầm tàng hình (0-Touch Background Daemon).
- Khi bạn mở ETS2 hoặc ATS: Màn hình tự động hiển thị Taplo Kỹ thuật số.
- Khi bạn tắt game: Màn hình tự động chuyển về Mắt Robot Biểu cảm.

### 2. Dừng Chế Độ Chạy Tự Động
- Nhấp đúp chuột vào tập tin: `scripts/TAT_TU_DONG_CHAY_NGAM.bat`

### 3. Vận Hành Thủ Công Có Bảng Điều Khiển
- Nhấp đúp chuột vào tập tin: `scripts/CHAY_DASHBOARD_ETS2.bat`
- Màn hình Console sẽ hiển thị dòng trạng thái đo đạc thời gian thực.

### 4. Kiểm Thử Trực Quan Không Cần Mở Game
- Nhấp đúp chuột vào tập tin: `scripts/CHAY_DEMO_THU_NGHIEM.bat`
- Bộ phát dữ liệu mô phỏng sẽ phát luồng dữ liệu 30 FPS để kiểm tra tất cả biểu tượng và thanh đo trên màn hình OLED.

---

## 🤖 Cổng Kết Nối AI IDE (Model Context Protocol - MCP)

Dự án cung cấp máy chủ chuẩn **Model Context Protocol (MCP)** tại `tools/esp32_mcp_server.py`, cho phép các trợ lý AI (Google Antigravity, Claude Desktop, Cursor) kết nối và tương tác trực tiếp với thiết bị phần cứng:

### Danh Mục Công Cụ AI Điều Khiển (MCP Tools)
- `esp32_get_status`: Kiểm tra trạng thái cổng USB COM, tốc độ truyền nhận FPS và tình trạng game.
- `esp32_get_telemetry`: Đọc toàn bộ gói dữ liệu buồng lái tức thời (Vận tốc, Cấp số, RPM, Xăng, Hư hại, Đèn).
- `esp32_get_logs`: Trích xuất nhật ký hành trình, sự kiện phạt tiền, trạm thu phí và va chạm giao thông.
- `esp32_send_command`: Ra lệnh chuyển đổi trạng thái hiển thị trên thiết bị (`GAME`, `EYES`, `CLOCK`, `OFF`).
- `esp32_trigger_event`: Bắn thông báo tùy biến tức thời lên màn hình OLED của buồng lái.

---

## 📂 Danh Mục Thư Mục Dự Án (Repository Structure)

```
E:\MIU_EILIK_ESP32-S3\
├── API-ETS2\                     ← Bộ máy chủ Telemetry Server & Thư viện liên kết game
│   └── server\                   ← Các tệp thực thi Ets2Telemetry.exe và thư viện kết nối
├── miu_eilik\                    ← Mã nguồn C++ Firmware nhúng trên ESP32-S3
│   ├── miu_eilik.ino             ← Điểm khởi chạy hệ thống & bộ điều phối FreeRTOS 2 nhân
│   ├── display_controller.cpp    ← Bộ dựng đồ họa Taplo ETS2, Fonts chữ to và Icons
│   ├── usb_protocol.cpp          ← Bộ giải mã gói tin dữ liệu tốc độ cao 460800 baud
│   └── emoji_controller.cpp      ← Bộ điều khiển diễn hoạt mắt robot biểu cảm
├── tools\                        ← Phân hệ phần mềm cầu nối máy tính & Cổng AI MCP
│   ├── miu_desktop_ets2.py       ← Daemon cầu nối đọc Shared Memory và truyền qua Serial
│   ├── esp32_mcp_server.py       ← Máy chủ chuẩn MCP dành cho các trợ lý AI IDE
│   └── mock_telemetry.json       ← Tệp dữ liệu mô phỏng kiểm thử độc lập
├── scripts\                      ← Bộ kịch bản thao tác nhanh cho hệ điều hành Windows
├── docs\                         ← Tài liệu kiến trúc và kế hoạch phát triển hệ thống
├── platformio.ini                ← Tệp cấu hình biên dịch nhúng PlatformIO
└── README.md                     ← Tài liệu hướng dẫn kỹ thuật toàn diện
```

---

## ⚖️ Bản Quyền & Tuyên Bố Miễn Trừ (License & Disclaimer)

- Dự án này được phát triển độc lập cho mục đích học tập, mô phỏng và phục vụ cộng đồng đam mê mô phỏng lái xe (Sim Racing Community).
- Euro Truck Simulator 2 và American Truck Simulator là nhãn hiệu đã đăng ký của SCS Software s.r.o.
- Mọi thư viện của bên thứ ba được giữ nguyên bản quyền thuộc về các tác giả gốc tương ứng theo giấy phép mã nguồn mở quy định.

---
**Tác giả & Phát triển: ROSHINGUYEN — Hệ Sinh Thái Taplo Telemetry Thông Minh Miu Eilik (2026)**
