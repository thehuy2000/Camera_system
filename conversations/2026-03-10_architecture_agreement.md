# Conversation Log: 2026-03-10 - Architecture Agreement

## Bối cảnh (Context)
- Phân tích yêu cầu hệ thống Embedded Linux Camera System (ELCS) từ `REQUIREMENT.md` và tuân theo constraints trong `ANTIGRAVY.md`.
- Đã thảo luận và thống nhất về kiến trúc tổng thể cũng như luồng xử lý dữ liệu.

## Quyết định (Decisions)
1. **Kiến trúc (Architecture)**: 
   - Sử dụng mô hình Producer-Consumer để tách biệt luồng capture phần cứng (camera) và luồng xử lý I/O (lưu file).
   - Core API bao gồm: `logger` (mini logging framework), `mem_pool` (memory pool allocator O(1)), và `ring_buf` (thread-safe queue).
2. **Luồng dữ liệu (Data Flow)**:
   - Thay vì dùng malloc/free trong real-time loop, các thread sẽ xin cấp phát một block cố định từ Memory Pool, đẩy con trỏ (pointer) qua Ring Buffer, sau đó Consumer sẽ xử lý và trả block về Pool.
3. **Pha tiếp theo**:
   - Bắt đầu phase "Vibe coding" và TDD cho các core modules, ưu tiên bắt đầu với `mem_pool`.

## Lý do (WHY)
- Việc dùng mô hình này là bắt buộc trên môi trường Embedded để tránh hiện tượng CPU bị stall khi thực hiện I/O (ghi disk), dẫn tới rớt frame (dropped frames) ở luồng capture.
- Quản lý bộ nhớ tĩnh với Memory Pool tránh được memory fragmentation và đảm bảo thời gian cấp phát bộ nhớ dự đoán được (predictable execution time), đáp ứng yêu cầu theo chuẩn code system/kernel.
