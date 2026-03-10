#include "camera.h"
#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <errno.h>

struct mmap_buffer {
	void *start;
	size_t length;
};

struct camera_device {
	int fd;
	char dev_name[64];
	struct mmap_buffer *buffers;
	unsigned int n_buffers;
};

cam_dev_t *cam_open(const char *dev_name)
{
	cam_dev_t *cam;
	const char *target = dev_name ? dev_name : DEFAULT_CAM_DEV;

	cam = calloc(1, sizeof(*cam));
	if (!cam)
		return NULL;

	strncpy(cam->dev_name, target, sizeof(cam->dev_name) - 1);

	cam->fd = open(target, O_RDWR | O_NONBLOCK, 0);
	if (cam->fd < 0) {
		LOG_ERROR("Cannot open device %s: %s", target, strerror(errno));
		free(cam);
		return NULL;
	}

	LOG_INFO("Opened camera device: %s (fd=%d)", target, cam->fd);
	return cam;
}

int cam_init(cam_dev_t *cam, int width, int height, uint32_t pixelformat)
{
	struct v4l2_format fmt;
	struct v4l2_requestbuffers req;
	enum v4l2_buf_type type;
	unsigned int i;

	if (!cam || cam->fd < 0)
		return -1;

	memset(&fmt, 0, sizeof(fmt));
	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt.fmt.pix.width       = width;
	fmt.fmt.pix.height      = height;
	fmt.fmt.pix.pixelformat = pixelformat;
	fmt.fmt.pix.field       = V4L2_FIELD_ANY;

	if (ioctl(cam->fd, VIDIOC_S_FMT, &fmt) < 0) {
		LOG_ERROR("Failed to set format on device %s: %s", cam->dev_name, strerror(errno));
		return -1;
	}

	if (fmt.fmt.pix.width != (uint32_t)width || fmt.fmt.pix.height != (uint32_t)height ||
		fmt.fmt.pix.pixelformat != pixelformat) {
		LOG_WARN("Camera driver altered format request: got %dx%d instead of requested %dx%d.",
				 fmt.fmt.pix.width, fmt.fmt.pix.height, width, height);
	} else {
		LOG_INFO("Camera configured to %dx%d.", width, height);
	}

	/* Yêu cầu cấp phát kernel buffers (MMAP) */
	memset(&req, 0, sizeof(req));
	req.count  = 4;
	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory = V4L2_MEMORY_MMAP;

	if (ioctl(cam->fd, VIDIOC_REQBUFS, &req) < 0) {
		if (errno == EINVAL) {
			LOG_ERROR("Device %s does not support memory mapping", cam->dev_name);
		} else {
			LOG_ERROR("VIDIOC_REQBUFS failed on device %s", cam->dev_name);
		}
		return -1;
	}

	if (req.count < 2) {
		LOG_ERROR("Insufficient buffer memory on %s", cam->dev_name);
		return -1;
	}

	cam->buffers = calloc(req.count, sizeof(struct mmap_buffer));
	if (!cam->buffers) {
		LOG_ERROR("Out of memory");
		return -1;
	}

	/* Map các buffers vào user space */
	for (cam->n_buffers = 0; cam->n_buffers < req.count; ++cam->n_buffers) {
		struct v4l2_buffer buf;

		memset(&buf, 0, sizeof(buf));

		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = cam->n_buffers;

		if (ioctl(cam->fd, VIDIOC_QUERYBUF, &buf) < 0) {
			LOG_ERROR("VIDIOC_QUERYBUF failed");
			return -1;
		}

		cam->buffers[cam->n_buffers].length = buf.length;
		cam->buffers[cam->n_buffers].start =
			mmap(NULL /* start anywhere */, buf.length,
				 PROT_READ | PROT_WRITE /* required */,
				 MAP_SHARED /* recommended */,
				 cam->fd, buf.m.offset);

		if (cam->buffers[cam->n_buffers].start == MAP_FAILED) {
			LOG_ERROR("mmap failed");
			return -1;
		}
	}

	/* Queue các buffers để sẵn sàng catch hình ảnh từ camera */
	for (i = 0; i < cam->n_buffers; ++i) {
		struct v4l2_buffer buf;
		memset(&buf, 0, sizeof(buf));
		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory = V4L2_MEMORY_MMAP;
		buf.index = i;

		if (ioctl(cam->fd, VIDIOC_QBUF, &buf) < 0) {
			LOG_ERROR("VIDIOC_QBUF buffer %d failed", i);
			return -1;
		}
	}

	/* Start Streaming */
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (ioctl(cam->fd, VIDIOC_STREAMON, &type) < 0) {
		LOG_ERROR("VIDIOC_STREAMON failed on %s", cam->dev_name);
		return -1;
	}

	LOG_INFO("Camera streaming initialized with %d buffers via mmap.", cam->n_buffers);
	return 0;
}

