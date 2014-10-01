/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#                                                                               #
# This program is free software; you can redistribute it and/or modify          #
# it under the terms of the GNU General Public License as published by          #
# the Free Software Foundation; either version 2 of the License, or             #
# (at your option) any later version.                                           #
#                                                                               #
# This program is distributed in the hope that it will be useful,               #
# but WITHOUT ANY WARRANTY; without even the implied warranty of                #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 #
# GNU General Public License for more details.                                  #
#                                                                               #
# You should have received a copy of the GNU General Public License             #
# along with this program; if not, write to the Free Software                   #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA     #
#                                                                               #
********************************************************************************/

/*******************************************************************************#
#                                                                               #
#  V4L2 core library                                                            #
#                                                                               #
********************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <libv4l2.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <errno.h>
#include <assert.h>
/* support for internationalization - i18n */
#include <locale.h>
#include <libintl.h>

#include "gview.h"
#include "gviewv4l2core.h"
#include "core_time.h"
#include "uvc_h264.h"
#include "frame_decoder.h"
#include "v4l2_formats.h"
#include "v4l2_controls.h"
#include "v4l2_devices.h"
#include "../config.h"

#ifndef GETTEXT_PACKAGE_V4L2CORE
#define GETTEXT_PACKAGE_V4L2CORE "gview_v4l2core"
#endif
/*video device data mutex*/
static __MUTEX_TYPE mutex = __STATIC_MUTEX_INIT;
#define __PMUTEX &mutex

/*verbosity (global scope)*/
int verbosity = 0;

/*requested format data*/
static int my_pixelformat = 0;
static int my_width = 0;
static int my_height = 0;

static double real_fps = 0.0;
static uint64_t fps_ref_ts = 0;
static uint32_t fps_frame_count = 0;

static uint8_t flag_fps_change = 0; /*set to 1 to request a fps change*/

static uint8_t disable_libv4l2 = 0; /*set to 1 to disable libv4l2 calls*/

/*
 * ioctl with a number of retries in the case of I/O failure
 * args:
 *   fd - device descriptor
 *   IOCTL_X - ioctl reference
 *   arg - pointer to ioctl data
 *
 * asserts:
 *   none
 *
 * returns - ioctl result
 */
int xioctl(int fd, int IOCTL_X, void *arg)
{
	int ret = 0;
	int tries= IOCTL_RETRY;
	do
	{
		if(!disable_libv4l2)
			ret = v4l2_ioctl(fd, IOCTL_X, arg);
		else
			ret = ioctl(fd, IOCTL_X, arg);
	}
	while (ret && tries-- &&
			((errno == EINTR) || (errno == EAGAIN) || (errno == ETIMEDOUT)));

	if (ret && (tries <= 0)) fprintf(stderr, "V4L2_CORE: ioctl (%i) retried %i times - giving up: %s)\n", IOCTL_X, IOCTL_RETRY, strerror(errno));

	return (ret);
}

/*
 * Query video device capabilities and supported formats
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *   vd->fd is valid ( > 0 )
 *
 * returns: error code  (E_OK)
 */
static int check_v4l2_dev(v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);
	assert(vd->fd > 0);

	memset(&vd->cap, 0, sizeof(struct v4l2_capability));

	if ( xioctl(vd->fd, VIDIOC_QUERYCAP, &vd->cap) < 0 )
	{
		fprintf( stderr, "V4L2_CORE: (VIDIOC_QUERYCAP) error: %s\n", strerror(errno));
		return E_QUERYCAP_ERR;
	}

	if ( ( vd->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE ) == 0)
	{
		fprintf(stderr, "V4L2_CORE: Error opening device %s: video capture not supported.\n",
				vd->videodevice);
		return E_QUERYCAP_ERR;
	}
	if (!(vd->cap.capabilities & V4L2_CAP_STREAMING))
	{
		fprintf(stderr, "V4L2_CORE: %s does not support streaming i/o\n",
			vd->videodevice);
		return E_QUERYCAP_ERR;
	}

	if(vd->cap_meth == IO_READ)
	{

		vd->mem[vd->buf.index] = NULL;
		if (!(vd->cap.capabilities & V4L2_CAP_READWRITE))
		{
			fprintf(stderr, "V4L2_CORE: %s does not support read, try with mmap\n",
				vd->videodevice);
			return E_READ_ERR;
		}
	}
	if(verbosity > 0)
		printf("V4L2_CORE: Init. %s (location: %s)\n", vd->cap.card, vd->cap.bus_info);

	/*enumerate frame formats supported by device*/
	enum_frame_formats(vd);

	/*add h264 (uvc muxed) to format list if supported by device*/
	add_h264_format(vd);

	/*enumerate device controls*/
	enumerate_v4l2_control(vd);
	/*gets the current control values and sets their flags*/
	get_v4l2_control_values(vd);

	/*if we have a focus control initiate the software autofocus*/
	if(vd->has_focus_control_id)
	{
		if(v4l2core_soft_autofocus_init (vd) != E_OK)
			vd->has_focus_control_id = 0;
	}

	return E_OK;
}

