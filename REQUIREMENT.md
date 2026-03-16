# Project Requirement: Embedded Linux Camera System (ELCS)

## 1. Tổng quan dự án (Project Overview)
Xây dựng hệ thống camera trên môi trường Embedded Linux hỗ trợ capture hình ảnh và video từ Webcam (V4L2). Hệ thống tập trung vào hiệu năng cao, quản lý bộ nhớ chặt chẽ thông qua cơ chế Memory Pool và đảm bảo an toàn đa luồng (Thread-safety) bằng Ring Buffer.

## 2. Ràng buộc kỹ thuật (Technical Constraints)
* **Ngôn ngữ:** C (Tiêu chuẩn C11).
* **Thư viện:** POSIX Threads (pthreads), V4L2 (Video4Linux2).
* **Quản lý bộ nhớ (Memory Management):**
    * Sử dụng **Memory Pool Allocator** để cấp phát tĩnh một vùng nhớ lớn ngay từ đầu.
    * Không sử dụng `malloc`/`free` trong luồng xử lý thời gian thực (real-time loop).
    * Kiểm soát chặt chẽ memory leak bằng **Valgrind**.
* **Mô hình luồng (Concurrency):**
    * Mô hình Producer-Consumer.
    * Sử dụng **Thread-safe Ring Buffer** với `mutex` và `condition variable`.
* **Chất lượng (Quality Gate):** Yêu cầu Unit Test cho từng module trong phần `core/`.

## 3. Chức năng chính (Functional Requirements)
Chương trình cung cấp 2 chế độ hoạt động chính:

### Option 1: Capture Image (Snapshot)
* Chụp 1 khung hình (frame) duy nhất từ `/dev/video0`.
* Lưu trữ dưới định dạng ảnh (.jpg hoặc .pnm).
* Tên file định dạng: `snap_YYYYMMDD_HHMMSS.jpg`.

### Option 2: Record Video
* Capture luồng frame liên tục từ webcam.
* Lưu trữ dữ liệu vào file video (container .avi hoặc raw stream).
* Đảm bảo không mất frame (dropped frames) khi hệ thống chịu tải.

## 4. Đặc tả các Module lõi (Core Specifications)

### A. Mini Logging Framework (`logger`)
* Hỗ trợ Level: `DEBUG`, `INFO`, `WARN`, `ERROR`.
* Định dạng: `[TIMESTAMP] [LEVEL] [FILE:LINE] - Message`.
* An toàn khi gọi từ nhiều thread đồng thời.

### B. Memory Pool Allocator (`mem_pool`)
* Cấp phát sẵn `N` blocks có kích thước `MAX_FRAME_SIZE`.
* API: `pool_alloc()` lấy 1 block trống, `pool_free()` trả lại block vào pool.
* Tránh hiện tượng phân mảnh bộ nhớ (Memory Fragmentation).

### C. Thread-safe Ring Buffer (`ring_buf`)
* Quản lý hàng đợi vòng chứa các con trỏ tới dữ liệu trong Memory Pool.
* Đảm bảo thứ tự dữ liệu (FIFO).
* Sử dụng `pthread_cond_wait` để block luồng consumer khi buffer trống và luồng producer khi buffer đầy.

## 5. Cấu trúc cây thư mục (Project Structure)
```text
.
├── include/           # File tiêu đề (.h)
├── src/
│   ├── core/          # Module nền tảng (logger, mem_pool, ring_buf)
│   ├── hardware/      # Giao tiếp V4L2 Camera
│   ├── app/           # Logic xử lý Snapshot & Record
│   └── main.c         # Entry point & CLI handling
├── tests/             # Mã nguồn Unit Test 
├── output/            # Thư mục lưu kết quả
└── CMakeLists.txt     # Build system