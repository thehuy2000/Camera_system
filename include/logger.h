#ifndef LOGGER_H
#define LOGGER_H

typedef enum {
	LOG_LVL_DEBUG,
	LOG_LVL_INFO,
	LOG_LVL_WARN,
	LOG_LVL_ERROR
} log_level_t;

/*
 * init_logger() - Khởi tạo logging framework (mutex/lock)
 * Return: 0 nếu OK, -1 nếu bị lỗi
 */
int init_logger(void);

/*
 * logger_log() - Hàm nội bộ của framework, không gọi trực tiếp.
 */
void logger_log(log_level_t level, const char *file, int line, const char *fmt, ...);

/*
 * destroy_logger() - Giải phóng tài nguyên của logging framework.
 */
void destroy_logger(void);

/* MACROS để gọi log với tự động get FILE:LINE */
#define LOG_DEBUG(...) logger_log(LOG_LVL_DEBUG, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  logger_log(LOG_LVL_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  logger_log(LOG_LVL_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) logger_log(LOG_LVL_ERROR, __FILE__, __LINE__, __VA_ARGS__)

#endif /* LOGGER_H */