int cam_get_frame(cam_dev_t *cam, void *buffer, size_t max_size, size_t *bytes_read)
{
	fd_set fds;
	struct timeval tv;
	int r;
	struct v4l2_buffer buf;

	if (!cam || cam->fd < 0 || !buffer || !bytes_read)
		return -1;

	FD_ZERO(&fds);
	FD_SET(cam->fd, &fds);

	tv.tv_sec = 2;
	tv.tv_usec = 0;

	r = select(cam->fd + 1, &fds, NULL, NULL, &tv);
	if (r == -1) {
		if (errno == EINTR) {
			LOG_INFO("select() interrupted by loop signal");
			return 0;
		}
		LOG_ERROR("select() error: %s", strerror(errno));
		return -1;
	}

	if (r == 0) {
		LOG_WARN("select() timeout. Frame dropped or device busy?");
		*bytes_read = 0;
		return 0; 
	}

	memset(&buf, 0, sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	/* Lấy buffer đã có hình ra khỏi queue của kernel */
	if (ioctl(cam->fd, VIDIOC_DQBUF, &buf) < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			*bytes_read = 0;
			return 0;
		}
		LOG_ERROR("VIDIOC_DQBUF failed: %s", strerror(errno));
		return -1;
	}

	if (buf.bytesused > max_size) {
		LOG_ERROR("Buffer too small! Max allowable: %zu, Provided frame: %u", max_size, buf.bytesused);
		*bytes_read = 0;
	} else {
		/* Copy raw frame từ mmap region vào custom memory pool buffer mà app yêu cầu.
		 * Mặc dù sử dụng dynamic pointers mmap() lúc khởi tạo nhưng quá trình lặp real-time là lock-free, 
		 * chỉ copy raw bytes thoả mãn constraint của kiến trúc.
		 */
		memcpy(buffer, cam->buffers[buf.index].start, buf.bytesused);
		*bytes_read = buf.bytesused;
	}

	/* Nhét lại buffer đó vào trong queue để kernel tiếp tục nạp frame sau */
	if (ioctl(cam->fd, VIDIOC_QBUF, &buf) < 0) {
		LOG_ERROR("VIDIOC_QBUF re-queue failed");
		return -1;
	}

	return 0;
}

void cam_close(cam_dev_t *cam)
{
	if (!cam)
		return;

	if (cam->fd >= 0) {
		enum v4l2_buf_type type;
		unsigned int i;

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		
		/* Stop streaming */
		ioctl(cam->fd, VIDIOC_STREAMOFF, &type);

		/* Unmap buffers */
		if (cam->buffers) {
			for (i = 0; i < cam->n_buffers; ++i) {
				munmap(cam->buffers[i].start, cam->buffers[i].length);
			}
			free(cam->buffers);
		}

		close(cam->fd);
		LOG_INFO("Closed camera device %s", cam->dev_name);
	}
	free(cam);
}