/*
 * unmaps v4l2 buffers
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code  (0- E_OK)
 */
static int unmap_buff(v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);

	if(verbosity > 2)
		printf("V4L2_CORE: unmapping v4l2 buffers\n");
	int i=0;
	int ret=E_OK;

	switch(vd->cap_meth)
	{
		case IO_READ:
			break;

		case IO_MMAP:
			for (i = 0; i < NB_BUFFER; i++)
			{
				// unmap old buffer
				if((vd->mem[i] != MAP_FAILED) && vd->buff_length[i])
					if((ret=v4l2_munmap(vd->mem[i], vd->buff_length[i]))<0)
					{
						fprintf(stderr, "V4L2_CORE: couldn't unmap buff: %s\n", strerror(errno));
					}
			}
	}
	return ret;
}

/*
 * maps v4l2 buffers
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code  (0- E_OK)
 */
static int map_buff(v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);

	if(verbosity > 2)
		printf("V4L2_CORE: mapping v4l2 buffers\n");

	int i = 0;
	// map new buffer
	for (i = 0; i < NB_BUFFER; i++)
	{
		vd->mem[i] = v4l2_mmap( NULL, // start anywhere
			vd->buff_length[i],
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			vd->fd,
			vd->buff_offset[i]);
		if (vd->mem[i] == MAP_FAILED)
		{
			fprintf(stderr, "V4L2_CORE: Unable to map buffer: %s\n", strerror(errno));
			return E_MMAP_ERR;
		}
		if(verbosity > 1)
			printf("V4L2_CORE: mapped buffer[%i] with length %i to pos %p\n",
				i,
				vd->buff_length[i],
				vd->mem[i]);
	}

	return (E_OK);
}

/*
 * Query and map buffers
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code  (0- E_OK)
 */
static int query_buff(v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);

	if(verbosity > 2)
		printf("V4L2_CORE: query v4l2 buffers\n");

	int i=0;
	int ret=E_OK;

	switch(vd->cap_meth)
	{
		case IO_READ:
			vd->raw_frame_max_size = vd->buf.length;
			break;

		case IO_MMAP:
			for (i = 0; i < NB_BUFFER; i++)
			{
				memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
				vd->buf.index = i;
				vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				//vd->buf.flags = V4L2_BUF_FLAG_TIMECODE;
				//vd->buf.timecode = vd->timecode;
				//vd->buf.timestamp.tv_sec = 0;
				//vd->buf.timestamp.tv_usec = 0;
				vd->buf.memory = V4L2_MEMORY_MMAP;
				ret = xioctl(vd->fd, VIDIOC_QUERYBUF, &vd->buf);

				if (ret < 0)
				{
					fprintf(stderr, "V4L2_CORE: (VIDIOC_QUERYBUF) Unable to query buffer[%i]: %s\n", i, strerror(errno));
					if(errno == EINVAL)
						fprintf(stderr, "         try with read method instead\n");

					return E_QUERYBUF_ERR;
				}

				if (vd->buf.length <= 0)
					fprintf(stderr, "V4L2_CORE: (VIDIOC_QUERYBUF) - buffer length is %i\n",
						vd->buf.length);

				vd->buff_length[i] = vd->buf.length;
				vd->buff_offset[i] = vd->buf.m.offset;
			}
			// map the new buffers
			if(map_buff(vd) != 0)
				ret = E_MMAP_ERR;
			vd->raw_frame_max_size = vd->buf.length;
			break;
	}
				
	return ret;
}

/*
 * Queue Buffers
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code  (0- E_OK)
 */
static int queue_buff(v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);

	if(verbosity > 2)
		printf("V4L2_CORE: queue v4l2 buffers\n");

	int i=0;
	int ret=E_OK;

	switch(vd->cap_meth)
	{
		case IO_READ:
			break;

		case IO_MMAP:
		default:
			for (i = 0; i < NB_BUFFER; ++i)
			{
				memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
				vd->buf.index = i;
				vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				//vd->buf.flags = V4L2_BUF_FLAG_TIMECODE;
				//vd->buf.timecode = vd->timecode;
				//vd->buf.timestamp.tv_sec = 0;
				//vd->buf.timestamp.tv_usec = 0;
				vd->buf.memory = V4L2_MEMORY_MMAP;
				ret = xioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
				if (ret < 0)
				{
					fprintf(stderr, "V4L2_CORE: (VIDIOC_QBUF) Unable to queue buffer: %s\n", strerror(errno));
					return E_QBUF_ERR;
				}
			}
			vd->buf.index = 0; /*reset index*/
	}
	return ret;
}

/*
 * do a VIDIOC_S_PARM ioctl for setting frame rate
 * args:
 *    vd - pointer to video device data
 *
 * asserts:
 *    vd is not null
 *
 * returns: error code
 */
