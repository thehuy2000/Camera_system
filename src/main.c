#include "logger.h"
#include "camera.h"
#include "mem_pool.h"
#include "ring_buff.h"
#include "encoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include <linux/videodev2.h>
#include <time.h>
#include <signal.h>

#define POOL_BLOCKS 5
/* Giả định độ phân giải 640x480 dạng YUYV 16 bit -> Cần khoảng 600KB/frame */
#define MAX_FRAME_SIZE (640 * 480 * 2) 

/* Các module nội bộ */
static mem_pool_t *g_pool = NULL;
static ring_buff_t *g_ring = NULL;
static cam_dev_t *g_cam = NULL;

static volatile bool g_running = true;

/* Bắt tín hiệu Ctrl+C để thoát an toàn */
void sigint_handler(int sig)
{
	(void)sig;
	g_running = false;
	LOG_WARN("Caught SIGINT, stopping processes...");
}

/* 
 * Chế độ 1: Snapshot 
 * Lấy một frame duy nhất và ghi thành file ảnh (Raw).
 */
static int do_snapshot(const char *dev_name)
{
	g_cam = cam_open(dev_name);
	if (!g_cam) return -1;

	/* Thử format YUYV - chuẩn thông dụng nhất */
	if (cam_init(g_cam, 640, 480, V4L2_PIX_FMT_YUYV) < 0) {
		cam_close(g_cam);
		return -1;
	}

	void *buffer = malloc(MAX_FRAME_SIZE);
	if (!buffer) {
		LOG_ERROR("Failed to allocate frame buffer for snapshot");
		cam_close(g_cam);
		return -1;
	}

	size_t bytes_read = 0;
	LOG_INFO("Taking snapshot... Dropping the first 50 frames.");
	
	for (int i = 0; i < 50; i++) {
		cam_get_frame(g_cam, buffer, MAX_FRAME_SIZE, &bytes_read);
	}

	LOG_INFO("Capturing the 51th frame...");
	if (cam_get_frame(g_cam, buffer, MAX_FRAME_SIZE, &bytes_read) < 0 || bytes_read == 0) {
		LOG_ERROR("Failed to capture snapshot frame");
		free(buffer);
		cam_close(g_cam);
		return -1;
	}

	/* Đặt tên file bằng thời gian */
	time_t t = time(NULL);
	struct tm *tm_info = localtime(&t);
	char filename[64];
	strftime(filename, sizeof(filename), "output/snap_%Y%m%d_%H%M%S.raw", tm_info);

	FILE *fp = fopen(filename, "wb");
	if (fp) {
		fwrite(buffer, 1, bytes_read, fp);
		fclose(fp);
		LOG_INFO("Snapshot saved to: %s", filename);
	} else {
		LOG_ERROR("Could not open file %s for writing", filename);
	}

	free(buffer);
	cam_close(g_cam);
	return 0;
}

/* 
 * Context truyền qua lại giữa các threads trong Video Recording Mode 
 */
struct record_context {
	FILE            *out_file;
	h264_encoder_t  *encoder;       /* NULL = raw mode, non-NULL = H.264 mode */
	size_t           frames_captured;
	size_t           frames_written;
};

/* Luồng lấy hình ảnh */
static void *producer_thread(void *arg)
{
	struct record_context *ctx = (struct record_context *)arg;
	void *frame_ptr;
	size_t bytes_read;

	LOG_INFO("Producer thread started.");

	/* Bỏ qua 50 frame đầu để camera ổn định (AWB/AE) */
	void *drop_buf = malloc(MAX_FRAME_SIZE);
	if (drop_buf) {
		size_t drop_bytes;
		LOG_INFO("Dropping first 50 frames...");
		for (int i = 0; i < 50; i++) {
			cam_get_frame(g_cam, drop_buf, MAX_FRAME_SIZE, &drop_bytes);
		}
		free(drop_buf);
	}
	LOG_INFO("Start capturing frames...");

	while (g_running) {
		/* Chờ cho tới khi có block rảnh; không bao giờ bỏ frame */
		frame_ptr = pool_alloc_blocking(g_pool);

		/* Timeout đọc tự xử lý bên trong cam_get_frame */
		if (cam_get_frame(g_cam, frame_ptr, MAX_FRAME_SIZE, &bytes_read) < 0) {
			pool_free(g_pool, frame_ptr);
			continue;
		}

		if (bytes_read == 0) {
			/* Timeout hoặc bị interrupt */
			pool_free(g_pool, frame_ptr);
			continue;
		}

		/* Lưu kích thước data ở 4 bytes đầu của block để Consumer biết (hack nhỏ) */
		*(uint32_t *)frame_ptr = bytes_read;

		/* Đẩy payload vào Ring Buffer */
		ring_buff_push(g_ring, frame_ptr);
		ctx->frames_captured++;
	}

	/* Signal consumer to stop */
	void *eof_marker = pool_alloc(g_pool);
	if (eof_marker) {
		*(uint32_t *)eof_marker = 0; /* Đánh dấu đây là frame rỗng báo hiệu EOF */
		ring_buff_push(g_ring, eof_marker);
	}

	LOG_INFO("Producer thread exiting.");
	return NULL;
}

