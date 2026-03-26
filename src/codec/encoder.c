#include "encoder.h"
#include "logger.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <x264.h>

/*
 * encoder.c - H.264 Encoder Implementation
 *
 * Pipeline per frame:
 *  1. YUYV (4:2:2 packed) → I420 (4:2:0 planar)  [yuyv_to_i420()]
 *  2. I420 → x264_picture_t
 *  3. x264_encoder_encode() → NAL units
 *  4. Write NAL units (Annex-B) to FILE*
 */

struct h264_encoder {
    x264_t          *handle;    /* x264 encoder context     */
    x264_picture_t   pic_in;    /* Input picture (I420)     */
    x264_picture_t   pic_out;   /* Output picture (from x264) */
    int              width;
    int              height;
    int64_t          frame_idx; /* Monotonically increasing PTS */
};

/* ---------------------------------------------------------------------------
 * yuyv_to_i420() - Chuyển đổi YUYV (4:2:2) sang I420 (4:2:0)
 *
 * Layout YUYV (packed, 2 byte/pixel):
 *   [Y0 U0 Y1 V0] [Y2 U2 Y3 V2] ...
 *
 * Layout I420 (planar):
 *   Y plane  : width * height bytes
 *   Cb plane : (width/2) * (height/2) bytes
 *   Cr plane : (width/2) * (height/2) bytes
 *
 * Chiến lược downsample UV: ưu tiên tốc độ, lấy mẫu từ row chẵn.
 * ---------------------------------------------------------------------------
 */
