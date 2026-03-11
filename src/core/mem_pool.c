#include "mem_pool.h"
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

struct mem_pool {
	void *memory;          /* Vùng nhớ vật lý liên tục */
	void **free_blocks;    /* Stack chứa con trỏ tới các block đang trống */
	size_t num_blocks;     /* Tổng số block */
	size_t block_size;     /* Kích thước mỗi block */
	size_t free_count;     /* Số lượng block còn trống */
	pthread_mutex_t lock;  /* Mutex bảo vệ trạng thái nội bộ */
	pthread_cond_t  not_empty; /* Signal khi có block được trả về (pool_free) */
};

mem_pool_t *pool_init(size_t num_blocks, size_t block_size)
{
	mem_pool_t *pool;
	size_t i;

	if (num_blocks == 0 || block_size == 0)
		return NULL;

	pool = calloc(1, sizeof(*pool));
	if (!pool)
		return NULL;

	/* 
	 * Cấp phát vùng nhớ liên tục duy nhất, tránh overhead của malloc nhỏ lẻ
	 * đồng thời giữ locality tốt cho cache bộ nhớ.
	 */
	pool->memory = calloc(num_blocks, block_size);
	if (!pool->memory) {
		free(pool);
		return NULL;
	}

	pool->free_blocks = calloc(num_blocks, sizeof(void *));
	if (!pool->free_blocks) {
		free(pool->memory);
		free(pool);
		return NULL;
	}

	for (i = 0; i < num_blocks; i++) {
		pool->free_blocks[i] = (char *)pool->memory + (i * block_size);
	}

	pool->num_blocks = num_blocks;
	pool->block_size = block_size;
	pool->free_count = num_blocks;

	if (pthread_mutex_init(&pool->lock, NULL) != 0) {
		free(pool->free_blocks);
		free(pool->memory);
		free(pool);
		return NULL;
	}

	if (pthread_cond_init(&pool->not_empty, NULL) != 0) {
		pthread_mutex_destroy(&pool->lock);
		free(pool->free_blocks);
		free(pool->memory);
		free(pool);
		return NULL;
	}

	return pool;
}

void *pool_alloc(mem_pool_t *pool)
{
	void *block = NULL;

	if (!pool)
		return NULL;

	pthread_mutex_lock(&pool->lock);
	if (pool->free_count > 0) {
		pool->free_count--;
		block = pool->free_blocks[pool->free_count];
	}
	pthread_mutex_unlock(&pool->lock);

	return block;
}

void *pool_alloc_blocking(mem_pool_t *pool)
{
	void *block;

	if (!pool)
		return NULL;

	pthread_mutex_lock(&pool->lock);
	while (pool->free_count == 0) {
		/* Chờ cho tới khi pool_free() trả lại ít nhất một block */
		pthread_cond_wait(&pool->not_empty, &pool->lock);
	}
	pool->free_count--;
	block = pool->free_blocks[pool->free_count];
	pthread_mutex_unlock(&pool->lock);

	return block;
}

int pool_free(mem_pool_t *pool, void *block)
{
	int ret = -1;

	if (!pool || !block)
		return ret;

	/* 
	 * Cần kiểm tra pointer có thực sự thuộc vùng nhớ memory hay không
	 * Tránh việc User trả nhầm địa chỉ rác dẫn tới corrupt pool stack.
	 */
	if ((char *)block < (char *)pool->memory || 
	    (char *)block >= (char *)pool->memory + (pool->num_blocks * pool->block_size)) {
		return -1;
	}

	/* Đảm bảo block memory align chính xác theo block_size */
	if (((char *)block - (char *)pool->memory) % pool->block_size != 0) {
		return -1;
	}

	pthread_mutex_lock(&pool->lock);
	if (pool->free_count < pool->num_blocks) {
		pool->free_blocks[pool->free_count] = block;
		pool->free_count++;
		ret = 0;
		/* Báo cho pool_alloc_blocking() đang chờ biết có block mới */
		pthread_cond_signal(&pool->not_empty);
	}
	pthread_mutex_unlock(&pool->lock);

	return ret;
}

void pool_destroy(mem_pool_t *pool)
{
	if (!pool)
		return;

	pthread_cond_destroy(&pool->not_empty);
	pthread_mutex_destroy(&pool->lock);
	free(pool->free_blocks);
	free(pool->memory);
	free(pool);
}