static int do_v4l2_framerate_update(v4l2_dev_t *vd)
{
	/*asserts*/
	assert(vd != NULL);

	int ret = 0;

	/*get the current stream parameters*/
	vd->streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = xioctl(vd->fd, VIDIOC_G_PARM, &vd->streamparm);
	if (ret < 0)
	{
		fprintf(stderr, "V4L2_CORE: (VIDIOC_G_PARM) error: %s\n", strerror(errno));
		fprintf(stderr, "V4L2_CORE: Unable to set %d/%d fps\n", vd->fps_num, vd->fps_denom);
		return ret;
	}

	if (!(vd->streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME))
	{
		fprintf(stderr, "V4L2_CORE: V4L2_CAP_TIMEPERFRAME not supported\n");
		fprintf(stderr, "V4L2_CORE: Unable to set %d/%d fps\n", vd->fps_num, vd->fps_denom);
		return -ENOTSUP;
	}

	vd->streamparm.parm.capture.timeperframe.numerator = vd->fps_num;
	vd->streamparm.parm.capture.timeperframe.denominator = vd->fps_denom;

	/*request the new frame rate*/
	ret = xioctl(vd->fd, VIDIOC_S_PARM, &vd->streamparm);

	if (ret < 0)
	{
		fprintf(stderr, "V4L2_CORE: (VIDIOC_S_PARM) error: %s\n", strerror(errno));
		fprintf(stderr, "V4L2_CORE: Unable to set %d/%d fps\n", vd->fps_num, vd->fps_denom);
	}

	return ret;
}

/*
 * sets video device frame rate
 * args:
 *   vd- pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: VIDIOC_S_PARM ioctl result value
 * (sets vd->fps_denom and vd->fps_num to device value)
 */
static int set_v4l2_framerate (v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);

	if(verbosity > 2)
		printf("V4L2_CORE: trying to change fps to %i/%i\n", vd->fps_num, vd->fps_denom);

	int ret = 0;

	/*lock the mutex*/
	__LOCK_MUTEX( __PMUTEX );

	/*store streaming flag*/
	uint8_t stream_status = vd->streaming;

	/*try to stop the video stream*/
	if(stream_status == STRM_OK)
		v4l2core_stop_stream(vd);

	switch(vd->cap_meth)
	{
		case IO_READ:
			ret = do_v4l2_framerate_update(vd);
			break;

		case IO_MMAP:
			if(stream_status == STRM_OK)
			{
				/*unmap the buffers*/
				unmap_buff(vd);
			}

			ret = do_v4l2_framerate_update(vd);
			/*
			 * For uvc muxed H264 stream
			 * since we are restarting the video stream and codec values will be reset
			 * commit the codec data again
			 */
			if(vd->requested_fmt == V4L2_PIX_FMT_H264 && h264_get_support() == H264_MUXED)
			{
				if(verbosity > 0)
					printf("V4L2_CORE: setting muxed H264 stream in MJPG container\n");
				set_h264_muxed_format(vd);
			}
			break;
	}
	
	if(stream_status == STRM_OK)
	{
		query_buff(vd); /*also mmaps the buffers*/
		queue_buff(vd);
	}

	/*try to start the video stream*/
	if(stream_status == STRM_OK)
		v4l2core_start_stream(vd);

	/*unlock the mutex*/
	__UNLOCK_MUTEX( __PMUTEX );

	return ret;
}

/*
 * checks if frame data is available
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code  (0- E_OK)
 */
static int check_frame_available(v4l2_dev_t *vd)
{
	/*asserts*/
	assert(vd != NULL);

	int ret = E_OK;
	fd_set rdset;
	struct timeval timeout;

	/*lock the mutex*/
	__LOCK_MUTEX( __PMUTEX );
	int stream_state = vd->streaming;
	/*unlock the mutex*/
	__UNLOCK_MUTEX( __PMUTEX );

	/*make sure streaming is on*/
	if(stream_state != STRM_OK)
	{
		if(stream_state == STRM_REQ_STOP)
			v4l2core_stop_stream(vd);

		fprintf(stderr, "V4L2_CORE: (get_v4l2_frame) video stream must be started first\n");
		return E_NO_STREAM_ERR;
	}

	/*a fps change was requested while streaming*/
	if(flag_fps_change > 0)
	{
		if(verbosity > 2)
			printf("V4L2_CORE: fps change request detected\n");
		set_v4l2_framerate(vd);
		flag_fps_change = 0;
	}

	FD_ZERO(&rdset);
	FD_SET(vd->fd, &rdset);
	timeout.tv_sec = 1; /* 1 sec timeout*/
	timeout.tv_usec = 0;
	/* select - wait for data or timeout*/
	ret = select(vd->fd + 1, &rdset, NULL, NULL, &timeout);
	if (ret < 0)
	{
		fprintf(stderr, "V4L2_CORE: Could not grab image (select error): %s\n", strerror(errno));
		vd->timestamp = 0;
		return E_SELECT_ERR;
	}

	if (ret == 0)
	{
		fprintf(stderr, "V4L2_CORE: Could not grab image (select timeout): %s\n", strerror(errno));
		vd->timestamp = 0;
		return E_SELECT_TIMEOUT_ERR;
	}

	if ((ret > 0) && (FD_ISSET(vd->fd, &rdset)))
		return E_OK;

	return E_UNKNOWN_ERR;
}

