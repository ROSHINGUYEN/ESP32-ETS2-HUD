---
trigger: always_on
---

## SKILL: CONTINUOUS MARKDOWN WORK REPORT

Đọc code và quy tắc dự án hiện tại trước khi thực hiện. Sau đó triển khai quy trình báo cáo công việc liên tục theo yêu cầu dưới đây.

### 1. FILE BÁO CÁO DUY NHẤT
- Tạo `WORK_REPORT.md` tại thư mục gốc dự án nếu chưa tồn tại.
- Nếu file đã tồn tại, đọc và giữ nguyên lịch sử cũ.
- Không tạo file báo cáo mới cho từng nhiệm vụ.
- Không ghi đè hoặc xóa nội dung lịch sử.

### 2. GHI BÁO CÁO LIÊN TỤC
Cập nhật `WORK_REPORT.md` trong suốt quá trình làm việc, không đợi đến khi hoàn thành mới ghi báo cáo.

Ghi lại:
- Mục tiêu và tiêu chí hoàn thành.
- Trạng thái ban đầu của dự án.
- Phân tích nguyên nhân và bằng chứng.
- Kế hoạch, quyết định và lý do lựa chọn.
- Từng bước triển khai đã thực hiện.
- File được thêm, sửa hoặc xóa và lý do.
- Lệnh kiểm tra, kết quả test thực tế.
- Lỗi còn tồn tại, vấn đề bị chặn và bước tiếp theo.

Cập nhật sau mỗi cột mốc quan trọng, đặc biệt trước và sau những thay đổi lớn.

### 3. QUY TẮC BẢO VỆ
- Không bịa tiến độ, file đã sửa hoặc kết quả kiểm thử.
- Không ghi mật khẩu, API key, token, cookie hoặc dữ liệu nhạy cảm vào báo cáo.
- Không sao chép toàn bộ mã nguồn hoặc log dài vào Markdown.
- Giữ báo cáo ngắn gọn, dễ đọc, có thể tiếp tục qua nhiều phiên AI.
- Không sửa code chỉ để làm đẹp báo cáo.
- Nếu không thể cập nhật file, báo rõ vấn đề; không giả vờ đã ghi thành công.

### 4. KẾT THÚC NHIỆM VỤ
Bổ sung phần tổng kết gồm:
- Trạng thái: PASS / FAIL / BLOCKED / PARTIAL.
- Những gì đã hoàn thành.
- Danh sách file thay đổi.
- Các lệnh test/build đã chạy và kết quả thực tế.
- Tiêu chí nào đã đạt, chưa đạt hoặc chưa kiểm tra.
- Rủi ro, giới hạn và công việc còn lại.

Chỉ ghi PASS khi có bằng chứng xác minh.

### 5. CẤU TRÚC MARKDOWN
Mỗi nhiệm vụ thêm một mục mới theo mẫu:

## YYYY-MM-DD — Tên nhiệm vụ

- **Mục tiêu:**
- **Tiêu chí hoàn thành:**
- **Trạng thái ban đầu:**
- **Phân tích và bằng chứng:**
- **Các bước đã thực hiện:**
- **File thay đổi:**
- **Kiểm thử và kết quả:**
- **Vấn đề còn tồn tại:**
- **Trạng thái:** PASS / FAIL / BLOCKED / PARTIAL
- **Bước tiếp theo:**

Giữ các mục lịch sử cũ và bổ sung mục mới theo trình tự thời gian.

### 6. TRIỂN KHAI NGAY
1. Kiểm tra cấu trúc dự án và các quy tắc AI hiện có.
2. Kiểm tra `WORK_REPORT.md`.
3. Tạo hoặc cập nhật file theo yêu cầu.
4. Xác minh nội dung đã được ghi vào đĩa.
5. Báo cáo đường dẫn file và trạng thái thực tế.

Chỉ thực hiện thay đổi cần thiết để bổ sung cơ chế báo cáo này. Không viết lại dự án, không sửa chức năng không liên quan và không tự ý ghi đè các quy tắc hiện có.