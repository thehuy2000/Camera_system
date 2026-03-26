#ifndef ENCODER_H
#define ENCODER_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * encoder.h - H.264 Encoder Module Interface
 *
 * Sử dụng libx264 để nén raw YUYV frames thành H.264 NAL units (Annex-B).
 * Pipeline: YUYV (4:2:2 packed) → I420 (4:2:0 planar) → x264 → .h264 file
 */

/* Opaque encoder handle */
typedef struct h264_encoder h264_encoder_t;

/*
 * encoder_cfg_t - Cấu hình encoder
 *
 * @width       : Chiều rộng frame (px), phải khớp với camera
 * @height      : Chiều cao frame (px), phải khớp với camera
 * @fps         : Framerate mục tiêu (dùng để tính timestamps)
 * @bitrate_kbps: Target bitrate (kbps). 0 = dùng CRF mode.
 * @crf         : Constant Rate Factor [0–51]. 23 = mặc định tốt;
 *                giá trị nhỏ hơn = chất lượng cao hơn / file to hơn.
 *                Chỉ có hiệu lực khi bitrate_kbps == 0.
 */
typedef struct {
    int width;
    int height;
    int fps;
    int bitrate_kbps;
    int crf;
} encoder_cfg_t;

/*
 * encoder_init() - Khởi tạo encoder context
 * @cfg: Con trỏ tới cấu hình encoder. Không được NULL.
 *
 * Return: Con trỏ encoder nếu thành công, NULL nếu thất bại.
 *
 * Side-effect: Log thông tin cấu hình thông qua logger module.
 */
h264_encoder_t *encoder_init(const encoder_cfg_t *cfg);

/*
 * encoder_encode_frame() - Nén một YUYV frame thành H.264 NAL units
 * @enc       : Encoder handle từ encoder_init()
 * @yuyv_data : Con trỏ tới dữ liệu YUYV thô (kích thước = width * height * 2)
 * @yuyv_size : Kích thước thực tế của buffer YUYV (bytes)
 * @fp        : File đầu ra để ghi NAL units (Annex-B format)
 *
 * Return: Số NAL units đã ghi >= 0 nếu thành công, -1 nếu lỗi.
 *
 * Lưu ý: Hàm này KHÔNG thread-safe trên cùng một encoder instance.
 *         Mỗi luồng recording phải có encoder riêng.
 */
int encoder_encode_frame(h264_encoder_t *enc,
                         const void     *yuyv_data,
                         size_t          yuyv_size,
                         FILE           *fp);

/*
 * encoder_encode_frame_cb() - Giống encode_frame() nhưng gọi callback thay vì ghi file
 *
 * @enc       : Encoder handle
 * @yuyv_data : YUYV raw frame
 * @yuyv_size : Kích thước YUYV buffer (bytes)
 * @cb        : Hàm callback được gọi với mỗi NAL unit
 *              Prototype: void cb(const uint8_t *data, size_t size, void *userdata)
 * @userdata  : Con trỏ tùy ý truyền qua cho callback
 *
 * Return: Số NAL units >= 0, -1 nếu lỗi.
 */
/*
 * nal_cb_t - Callback type dùng trong encoder_encode_frame_cb()
 * Gưu ý: dùng `const void *` để tránh cảnh báo khi cast từ C sang C++.
 */
typedef void (*nal_cb_t)(const void *data, size_t size, void *userdata);

int encoder_encode_frame_cb(h264_encoder_t *enc,
                            const void     *yuyv_data,
                            size_t          yuyv_size,
                            nal_cb_t        cb,
                            void           *userdata);

/*
 * encoder_flush() - Flush các frames đang delay trong bộ đệm của x264
 * @enc: Encoder handle
 * @fp : File đầu ra (phải cùng file đã dùng ở encode_frame)
 *
 * PHẢI gọi hàm này trước khi đóng file để đảm bảo không mất frame cuối.
 *
 * Return: Số NAL units được flush >= 0, -1 nếu lỗi.
 */
int encoder_flush(h264_encoder_t *enc, FILE *fp);

/*
 * encoder_destroy() - Giải phóng toàn bộ tài nguyên encoder
 * @enc: Encoder handle (có thể là NULL, hàm sẽ kiểm tra)
 */
void encoder_destroy(h264_encoder_t *enc);

#endif /* ENCODER_H */
