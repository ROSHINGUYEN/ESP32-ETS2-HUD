=====================================================================================
PROMPT: ROBOT EILIK ĐẦU PAN/TILT + MẮT OLED + NHẠC + USB/BLUETOOTH (ESP32-S3 N16R8)
=====================================================================================

1. VAI TRÒ
Bạn là kỹ sư firmware ESP32-S3 (Arduino framework, FreeRTOS) giàu kinh nghiệm về robot
biểu cảm, điều khiển servo mượt, âm thanh I2S, xử lý tín hiệu và UI nhúng. Bạn cũng viết
được công cụ phía PC (Python + Web Serial). Viết code đầy đủ, chạy được, comment tiếng
Việt chi tiết theo phong cách mã nguồn gốc (mochan.ino).

2. BỐI CẢNH VÀ MỤC TIÊU
Dự án gốc (mochan.ino): robot để bàn kiểu Eilik dùng 2 bánh DC (TB6612), OLED SSD1306,
thư viện RoboEyes, WiFi SoftAP "mochan" và captive portal.
VIẾT LẠI HOÀN TOÀN thành robot ngồi yên có biểu cảm:
 - Bỏ bánh xe. Dùng 2 servo SG90: servo 1 là CỔ (quay trái-phải), servo 2 là ĐẦU (gật lên-xuống).
 - Mắt OLED biểu cảm đồng bộ với chuyển động đầu.
 - Cảm biến hồng ngoại LM393 để ra lệnh bằng tay (chạm/giữ/vẫy tay gần robot).
 - Loa MAX98357A: phát nhạc, hiệu ứng âm thanh (SFX).
 - Mắt "sóng nhạc" và đầu nhún theo nhịp khi có nhạc.
 - Nhận âm thanh và hình ảnh từ máy tính qua USB; Bluetooth qua module ngoài.
 - Tự hành có cá tính, điều khiển qua web (captive portal).

Các lỗi của mã gốc PHẢI tránh:
 a) MOTOR() dùng delay() dài làm đứng mắt/web/DNS.
 b) manualActive đặt true rồi false trong cùng handler nên cờ vô tác dụng.
 c) Chưa Wire.setClock; yêu cầu 100 FPS là phi thực tế trên I2C.
 d) Dùng GPIO strapping 0 và 3.
 e) WiFi mở, không mật khẩu.

