#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

static pthread_mutex_t log_lock;
static int initialized = 0;

static const char *level_to_str(log_level_t level)
{
	switch (level) {
	case LOG_LVL_DEBUG: return "DEBUG";
	case LOG_LVL_INFO:  return "INFO";
	case LOG_LVL_WARN:  return "WARN";
	case LOG_LVL_ERROR: return "ERROR";
	default:            return "UNKNOWN";
	}
}

int init_logger(void)
{
	if (initialized)
		return 0;

	if (pthread_mutex_init(&log_lock, NULL) != 0) {
		return -1; // Lỗi khởi tạo
	}
	
	initialized = 1;
	return 0;
}

void logger_log(log_level_t level, const char *file, int line, const char *fmt, ...)
{
	va_list args;
	time_t t = time(NULL);
	struct tm *tm_info = localtime(&t);
	char time_buf[24];

	if (!initialized) {
		/* Fallback nếu chưa gọi init_logger, dù không thread-safe lắm */
		init_logger();
	}

	/* Định dạng: YYYY-MM-DD HH:MM:SS */
	strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

	pthread_mutex_lock(&log_lock);

	/* In metadata */
	fprintf(stderr, "[%s] [%s] [%s:%d] - ", 
			time_buf, level_to_str(level), file, line);

	/* In nội dung biến */
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);

	fprintf(stderr, "\n");

	pthread_mutex_unlock(&log_lock);
}

void destroy_logger(void)
{
	if (initialized) {
		pthread_mutex_destroy(&log_lock);
		initialized = 0;
	}
}
