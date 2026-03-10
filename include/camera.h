#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>
#include <stddef.h>

/* Thông số cấu hình mặc định (có thể được ghi đè) */
#define DEFAULT_CAM_DEV "/dev/video0"
#define DEFAULT_WIDTH   640
#define DEFAULT_HEIGHT  480
#define DEFAULT_FPS     30

/* Cấu trúc opaque quản lý thông tin device camera */
typedef struct camera_device cam_dev_t;

/*
 * cam_open() - Mở file descriptor tới thiết bị V4L2
 * @dev_name:  Tên device (VD: "/dev/video0"), truyền NULL để dùng mặc định
 *
 * Return: Con trỏ cấu trúc cam_dev_t nếu OK, NULL nếu thất bại
 */
cam_dev_t *cam_open(const char *dev_name);

/*
 * cam_init() - Cấu hình định dạng Video (Format, Resolution, Framerate)
 * @cam:    Con trỏ device camera
 * @width:  Chiều rộng frame (px)
 * @height: Chiều cao frame (px)
 * @pixelformat: Mã pixel format theo chuẩn V4L2 (VD: V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_MJPEG).
 *
 * Return: 0 nếu OK, -1 nếu thiết bị không hỗ trợ params yêu cầu
 */
int cam_init(cam_dev_t *cam, int width, int height, uint32_t pixelformat);

/*
 * cam_get_frame() - Yêu cầu camera đọc 1 frame thông qua READ I/O hoặc MMAP
 * @cam: Con trỏ device camera
 * @buffer:     Bộ nhớ do user chuẩn bị sẵn (ví dụ lấy từ mem_pool) để copy dữ liệu vào
 * @max_size:   Kích thước tối đa của buffer (để tránh buffer overflow)
 * @bytes_read: (Output) Số lượng bytes thực tế đã được chụp từ camera
 *
 * Return: 0 nếu OK, -1 nếu thất bại
 */
int cam_get_frame(cam_dev_t *cam, void *buffer, size_t max_size, size_t *bytes_read);

/*
 * cam_close() - Đóng thiết bị và giải phóng tài nguyên
 * @cam: Con trỏ device camera
 */
void cam_close(cam_dev_t *cam);

#endif /* CAMERA_H */
