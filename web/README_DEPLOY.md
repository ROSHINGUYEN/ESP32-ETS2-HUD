# HƯỚNG DẪN TRIỂN KHAI WEB LÊN CLOUDFLARE PAGES & BIÊN DỊCH FIRMWARE

## 1. TẠI SAO CẦN FILE .BIN ĐỂ NẠP VÀO ESP32-S3?
- Trình duyệt Web và chip ESP32-S3 **không thể nạp trực tiếp file mã nguồn C++ (`miu_eilik.ino`)**.
- ESP32-S3 chỉ nhận mã máy đã biên dịch dạng tệp nhị phân **`.bin`**.

### Cách tạo file `.bin` từ Arduino IDE (Rất đơn giản, mất 15 giây):
1. Mở file `miu_eilik.ino` bằng **Arduino IDE**.
2. Kiểm tra chọn đúng bo mạch: `ESP32S3 Dev Module` (Flash Size 16MB, PSRAM OPI).
3. Vào menu trên cùng: **Sketch** $\rightarrow$ Chọn **Export Compiled Binary** (hoặc bấm phím tắt `Ctrl + Alt + S`).
4. Arduino IDE sẽ biên dịch và tự động tạo ra file `miu_eilik.ino.bin` nằm trong thư mục `build/` của dự án.
5. Bạn có thể:
   - **Cách 1**: Kéo thả trực tiếp file `.bin` đó vào giao diện Web Flasher để nạp ngay.
   - **Cách 2**: Đổi tên thành `miu_eilik.bin` rồi copy vào thư mục `web/firmware/miu_eilik.bin` trước khi đẩy lên Cloudflare Pages. Khi đó bất kỳ ai truy cập web cũng có thể bấm 1 nút là nạp được ngay!

---

## 2. CÁCH ĐẨY THƯ MỤC `web/` LÊN CLOUDFLARE PAGES

### Cách A: Kéo thả trực tiếp (Direct Upload - Nhanh nhất, không cần cài đặt)
1. Đăng nhập vào trang quản trị [Cloudflare Dashboard](https://dash.cloudflare.com/).
2. Chọn **Workers & Pages** $\rightarrow$ Bấm nút **Create application** $\rightarrow$ Chọn tab **Pages**.
3. Chọn **Upload assets**.
4. Đặt tên dự án (Ví dụ: `miu-eilik-flasher`).
5. Kéo thả toàn bộ thư mục `web/` vào khung tải lên của Cloudflare.
6. Bấm **Deploy site** $\rightarrow$ Bạn sẽ nhận được đường link web miễn phí vĩnh viễn (ví dụ: `https://miu-eilik-flasher.pages.dev`).

### Cách B: Kết nối qua GitHub Repository (Tự động cập nhật mỗi khi commit)
1. Trong Cloudflare Pages: Chọn **Connect to Git** $\rightarrow$ Chọn repository `ROSHINGUYEN/MIU_EILIK`.
2. Trong phần **Build configuration**:
   - **Framework preset**: `None`
   - **Build command**: *(Để trống)*
   - **Build output directory**: Nhập `web`
3. Bấm **Save and Deploy**. Mỗi khi bạn `git push`, Cloudflare Pages sẽ tự động cập nhật web mới nhất!