/*
 * set verbosity
 * args:
 *   level - verbosity level
 *
 * asserts:
 *   none
 *
 * returns void
 */
void v4l2core_set_verbosity(int level)
{
	verbosity = level;
}

/*
 * disable libv4l2 calls
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns void
 */
void v4l2core_disable_libv4l2()
{
	disable_libv4l2 = 1;
}

/*
 * enable libv4l2 calls (default)
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns void
 */
void v4l2core_enable_libv4l2()
{
	disable_libv4l2 = 0;
}

/*
 * Set v4l2 capture method
 * args:
 *   vd - pointer to video device data
 *   method - capture method (IO_READ or IO_MMAP)
 *
 * asserts:
 *   vd is not null
 *
 * returns: VIDIOC_STREAMON ioctl result (E_OK or E_STREAMON_ERR)
*/
void v4l2core_set_capture_method(v4l2_dev_t *vd, int method)
{
	/*asserts*/
	assert(vd != NULL);

	vd->cap_meth = method;
}

/*
 * get real fps
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: double with real fps value
 */
double v4l2core_get_realfps()
{
	return(real_fps);
}

/*
 * Start video stream
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: VIDIOC_STREAMON ioctl result (E_OK or E_STREAMON_ERR)
*/
int v4l2core_start_stream(v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);

	if(vd->streaming == STRM_OK)
	{
		fprintf(stderr, "V4L2_CORE: (VIDIOC_STREAMON) stream_status = STRM_OK\n");
		return;
	}

	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int ret=E_OK;
	switch(vd->cap_meth)
	{
		case IO_READ:
			//do nothing
			break;

		case IO_MMAP:
		default:
			ret = xioctl(vd->fd, VIDIOC_STREAMON, &type);
			if (ret < 0)
			{
				fprintf(stderr, "V4L2_CORE: (VIDIOC_STREAMON) Unable to start capture: %s \n", strerror(errno));
				return E_STREAMON_ERR;
			}
			break;
	}

	vd->streaming = STRM_OK;

	return ret;
}

/*
 * request video stream to stop
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code (0 -OK)
*/
int v4l2core_request_stop_stream(v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);

	if(vd->streaming != STRM_OK)
		return -1;

	vd->streaming = STRM_REQ_STOP;

	return 0;
}

/*
 * Stops the video stream
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: VIDIOC_STREAMON ioctl result (E_OK)
*/
int v4l2core_stop_stream(v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);

	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int ret=E_OK;
	switch(vd->cap_meth)
	{
		case IO_READ:
		case IO_MMAP:
		default:
			ret = xioctl(vd->fd, VIDIOC_STREAMOFF, &type);
			if (ret < 0)
			{
				if(errno == 9) /* stream allready stoped*/
					vd->streaming = STRM_STOP;
				fprintf(stderr, "V4L2_CORE: (VIDIOC_STREAMOFF) Unable to stop capture: %s\n", strerror(errno));
				return E_STREAMOFF_ERR;
			}
			break;
	}

	vd->streaming = STRM_STOP;
	return ret;
}


/*
 * process input buffer
 *
 */
static void process_input_buffer(v4l2_dev_t *vd)
{
	/*
     * driver timestamp is unreliable
	 * use monotonic system time
	 */
	vd->timestamp = ns_time_monotonic();
	 
	vd->frame_index++;
	
	vd->raw_frame_size = vd->buf.bytesused;
	if(vd->raw_frame_size == 0)
	{
		if(verbosity > 1)
			fprintf(stderr, "V4L2_CORE: VIDIOC_QBUF returned buf.bytesused = 0 \n");
	}
	
	/*point vd->raw_frame to current frame buffer*/
	vd->raw_frame = vd->mem[vd->buf.index];
} 
 
/*
 * gets the next video frame (must be released after processing)
 * args:
 * vd: pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code (E_OK)
 */
