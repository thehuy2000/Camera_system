/*
 * rtsp_server.cpp
 *
 * RTSP Server wrapper sử dụng live555. Expose C API từ streaming.h.
 *
 * Kiến trúc:
 *   - g_nal_queue: NAL queue chia sẻ giữa consumer_thread (C) và live355
 *   - g_server_thread: background thread chạy live555 event loop
 *   - H264VideoStreamDiscreteFramer: framer nhận NAL từ H264LiveSource
 *   - H264VideoRTPSink: RTP packetizer
 *   - RTSPServer: RTSP protocol handler (port 8554)
 *
 * Luồng dữ liệu:
 *   consumer_thread → rtsp_server_push_nal() → nal_queue →
 *   H264LiveSource::doGetNextFrame() → H264VideoStreamDiscreteFramer →
 *   H264VideoRTPSink → UDP RTP → VLC
 */

#include "streaming.h"
#include "nal_queue.h"
#include "h264_live_source.hpp"

#include <liveMedia.hh>
#include <BasicUsageEnvironment.hh>
#include <GroupsockHelper.hh>

#include <pthread.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Global state (singleton RTSP server)
 * ------------------------------------------------------------------------- */
static nal_queue_t          g_nal_queue;
static UsageEnvironment    *g_env          = NULL;
static RTSPServer          *g_rtsp_server  = NULL;
static H264LiveSource      *g_live_source  = NULL;
static pthread_t            g_server_thread;
static EventLoopWatchVariable g_event_loop_watch; /* live555 type: atomic_char */
static int                  g_initialized  = 0;

/* -------------------------------------------------------------------------
 * Custom OnDemandServerMediaSubsession for H264 Live Stream
 * ------------------------------------------------------------------------- */
class H264LiveServerMediaSubsession : public OnDemandServerMediaSubsession {
public:
    static H264LiveServerMediaSubsession* createNew(UsageEnvironment& env, bool reuseFirstSource) {
        return new H264LiveServerMediaSubsession(env, reuseFirstSource);
    }

protected:
    H264LiveServerMediaSubsession(UsageEnvironment& env, bool reuseFirstSource)
        : OnDemandServerMediaSubsession(env, reuseFirstSource) {}

    virtual FramedSource* createNewStreamSource(unsigned /*clientSessionId*/,
                                                unsigned& estBitrate) {
        estBitrate = 800; /* kbps */
        H264LiveSource *live_source = H264LiveSource::createNew(envir(), &g_nal_queue);
        return H264VideoStreamDiscreteFramer::createNew(envir(), live_source);
    }

    virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
                                      unsigned char rtpPayloadTypeIfDynamic,
                                      FramedSource* /*inputSource*/) {
        return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
    }
};

/* -------------------------------------------------------------------------
 * RTSP server event loop (chạy trong background thread)
 * ------------------------------------------------------------------------- */
static void *rtsp_event_loop(void *arg)
{
    (void)arg;
    g_env->taskScheduler().doEventLoop(&g_event_loop_watch);
    return NULL;
}

/* -------------------------------------------------------------------------
 * rtsp_server_create()
 * ------------------------------------------------------------------------- */
int rtsp_server_create(int port, int fps, int width, int height,
                       const char *stream_name)
{
    (void)fps; (void)width; (void)height;  /* dùng cho extension sau */

    if (g_initialized) return -1;

    /* Khởi tạo NAL queue */
    if (nal_queue_init(&g_nal_queue) != 0) {
        fprintf(stderr, "[RTSP] nal_queue_init failed\n");
        return -1;
    }

    /* Khởi tạo live555 usage environment */
    TaskScheduler *scheduler = BasicTaskScheduler::createNew();
    g_env = BasicUsageEnvironment::createNew(*scheduler);

    /* Tạo RTSP server */
    g_rtsp_server = RTSPServer::createNew(*g_env, (portNumBits)port);
    if (!g_rtsp_server) {
        *g_env << "[RTSP] Failed to create RTSP server: "
               << g_env->getResultMsg() << "\n";
        nal_queue_destroy(&g_nal_queue);
        return -1;
    }

    /* OutPacketBuffer: đủ lớn cho một NAL 640x480 */
    OutPacketBuffer::maxSize = 300000;

    ServerMediaSession *sms = ServerMediaSession::createNew(
        *g_env, stream_name, stream_name,
        "Live H.264 stream from ELCS Camera System");

    /* Sử dụng OnDemandServerMediaSubsession với reuseFirstSource=true
     * để luồng được chia sẻ cho nhiều client từ cùng 1 NAL queue. */
    sms->addSubsession(H264LiveServerMediaSubsession::createNew(*g_env, true));
    g_rtsp_server->addServerMediaSession(sms);

    /* In URL ra stderr */
    char *url = g_rtsp_server->rtspURL(sms);
    fprintf(stderr, "[RTSP] Server ready. Watch at: %s\n", url);
    delete[] url;

    g_initialized = 1;
    return 0;
}

/* -------------------------------------------------------------------------
 * rtsp_server_start()
 * ------------------------------------------------------------------------- */
int rtsp_server_start(void)
{
    if (!g_initialized) return -1;
    g_event_loop_watch = 0;
    if (pthread_create(&g_server_thread, NULL, rtsp_event_loop, NULL) != 0) {
        fprintf(stderr, "[RTSP] Failed to create event loop thread\n");
        return -1;
    }
    return 0;
}

/* -------------------------------------------------------------------------
 * rtsp_server_push_nal()
 * ------------------------------------------------------------------------- */
int rtsp_server_push_nal(const uint8_t *data, size_t size)
{
    if (!g_initialized) return -1;
    return nal_queue_push(&g_nal_queue, data, size);
}

/* -------------------------------------------------------------------------
 * rtsp_server_stop()
 * ------------------------------------------------------------------------- */
void rtsp_server_stop(void)
{
    if (!g_initialized) return;

    /* Yêu cầu live555 event loop thoát */
    g_event_loop_watch = 1;

    /* Unblock bất kỳ push_nal() đang chờ */
    nal_queue_close(&g_nal_queue);

    pthread_join(g_server_thread, NULL);
}

/* -------------------------------------------------------------------------
 * rtsp_server_destroy()
 * ------------------------------------------------------------------------- */
void rtsp_server_destroy(void)
{
    if (!g_initialized) return;

    /* live555 dọn dẹp các object qua Medium::close + scheduler */
    if (g_rtsp_server) {
        Medium::close(g_rtsp_server);
        g_rtsp_server = NULL;
    }

    /* Cleanup scheduler + env */
    if (g_env) {
        TaskScheduler *sched = &g_env->taskScheduler();
        g_env->reclaim();
        g_env = NULL;
        delete sched;
    }

    nal_queue_destroy(&g_nal_queue);
    g_initialized = 0;
    fprintf(stderr, "[RTSP] Server destroyed.\n");
}