/* Luồng xử lý và ghi đĩa */
static void *consumer_thread(void *arg)
{
	struct record_context *ctx = (struct record_context *)arg;
	void *frame_ptr;
	uint32_t payload_size;

	LOG_INFO("Consumer thread started.");

	while (1) {
		ring_buff_pop(g_ring, &frame_ptr);
		payload_size = *(uint32_t *)frame_ptr;

		if (payload_size == 0) {
			/* Đã nhận cờ hiệu EOF từ producer, dừng vòng lặp */
			pool_free(g_pool, frame_ptr);
			break;
		}

		/* Bỏ qua 4 byte dung lượng đầu (offset 4 byte) để ghi data gốc */
		if (ctx->encoder) {
			/* H.264 encode mode */
			encoder_encode_frame(ctx->encoder,
			                     (char *)frame_ptr + 4,
			                     payload_size,
			                     ctx->out_file);
		} else {
			/* Raw dump mode */
			fwrite((char *)frame_ptr + 4, 1, payload_size, ctx->out_file);
		}
		ctx->frames_written++;

		pool_free(g_pool, frame_ptr);
	}

	LOG_INFO("Consumer thread exiting.");
	return NULL;
}

/*
 * Chế độ 2: Video Recording (Raw stream) đa luồng
 */
static int do_record(const char *dev_name)
{
	pthread_t prod_tid, cons_tid;
	struct record_context ctx = {0};

	/* Khởi tạo Core */
	g_pool = pool_init(POOL_BLOCKS, MAX_FRAME_SIZE + 4); /* +4 byte lưu payload size */
	if (!g_pool) {
		LOG_ERROR("Failed to init memory pool");
		return -1;
	}
	
	g_ring = ring_buff_init(POOL_BLOCKS);
	if (!g_ring) {
		LOG_ERROR("Failed to init ring buffer");
		pool_destroy(g_pool);
		return -1;
	}

	/* Mở tệp ghi Video (vẫn dùng Raw thay avi để đơn giản demo architecture) */
	time_t t = time(NULL);
	struct tm *tm_info = localtime(&t);
	char filename[64];
	strftime(filename, sizeof(filename), "output/vid_%Y%m%d_%H%M%S.raw", tm_info);

	ctx.out_file = fopen(filename, "wb");
	if (!ctx.out_file) {
		LOG_ERROR("Cannot open file %s for video writing", filename);
		goto cleanup_core;
	}

	/* Khởi tạo Camera */
	g_cam = cam_open(dev_name);
	if (!g_cam) goto cleanup_file;

	if (cam_init(g_cam, 640, 480, V4L2_PIX_FMT_YUYV) < 0) {
		goto cleanup_cam;
	}

	/* Bắt đầu Multithreading Loop */
	LOG_INFO("Starting threads. Press Ctrl+C to stop.");
	pthread_create(&prod_tid, NULL, producer_thread, &ctx);
	pthread_create(&cons_tid, NULL, consumer_thread, &ctx);

	/* Chờ tín hiệu kết thúc */
	pthread_join(prod_tid, NULL);
	pthread_join(cons_tid, NULL);

	LOG_INFO("Recording finished. Captured: %zu, Written: %zu frames", 
	         ctx.frames_captured, ctx.frames_written);

cleanup_cam:
	cam_close(g_cam);
cleanup_file:
	if(ctx.out_file) fclose(ctx.out_file);
cleanup_core:
	ring_buff_destroy(g_ring);
	pool_destroy(g_pool);
	return 0;
}

