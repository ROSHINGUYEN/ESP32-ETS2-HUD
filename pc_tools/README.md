# Eilik PC Bridge Tools

Công cụ giao tiếp PC dành cho Robot Miu Eilik qua cổng USB Native CDC (GPIO 19/20).

## Cài đặt thư viện Python (3.10+)
```bash
pip install -r requirements.txt
```

## Hướng dẫn sử dụng `eilik_bridge.py`:
1. **Gửi lệnh JSON điều khiển cảm xúc/đầu:**
   ```bash
   python eilik_bridge.py --cmd "{\"emotion\":1,\"pan\":90,\"tilt\":90}"
   ```
2. **Gửi ảnh lên màn hình OLED (tự động dither 128x64):**
   ```bash
   python eilik_bridge.py --image "sample.png"
   ```
3. **Lắng nghe dữ liệu Telemetry (FPS, Heap, Trạng thái):**
   ```bash
   python eilik_bridge.py
   ```

## Bộ công cụ Nạp Code & Gỡ lỗi Trực tiếp trên Web (`miu_web_flasher.html`)
Mở file `miu_web_flasher.html` bằng Google Chrome hoặc Microsoft Edge để:
1. **Nạp Firmware trực tiếp (Web Serial Flasher):** Chọn file `.bin` và nạp thẳng vào ESP32-S3 không cần cài đặt Arduino IDE.
2. **Nạp không dây (Wi-Fi OTA):** Nạp firmware qua sóng Wi-Fi của Robot (`EILIK_ROBOT`).
3. **Trình xem Log & Phân tích lỗi (Real-time Diagnostic):** Tự động phát hiện lỗi sụt áp (Brownout), treo luồng (Watchdog), và vấp bộ đệm I2S với hướng dẫn khắc phục tức thì.

## Web Serial Controller (`web_bridge.html`)
Mở file `web_bridge.html` trong trình duyệt Chrome/Edge để kết nối điều khiển trực tiếp robot từ trình duyệt không cần cài đặt Python.

