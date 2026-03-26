#ifndef NAL_QUEUE_H
#define NAL_QUEUE_H

/*
 * nal_queue.h - Thread-safe NAL unit queue
 *
 * Dùng để truyền H.264 NAL units từ consumer_thread (C) sang
 * live555 FramedSource (C++) mà không blocking event loop.
 *
 * Giới hạn: MAX_NAL_QUEUE_DEPTH phần tử, mỗi phần tử tối đa
 * MAX_NAL_SIZE bytes. Khi queue đầy, push() sẽ block chờ chỗ.
 */

#include <stddef.h>
#include <stdint.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NAL_QUEUE_DEPTH  64
#define MAX_NAL_SIZE         (256 * 1024)  /* 256 KB per NAL — đủ cho 640x480 */

typedef struct nal_entry {
    uint8_t  data[MAX_NAL_SIZE];
    size_t   size;
} nal_entry_t;

typedef struct nal_queue {
    nal_entry_t     entries[MAX_NAL_QUEUE_DEPTH];
    int             head;           /* read index  */
    int             tail;           /* write index */
    int             count;
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;      /* consumer chờ khi trống  */
    pthread_cond_t  not_full;       /* producer chờ khi đầy    */
    int             closed;         /* 1 = đã đóng, consumer sẽ thoát */
} nal_queue_t;

/*
 * nal_queue_init() - Khởi tạo queue
 * Return: 0 thành công, -1 lỗi.
 */
int nal_queue_init(nal_queue_t *q);

/*
 * nal_queue_push() - Đẩy NAL vào cuối queue (block nếu đầy)
 * Return: 0 thành công, -1 nếu queue đã closed.
 */
int nal_queue_push(nal_queue_t *q, const uint8_t *data, size_t size);

/*
 * nal_queue_pop() - Lấy NAL từ đầu queue (block nếu trống)
 * @out_size: [out] kích thước thực tế của NAL
 * Return: Con trỏ tới buffer nội bộ (valid cho đến lần pop tiếp theo),
 *         NULL nếu queue closed và trống.
 */
const uint8_t *nal_queue_pop(nal_queue_t *q, size_t *out_size);

/*
 * nal_queue_try_pop() - Non-blocking pop
 * Return: Con trỏ data hoặc NULL nếu trống/closed.
 */
const uint8_t *nal_queue_try_pop(nal_queue_t *q, size_t *out_size);

/*
 * nal_queue_close() - Đánh dấu đóng để unblock các waiter
 */
void nal_queue_close(nal_queue_t *q);

/*
 * nal_queue_destroy() - Giải phóng tài nguyên
 */
void nal_queue_destroy(nal_queue_t *q);

#ifdef __cplusplus
}
#endif

#endif /* NAL_QUEUE_H */
