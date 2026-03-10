#ifndef MEM_POOL_H
#define MEM_POOL_H

#include <stddef.h>

/*
 * mem_pool_t: Opaque structure for the memory pool
 */
typedef struct mem_pool mem_pool_t;

/*
 * pool_init() - Initialize a memory pool
 * @num_blocks: Số lượng block tối đa trong pool
 * @block_size: Kích thước của mỗi block (tính bằng byte)
 *
 * Return: Con trỏ tới pool nếu thành công, NULL nếu thất bại
 */
mem_pool_t *pool_init(size_t num_blocks, size_t block_size);

/*
 * pool_alloc() - Lấy một block nhớ trống từ pool
 * @pool: Con trỏ tới memory pool
 *
 * Return: Con trỏ tới block nhớ, hoặc NULL nếu pool đã hết block trống
 */
void *pool_alloc(mem_pool_t *pool);

/*
 * pool_free() - Trả lại block nhớ vào pool
 * @pool: Con trỏ tới memory pool
 * @block: Con trỏ tới block bộ nhớ cần trả
 *
 * Return: 0 nếu thành công, -1 nếu thất bại
 */
int pool_free(mem_pool_t *pool, void *block);

/*
 * pool_destroy() - Giải phóng toàn bộ tài nguyên của pool
 * @pool: Con trỏ tới memory pool cần giải phóng
 */
void pool_destroy(mem_pool_t *pool);

#endif /* MEM_POOL_H */
