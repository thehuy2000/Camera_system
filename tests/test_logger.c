#include "logger.h"
#include <pthread.h>
#include <stdio.h>
#include <assert.h>

#define NUM_THREADS 5
#define ITERATIONS 10

void *log_thread(void *arg)
{
	int id = *(int *)arg;
	int i;
	
	for (i = 0; i < ITERATIONS; i++) {
		LOG_INFO("Thread %d, loop %d", id, i);
		LOG_DEBUG("This is a debug trace from thread %d", id);
	}
	return NULL;
}

int main(void)
{
	assert(init_logger() == 0);

	pthread_t threads[NUM_THREADS];
	int tids[NUM_THREADS];
	int i;

	LOG_INFO("--- BẮT ĐẦU TEST LOGGER CONCURRENCY ---");

	for (i = 0; i < NUM_THREADS; i++) {
		tids[i] = i;
		pthread_create(&threads[i], NULL, log_thread, &tids[i]);
	}

	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(threads[i], NULL);
	}

	LOG_WARN("Testing Warning Event");
	LOG_ERROR("Testing Error Event");

	LOG_INFO("--- KẾT THÚC TEST LOGGER CONCURRENCY ---");
	destroy_logger();
	
	printf("All logger tests PASSED.\n");
	return 0;
}