int v4l2core_get_frame(v4l2_dev_t *vd)
{
	/*asserts*/
	assert(vd != NULL);

	/*for H264 streams request a IDR frame with SPS and PPS data if it's the first frame*/
	if(vd->requested_fmt == V4L2_PIX_FMT_H264 && vd->frame_index < 1)
		request_h264_frame_type(vd, PICTURE_TYPE_IDR_FULL);

	int res = 0;
	int ret = check_frame_available(vd);

	if (ret < 0)
		return ret;

	int bytes_used = 0;

	switch(vd->cap_meth)
	{
		case IO_READ:

			/*lock the mutex*/
			__LOCK_MUTEX( __PMUTEX );
			if(vd->streaming == STRM_OK)
			{
				vd->buf.bytesused = v4l2_read (vd->fd, vd->mem[vd->buf.index], vd->buf.length);
				bytes_used = vd->buf.bytesused;
			}
			else res = -1;
			/*unlock the mutex*/
			__UNLOCK_MUTEX( __PMUTEX );

			if(res < 0)
				return E_NO_STREAM_ERR;

			if (-1 == bytes_used )
			{
				switch (errno)
				{
					case EAGAIN:
						fprintf(stderr, "V4L2_CORE: No data available for read: %s\n", strerror(errno));
						return E_SELECT_TIMEOUT_ERR;
						break;
					case EINVAL:
						fprintf(stderr, "V4L2_CORE: Read method error, try mmap instead: %s\n", strerror(errno));
						return E_READ_ERR;
						break;
					case EIO:
						fprintf(stderr, "V4L2_CORE: read I/O Error: %s\n", strerror(errno));
						return E_READ_ERR;
						break;
					default:
						fprintf(stderr, "V4L2_CORE: read: %s\n", strerror(errno));
						return E_READ_ERR;
						break;
				}
				vd->timestamp = 0;
			}
			
			process_input_buffer(vd);
			break;

		case IO_MMAP:
		default:
			//if((vd->setH264ConfigProbe > 0))
			//{

				//if(vd->setH264ConfigProbe)
				//{
					//video_disable(vd);
					//unmap_buff(vd);

					//h264_commit(vd, global);

					//vd->setH264ConfigProbe = 0;
					//query_buff(vd);
					//queue_buff(vd);
					//video_enable(vd);
				//}

				//ret = check_frame_available(vd);

				//if (ret < 0)
					//return ret;
			//}

			/* dequeue the buffers */

			/*lock the mutex*/
			__LOCK_MUTEX( __PMUTEX );

			if(vd->streaming == STRM_OK)
			{
				memset(&vd->buf, 0, sizeof(struct v4l2_buffer));

				vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				vd->buf.memory = V4L2_MEMORY_MMAP;

				ret = xioctl(vd->fd, VIDIOC_DQBUF, &vd->buf);

				if(!ret)
					process_input_buffer(vd);
				else
					fprintf(stderr, "V4L2_CORE: (VIDIOC_DQBUF) Unable to dequeue buffer: %s\n", strerror(errno));
			}
			else res = -1;

			/*unlock the mutex*/
			__UNLOCK_MUTEX( __PMUTEX );

			if(res < 0)
				return E_NO_STREAM_ERR;

			if (ret < 0)
				return E_DQBUF_ERR;
	}

	/*determine real fps every 3 sec aprox.*/
	fps_frame_count++;

	if(vd->timestamp - fps_ref_ts >= (3 * NSEC_PER_SEC))
	{
		if(verbosity > 2)
			printf("V4L2CORE: (fps) ref:%"PRId64" ts:%"PRId64" frames:%i\n",
				fps_ref_ts, vd->timestamp, fps_frame_count);
		real_fps = (double) (fps_frame_count * NSEC_PER_SEC) / (double) (vd->timestamp - fps_ref_ts);
		fps_frame_count = 0;
		fps_ref_ts = vd->timestamp;
	}

	return E_OK;
}

/*
 * releases the current video frame (so that it can be reused by the driver)
 * args:
 * vd: pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code (E_OK)
 */
int v4l2core_release_frame(v4l2_dev_t *vd)
{
	int ret = 0;
	
	switch(vd->cap_meth)
	{
		case IO_READ:
			break;
		
		case IO_MMAP:
		default:
			/* queue the buffers */
			ret = xioctl(vd->fd, VIDIOC_QBUF, &vd->buf);

			if(ret)
				fprintf(stderr, "V4L2_CORE: (VIDIOC_QBUF) Unable to queue buffer: %s\n", strerror(errno));
			
			vd->raw_frame = NULL;
			vd->raw_frame_size = 0;
			break;	
	}
	
	if (ret < 0)
		return E_QBUF_ERR;
	
	return E_OK;		
}

/*
 * Try/Set device video stream format
 * args:
 *   vd - pointer to video device data
 *   width - requested video frame width
 *   height - requested video frame height
 *   pixelformat - requested v4l2 pixelformat
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code ( E_OK)
 */