3. PHẦN CỨNG VÀ BẢN ĐỒ CHÂN
Board: ESP32-S3 N16R8 (Flash 16MB, PSRAM OPI 8MB), Arduino-ESP32 core 3.x.

 OLED SSD1306 128x64 I2C 0x3C        : SDA=GPIO8, SCL=GPIO9
 Servo PAN (cổ) SG90                  : GPIO4   (xung 500-2400us, 50Hz)
 Servo TILT (đầu) SG90                : GPIO5
 IR LM393 chính (cấp 3.3V, OUT LOW)   : GPIO6
 IR LM393 thứ 2 (tuỳ chọn #define)    : GPIO7
 MAX98357A I2S                        : BCLK=15, LRC(WS)=16, DIN=17, SD(mute)=18
 Mux 74LVC157 SEL (chọn nguồn amp)    : GPIO10
 Module BT I2S nghe lén (I2S1 RX)     : BCLK_IN=11, WS_IN=12, DATA_IN=13
 BT status (tuỳ chọn)                 : GPIO14
 USB native (CDC dữ liệu)             : GPIO19(D-), GPIO20(D+)  — dành riêng
 Log debug Serial0 (cổng UART)        : GPIO43/44 — dành riêng, không dùng làm GPIO
 TRÁNH: GPIO 0,3,45,46 (strapping), 26-37 (flash/PSRAM).

Giới hạn cơ khí trong config.h: PAN 40..140, TILT 70..115, giữa=90 (có thể hiệu chỉnh).

NGUỒN ĐIỆN:
 - Rail công suất (servo + MAX98357A) lấy THẲNG từ nguồn ngoài 5V/3A trở lên, KHÔNG đi qua
   chân 5V của board. Chung GND với ESP32. Tụ 1000uF+100nF ở nguồn servo, 470uF+100nF
   sát VIN của MAX98357A.
 - ESP32 cấp qua diode OR (Schottky) giữa nguồn ngoài và USB. Ghi rõ cảnh báo dòng ngược vào
   cổng USB của PC (kiểm tra board có diode chặn VBUS không; nếu không, dùng cáp cắt dây VBUS).
 - IR LM393 CHỈ cấp 3.3V (cấp 5V thì OUT 5V làm hỏng GPIO).
 - Âm lượng phần mềm có trần mặc định 40% (cấu hình được) để tránh brownout.

4. THƯ VIỆN (ghi rõ phiên bản đã dùng; chỉ dùng API đúng phiên bản đó)
 - FluxGarage RoboEyes v1.1.2 (cài từ Library Manager; xoá bản header copy trong sketch).
   Không sửa thư viện. #include thư viện này SAU CÙNG vì macro N,E,S,W,ON,OFF,DEFAULT.
 - Adafruit SSD1306 + Adafruit GFX, ESP32Servo
 - ESP32-audioI2S (schreibfaul1) - API thay đổi nhiều giữa các phiên bản, ghi rõ bản dùng
 - ESPAsyncWebServer + AsyncTCP (bản ESP32Async), ArduinoJson, arduinoFFT (hoặc Goertzel)
 - LittleFS, Preferences (NVS), ESP_I2S (core 3.x)

5. KIẾN TRÚC VÀ RÀNG BUỘC BẮT BUỘC
 1. KHÔNG delay() ngoài setup(). Mọi thứ dùng millis()/FreeRTOS.
 2. Task: ServoTask (core0, 50Hz, ưu tiên cao), AudioTask (theo thư viện audio; nếu thư viện
    tự tạo task thì không tạo thêm), DisplayTask (chủ DUY NHẤT của OLED), BrainTask (50Hz:
    cảm xúc, tự hành, IR gesture, máy trạng thái), PcLink (parser USB-CDC).
    Web handler async chỉ đẩy lệnh vào Queue, không xử lý nặng. Đề xuất và giải thích phân
    bổ core/ưu tiên (mặc định: audio+display core1, servo+WiFi core0).
 3. Giao tiếp giữa task bằng FreeRTOS Queue hoặc struct nguyên tử (portMUX/atomic).
 4. Servo: giới hạn tốc độ (độ/giây) + easing (LUT smoothstep), clamp theo soft-limit; khởi
    động về giữa từ từ; không cho 2 servo chạy tốc độ tối đa cùng lúc khi boot; tự detach
    sau 1500ms nghỉ (cờ HOLD_TORQUE_TILT để giữ tilt); offset hiệu chỉnh lưu NVS.
 5. OLED: Wire.setClock thử 400k/800k/1M qua hằng số; FPS giới hạn 30 (15 khi SLEEP);
    roboEyes.update() chỉ gọi ở chế độ EYES.
 6. Hạn chế String; buffer lớn đặt PSRAM; WiFi SoftAP có mật khẩu + captive portal; công suất
    phát WiFi thấp; tối đa 3 client; HTML đặt ở LittleFS.
 7. Giám sát mỗi 10s qua Serial0: FPS mắt, loop time max, heap/min heap, stack high-water mark
    của từng task, số underrun audio.
 8. Bảng phân vùng 16MB tuỳ chỉnh (partitions.csv): app (có thể OTA) + LittleFS >= 9MB.
    Bật PSRAM OPI.

6. TÍNH NĂNG

A. ĐẦU VÀ CỬ CHỈ (keyframe {pan, tilt, ms, easing}):
   gật yes (3 nhịp), lắc no (3 nhịp), nhìn trái/phải/lên/xuống, nghiêng tò mò, cúi ngủ chậm,
   thức dậy (ngẩng + vươn), nhún vui (gật nhanh 2 nhịp), lắc giận (1 lần rồi nhìn thẳng),
   thở (tilt sin ±1.5 độ, chu kỳ 4s).

B. CẢM XÚC = mood mắt + animation mắt + cử chỉ đầu:
   NEUTRAL, HAPPY (HAPPY + anim_laugh), ANGRY, SLEEPY (TIRED, tắt autoblink),
   CURIOUS (setCuriosity), CONFUSED (anim_confused + lắc no), WAKE_UP.

C. ĐỒNG BỘ MẮT-ĐẦU: dùng roboEyes.setPosition() (lưới 3x3) ánh xạ sang pan/tilt:
   cột W/giữa/E = -25/0/+25 độ; hàng N/giữa/S = +12/0/-12 độ. Mắt đổi hướng trước, đầu bám
   theo trễ 150-250ms (hằng số trong config.h).

D. TỰ HÀNH: SLEEP, CALM (nhìn quanh 10-20s/lần), CURIOUS, PLAYFUL, MUSIC (nhảy theo nhạc).
   Chọn hành vi theo trọng số, có cooldown tránh lặp. Lệnh tay (web/IR) được ưu tiên 8s bằng
   timestamp manualUntil (KHÔNG dùng cờ bool bật/tắt trong cùng handler).

E. ÂM THANH ĐA NGUỒN VỚI AudioRouter
   Nguồn: FILE (LittleFS + ESP32-audioI2S, tạo/huỷ động đối tượng Audio khi đổi nguồn),
   USB_PC (USB-CDC PCM), BT_EXT (module BT ngoài), WIFI_STREAM (tuỳ chọn, dùng lại giao thức
   USB qua TCP).
   - Chỉ một nguồn phát tại một thời điểm. Đổi nguồn: fade-out -> mute chân SD amp (GPIO18)
     -> dừng engine cũ -> init engine mới -> fade-in (chống tiếng bụp).
   - Chính sách AUTO: ưu tiên USB > BT > FILE; nguồn "hoạt động" khi tín hiệu vượt ngưỡng
     liên tục 0.5s; quay lại nguồn trước khi im lặng quá 5s. Có thể ép tay qua web/IR.
   - Nếu ESP32-audioI2S không giải phóng I2S sạch khi huỷ đối tượng thì DỪNG và báo lại kèm
     phương án thay thế (ví dụ I2S_NUM_1 + mux), không tự ý đổi kiến trúc.
   - Phát nhạc FILE: MP3/WAV từ LittleFS; play, pause, next, prev, shuffle, repeat, volume,
     hẹn giờ ngủ, danh sách bài, upload/xoá/sắp xếp qua web. Khuyến nghị MP3 128kbps 44.1kHz.
   - SFX ngắn (chime, beep) cho Happy/Angry/Wake; KHÔNG phát khi đang có nhạc từ bất kỳ nguồn
     nào (chỉ phản hồi bằng hình).

F. ANALYZER (dùng chung mọi nguồn, <1ms/khối): RMS trái/phải, 8 băng tần log (FFT 256 hoặc
   Goertzel), phát hiện beat (bass so với trung bình trượt, ngưỡng ~1.3x, cooldown ~250ms),
   ước lượng BPM thô. Kết quả ghi vào struct nguyên tử cho DisplayTask và BrainTask.
   Điểm lấy mẫu: hook xử lý mẫu của thư viện audio (FILE), bộ ghi I2S tự viết (USB),
   I2S1 RX slave (BT).

G. MẮT SÓNG NHẠC, 3 kiểu chuyển đổi được (lưu NVS):
   (A) Beat Eyes: RoboEyes mood HAPPY, tắt idle, setHeight/setWidth phồng theo bass.
   (B) Wave Eyes: 2 khung mắt bo góc, bên trong là oscilloscope trái/phải.
   (C) Bars Eyes: mỗi mắt 4 cột equalizer, đỉnh rơi chậm kiểu VU.
   Chuyển cảnh EYES -> VISUAL: close() mắt, đổi renderer khi mắt gần khép, mở bằng hiệu ứng
   cột mọc lên; khi dừng nhạc làm ngược lại. DisplayTask có 3 chế độ: EYES, VISUAL, STREAM.

H. ĐẦU THEO NHẠC: nhún tilt ±4..6 độ theo beat (BPM>120 thì nhún 1 trong 2 nhịp), pan đung
   đưa chậm ±10 độ; giới hạn tốc độ để servo không ồn lấn nhạc.

I. IR LM393 (ngắt cạnh + debounce, đo thời gian che). Phân loại: chạm nhanh (<400ms), chạm
   đúp (<600ms giữa 2 lần), chạm ba, giữ (>=800ms), giữ rất lâu (>=2.5s), vẫy (>=3 chuyển
   trạng thái trong 1s). Ngưỡng cấu hình được qua web.
   - Bình thường: chạm = nhìn về tay + HAPPY; đúp = đổi cảm xúc; ba = bật nhạc; giữ = vuốt ve
     (HAPPY, nheo mắt, cúi đầu vào); giữ rất lâu = ngủ/thức; vẫy = CONFUSED.
   - Đang phát nhạc: chạm = pause/resume; đúp = bài kế; ba = bài trước; giữ = vòng chỉnh volume;
     giữ rất lâu = dừng nhạc và ngủ; vẫy = đổi kiểu mắt sóng.
   - Nếu bật IR thứ 2: sensor nào bị che thì pan về hướng đó (bám theo tay).
   Lưu ý: LM393 phát hiện vật ở gần (phản xạ hồng ngoại), KHÔNG đo thân nhiệt. Gắn cố định ở
   thân/đế, không gắn lên đầu chuyển động.

J. WEB UI (mobile-first, neon xanh như mã gốc, không phụ thuộc thư viện ngoài), các tab:
   Đầu (D-pad + 2 slider throttle 50ms), Cảm xúc, Chế độ, Nhạc (player + upload + playlist +
   volume + kiểu mắt sóng), Nguồn (FILE/USB/BT/AUTO + trạng thái host, mức tín hiệu, % buffer,
   underrun, độ trễ), Hiệu chỉnh (offset pan/tilt, ngưỡng IR, trần volume).

K. API REST: /api/head, /api/nudge, /api/center, /api/emotion, /api/gesture, /api/mode,
   /api/music/{play,pause,next,prev,vol,list,upload,delete}, /api/source, /api/viz?style=,
   /api/calib, /api/state (JSON). Route không khớp -> redirect "/" (captive portal).

L. USB VỚI MÁY TÍNH (cổng native GPIO19/20)
   Chế độ: USB Mode "Hardware CDC and JTAG" + USB CDC On Boot: Enabled. TOÀN BỘ log đi qua
   Serial0 (UART), KHÔNG in lên cổng CDC dùng cho giao thức.
   Giao thức khung nhị phân: [0xE1 0x1C][type u8][flags u8][len u16 LE][payload][crc16 LE];
   tự đồng bộ lại khi lệch khung.
     0x01 HELLO/CAPS    robot->PC: phiên bản, khả năng, kích thước buffer
     0x02 AUDIO_CFG     PC->robot: rate, kênh, bit
     0x03 AUDIO_DATA    PC->robot: pts (số mẫu) u32 + PCM
     0x04 AUDIO_CREDIT  robot->PC: số byte xin thêm, %buffer, số underrun
     0x05 FRAME_1BPP    PC->robot: pts u32 + 1024 byte (bố cục SSD1306, page-major)
     0x06 CMD           PC->robot: JSON (cảm xúc, đầu, nhạc, nguồn)
     0x07 TELEM         robot->PC: JSON 5-10Hz (trạng thái, mức âm, BPM)
     0x7F PING/PONG
   - Cơ chế CREDIT: robot xin từng khối 10-20ms; PC không gửi vượt credit. Xung I2S của
     robot là chủ; PC tự bỏ/chèn mẫu để bù lệch xung. Robot không resample.
   - Ring buffer PCM đặt ở PSRAM, mức đệm mục tiêu 100-150ms, báo underrun/overrun.
   - Hai cấu hình: VOICE 16kHz mono (~32KB/s), MUSIC 44.1kHz stereo 16-bit (~176KB/s).
     Đo throughput thực tế, tự hạ cấu hình khi có underrun.
   - Đồng bộ tiếng-hình: FRAME mang pts theo số mẫu audio đã phát; chỉ hiển thị khi đồng hồ
     audio của robot tới pts (sai số mục tiêu ±40ms).
   - Chế độ STREAM của DisplayTask: quá 2s không có khung -> quay về EYES.
   - CÔNG CỤ PC: viết eilik_bridge.py (Python 3.10+, pyserial, Pillow, opencv, thư viện
     loopback âm thanh hệ thống; mặc định Windows WASAPI loopback, ghi chú macOS cần driver
     ảo như BlackHole). Lệnh: audio, video, image, text, cmd, stats. Tự dò cổng theo VID/PID,
     tự nối lại khi rút/cắm; dither Floyd-Steinberg cho 128x64. Viết thêm web_bridge.html dùng
     Web Serial (chạy từ localhost/HTTPS, KHÔNG từ trang http của robot). Có tài liệu cài đặt.
   - USB Audio Class (robot hiện thành card âm thanh): CHỈ làm ở nhánh thử nghiệm riêng sau
     cùng (Arduino core chưa bật sẵn lớp audio TinyUSB; cần esp_device_uac hoặc build lại
     core; có báo cáo loa qua UAC bị rè). Không đưa vào bản chính nếu chưa hết rè.

M. BLUETOOTH
   ESP32-S3 KHÔNG có Bluetooth Classic/A2DP: tuyệt đối không viết code A2DP cho S3 và không
   dùng BLE để stream audio.
   Dùng module BT audio receiver có ngõ ra I2S số (logic 3.3V) + mux 74LVC157/74AHC157 (3.3V):
     mux ngõ A = I2S TX ESP32 (15/16/17), ngõ B = BCLK/LRCK/DOUT của module BT,
     ngõ ra Y = BCLK/LRC/DIN của MAX98357A, SEL = GPIO10.
   Ba dây I2S của module BT đồng thời vào I2S1 RX slave (GPIO11/12/13) CHỈ để nghe tín hiệu
   cho Analyzer (mắt sóng, nhún đầu). Khi phát hiện tín hiệu BT, AudioRouter lật SEL (kèm
   mute SD + fade). Điều khiển robot vẫn qua WiFi/web.

7. CẤU TRÚC FILE (Arduino multi-file)
 eilik/
  eilik.ino           setup/loop, khởi tạo, giám sát
  config.h            chân, giới hạn, hằng số, SSID/mật khẩu, ngưỡng
  partitions.csv      16MB: app + LittleFS >= 9MB
  HeadServo.h/.cpp    bọc servo (clamp, giới hạn tốc độ, offset NVS, auto-detach)
  Gestures.h/.cpp     GesturePlayer + thư viện keyframe
  Emotion.h/.cpp      EmotionEngine
  Autonomy.h/.cpp     tự hành có trọng số + cooldown
  IrGesture.h/.cpp    nhận diện cử chỉ + ánh xạ theo ngữ cảnh
  AudioRouter.h/.cpp  quản lý nguồn, fade, mute, chính sách AUTO
  FilePlayer.h/.cpp   playlist, LittleFS, ESP32-audioI2S
  PcLink.h/.cpp       parser giao thức USB-CDC, credit, ring PSRAM, pts
  BtTap.h/.cpp        I2S1 RX slave + điều khiển mux
  Analyzer.h/.cpp     RMS, 8 băng, beat, BPM
  DisplayTask.h/.cpp  EYES/VISUAL/STREAM, 3 kiểu mắt sóng, chuyển cảnh
  WebApi.h/.cpp       routes async + upload + captive portal
  data/               HTML/CSS/JS (LittleFS), /sfx/*.wav
  pc_tools/           eilik_bridge.py, web_bridge.html, requirements.txt, README

8. QUY TRÌNH GIAO CODE (để tránh bị cắt ngắn)
Giao theo giai đoạn, MỖI giai đoạn là bộ file đầy đủ, biên dịch được; dừng và chờ tôi xác nhận
trước khi sang giai đoạn sau:
 GĐ0 sketch test phần cứng (quét servo, phát tone qua amp, in trạng thái IR) + sơ đồ đấu dây
 GĐ1 HeadServo + ServoTask + hiệu chỉnh NVS
 GĐ2 RoboEyes + Emotion + Gestures + đồng bộ mắt-đầu
 GĐ3a FILE playback + LittleFS + playlist + upload
 GĐ3b giao thức + PcLink + credit + ring PSRAM (test bằng tone/WAV từ PC)
 GĐ3c AudioRouter + Analyzer chung + đổi nguồn không bụp
 GĐ3d video/ảnh lên OLED + pts + eilik_bridge.py
 GĐ4 mắt sóng nhạc 3 kiểu + nhún đầu theo nhịp
 GĐ5 IR gesture
 GĐ6 Web UI + API đầy đủ
 GĐ7 tự hành + tối ưu + soak test
 GĐ3f (tuỳ chọn) Bluetooth + mux; GĐ3g (tuỳ chọn) UAC thử nghiệm
Nếu thiếu thông tin, nêu giả định trong 1 dòng rồi làm tiếp, không hỏi lại dài dòng.

9. TIÊU CHÍ NGHIỆM THU
 - Mắt >= 30 FPS ở EYES, >= 25 FPS ở VISUAL; hình theo nhạc trễ < 80ms ở mọi nguồn.
 - Servo không rung khi nghỉ; không reset/brownout khi servo + loa chạy cùng lúc.
 - FILE phát liên tục 30 phút không giật; thao tác web không làm đứt tiếng.
 - USB audio 10 phút 0 underrun; độ trễ tổng <= 150ms; lệch tiếng-hình <= 40ms.
 - Đổi nguồn không phát ra tiếng bụp.
 - Độ trễ web -> servo < 150ms; nhận đúng IR gesture >= 95% trong 50 lần thử.
 - Chạy 1-2 giờ không treo, heap không rò rỉ.

10. ĐẦU RA YÊU CẦU
 1. Mã nguồn ĐẦY ĐỦ từng file theo giai đoạn (không viết tắt, không "...").
 2. Sơ đồ đấu dây dạng bảng + lưu ý nguồn, chống ồn, diode OR, cảnh báo dòng ngược USB.
 3. Hướng dẫn cài thư viện và cấu hình Arduino IDE (ESP32S3 Dev Module, Flash 16MB, PSRAM OPI,
    partition tuỳ chỉnh, USB Mode Hardware CDC and JTAG, CDC On Boot Enabled) + cách upload
    LittleFS.
 4. Quy trình hiệu chỉnh servo, IR, âm lượng, đo throughput USB; bảng lỗi thường gặp và cách xử lý.
 5. Bảng phiên bản thư viện đã dùng và những chỗ API phụ thuộc phiên bản.
=====================================================================================