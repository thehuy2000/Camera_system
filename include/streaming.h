#ifndef STREAMING_H
#define STREAMING_H

/*
 * streaming.h - RTSP Streaming Server Interface (C API)
 *
 * Wrapper C cho live555 RTSP server. Được implement bằng C++ (rtsp_server.cpp)
 * nhưng expose interface thuần C để main.c có thể #include và gọi trực tiếp.
 *
 * Luồng sử dụng điển hình:
 *   1. rtsp_server_create(8554, 30, 640, 480)   → khởi tạo
 *   2. rtsp_server_start()                       → chạy event loop trong thread riêng
 *   3. rtsp_server_push_nal(data, size)          → gọi từ consumer thread với mỗi NAL
 *   4. rtsp_server_stop()                        → dừng server
 *   5. rtsp_server_destroy()                     → dọn dẹp tài nguyên
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * rtsp_server_create() - Khởi tạo RTSP server context
 *
 * @port       : TCP port lắng nghe RTSP (thường là 8554)
 * @fps        : Framerate dùng cho timestamping (ví dụ: 30)
 * @width      : Chiều rộng video (px)
 * @height     : Chiều cao video (px)
 * @stream_name: Tên path RTSP, ví dụ "live" → URL: rtsp://host:8554/live
 *
 * Return: 0 thành công, -1 lỗi.
 */
int rtsp_server_create(int port, int fps, int width, int height,
                       const char *stream_name);

/*
 * rtsp_server_start() - Khởi động RTSP event loop trong background thread
 *
 * Gọi sau khi rtsp_server_create(). Non-blocking; RTSP server chạy song song.
 *
 * Return: 0 thành công, -1 lỗi.
 */
int rtsp_server_start(void);

/*
 * rtsp_server_push_nal() - Đẩy một NAL unit (Annex-B) vào hàng đợi phát
 *
 * @data: Con trỏ tới NAL data (bao gồm start code 0x00 0x00 0x00 0x01)
 * @size: Kích thước NAL (bytes)
 *
 * Hàm này thread-safe, được gọi từ consumer thread.
 * Block nếu queue đầy (back-pressure tránh OOM).
 *
 * Return: 0 thành công, -1 nếu server đã stop.
 */
int rtsp_server_push_nal(const uint8_t *data, size_t size);

/*
 * rtsp_server_stop() - Yêu cầu dừng event loop
 *
 * Gọi khi muốn kết thúc streaming (ví dụ: nhận SIGINT).
 * Unblock mọi push_nal() đang chờ.
 */
void rtsp_server_stop(void);

/*
 * rtsp_server_destroy() - Giải phóng toàn bộ tài nguyên
 *
 * Phải gọi sau rtsp_server_stop() và sau khi tất cả thread đã join.
 */
void rtsp_server_destroy(void);

#ifdef __cplusplus
}
#endif

#endif /* STREAMING_H */
