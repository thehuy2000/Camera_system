#ifndef RING_BUFF_H
#define RING_BUFF_H

#include <stddef.h>

/*
 * ring_buff_t: Opaque structure for the thread-safe ring buffer
 */
typedef struct ring_buff ring_buff_t;

/*
 * ring_buff_init() - Khởi tạo ring buffer
 * @capacity: Sức chứa tối đa (số lượng pointer) của buffer
 *
 * Return: Con trỏ tới ring buffer, hoặc NULL nếu thất bại
 */
ring_buff_t *ring_buff_init(size_t capacity);

/*
 * ring_buff_push() - Đẩy một pointer vào ring buffer (Producer)
 * @rb: Con trỏ tới ring buffer
 * @data: Con trỏ dữ liệu cần đẩy vào
 *
 * Hàm này sẽ block nếu buffer đã đầy.
 * Return: 0 nếu thành công, -1 nếu lỗi tham số
 */
int ring_buff_push(ring_buff_t *rb, void *data);

/*
 * ring_buff_pop() - Lấy một pointer từ ring buffer (Consumer)
 * @rb: Con trỏ tới ring buffer
 * @data: Con trỏ kép (double pointer) để nhận dữ liệu lấy ra
 *
 * Hàm này sẽ block nếu buffer đang rỗng.
 * Return: 0 nếu thành công, -1 nếu lỗi tham số
 */
int ring_buff_pop(ring_buff_t *rb, void **data);

/*
 * ring_buff_destroy() - Giải phóng tài nguyên
 * @rb: Con trỏ tới ring buffer
 */
void ring_buff_destroy(ring_buff_t *rb);

#endif /* RING_BUFF_H */
