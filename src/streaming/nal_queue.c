/*
 * nal_queue.c - Thread-safe NAL unit queue implementation
 */

#include "nal_queue.h"
#include <string.h>

int nal_queue_init(nal_queue_t *q)
{
    if (!q) return -1;
    memset(q, 0, sizeof(*q));
    if (pthread_mutex_init(&q->mutex, NULL) != 0) return -1;
    if (pthread_cond_init(&q->not_empty, NULL) != 0) {
        pthread_mutex_destroy(&q->mutex);
        return -1;
    }
    if (pthread_cond_init(&q->not_full, NULL) != 0) {
        pthread_cond_destroy(&q->not_empty);
        pthread_mutex_destroy(&q->mutex);
        return -1;
    }
    q->head   = 0;
    q->tail   = 0;
    q->count  = 0;
    q->closed = 0;
    return 0;
}

int nal_queue_push(nal_queue_t *q, const uint8_t *data, size_t size)
{
    if (!q || !data || size == 0 || size > MAX_NAL_SIZE) return -1;

    pthread_mutex_lock(&q->mutex);

    /* Chờ cho tới khi có chỗ trống hoặc queue bị đóng */
    while (q->count >= MAX_NAL_QUEUE_DEPTH && !q->closed) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    if (q->closed) {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }

    /* Sao chép NAL vào slot tail */
    nal_entry_t *entry = &q->entries[q->tail];
    memcpy(entry->data, data, size);
    entry->size = size;

    q->tail = (q->tail + 1) % MAX_NAL_QUEUE_DEPTH;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
    return 0;
}

const uint8_t *nal_queue_pop(nal_queue_t *q, size_t *out_size)
{
    if (!q || !out_size) return NULL;

    pthread_mutex_lock(&q->mutex);

    while (q->count == 0 && !q->closed) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    if (q->count == 0) {  /* closed và trống */
        pthread_mutex_unlock(&q->mutex);
        *out_size = 0;
        return NULL;
    }

    nal_entry_t *entry = &q->entries[q->head];
    *out_size = entry->size;

    q->head = (q->head + 1) % MAX_NAL_QUEUE_DEPTH;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    return entry->data;
}

const uint8_t *nal_queue_try_pop(nal_queue_t *q, size_t *out_size)
{
    if (!q || !out_size) return NULL;

    pthread_mutex_lock(&q->mutex);

    if (q->count == 0) {
        pthread_mutex_unlock(&q->mutex);
        *out_size = 0;
        return NULL;
    }

    nal_entry_t *entry = &q->entries[q->head];
    *out_size = entry->size;

    q->head = (q->head + 1) % MAX_NAL_QUEUE_DEPTH;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    return entry->data;
}

void nal_queue_close(nal_queue_t *q)
{
    if (!q) return;
    pthread_mutex_lock(&q->mutex);
    q->closed = 1;
    pthread_cond_broadcast(&q->not_empty);
    pthread_cond_broadcast(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
}

void nal_queue_destroy(nal_queue_t *q)
{
    if (!q) return;
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
    pthread_mutex_destroy(&q->mutex);
}