static int try_video_stream_format(v4l2_dev_t *vd, int width, int height, int pixelformat)
{
	/*assertions*/
	assert(vd != NULL);

	int ret = E_OK;

	/*lock the mutex*/
	__LOCK_MUTEX( __PMUTEX );

	vd->requested_fmt = pixelformat;

	uint8_t stream_status = vd->streaming;

	if(stream_status == STRM_OK)
		v4l2core_stop_stream(vd);

	if(vd->requested_fmt == V4L2_PIX_FMT_H264 && h264_get_support() == H264_MUXED)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: requested H264 stream is supported through muxed MJPG\n");
		pixelformat = V4L2_PIX_FMT_MJPEG;
	}

	vd->format.fmt.pix.pixelformat = pixelformat;
	vd->format.fmt.pix.width = width;
	vd->format.fmt.pix.height = height;

	/* make sure we set a valid format*/
	if(verbosity > 0)
		printf("V4L2_CORE: checking format: %c%c%c%c\n",
			(vd->format.fmt.pix.pixelformat) & 0xFF, ((vd->format.fmt.pix.pixelformat) >> 8) & 0xFF,
			((vd->format.fmt.pix.pixelformat) >> 16) & 0xFF, ((vd->format.fmt.pix.pixelformat) >> 24) & 0xFF);

	/*override field and type entries*/
	vd->format.fmt.pix.field = V4L2_FIELD_ANY;
	vd->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	ret = xioctl(vd->fd, VIDIOC_S_FMT, &vd->format);

	if(!ret && (vd->requested_fmt == V4L2_PIX_FMT_H264) && (h264_get_support() == H264_MUXED))
	{
		if(verbosity > 0)
			printf("V4L2_CORE: setting muxed H264 stream in MJPG container\n");
		set_h264_muxed_format(vd);
	}

	/*unlock the mutex*/
	__UNLOCK_MUTEX( __PMUTEX );

	if (ret != 0)
	{
		fprintf(stderr, "V4L2_CORE: (VIDIOC_S_FORMAT) Unable to set format: %s\n", strerror(errno));
		return E_FORMAT_ERR;
	}

	if ((vd->format.fmt.pix.width != width) ||
		(vd->format.fmt.pix.height != height))
	{
		fprintf(stderr, "V4L2_CORE: Requested resolution unavailable: got width %d height %d\n",
		vd->format.fmt.pix.width, vd->format.fmt.pix.height);
	}

	/*
	 * try to alloc frame buffers based on requested format
	 */
	ret = alloc_v4l2_frames(vd);
	if( ret != E_OK)
	{
		fprintf(stderr, "V4L2_CORE: Frame allocation returned error (%i)\n", ret);
		return E_ALLOC_ERR;
	}

	switch (vd->cap_meth)
	{
		case IO_READ: /*allocate buffer for read*/
			/*lock the mutex*/
			__LOCK_MUTEX( __PMUTEX );

			memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
			vd->buf.length = (vd->format.fmt.pix.width) * (vd->format.fmt.pix.height) * 3; //worst case (rgb)
			vd->mem[vd->buf.index] = calloc(vd->buf.length, sizeof(uint8_t));
			if(vd->mem[vd->buf.index] == NULL)
			{
				fprintf(stderr, "V4L2_CORE: FATAL memory allocation failure (try_video_stream_format): %s\n", strerror(errno));
				exit(-1);
			}

			/*unlock the mutex*/
			__UNLOCK_MUTEX( __PMUTEX );
			break;

		case IO_MMAP:
		default:
			/* request buffers */
			memset(&vd->rb, 0, sizeof(struct v4l2_requestbuffers));
			vd->rb.count = NB_BUFFER;
			vd->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			vd->rb.memory = V4L2_MEMORY_MMAP;

			ret = xioctl(vd->fd, VIDIOC_REQBUFS, &vd->rb);

			if (ret < 0)
			{
				fprintf(stderr, "V4L2_CORE: (VIDIOC_REQBUFS) Unable to allocate buffers: %s\n", strerror(errno));
				return E_REQBUFS_ERR;
			}
			/* map the buffers */
			if (query_buff(vd))
			{
				fprintf(stderr, "V4L2_CORE: (VIDIOC_QBUFS) Unable to query buffers: %s\n", strerror(errno));
				/*
				 * delete requested buffers
				 * no need to unmap as mmap failed for sure
				 */
				if(verbosity > 0)
					printf("V4L2_CORE: cleaning requestbuffers\n");
				memset(&vd->rb, 0, sizeof(struct v4l2_requestbuffers));
				vd->rb.count = 0;
				vd->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				vd->rb.memory = V4L2_MEMORY_MMAP;
				if(xioctl(vd->fd, VIDIOC_REQBUFS, &vd->rb)<0)
					fprintf(stderr, "V4L2_CORE: (VIDIOC_REQBUFS) Unable to delete buffers: %s\n", strerror(errno));

				return E_QUERYBUF_ERR;
			}

			/* Queue the buffers */
			if (queue_buff(vd))
			{
				fprintf(stderr, "V4L2_CORE: (VIDIOC_QBUFS) Unable to queue buffers: %s\n", strerror(errno));
				/*delete requested buffers */
				if(verbosity > 0)
					printf("V4L2_CORE: cleaning requestbuffers\n");
				unmap_buff(vd);
				memset(&vd->rb, 0, sizeof(struct v4l2_requestbuffers));
				vd->rb.count = 0;
				vd->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				vd->rb.memory = V4L2_MEMORY_MMAP;
				if(xioctl(vd->fd, VIDIOC_REQBUFS, &vd->rb)<0)
					fprintf(stderr, "V4L2_CORE: (VIDIOC_REQBUFS) Unable to delete buffers: %s\n", strerror(errno));
				return E_QBUF_ERR;
			}
	}

	/*this locks the mutex (can't be called while the mutex is being locked)*/
	v4l2core_request_framerate_update(vd);

	if(stream_status == STRM_OK)
		v4l2core_start_stream(vd);

	/*update the current framerate for the device*/
	v4l2core_get_framerate(vd);

	return E_OK;
}