/*
 * Chế độ 3: H.264 Encode - giống Record nhưng Consumer nén qua libx264
 */
static int do_encode(const char *dev_name)
{
	pthread_t prod_tid, cons_tid;
	struct record_context ctx = {0};
	encoder_cfg_t enc_cfg = {
		.width        = 640,
		.height       = 480,
		.fps          = DEFAULT_FPS,
		.bitrate_kbps = 0,   /* CRF mode */
		.crf          = 23,
	};

	/* Khởi tạo Core */
	g_pool = pool_init(POOL_BLOCKS, MAX_FRAME_SIZE + 4);
	if (!g_pool) {
		LOG_ERROR("Failed to init memory pool");
		return -1;
	}

	g_ring = ring_buff_init(POOL_BLOCKS);
	if (!g_ring) {
		LOG_ERROR("Failed to init ring buffer");
		pool_destroy(g_pool);
		return -1;
	}

	/* Khởi tạo Encoder */
	ctx.encoder = encoder_init(&enc_cfg);
	if (!ctx.encoder) {
		LOG_ERROR("Failed to init H.264 encoder");
		ring_buff_destroy(g_ring);
		pool_destroy(g_pool);
		return -1;
	}

	/* Mở tệp ghi H.264 */
	time_t t = time(NULL);
	struct tm *tm_info = localtime(&t);
	char filename[64];
	strftime(filename, sizeof(filename), "output/vid_%Y%m%d_%H%M%S.h264", tm_info);

	ctx.out_file = fopen(filename, "wb");
	if (!ctx.out_file) {
		LOG_ERROR("Cannot open file %s for H.264 writing", filename);
		goto encode_cleanup_encoder;
	}

	/* Khởi tạo Camera */
	g_cam = cam_open(dev_name);
	if (!g_cam) goto encode_cleanup_file;

	if (cam_init(g_cam, 640, 480, V4L2_PIX_FMT_YUYV) < 0)
		goto encode_cleanup_cam;

	/* Bắt đầu Multithreading Loop */
	LOG_INFO("Starting H.264 encode session. Press Ctrl+C to stop.");
	pthread_create(&prod_tid, NULL, producer_thread, &ctx);
	pthread_create(&cons_tid, NULL, consumer_thread, &ctx);

	pthread_join(prod_tid, NULL);
	pthread_join(cons_tid, NULL);

	/* Flush các frame còn lại trong bộ đệm x264 */
	encoder_flush(ctx.encoder, ctx.out_file);

	LOG_INFO("Encode finished. Captured: %zu, Written: %zu frames -> %s",
	         ctx.frames_captured, ctx.frames_written, filename);

encode_cleanup_cam:
	cam_close(g_cam);
encode_cleanup_file:
	if (ctx.out_file) fclose(ctx.out_file);
encode_cleanup_encoder:
	encoder_destroy(ctx.encoder);
	ring_buff_destroy(g_ring);
	pool_destroy(g_pool);
	return 0;
}

static void print_usage(const char *prog)
{
	printf("Usage: %s [snapshot|record|encode] [device]\n", prog);
	printf("  snapshot : Take a single picture (raw YUYV)\n");
	printf("  record   : Continuously record raw video until Ctrl+C\n");
	printf("  encode   : Record and encode to H.264 (.h264) until Ctrl+C\n");
	printf("  device   : Optional camera device (default: /dev/video0)\n");
}

int main(int argc, char **argv)
{
	init_logger();

	if (argc < 2) {
		print_usage(argv[0]);
		destroy_logger();
		return 1;
	}

	const char *mode = argv[1];
	const char *dev = (argc >= 3) ? argv[2] : DEFAULT_CAM_DEV;

	/* Gắn handler cho Ctrl+C để thoát Record an toàn */
	signal(SIGINT, sigint_handler);

	if (strcmp(mode, "snapshot") == 0) {
		do_snapshot(dev);
	} else if (strcmp(mode, "record") == 0) {
		do_record(dev);
	} else if (strcmp(mode, "encode") == 0) {
		do_encode(dev);
	} else {
		LOG_ERROR("Unknown mode: %s", mode);
		print_usage(argv[0]);
	}

	destroy_logger();
	return 0;
}
