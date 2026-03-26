/*
 * h264_live_source.cpp
 *
 * Custom live555 FramedSource đọc H.264 NAL units từ NAL queue
 * được fill bởi consumer thread. Mỗi lần live555 cần frame mới,
 * nó gọi doGetNextFrame(). Ta dùng scheduled task để poll queue
 * mà không block event loop.
 */

#include "h264_live_source.hpp"
#include <string.h>

#define POLL_INTERVAL_US 5000  /* Poll mỗi 5ms nếu queue trống */

H264LiveSource *H264LiveSource::createNew(UsageEnvironment &env,
                                          nal_queue_t      *queue)
{
    return new H264LiveSource(env, queue);
}

H264LiveSource::H264LiveSource(UsageEnvironment &env, nal_queue_t *queue)
    : FramedSource(env), fQueue(queue), fEventTriggerId(0)
{
    /* Tạo event trigger để consumer thread có thể "thức" source */
    fEventTriggerId = envir().taskScheduler().createEventTrigger(
        H264LiveSource::onNewNAL);
}

H264LiveSource::~H264LiveSource()
{
    if (fEventTriggerId != 0) {
        envir().taskScheduler().deleteEventTrigger(fEventTriggerId);
        fEventTriggerId = 0;
    }
}

void H264LiveSource::doGetNextFrame()
{
    /* Thử lấy ngay không block */
    size_t nal_size = 0;
    const uint8_t *nal_data = nal_queue_try_pop(fQueue, &nal_size);

    if (nal_data && nal_size > 0) {
        deliverNAL(nal_data, nal_size);
    } else {
        /* Queue trống: schedule lại sau POLL_INTERVAL_US */
        envir().taskScheduler().scheduleDelayedTask(
            POLL_INTERVAL_US,
            (TaskFunc *)H264LiveSource::pollCallback,
            this);
    }
}

void H264LiveSource::doStopGettingFrames()
{
    FramedSource::doStopGettingFrames();
}

void H264LiveSource::deliverNAL(const uint8_t *data, size_t size)
{
    /* Skip start code (0x00 0x00 0x01 or 0x00 0x00 0x00 0x01) */
    size_t startCodeSize = 0;
    if (size >= 4 && data[0] == 0 && data[1] == 0 && data[2] == 0 && data[3] == 1) {
        startCodeSize = 4;
    } else if (size >= 3 && data[0] == 0 && data[1] == 0 && data[2] == 1) {
        startCodeSize = 3;
    }

    if (startCodeSize > 0) {
        data += startCodeSize;
        size -= startCodeSize;
    }

    if (size == 0) return;

    if (size > fMaxSize) {
        fNumTruncatedBytes = size - fMaxSize;
        size = fMaxSize;
    } else {
        fNumTruncatedBytes = 0;
    }

    memcpy(fTo, data, size);
    fFrameSize = size;

    /* Lấy thời gian hệ thống làm presentation timestamp */
    gettimeofday(&fPresentationTime, NULL);

    /* Thông báo cho live555 rằng frame đã sẵn sàng */
    afterGetting(this);
}

/* Static callback được schedule bởi event loop khi poll interval hết */
void H264LiveSource::pollCallback(void *clientData)
{
    H264LiveSource *self = static_cast<H264LiveSource*>(clientData);
    if (!self->isCurrentlyAwaitingData()) return;

    size_t nal_size = 0;
    const uint8_t *nal_data = nal_queue_try_pop(self->fQueue, &nal_size);

    if (nal_data && nal_size > 0) {
        self->deliverNAL(nal_data, nal_size);
    } else {
        /* Vẫn trống, schedule tiếp */
        self->envir().taskScheduler().scheduleDelayedTask(
            POLL_INTERVAL_US,
            (TaskFunc *)H264LiveSource::pollCallback,
            self);
    }
}

/* Static callback cho event trigger (gọi từ consumer thread) */
void H264LiveSource::onNewNAL(void *clientData)
{
    H264LiveSource *self = static_cast<H264LiveSource*>(clientData);
    if (!self->isCurrentlyAwaitingData()) return;
    /* Gọi lại doGetNextFrame để lấy dữ liệu */
    self->doGetNextFrame();
}

EventTriggerId H264LiveSource::getEventTriggerId() const
{
    return fEventTriggerId;
}