/*
 * prepare new format
 * args:
 *   vd - pointer to device data
 *   new_format - new format
 *
 * asserts:
 *    vd is not null
 *
 * returns: none
 */
void v4l2core_prepare_new_format(v4l2_dev_t *vd, int new_format)
{
	/*asserts*/
	assert(vd != NULL);

	int format_index = v4l2core_get_frame_format_index(vd, new_format);

	if(format_index < 0)
		format_index = 0;

	my_pixelformat = vd->list_stream_formats[format_index].format;
}

/*
 * prepare a valid format (first in the format list)
 * args:
 *   vd - pointer to device data
 *
 * asserts:
 *    vd is not null
 *
 * returns: none
 */
void v4l2core_prepare_valid_format(v4l2_dev_t *vd)
{
	/*asserts*/
	assert(vd != NULL);

	int format_index = 0;

	my_pixelformat = vd->list_stream_formats[format_index].format;
}

/*
 * prepare new resolution
 * args:
 *   vd - pointer to device data
 *   new_width - new width
 *   new_height - new height
 *
 * asserts:
 *    vd is not null
 *
 * returns: none
 */
void v4l2core_prepare_new_resolution(v4l2_dev_t *vd, int new_width, int new_height)
{
	/*asserts*/
	assert(vd != NULL);

	int format_index = v4l2core_get_frame_format_index(vd, my_pixelformat);

	if(format_index < 0)
		format_index = 0;

	int resolution_index = v4l2core_get_format_resolution_index(vd, format_index, new_width, new_height);

	if(resolution_index < 0)
		resolution_index = 0;

	my_width  = vd->list_stream_formats[format_index].list_stream_cap[resolution_index].width;
	my_height = vd->list_stream_formats[format_index].list_stream_cap[resolution_index].height;
}

/*
 * prepare valid resolution (first in the resolution list for the format)
 * args:
 *   vd - pointer to device data
 *
 * asserts:
 *    vd is not null
 *
 * returns: none
 */
void v4l2core_prepare_valid_resolution(v4l2_dev_t *vd)
{
	/*asserts*/
	assert(vd != NULL);

	int format_index = v4l2core_get_frame_format_index(vd, my_pixelformat);

	if(format_index < 0)
		format_index = 0;

	int resolution_index = 0;

	my_width  = vd->list_stream_formats[format_index].list_stream_cap[resolution_index].width;
	my_height = vd->list_stream_formats[format_index].list_stream_cap[resolution_index].height;
}

/*
 * update the current format (pixelformat, width and height)
 * args:
 *    device - pointer to device data
 *
 * asserts:
 *    device is not null
 *
 * returns:
 *    error code
 */
int v4l2core_update_current_format(v4l2_dev_t *vd)
{
	/*asserts*/
	assert(vd != NULL);

	return(try_video_stream_format(vd, my_width, my_height, my_pixelformat));
}

/*
 * clean video device data allocation
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: void
 */
static void clean_v4l2_dev(v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);

	if(vd->videodevice)
		free(vd->videodevice);
	vd->videodevice = NULL;

	if(vd->has_focus_control_id)
		v4l2core_soft_autofocus_close(vd);

	if(vd->list_device_controls)
		free_v4l2_control_list(vd);

	if(vd->list_stream_formats)
		free_frame_formats(vd);

	if(vd->list_device_controls)
		free_v4l2_control_list(vd);
	
	/*close descriptor*/
	if(vd->fd > 0)
		v4l2_close(vd->fd);

	vd->fd = 0;

	free(vd);
}

/*
 * Initiate video device data with default values
 * args:
 *   device - device name (e.g: "/dev/video0")
 *
 * asserts:
 *   device is not null
 *
 * returns: pointer to newly allocated video device data  ( NULL on error)
 */