static void yuyv_to_i420(const uint8_t *yuyv,
                          uint8_t       *y_plane,
                          uint8_t       *u_plane,
                          uint8_t       *v_plane,
                          int            width,
                          int            height)
{
    int row, col;
    /* 1 pixel YUYV = 2 bytes; 2 pixels = 4 bytes [Y0 U0 Y1 V0] */
    const int yuyv_stride = width * 2;

    for (row = 0; row < height; row++) {
        const uint8_t *src = yuyv + row * yuyv_stride;
        uint8_t       *y   = y_plane + row * width;

        /* Lấy Y plane cho toàn bộ row */
        for (col = 0; col < width; col += 2) {
            y[col]     = src[col * 2];       /* Y0 */
            y[col + 1] = src[col * 2 + 2];  /* Y1 */
        }

        /* Lấy UV plane chỉ ở các row chẵn (downsample 2:1 theo vertical) */
        if ((row & 1) == 0) {
            uint8_t *u = u_plane + (row / 2) * (width / 2);
            uint8_t *v = v_plane + (row / 2) * (width / 2);

            for (col = 0; col < width; col += 2) {
                u[col / 2] = src[col * 2 + 1]; /* U0 */
                v[col / 2] = src[col * 2 + 3]; /* V0 */
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * write_nals() - Ghi NAL units ra FILE* (format Annex-B)
 *
 * x264 đặt start code (0x00 0x00 0x00 0x01) ở đầu mỗi NAL khi b_annexb=1.
 * Chúng ta chỉ cần ghi thẳng p_payload với i_payload bytes.
 * ---------------------------------------------------------------------------
 */
static int write_nals(x264_nal_t *nals, int n_nals, FILE *fp)
{
    int i;
    int total = 0;
    for (i = 0; i < n_nals; i++) {
        size_t written = fwrite(nals[i].p_payload, 1,
                                (size_t)nals[i].i_payload, fp);
        if ((int)written != nals[i].i_payload) {
            return -1;
        }
        total += nals[i].i_payload;
    }
    return total;
}

/* ---------------------------------------------------------------------------
 * encoder_init()
 * ---------------------------------------------------------------------------
 */
h264_encoder_t *encoder_init(const encoder_cfg_t *cfg)
{
    x264_param_t param;
    h264_encoder_t *enc;

    if (!cfg || cfg->width <= 0 || cfg->height <= 0) {
        LOG_ERROR("encoder_init: invalid configuration");
        return NULL;
    }

    /* Kiểm tra width/height phải chẵn (I420 yêu cầu) */
    if ((cfg->width & 1) || (cfg->height & 1)) {
        LOG_ERROR("encoder_init: width/height must be even (I420 constraint)");
        return NULL;
    }

    enc = calloc(1, sizeof(*enc));
    if (!enc) {
        LOG_ERROR("encoder_init: out of memory");
        return NULL;
    }
    enc->width  = cfg->width;
    enc->height = cfg->height;

    /* ----------------------------------------------------------------
     * Cấu hình x264 parameters
     * ---------------------------------------------------------------- */
    /* ultrafast + zerolatency: tốt nhất cho camera real-time */
    if (x264_param_default_preset(&param, "ultrafast", "zerolatency") < 0) {
        LOG_ERROR("encoder_init: x264_param_default_preset failed");
        free(enc);
        return NULL;
    }

    /* Dimensions */
    param.i_width  = cfg->width;
    param.i_height = cfg->height;

    /* Timebase và FPS */
    param.i_fps_num = (uint32_t)(cfg->fps > 0 ? cfg->fps : 30);
    param.i_fps_den = 1;
    param.i_timebase_num = 1;
    param.i_timebase_den = param.i_fps_num;

    /* Input colour space: I420 */
    param.i_csp = X264_CSP_I420;

    /* Rate control */
    if (cfg->bitrate_kbps > 0) {
        param.rc.i_rc_method   = X264_RC_ABR;
        param.rc.i_bitrate     = cfg->bitrate_kbps;
        LOG_INFO("Encoder: ABR mode, bitrate=%d kbps", cfg->bitrate_kbps);
    } else {
        param.rc.i_rc_method   = X264_RC_CRF;
        param.rc.f_rf_constant = (float)(cfg->crf > 0 ? cfg->crf : 23);
        LOG_INFO("Encoder: CRF mode, crf=%.0f", param.rc.f_rf_constant);
    }

    /* Annex-B framing (start codes) */
    param.b_annexb           = 1;
    /* Repeat SPS/PPS với mỗi IDR (dễ seek) */
    param.b_repeat_headers   = 1;

    /* Áp dụng H.264 Baseline profile (tương thích rộng nhất) */
    if (x264_param_apply_profile(&param, "baseline") < 0) {
        LOG_ERROR("encoder_init: x264_param_apply_profile failed");
        free(enc);
        return NULL;
    }

    /* Tạo encoder handle */
    enc->handle = x264_encoder_open(&param);
    if (!enc->handle) {
        LOG_ERROR("encoder_init: x264_encoder_open failed");
        free(enc);
        return NULL;
    }

    /* Allocate input picture buffer (I420) */
    if (x264_picture_alloc(&enc->pic_in, X264_CSP_I420,
                           cfg->width, cfg->height) < 0) {
        LOG_ERROR("encoder_init: x264_picture_alloc failed");
        x264_encoder_close(enc->handle);
        free(enc);
        return NULL;
    }

    LOG_INFO("H.264 encoder initialized: %dx%d @ %d fps",
             cfg->width, cfg->height,
             cfg->fps > 0 ? cfg->fps : 30);
    return enc;
}

/* ---------------------------------------------------------------------------
 * encoder_encode_frame()
 * ---------------------------------------------------------------------------
 */
int encoder_encode_frame(h264_encoder_t *enc,
                         const void     *yuyv_data,
                         size_t          yuyv_size,
                         FILE           *fp)
{
    x264_nal_t *nals;
    int         n_nals;
    int         frame_size;

    if (!enc || !yuyv_data || !fp) {
        return -1;
    }

    /* Validate input size */
    size_t expected = (size_t)(enc->width * enc->height * 2);
    if (yuyv_size < expected) {
        LOG_ERROR("encoder_encode_frame: buffer too small (%zu < %zu)",
                  yuyv_size, expected);
        return -1;
    }

    /* Bước 1: Chuyển đổi YUYV → I420 trực tiếp vào pic_in planes */
    yuyv_to_i420((const uint8_t *)yuyv_data,
                 enc->pic_in.img.plane[0],   /* Y  */
                 enc->pic_in.img.plane[1],   /* Cb */
                 enc->pic_in.img.plane[2],   /* Cr */
                 enc->width,
                 enc->height);

    /* Gán PTS (Presentation Timestamp) */
    enc->pic_in.i_pts = enc->frame_idx++;

    /* Bước 2: Encode */
    frame_size = x264_encoder_encode(enc->handle, &nals, &n_nals,
                                     &enc->pic_in, &enc->pic_out);
    if (frame_size < 0) {
        LOG_ERROR("encoder_encode_frame: x264_encoder_encode failed");
        return -1;
    }

    if (frame_size == 0) {
        /* Frame bị delay trong bộ đệm của x264 - bình thường */
        return 0;
    }

    /* Bước 3: Ghi NAL units ra file */
    if (write_nals(nals, n_nals, fp) < 0) {
        LOG_ERROR("encoder_encode_frame: write_nals failed");
        return -1;
    }

    return n_nals;
}

/* ---------------------------------------------------------------------------
 * encoder_encode_frame_cb() - Encode một frame, gọi callback với mỗi NAL
 * ---------------------------------------------------------------------------
 */
int encoder_encode_frame_cb(h264_encoder_t *enc,
                            const void     *yuyv_data,
                            size_t          yuyv_size,
                            nal_cb_t        cb,
                            void           *userdata)
{
    x264_nal_t *nals;
    int         n_nals;
    int         frame_size;
    int         i;

    if (!enc || !yuyv_data || !cb) return -1;

    size_t expected = (size_t)(enc->width * enc->height * 2);
    if (yuyv_size < expected) {
        LOG_ERROR("encoder_encode_frame_cb: buffer too small (%zu < %zu)",
                  yuyv_size, expected);
        return -1;
    }

    yuyv_to_i420((const uint8_t *)yuyv_data,
                 enc->pic_in.img.plane[0],
                 enc->pic_in.img.plane[1],
                 enc->pic_in.img.plane[2],
                 enc->width, enc->height);

    enc->pic_in.i_pts = enc->frame_idx++;

    frame_size = x264_encoder_encode(enc->handle, &nals, &n_nals,
                                     &enc->pic_in, &enc->pic_out);
    if (frame_size < 0) {
        LOG_ERROR("encoder_encode_frame_cb: x264_encoder_encode failed");
        return -1;
    }

    if (frame_size == 0) return 0; /* delay buffered, bình thường */

    /* Gọi callback cho từng NAL unit */
    for (i = 0; i < n_nals; i++) {
        cb((const void *)nals[i].p_payload,
           (size_t)nals[i].i_payload,
           userdata);
    }

    return n_nals;
}

/* ---------------------------------------------------------------------------
 * encoder_flush()
 * ---------------------------------------------------------------------------
 */
int encoder_flush(h264_encoder_t *enc, FILE *fp)
{
    x264_nal_t *nals;
    int         n_nals;
    int         frame_size;
    int         total_nals = 0;

    if (!enc || !fp)
        return -1;

    /* x264 có thể buffer vài frame; flush cho đến khi trả về 0 */
    while (x264_encoder_delayed_frames(enc->handle) > 0) {
        frame_size = x264_encoder_encode(enc->handle, &nals, &n_nals,
                                         NULL, &enc->pic_out);
        if (frame_size < 0) {
            LOG_ERROR("encoder_flush: encode error during flush");
            return -1;
        }
        if (frame_size > 0) {
            if (write_nals(nals, n_nals, fp) < 0) {
                LOG_ERROR("encoder_flush: write_nals failed");
                return -1;
            }
            total_nals += n_nals;
        }
    }

    LOG_INFO("Encoder flush complete. %d delayed NAL unit(s) written.", total_nals);
    return total_nals;
}

/* ---------------------------------------------------------------------------
 * encoder_destroy()
 * ---------------------------------------------------------------------------
 */
void encoder_destroy(h264_encoder_t *enc)
{
    if (!enc)
        return;

    x264_picture_clean(&enc->pic_in);

    if (enc->handle)
        x264_encoder_close(enc->handle);

    free(enc);
    LOG_INFO("H.264 encoder destroyed.");
}
