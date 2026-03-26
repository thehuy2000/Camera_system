#ifndef H264_LIVE_SOURCE_HPP
#define H264_LIVE_SOURCE_HPP

/*
 * h264_live_source.hpp
 *
 * Custom live555 FramedSource cung cấp H.264 NAL units từ NAL queue
 * cho RTSP/RTP pipeline.
 */

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include "nal_queue.h"

class H264LiveSource : public FramedSource {
public:
    static H264LiveSource *createNew(UsageEnvironment &env,
                                     nal_queue_t      *queue);

    EventTriggerId getEventTriggerId() const;

protected:
    H264LiveSource(UsageEnvironment &env, nal_queue_t *queue);
    virtual ~H264LiveSource();

    virtual void doGetNextFrame();
    virtual void doStopGettingFrames();

private:
    void deliverNAL(const uint8_t *data, size_t size);

    static void pollCallback(void *clientData);
    static void onNewNAL(void *clientData);

    nal_queue_t    *fQueue;
    EventTriggerId  fEventTriggerId;
};

#endif /* H264_LIVE_SOURCE_HPP */
