/*
 * test_encoder.c - Unit tests for H.264 Encoder Module
 *
 * KHÔNG cần camera thật: tạo frame YUYV giả (solid grey) để kiểm tra
 * toàn bộ pipeline encoder_init → encode_frame → flush → destroy.
 */
#include "encoder.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#define WIDTH   640
#define HEIGHT  480
/* YUYV: 2 bytes/pixel */
#define YUYV_SIZE (WIDTH * HEIGHT * 2)

/* Tạo frame YUYV xám đều (Y=128, U=128, V=128) */
static uint8_t *make_grey_yuyv_frame(void)
{
    uint8_t *buf = malloc(YUYV_SIZE);
    assert(buf != NULL);
    /* YUYV pattern [Y0=128, U=128, Y1=128, V=128] */
    for (size_t i = 0; i < YUYV_SIZE; i += 4) {
        buf[i]     = 128; /* Y0 */
        buf[i + 1] = 128; /* U  */
        buf[i + 2] = 128; /* Y1 */
        buf[i + 3] = 128; /* V  */
    }
    return buf;
}

/* -----------------------------------------------------------------------
 * Test 1: encoder_init() thành công với cấu hình hợp lệ
 * ---------------------------------------------------------------------- */
static void test_encoder_init(void)
{
    encoder_cfg_t cfg = {
        .width        = WIDTH,
        .height       = HEIGHT,
        .fps          = 30,
        .bitrate_kbps = 0,
        .crf          = 23,
    };
    h264_encoder_t *enc = encoder_init(&cfg);
    assert(enc != NULL && "encoder_init() should not return NULL");
    encoder_destroy(enc);
    printf("test_encoder_init: PASSED\n");
}

/* -----------------------------------------------------------------------
 * Test 2: encoder_init() với tham số NULL/xấu phải trả NULL
 * ---------------------------------------------------------------------- */
static void test_encoder_init_invalid(void)
{
    h264_encoder_t *enc;

    enc = encoder_init(NULL);
    assert(enc == NULL && "NULL cfg must return NULL");

    encoder_cfg_t bad_cfg = { .width = 0, .height = 0, .fps = 30 };
    enc = encoder_init(&bad_cfg);
    assert(enc == NULL && "zero dimensions must return NULL");

    /* Odd dimensions (I420 requires even) */
    encoder_cfg_t odd_cfg = { .width = 641, .height = 481, .fps = 30 };
    enc = encoder_init(&odd_cfg);
    assert(enc == NULL && "odd dimensions must return NULL");

    printf("test_encoder_init_invalid: PASSED\n");
}

/* -----------------------------------------------------------------------
 * Test 3: encode N frames → file không rỗng
 * ---------------------------------------------------------------------- */
static void test_encode_frames(void)
{
    encoder_cfg_t cfg = {
        .width = WIDTH, .height = HEIGHT, .fps = 30, .crf = 23,
    };
    h264_encoder_t *enc = encoder_init(&cfg);
    assert(enc != NULL);

    const char *tmpfile = "/tmp/test_encoder_out.h264";
    FILE *fp = fopen(tmpfile, "wb");
    assert(fp != NULL);

    uint8_t *frame = make_grey_yuyv_frame();

    /* Encode 10 frame - đủ để x264 xả ít nhất 1 frame ra */
    for (int i = 0; i < 10; i++) {
        int r = encoder_encode_frame(enc, frame, YUYV_SIZE, fp);
        assert(r >= 0 && "encoder_encode_frame must not return error");
    }

    encoder_flush(enc, fp);
    encoder_destroy(enc);
    free(frame);
    fclose(fp);

    /* Kiểm tra file có dữ liệu */
    FILE *check = fopen(tmpfile, "rb");
    assert(check != NULL);
    fseek(check, 0, SEEK_END);
    long sz = ftell(check);
    fclose(check);
    assert(sz > 0 && "Output H.264 file must not be empty");

    printf("test_encode_frames: PASSED (output %ld bytes)\n", sz);
}

/* -----------------------------------------------------------------------
 * Test 4: Kiểm tra Annex-B start code ở đầu file
 * Byte đầu của file .h264 phải là 0x00 0x00 0x00 0x01 (NAL start code)
 * ---------------------------------------------------------------------- */
static void test_start_code(void)
{
    const char *tmpfile = "/tmp/test_encoder_out.h264";
    FILE *fp = fopen(tmpfile, "rb");
    assert(fp != NULL);

    uint8_t header[4];
    size_t n = fread(header, 1, 4, fp);
    fclose(fp);

    assert(n == 4 && "File too small to contain a start code");
    assert(header[0] == 0x00 && "Annex-B: byte 0 must be 0x00");
    assert(header[1] == 0x00 && "Annex-B: byte 1 must be 0x00");
    assert(header[2] == 0x00 && "Annex-B: byte 2 must be 0x00");
    assert(header[3] == 0x01 && "Annex-B: byte 3 must be 0x01");

    printf("test_start_code: PASSED (0x%02x 0x%02x 0x%02x 0x%02x)\n",
           header[0], header[1], header[2], header[3]);
}

/* -----------------------------------------------------------------------
 * Test 5: destroy(NULL) không crash
 * ---------------------------------------------------------------------- */
static void test_destroy_null(void)
{
    encoder_destroy(NULL); /* Không được crash hoặc abort */
    printf("test_destroy_null: PASSED\n");
}

int main(void)
{
    init_logger();

    test_encoder_init();
    test_encoder_init_invalid();
    test_encode_frames();
    test_start_code();
    test_destroy_null();

    printf("All encoder tests PASSED.\n");

    destroy_logger();
    return 0;
}
