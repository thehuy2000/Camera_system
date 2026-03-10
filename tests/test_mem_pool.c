#include "mem_pool.h"
#include <stdio.h>
#include <assert.h>

void test_pool_init_and_destroy(void)
{
	mem_pool_t *pool = pool_init(10, 1024);
	assert(pool != NULL);
	pool_destroy(pool);
	printf("test_pool_init_and_destroy: PASSED\n");
}

void test_pool_alloc_free(void)
{
	mem_pool_t *pool = pool_init(2, 1024);
	assert(pool != NULL);

	void *b1 = pool_alloc(pool);
	void *b2 = pool_alloc(pool);
	assert(b1 != NULL);
	assert(b2 != NULL);

	/* Hết block */
	void *b3 = pool_alloc(pool);
	assert(b3 == NULL);

	/* Free */
	assert(pool_free(pool, b1) == 0);
	assert(pool_free(pool, b2) == 0);

	/* Alloc lại */
	void *b4 = pool_alloc(pool);
	assert(b4 != NULL);

	pool_destroy(pool);
	printf("test_pool_alloc_free: PASSED\n");
}

void test_pool_free_invalid(void)
{
	mem_pool_t *pool = pool_init(2, 1024);
	int invalid_var = 0;

	/* Free block rác */
	assert(pool_free(pool, &invalid_var) == -1);

	pool_destroy(pool);
	printf("test_pool_free_invalid: PASSED\n");
}

int main(void)
{
	test_pool_init_and_destroy();
	test_pool_alloc_free();
	test_pool_free_invalid();
	printf("All mem_pool tests PASSED.\n");
	return 0;
}
