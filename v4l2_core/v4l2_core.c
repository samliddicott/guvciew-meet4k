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
#include <sys/time.h>
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

#include "v4l2_core.h"
#include "v4l2_formats.h"
#include "v4l2_controls.h"
#include "v4l2_devices.h"

/*video device data mutex*/
static __MUTEX_TYPE mutex;
#define __PMUTEX &mutex

/*verbosity (global scope)*/
int verbosity = 0;

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
		ret = v4l2_ioctl(fd, IOCTL_X, arg);
	}
	while (ret && tries-- &&
			((errno == EINTR) || (errno == EAGAIN) || (errno == ETIMEDOUT)));

	if (ret && (tries <= 0)) fprintf(stderr, "V4L2_CORE: ioctl (%i) retried %i times - giving up: %s)\n", IOCTL_X, IOCTL_RETRY, strerror(errno));

	return (ret);
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
void set_v4l2_verbosity(int level)
{
	verbosity = level;
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
static int check_v4l2_dev(v4l2_dev* vd)
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
			fprintf(stderr, "V4L2_CORE: %s does not support read i/o\n",
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

	/*
	 * logitech c930 camera supports h264 through aux stream multiplexing in the MJPG container
	 * check if H264 UVCX XU h264 controls exist and add a virtual H264 format entry to the list
	 */
	//check_uvc_h264_format(vd, global);

	//if(!(vd->listFormats->listVidFormats))
	//	g_printerr("V4L2_CORE: Couldn't detect any supported formats on your device (%i)\n", vd->listFormats->numb_formats);

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
static int unmap_buff(v4l2_dev* vd)
{
	/*assertions*/
	assert(vd != NULL);

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
static int map_buff(v4l2_dev* vd)
{
	/*assertions*/
	assert(vd != NULL);

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
static int query_buff(v4l2_dev* vd)
{
	/*assertions*/
	assert(vd != NULL);

	int i=0;
	int ret=E_OK;

	switch(vd->cap_meth)
	{
		case IO_READ:
			break;

		case IO_MMAP:
			for (i = 0; i < NB_BUFFER; i++)
			{
				memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
				vd->buf.index = i;
				vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				//vd->buf.flags = V4L2_BUF_FLAG_TIMECODE;
				//vd->buf.timecode = vd->timecode;
				//vd->buf.timestamp.tv_sec = 0;//get frame as soon as possible
				//vd->buf.timestamp.tv_usec = 0;
				vd->buf.memory = V4L2_MEMORY_MMAP;
				ret = xioctl(vd->fd, VIDIOC_QUERYBUF, &vd->buf);
				if (ret < 0)
				{
					fprintf(stderr, "V4L2_CORE: (VIDIOC_QUERYBUF) Unable to query buffer: %s\n", strerror(errno));
					if(errno == EINVAL)
					{
						fprintf(stderr, "  trying with read method instead\n");
						vd->cap_meth = IO_READ;
					}
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
				return E_MMAP_ERR;
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
static int queue_buff(v4l2_dev* vd)
{
	/*assertions*/
	assert(vd != NULL);

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
				//vd->buf.timestamp.tv_sec = 0;//get frame as soon as possible
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
 * Start video stream
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: VIDIOC_STREAMON ioctl result (E_OK or E_STREAMON_ERR)
*/
int start_video_stream(v4l2_dev* vd)
{
	/*assertions*/
	assert(vd != NULL);

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

	return ret;
}

/*
 * Stop video stream
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: VIDIOC_STREAMON ioctl result (E_OK)
*/
int stop_video_stream(v4l2_dev* vd)
{
	/*assertions*/
	assert(vd != NULL);

	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int ret=E_OK;
	switch(vd->cap_meth)
	{
		case IO_READ:
			//do nothing
			break;

		case IO_MMAP:
		default:
			ret = xioctl(vd->fd, VIDIOC_STREAMOFF, &type);
			if (ret < 0)
			{
				/* if(errno == 9) - stream allready stoped*/
				fprintf(stderr, "V4L2_CORE: (VIDIOC_STREAMOFF) Unable to stop capture: %s\n", strerror(errno));
				return E_STREAMOFF_ERR;
			}
			break;
	}

	return ret;
}


/*
 * Try/Set device video stream format
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code ( E_OK)
 */
int try_video_stream(v4l2_dev* vd)
{
	/*assertions*/
	assert(vd != NULL);

	int ret = E_OK;

	/* make sure we set a valid format*/
	if(verbosity > 0)
		printf("checking format: %c%c%c%c\n",
			(vd->format.fmt.pix.pixelformat) & 0xFF, ((vd->format.fmt.pix.pixelformat) >> 8) & 0xFF,
			((vd->format.fmt.pix.pixelformat) >> 16) & 0xFF, ((vd->format.fmt.pix.pixelformat) >> 24) & 0xFF);

	/*override field and type entries*/
	vd->format.fmt.pix.field = V4L2_FIELD_ANY;
	vd->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	/*store requested format*/
	int width = vd->format.fmt.pix.width;
	int height = vd->format.fmt.pix.height;

	ret = xioctl(vd->fd, VIDIOC_S_FMT, &vd->format);
	if (ret < 0)
	{
		fprintf(stderr, "V4L2_CORE: (VIDIOC_S_FORMAT) Unable to set format: %s\n", strerror(errno));
		return E_FORMAT_ERR;
	}
	if ((vd->format.fmt.pix.width != width) ||
		(vd->format.fmt.pix.height != height))
	{
		fprintf(stderr, "V4L2_CORE: Requested Format unavailable: got width %d height %d\n",
		vd->format.fmt.pix.width, vd->format.fmt.pix.height);
	}

	/* ----------- FPS --------------*/
	set_v4l2_framerate(vd);

	switch (vd->cap_meth)
	{
		case IO_READ: //allocate buffer for read
			memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
			vd->buf.length = (vd->format.fmt.pix.width) * (vd->format.fmt.pix.height) * 3; //worst case (rgb)
			vd->mem[vd->buf.index] = calloc(vd->buf.length, sizeof(uint8_t));
			break;

		case IO_MMAP:
		default:
			// request buffers
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
			// map the buffers
			if (query_buff(vd))
			{
				//delete requested buffers
				//no need to unmap as mmap failed for sure
				memset(&vd->rb, 0, sizeof(struct v4l2_requestbuffers));
				vd->rb.count = 0;
				vd->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
				vd->rb.memory = V4L2_MEMORY_MMAP;
				if(xioctl(vd->fd, VIDIOC_REQBUFS, &vd->rb)<0)
					fprintf(stderr, "V4L2_CORE: (VIDIOC_REQBUFS) Unable to delete buffers: %s\n", strerror(errno));
				return E_QUERYBUF_ERR;
			}
			// Queue the buffers
			if (queue_buff(vd))
			{
				//delete requested buffers
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

	return ret;
}

/*
 * clear video device data allocation
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: void
 */
static void clear_v4l2_dev(v4l2_dev* vd)
{
	/*assertions*/
	assert(vd != NULL);

	if(vd->videodevice)
		free(vd->videodevice);

	if(vd->h264_SPS)
		free(vd->h264_SPS);

	if(vd->h264_PPS)
		free(vd->h264_PPS);

	if(vd->h264_last_IDR)
		free(vd->h264_last_IDR);

	if(vd->tmpbuffer)
		free(vd->tmpbuffer);

	if(vd->framebuffer)
		free(vd->framebuffer);

	if(vd->h264_frame)
		free(vd->h264_frame);

	if(vd->list_device_controls)
		free_v4l2_control_list(vd);

	if(vd->list_devices)
		free_v4l2_devices_list(vd);

	if(vd->list_stream_formats)
		free_frame_formats(vd);

	if(vd->list_device_controls)
		free_v4l2_control_list(vd);

	/*unmap/free request buffers*/

	if (vd->udev)
		udev_unref(vd->udev);

	/*close descriptor*/
	if(vd->fd > 0)
		v4l2_close(vd->fd);

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
v4l2_dev* init_v4l2_dev(const char *device)
{
	assert(device != NULL);

	v4l2_dev* vd = calloc(1, sizeof(v4l2_dev));

	assert(vd != NULL);

	/*MMAP by default*/
	vd->cap_meth = IO_MMAP;

	vd->videodevice = strdup(device);

	if(verbosity > 0)
	{
		printf("V4L2_CORE: capture method mmap (%i)\n",vd->cap_meth);
		printf("V4L2_CORE: video device: %s \n", vd->videodevice);
	}

	vd->h264_SPS = NULL;
	vd->h264_SPS_size = 0;
	vd->h264_PPS = NULL;
	vd->h264_PPS_size = 0;
	vd->h264_last_IDR = NULL;
	vd->h264_last_IDR_size = 0;

	vd->tmpbuffer = NULL;
	vd->framebuffer = NULL;
	vd->h264_frame = NULL;

	/* Create a udev object */
	vd->udev = udev_new();
	/*start udev device monitoring*/
	/* Set up a monitor to monitor v4l2 devices */
	if(vd->udev)
	{
		vd->udev_mon = udev_monitor_new_from_netlink(vd->udev, "udev");
		udev_monitor_filter_add_match_subsystem_devtype(vd->udev_mon, "video4linux", NULL);
		udev_monitor_enable_receiving(vd->udev_mon);
		/* Get the file descriptor (fd) for the monitor */
		vd->udev_fd = udev_monitor_get_fd(vd->udev_mon);

		enum_v4l2_devices(vd);
	}

	/*open device*/
	if ((vd->fd = v4l2_open(vd->videodevice, O_RDWR | O_NONBLOCK, 0)) < 0)
	{
		fprintf(stderr, "V4L2_CORE: ERROR opening V4L interface: %s\n", strerror(errno));
		clear_v4l2_dev(vd);
		return (NULL);
	}



	//For H264 we need to get the unit id before checking video formats

	memset(&vd->format, 0, sizeof(struct v4l2_format));
	// populate video capabilities structure array
	// should only be called after all vdIn struct elements
	// have been initialized
	if(check_v4l2_dev(vd) != E_OK)
	{
		clear_v4l2_dev(vd);
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
 * cleans video device data and allocations
 * args:
 *   vd: pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: void
 */
void close_v4l2_dev(v4l2_dev* vd)
{
	/*assertions*/
	assert(vd != NULL);

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

	clear_v4l2_dev(vd);
}

/*
 * sets video device frame rate
 * args:
 *   vd: pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: VIDIOC_S_PARM ioctl result value
 * (sets vd->fps_denom and vd->fps_num to device value)
 */
int set_v4l2_framerate (v4l2_dev* vd)
{
	/*assertions*/
	assert(vd != NULL);

	int ret = 0;

	vd->streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	ret = xioctl(vd->fd, VIDIOC_G_PARM, &vd->streamparm);
	if (ret < 0)
		return ret;

	if (!(vd->streamparm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME))
		return -ENOTSUP;

	vd->streamparm.parm.capture.timeperframe.numerator = vd->fps_num;
	vd->streamparm.parm.capture.timeperframe.denominator = vd->fps_denom;

	ret = xioctl(vd->fd, VIDIOC_S_PARM, &vd->streamparm);
	if (ret < 0)
	{
		fprintf(stderr, "V4L2_CORE: (VIDIOC_S_PARM) error: %s\n", strerror(errno));
		fprintf(stderr, "V4L2_CORE: Unable to set %d/%d fps\n", vd->fps_num, vd->fps_denom);
	}

	return ret;
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
int get_v4l2_framerate (v4l2_dev* vd)
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
