#include "ring_buff.h"
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

void test_ring_buff_init_and_destroy(void)
{
	ring_buff_t *rb = ring_buff_init(10);
	assert(rb != NULL);
	ring_buff_destroy(rb);
	printf("test_ring_buff_init_and_destroy: PASSED\n");
}

void test_ring_buff_push_pop_basic(void)
{
	ring_buff_t *rb = ring_buff_init(3);
	assert(rb != NULL);

	int a = 1, b = 2, c = 3;
	void *out_ptr;

	assert(ring_buff_push(rb, &a) == 0);
	assert(ring_buff_push(rb, &b) == 0);
	assert(ring_buff_push(rb, &c) == 0);

	assert(ring_buff_pop(rb, &out_ptr) == 0);
	assert(*(int *)out_ptr == 1);

	assert(ring_buff_pop(rb, &out_ptr) == 0);
	assert(*(int *)out_ptr == 2);

	assert(ring_buff_pop(rb, &out_ptr) == 0);
	assert(*(int *)out_ptr == 3);

	ring_buff_destroy(rb);
	printf("test_ring_buff_push_pop_basic: PASSED\n");
}

/* Biến toàn cục cho test thread-safe */
#define NUM_ITEMS 100
struct test_data {
	ring_buff_t *rb;
	int payload[NUM_ITEMS];
	int consumer_sum;
};

void *producer_thread(void *arg)
{
	struct test_data *td = (struct test_data *)arg;
	int i;
	for (i = 0; i < NUM_ITEMS; i++) {
		td->payload[i] = i + 1;
		ring_buff_push(td->rb, &td->payload[i]);
	}
	return NULL;
}

void *consumer_thread(void *arg)
{
	struct test_data *td = (struct test_data *)arg;
	int i;
	void *ptr;
	td->consumer_sum = 0;
	
	for (i = 0; i < NUM_ITEMS; i++) {
		ring_buff_pop(td->rb, &ptr);
		td->consumer_sum += *(int *)ptr;
	}
	return NULL;
}

void test_ring_buff_concurrency(void)
{
	struct test_data td;
	pthread_t prod, cons;

	/* Tạo buffer dung lượng nhỏ để ép concurrency chèn ép nhau */
	td.rb = ring_buff_init(5);
	assert(td.rb != NULL);

	pthread_create(&prod, NULL, producer_thread, &td);
	pthread_create(&cons, NULL, consumer_thread, &td);

	pthread_join(prod, NULL);
	pthread_join(cons, NULL);

	/* Tổng dãy số từ 1 đến 100 là 5050 */
	assert(td.consumer_sum == 5050);

	ring_buff_destroy(td.rb);
	printf("test_ring_buff_concurrency: PASSED\n");
}

int main(void)
{
	test_ring_buff_init_and_destroy();
	test_ring_buff_push_pop_basic();
	test_ring_buff_concurrency();
	printf("All ring_buff tests PASSED.\n");
	return 0;
}
