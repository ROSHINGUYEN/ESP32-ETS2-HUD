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

## Web Serial Tool (`web_bridge.html`)
Mở file `web_bridge.html` trong trình duyệt Chrome/Edge qua giao thức `http://localhost` hoặc `https` để kết nối điều khiển trực tiếp robot từ trình duyệt không cần cài đặt Python.