v4l2_dev_t *v4l2core_init_dev(const char *device)
{
	assert(device != NULL);

	/*localization*/
	char* lc_all = setlocale (LC_ALL, "");
	char* lc_dir = bindtextdomain (GETTEXT_PACKAGE_V4L2CORE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE_V4L2CORE, "UTF-8");
	if (verbosity > 1) printf("V4L2_CORE: language catalog=> dir:%s type:%s cat:%s.mo\n",
		lc_dir, lc_all, GETTEXT_PACKAGE_V4L2CORE);

	v4l2_dev_t *vd = calloc(1, sizeof(v4l2_dev_t));

	assert(vd != NULL);

	/*MMAP by default*/
	vd->cap_meth = IO_MMAP;

	vd->videodevice = strdup(device);

	if(verbosity > 0)
	{
		printf("V4L2_CORE: capture method mmap (%i)\n",vd->cap_meth);
		printf("V4L2_CORE: video device: %s \n", vd->videodevice);
	}

	vd->raw_frame = NULL;

	vd->h264_no_probe_default = 0;
	vd->h264_SPS = NULL;
	vd->h264_SPS_size = 0;
	vd->h264_PPS = NULL;
	vd->h264_PPS_size = 0;
	vd->h264_last_IDR = NULL;
	vd->h264_last_IDR_size = 0;

	vd->tmp_buffer = NULL;
	vd->yuv_frame = NULL;
	vd->h264_frame = NULL;

	/*set some defaults*/
	vd->fps_num = 1;
	vd->fps_denom = 25;

	vd->pan_step = 128;
	vd->tilt_step = 128;

	/*open device*/
	if ((vd->fd = v4l2_open(vd->videodevice, O_RDWR | O_NONBLOCK, 0)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: ERROR opening V4L interface: %s\n", strerror(errno));
		clean_v4l2_dev(vd);
		return (NULL);
	}

	vd->this_device = v4l2core_get_device_index(vd->videodevice);
	if(vd->this_device < 0)
		vd->this_device = 0;

	v4l2_device_list *device_list = v4l2core_get_device_list();
	if(device_list && device_list->list_devices)
		device_list->list_devices[vd->this_device].current = 1;

	/*try to map known xu controls (we could/should leave this for libwebcam)*/
	init_xu_ctrls(vd);

	/*zero structs*/
	memset(&vd->cap, 0, sizeof(struct v4l2_capability));
	memset(&vd->format, 0, sizeof(struct v4l2_format));
	memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
	memset(&vd->rb, 0, sizeof(struct v4l2_requestbuffers));
	memset(&vd->streamparm, 0, sizeof(struct v4l2_streamparm));

	if(check_v4l2_dev(vd) != E_OK)
	{
		clean_v4l2_dev(vd);
		return (NULL);
	}

	int i = 0;
	for (i = 0; i < NB_BUFFER; i++)
	{
		vd->mem[i] = MAP_FAILED; /*not mmaped yet*/
	}

	return (vd);
}

/*
 * clean v4l2 buffers
 * args:
 *    vd -pointer to video device data
 *
 * asserts:
 *    vd is not null
 *
 * return: none
 */
void v4l2core_clean_buffers(v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);

	if(verbosity > 1)
		printf("V4L2_CORE: cleaning v4l2 buffers\n");

	if(vd->streaming == STRM_OK)
		v4l2core_stop_stream(vd);

	clean_v4l2_frames(vd);

	// unmap queue buffers
	switch(vd->cap_meth)
	{
		case IO_READ:
			if(vd->mem[vd->buf.index]!= NULL)
	    	{
				free(vd->mem[vd->buf.index]);
				vd->mem[vd->buf.index] = NULL;
			}
			break;

		case IO_MMAP:
		default:
			//delete requested buffers
			unmap_buff(vd);
			memset(&vd->rb, 0, sizeof(struct v4l2_requestbuffers));
			vd->rb.count = 0;
			vd->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			vd->rb.memory = V4L2_MEMORY_MMAP;
			if(xioctl(vd->fd, VIDIOC_REQBUFS, &vd->rb)<0)
			{
				fprintf(stderr, "V4L2_CORE: (VIDIOC_REQBUFS) Failed to delete buffers: %s (errno %d)\n", strerror(errno), errno);
			}
			break;
	}
}
/*
 * cleans video device data and allocations
 * args:
 *   vd: pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: void
 */
void v4l2core_close_dev(v4l2_dev_t *vd)
{
	/*asserts*/
	assert(vd != NULL);

	v4l2core_clean_buffers(vd);
	clean_v4l2_dev(vd);
}

/*
 * request a fps update - this locks the mutex
 *   (can't be called while the mutex is being locked)
 * args:
 *    vd - pointer to video device data
 *
 * asserts:
 *    vd is not null
 *
 * returns: none
 */
void v4l2core_request_framerate_update(v4l2_dev_t *vd)
{
	/*
	 * if we are streaming flag a fps change when retrieving frame
	 * else change fps immediatly
	 */
	if(vd->streaming == STRM_OK)
		flag_fps_change = 1;
	else
		set_v4l2_framerate(vd);
}

/*
 * gets video device defined frame rate (not real - consider it a maximum value)
 * args:
 *   vd: pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: VIDIOC_G_PARM ioctl result value
 * (sets vd->fps_denom and vd->fps_num to device value)
 */
int v4l2core_get_framerate (v4l2_dev_t *vd)
{
	/*assertions*/
	assert(vd != NULL);

	int ret=0;

	vd->streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = xioctl(vd->fd, VIDIOC_G_PARM, &vd->streamparm);
	if (ret < 0)
	{
		fprintf(stderr, "V4L2_CORE: (VIDIOC_G_PARM) error: %s\n", strerror(errno));
		return ret;
	}
	else
	{
		if (vd->streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME)
		{
			vd->fps_denom = vd->streamparm.parm.capture.timeperframe.denominator;
			vd->fps_num = vd->streamparm.parm.capture.timeperframe.numerator;
		}
	}

	if(vd->fps_denom == 0 )
		vd->fps_denom = 1;
	if(vd->fps_num == 0)
		vd->fps_num = 1;

	return ret;
}
