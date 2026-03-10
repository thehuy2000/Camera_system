#include "ring_buff.h"
#include <stdlib.h>
#include <pthread.h>

struct ring_buff {
	void **buffer;          /* Mảng chứa các con trỏ tới dữ liệu */
	size_t capacity;        /* Sức chứa tối đa */
	size_t count;           /* Số lượng phần tử hiện tại */
	size_t head;            /* Vị trí ghi vào (Push) */
	size_t tail;            /* Vị trí đọc ra (Pop) */
	
	pthread_mutex_t lock;   /* Bảo vệ truy cập đồng thời */
	pthread_cond_t not_empty; /* Signal khi có dữ liệu mới (để Pop) */
	pthread_cond_t not_full;  /* Signal khi có chỗ trống mới (để Push) */
};

ring_buff_t *ring_buff_init(size_t capacity)
{
	ring_buff_t *rb;

	if (capacity == 0)
		return NULL;

	rb = calloc(1, sizeof(*rb));
	if (!rb)
		return NULL;

	rb->buffer = calloc(capacity, sizeof(void *));
	if (!rb->buffer) {
		free(rb);
		return NULL;
	}

	rb->capacity = capacity;
	rb->count = 0;
	rb->head = 0;
	rb->tail = 0;

	if (pthread_mutex_init(&rb->lock, NULL) != 0) {
		free(rb->buffer);
		free(rb);
		return NULL;
	}

	if (pthread_cond_init(&rb->not_empty, NULL) != 0) {
		pthread_mutex_destroy(&rb->lock);
		free(rb->buffer);
		free(rb);
		return NULL;
	}

	if (pthread_cond_init(&rb->not_full, NULL) != 0) {
		pthread_cond_destroy(&rb->not_empty);
		pthread_mutex_destroy(&rb->lock);
		free(rb->buffer);
		free(rb);
		return NULL;
	}

	return rb;
}

int ring_buff_push(ring_buff_t *rb, void *data)
{
	if (!rb || !data)
		return -1;

	pthread_mutex_lock(&rb->lock);

	/* Chờ cho tới khi có chỗ trống để ghi vào */
	while (rb->count == rb->capacity) {
		pthread_cond_wait(&rb->not_full, &rb->lock);
	}

	rb->buffer[rb->head] = data;
	rb->head = (rb->head + 1) % rb->capacity;
	rb->count++;

	/* Báo cho thread đang chờ Pop biết có dữ liệu */
	pthread_cond_signal(&rb->not_empty);

	pthread_mutex_unlock(&rb->lock);

	return 0;
}

int ring_buff_pop(ring_buff_t *rb, void **data)
{
	if (!rb || !data)
		return -1;

	pthread_mutex_lock(&rb->lock);

	/* Chờ cho tới khi có dữ liệu để đọc ra */
	while (rb->count == 0) {
		pthread_cond_wait(&rb->not_empty, &rb->lock);
	}

	*data = rb->buffer[rb->tail];
	rb->tail = (rb->tail + 1) % rb->capacity;
	rb->count--;

	/* Báo cho thread đang chờ Push biết có chỗ trống */
	pthread_cond_signal(&rb->not_full);

	pthread_mutex_unlock(&rb->lock);

	return 0;
}

void ring_buff_destroy(ring_buff_t *rb)
{
	if (!rb)
		return;

	pthread_mutex_destroy(&rb->lock);
	pthread_cond_destroy(&rb->not_empty);
	pthread_cond_destroy(&rb->not_full);
	free(rb->buffer);
	free(rb);
}
