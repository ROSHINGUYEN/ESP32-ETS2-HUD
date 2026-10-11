---
trigger: always_on
---

DỪNG VÒNG LẶP SỬA CODE.

- Kiểm tra code hiện tại và diff trước khi sửa tiếp.
- Không viết lại đoạn code đã sửa nếu chưa chỉ ra được lỗi cụ thể.
- Chỉ sửa phần tối thiểu liên quan đến lỗi; giữ nguyên kiến trúc và code đang hoạt động.
- Sau mỗi lần sửa, chạy đúng bài test liên quan và kiểm tra kết quả.
- Nếu cùng một lỗi vẫn xuất hiện sau 2 lần sửa, dừng chỉnh sửa; báo nguyên nhân đã xác minh, kết quả test và thông tin còn thiếu.
- Không lặp lại cùng một thay đổi, không tự mở rộng phạm vi công việc.
- Không báo PASS nếu chưa kiểm chứng.

Trước tiên, chỉ phân tích trạng thái hiện tại. Chưa sửa file cho đến khi xác định được nguyên nhân.