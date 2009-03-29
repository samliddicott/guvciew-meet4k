/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
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
#  V4L2 interface                                                               #
#                                                                               # 
********************************************************************************/

#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <errno.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>

#include "v4l2uvc.h"
#include "v4l2_dyna_ctrls.h"
#include "utils.h"
#include "picture.h"
#include "colorspaces.h"

/* needed only for language files (not used)*/
/*UVC driver control strings*/
#define	BRIGHT 		N_("Brightness")
#define	CONTRAST 	N_("Contrast")
#define	HUE 		N_("Hue")
#define	SATURAT		N_("Saturation")
#define	SHARP		N_("Sharpness")
#define	GAMMA		N_("Gamma")
#define	BLCOMP		N_("Backlight Compensation")
#define	GAIN		N_("Gain")
#define	PLFREQ		N_("Power Line Frequency")
#define HUEAUTO		N_("Hue, Auto")
#define	EXPAUTO		N_("Exposure, Auto")
#define	EXPAUTOPRI	N_("Exposure, Auto Priority")
#define	EXPABS		N_("Exposure (Absolute)")
#define	WBTAUTO		N_("White Balance Temperature, Auto")
#define	WBT		N_("White Balance Temperature")
#define WBCAUTO		N_("White Balance Component, Auto")
#define WBCB		N_("White Balance Blue Component")
#define	WBCR		N_("White Balance Red Component")
#define	FOCUSAUTO	N_("Focus, Auto")
#define EXPMENU1	N_("Manual Mode")
#define EXPMENU2	N_("Auto Mode")
#define EXPMENU3	N_("Shutter Priority Mode")
#define EXPMENU4	N_("Aperture Priority Mode")

/* Query video device capabilities and supported formats
 * args:
 * vd: pointer to a VdIn struct ( must be allready allocated )
 *
 * returns: error code  (0- OK)
*/
static int check_videoIn(struct vdIn *vd)
{
	if (vd == NULL)
		return VDIN_ALLOC_ERR;
	
	memset(&vd->cap, 0, sizeof(struct v4l2_capability));
	
	if ( ioctl(vd->fd, VIDIOC_QUERYCAP, &vd->cap) < 0 ) 
	{
		perror("VIDIOC_QUERYCAP error");
		return VDIN_QUERYCAP_ERR;
	}

	if ( ( vd->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE ) == 0) 
	{
		g_printerr("Error opening device %s: video capture not supported.\n",
				vd->videodevice);
		return VDIN_QUERYCAP_ERR;
	}
	if (vd->grabmethod) 
	{
		if (!(vd->cap.capabilities & V4L2_CAP_STREAMING)) 
		{
			g_printerr("%s does not support streaming i/o\n", 
				vd->videodevice);
			return VDIN_QUERYCAP_ERR;
		}
	} 
	else 
	{
		if (!(vd->cap.capabilities & V4L2_CAP_READWRITE)) 
		{
			g_printerr("%s does not support read i/o\n", 
				vd->videodevice);
			return VDIN_QUERYCAP_ERR;
		}
	}
	g_printf("Init. %s (location: %s)\n", vd->cap.card, vd->cap.bus_info);
	
	vd->listFormats = enum_frame_formats( &(vd->width), &(vd->height), vd->fd);
	
	if(!(vd->listFormats->listVidFormats))
		g_printerr("Couldn't detect any supported formats on your device (%i)\n", vd->listFormats->numb_formats);
	return VDIN_OK;
}

/* Query and map buffers
 * args:
 * vd: pointer to a VdIn struct ( must be allready allocated )
 * setUNMAP: ( flag )if set unmap old buffers first
 *
 * returns: error code  (0- OK)
*/
static int query_buff(struct vdIn *vd, const int setUNMAP) 
{
	int i=0;
	int ret=0;
	for (i = 0; i < NB_BUFFER; i++) 
	{
		
		// unmap old buffer
		if (setUNMAP)
			if(munmap(vd->mem[i],vd->buf.length)<0) perror("couldn't unmap buff");
		
		memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
		vd->buf.index = i;
		vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vd->buf.flags = V4L2_BUF_FLAG_TIMECODE;
		vd->buf.timecode = vd->timecode;
		vd->buf.timestamp.tv_sec = 0;//get frame as soon as possible
		vd->buf.timestamp.tv_usec = 0;
		vd->buf.memory = V4L2_MEMORY_MMAP;
		ret = ioctl(vd->fd, VIDIOC_QUERYBUF, &vd->buf);
		if (ret < 0) 
		{
			perror("VIDIOC_QUERYBUF - Unable to query buffer");
			return VDIN_QUERYBUF_ERR;
		}
		if (vd->buf.length <= 0) 
			g_printerr("WARNING VIDIOC_QUERYBUF - buffer length is %d\n",
					       vd->buf.length);
		// map new buffer
		vd->mem[i] = mmap( 0, // start anywhere
			  vd->buf.length, PROT_READ, MAP_SHARED, vd->fd,
			  vd->buf.m.offset);
		if (vd->mem[i] == MAP_FAILED) 
		{
			perror("Unable to map buffer");
			return VDIN_MMAP_ERR;
		}
	}
	return VDIN_OK;
}

/* Queue Buffers
 * args:
 * vd: pointer to a VdIn struct ( must be allready allocated )
 *
 * returns: error code  (0- OK)
*/
static int queue_buff(struct vdIn *vd)
{
	int i=0;
	int ret=0;
	for (i = 0; i < NB_BUFFER; ++i) 
	{
		memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
		vd->buf.index = i;
		vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vd->buf.flags = V4L2_BUF_FLAG_TIMECODE;
		vd->buf.timecode = vd->timecode;
		vd->buf.timestamp.tv_sec = 0;//get frame as soon as possible
		vd->buf.timestamp.tv_usec = 0;
		vd->buf.memory = V4L2_MEMORY_MMAP;
		ret = ioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
		if (ret < 0) 
		{
			perror("VIDIOC_QBUF - Unable to queue buffer");
			return VDIN_QBUF_ERR;
		}
	}
	return VDIN_OK;
}

/* Try/Set device video stream format
 * args:
 * vd: pointer to a VdIn struct ( must be allready allocated )
 *
 * returns: error code ( 0 - VDIN_OK)
*/
static int init_v4l2(struct vdIn *vd)
{
	int ret = 0;
	
	// make sure we set a valid format
	g_printf("checking format: %i\n", vd->formatIn);
	if ((ret=check_SupPixFormat(vd->formatIn)) < 0)
	{
		// not available - Fail so we can check other formats (don't bother trying it)
		g_printerr("Format unavailable: %d.\n",vd->formatIn);
		return VDIN_FORMAT_ERR;
	}
    
	// set format
	vd->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vd->fmt.fmt.pix.width = vd->width;
	vd->fmt.fmt.pix.height = vd->height;
	vd->fmt.fmt.pix.pixelformat = vd->formatIn;
	vd->fmt.fmt.pix.field = V4L2_FIELD_ANY;
	
	ret = ioctl(vd->fd, VIDIOC_S_FMT, &vd->fmt);
	if (ret < 0) 
	{
		perror("VIDIOC_S_FORMAT - Unable to set format");
		return VDIN_FORMAT_ERR;
	}
	if ((vd->fmt.fmt.pix.width != vd->width) ||
		(vd->fmt.fmt.pix.height != vd->height)) 
	{
		g_printerr("Requested Format unavailable: get width %d height %d \n",
		vd->fmt.fmt.pix.width, vd->fmt.fmt.pix.height);
		vd->width = vd->fmt.fmt.pix.width;
		vd->height = vd->fmt.fmt.pix.height;
	}
	
	vd->streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vd->streamparm.parm.capture.timeperframe.numerator = vd->fps_num;
	vd->streamparm.parm.capture.timeperframe.denominator = vd->fps;
	ret = ioctl(vd->fd,VIDIOC_S_PARM,&vd->streamparm);
	if (ret < 0) 
	{
		g_printerr("Unable to set %d fps\n",vd->fps);
		perror("VIDIOC_S_PARM error");
	}	
	// request buffers 
	memset(&vd->rb, 0, sizeof(struct v4l2_requestbuffers));
	vd->rb.count = NB_BUFFER;
	vd->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vd->rb.memory = V4L2_MEMORY_MMAP;

	ret = ioctl(vd->fd, VIDIOC_REQBUFS, &vd->rb);
	if (ret < 0) 
	{
		perror("VIDIOC_REQBUFS - Unable to allocate buffers");
		return VDIN_REQBUFS_ERR;
	}
	// map the buffers 
	if (query_buff(vd,0)) return VDIN_QUERYBUF_ERR;
	// Queue the buffers
	if (queue_buff(vd)) return VDIN_QBUF_ERR;
	
	return VDIN_OK;
}

/* Init VdIn struct with default and/or global values
 * args:
 * vd: pointer to a VdIn struct ( must be allready allocated )
 * global: pointer to a GLOBAL struct ( must be allready initiated )
 *
 * returns: error code ( 0 - VDIN_OK)
*/
int init_videoIn(struct vdIn *vd, struct GLOBAL *global)
{
	int ret = VDIN_OK;
	int i=0;
	char *device = global->videodevice;
	int width = global->width;
	int height = global->height;
	int format = global->format;
	int grabmethod = global->grabmethod;
	int fps = global->fps;
	int fps_num = global->fps_num;
	
	vd->mutex = g_mutex_new();
	if (vd == NULL || device == NULL)
		return VDIN_ALLOC_ERR;
	if (width == 0 || height == 0)
		return VDIN_RESOL_ERR;
	if (grabmethod < 0 || grabmethod > 1)
		grabmethod = 1;		//mmap by default
	vd->videodevice = NULL;
	vd->videodevice = g_strdup(device);
	g_printf("video device: %s \n", vd->videodevice);
	
	//flad to video thread
	vd->capAVI = FALSE;
	//flag from video thread
	vd->AVICapStop=TRUE;
	
	vd->AVIFName = g_strdup(DEFAULT_AVI_FNAME);
	
	vd->fps = fps;
	vd->fps_num = fps_num;
	vd->signalquit = 1;
	vd->PanTilt=0;
	vd->isbayer = 0; //bayer mode off
	vd->pix_order=0; // pix order for bayer mode def: gbgbgb..|rgrgrg..
	vd->setFPS=0;
	vd->width = width;
	vd->height = height;
	vd->formatIn = format;
	vd->grabmethod = grabmethod;
	vd->capImage=FALSE;
	vd->cap_raw=0;
	
	vd->ImageFName = g_strdup(DEFAULT_IMAGE_FNAME);
	
	//timestamps not supported by UVC driver
	vd->timecode.type = V4L2_TC_TYPE_25FPS;
	vd->timecode.flags = V4L2_TC_FLAG_DROPFRAME;
	
	vd->available_exp[0]=-1;
	vd->available_exp[1]=-1;
	vd->available_exp[2]=-1;
	vd->available_exp[3]=-1;
	
	vd->tmpbuffer = NULL;
	vd->framebuffer = NULL;

	vd->listDevices = enum_devices( vd->videodevice );
	
	if (vd->listDevices != NULL)
	{
		if(!(vd->listDevices->listVidDevices))
			g_printerr("unable to detect video devices on your system (%i)\n", vd->listDevices->num_devices);
	}
	else
		g_printerr("Unable to detect devices on your system\n");
	
	if (vd->fd <=0 ) //open device
	{
		if ((vd->fd = open(vd->videodevice, O_RDWR )) == -1) 
		{
			perror("ERROR opening V4L interface");
			ret = VDIN_DEVICE_ERR;
			goto error;
		}
	}
	
	size_t framebuf_size=0;
	size_t tmpbuf_size=0;
	//reset v4l2_format
	memset(&vd->fmt, 0, sizeof(struct v4l2_format));
	// populate video capabilities structure array
	// should only be called after all vdIn struct elements 
	// have been initialized
	check_videoIn(vd);
	
	if(!(global->control_only))
	{
		if ((ret=init_v4l2(vd)) < 0) 
		{
			g_printerr("Init v4L2 failed !! \n");
			goto error;
		}
		vd->framesizeIn = (vd->width * vd->height << 1); //2 bytes per pixel
		switch (vd->formatIn) 
		{
			case V4L2_PIX_FMT_JPEG:
			case V4L2_PIX_FMT_MJPEG:
				// alloc a temp buffer to reconstruct the pict (MJPEG)
				tmpbuf_size= vd->framesizeIn;
				vd->tmpbuffer = g_new0(unsigned char, tmpbuf_size);
			
				framebuf_size = vd->width * (vd->height + 8) * 2;
				vd->framebuffer = g_new0(unsigned char, framebuf_size); 
				break;
			
			case V4L2_PIX_FMT_UYVY:
			case V4L2_PIX_FMT_YVYU:
			case V4L2_PIX_FMT_YYUV:
			case V4L2_PIX_FMT_YUV420:
			case V4L2_PIX_FMT_YVU420:
				// alloc a temp buffer for converting to YUYV
				tmpbuf_size= vd->framesizeIn;
				vd->tmpbuffer = g_new0(unsigned char, tmpbuf_size);
			
				framebuf_size = vd->framesizeIn;
				vd->framebuffer = g_new0(unsigned char, framebuf_size);
				break;
			
			case V4L2_PIX_FMT_YUYV:
				//  YUYV doesn't need a temp buffer but we will set it if/when
				//  video processing disable control is checked (bayer processing).
				//            (logitech cameras only) 
				framebuf_size = vd->framesizeIn;
				vd->framebuffer = g_new0(unsigned char, framebuf_size);
				break;
			
			case V4L2_PIX_FMT_SGBRG8: //0
			case V4L2_PIX_FMT_SGRBG8: //1
			case V4L2_PIX_FMT_SBGGR8: //2
			case V4L2_PIX_FMT_SRGGB8: //3
				// Raw 8 bit bayer 
				// when grabbing use:
				//    bayer_to_rgb24(bayer_data, RGB24_data, width, height, 0..3)
				//    rgb2yuyv(RGB24_data, vd->framebuffer, width, height)
		
				// alloc a temp buffer for converting to YUYV
				// rgb buffer for decoding bayer data
				tmpbuf_size = vd->width * vd->height * 3;
				vd->tmpbuffer = g_new0(unsigned char, tmpbuf_size);
			
				framebuf_size = vd->framesizeIn;
				vd->framebuffer = g_new0(unsigned char, framebuf_size);
				break;
			
			default:
				g_printerr("(v4l2uvc.c) should never arrive (1)- exit fatal !!\n");
				ret = VDIN_UNKNOWN_ERR;
				goto error;
				break;
		}
	
		if ((!vd->framebuffer) || (framebuf_size <=0)) 
		{
			g_printerr("couldn't calloc %d bytes of memory for frame buffer\n",
				framebuf_size);
			ret = VDIN_FBALLOC_ERR;
			goto error;
		} 
		else
		{
			// set framebuffer to black (y=0x00 u=0x80 v=0x80) by default
			switch (vd->formatIn) {
				case V4L2_PIX_FMT_JPEG:
				case V4L2_PIX_FMT_MJPEG:
				case V4L2_PIX_FMT_SGBRG8: // converted to YUYV
				case V4L2_PIX_FMT_SGRBG8: // converted to YUYV
				case V4L2_PIX_FMT_SBGGR8: // converted to YUYV
				case V4L2_PIX_FMT_SRGGB8: // converted to YUYV
				case V4L2_PIX_FMT_YUV420: // converted to YUYV
				case V4L2_PIX_FMT_YVU420: // converted to YUYV
				case V4L2_PIX_FMT_YYUV:   // converted to YUYV
				case V4L2_PIX_FMT_YVYU:   // converted to YUYV
				case V4L2_PIX_FMT_UYVY:   // converted to YUYV
				case V4L2_PIX_FMT_YUYV:
					for (i=0; i<(framebuf_size-4); i+=4)
					{
						vd->framebuffer[i]=0x00;  //Y
						vd->framebuffer[i+1]=0x80;//U
						vd->framebuffer[i+2]=0x00;//Y
						vd->framebuffer[i+3]=0x80;//V
					}
					break;
					
				default:
					g_printerr("(v4l2uvc.c) should never arrive (2)- exit fatal !!\n");
					ret = VDIN_UNKNOWN_ERR;
					goto error;
					break;
		}	
		}
	}
	return VDIN_OK;
	//error: clean up allocs
error:
	g_free(vd->framebuffer);
	g_free(vd->tmpbuffer);
	close(vd->fd);
	vd->fd=0;
	g_free(vd->videodevice);
	g_free(vd->AVIFName);
	g_free(vd->ImageFName);
	g_mutex_free( vd->mutex );
	return ret;
}

/* Enable video stream
 * args:
 * vd: pointer to a VdIn struct ( must be allready initiated)
 *
 * returns: VIDIOC_STREAMON ioctl result (0- OK)
*/
static int video_enable(struct vdIn *vd)
{
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int ret=0;

	ret = ioctl(vd->fd, VIDIOC_STREAMON, &type);
	if (ret < 0) 
	{
		perror("VIDIOC_STREAMON - Unable to start capture");
		return ret;
	}
	vd->isstreaming = 1;
	return 0;
}

/* Disable video stream
 * args:
 * vd: pointer to a VdIn struct ( must be allready initiated)
 *
 * returns: VIDIOC_STREAMOFF ioctl result (0- OK)
*/
static int video_disable(struct vdIn *vd)
{
	int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	int ret=0;

	ret = ioctl(vd->fd, VIDIOC_STREAMOFF, &type);
	if (ret < 0) 
	{
		perror("VIDIOC_STREAMOFF - Unable to stop capture");
		if(errno == 9) vd->isstreaming = 0;/*capture as allready stoped*/
		return ret;
	}
	vd->isstreaming = 0;
	return 0;
}

/* Grabs video frame and decodes it if necessary
 * args:
 * vd: pointer to a VdIn struct ( must be allready initiated)
 *
 * returns: error code ( 0 - VDIN_OK)
*/
int uvcGrab(struct vdIn *vd)
{
#define HEADERFRAME1 0xaf
	int ret = VDIN_OK;
	fd_set rdset;
	struct timeval timeout;
	//make sure streaming is on
	if (!vd->isstreaming)
		if (video_enable(vd)) goto err;
	
	FD_ZERO(&rdset);
	FD_SET(vd->fd, &rdset);
	timeout.tv_sec = 1; // 1 sec timeout 
	timeout.tv_usec = 0;
	// select - wait for data or timeout
	ret = select(vd->fd + 1, &rdset, NULL, NULL, &timeout);
	if (ret < 0) 
	{
		perror(" Could not grab image (select error)");
		return VDIN_SELEFAIL_ERR;
	} 
	else if (ret == 0)
	{
		perror(" Could not grab image (select timeout)");
		return VDIN_SELETIMEOUT_ERR;
	}
	else if ((ret > 0) && (FD_ISSET(vd->fd, &rdset))) 
	{
		memset(&vd->buf, 0, sizeof(struct v4l2_buffer));
		vd->buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vd->buf.memory = V4L2_MEMORY_MMAP;

		ret = ioctl(vd->fd, VIDIOC_DQBUF, &vd->buf);
		if (ret < 0) 
		{
			perror("VIDIOC_DQBUF - Unable to dequeue buffer ");
			ret = VDIN_DEQBUFS_ERR;
			goto err;
		}
	}

	// save raw frame
	if (vd->cap_raw>0) 
	{
		if (vd->buf.bytesused > vd->framesizeIn)
			SaveBuff (vd->ImageFName,vd->buf.bytesused,vd->mem[vd->buf.index]);
		else
			SaveBuff (vd->ImageFName,vd->framesizeIn,vd->mem[vd->buf.index]);
	
		vd->cap_raw=0;
	}
	
	switch (vd->formatIn) 
	{
		case V4L2_PIX_FMT_JPEG:
		case V4L2_PIX_FMT_MJPEG:
			if(vd->buf.bytesused <= HEADERFRAME1) 
			{
				// Prevent crash on empty image
				g_printf("Ignoring empty buffer ...\n");
				return VDIN_OK;
			}
			memcpy(vd->tmpbuffer, vd->mem[vd->buf.index],vd->buf.bytesused);
			
			if (jpeg_decode(&vd->framebuffer, vd->tmpbuffer, &vd->width,
				&vd->height) < 0) 
			{
				g_printerr("jpeg decode errors\n");
				ret = VDIN_DECODE_ERR;
				goto err;
			}
			break;
		
		case V4L2_PIX_FMT_UYVY:
			memcpy(vd->tmpbuffer, vd->mem[vd->buf.index],vd->buf.bytesused);
			uyvy_to_yuyv(vd->framebuffer, vd->tmpbuffer, vd->width, vd->height);
			break;
			
		case V4L2_PIX_FMT_YVYU:
			memcpy(vd->tmpbuffer, vd->mem[vd->buf.index],vd->buf.bytesused);
			yvyu_to_yuyv(vd->framebuffer, vd->tmpbuffer, vd->width, vd->height);
			break;
			
		case V4L2_PIX_FMT_YYUV:
			memcpy(vd->tmpbuffer, vd->mem[vd->buf.index],vd->buf.bytesused);
			yyuv_to_yuyv(vd->framebuffer, vd->tmpbuffer, vd->width, vd->height);
			break;
			
		case V4L2_PIX_FMT_YUV420:
			memcpy(vd->tmpbuffer, vd->mem[vd->buf.index],vd->buf.bytesused);
			
			if (yuv420_to_yuyv(vd->framebuffer, vd->tmpbuffer, vd->width,
				vd->height) < 0) 
			{
				g_printerr("error converting yuv420 to yuyv\n");
				ret = VDIN_DECODE_ERR;
				goto err;
			}
			break;
		
		case V4L2_PIX_FMT_YVU420:
			memcpy(vd->tmpbuffer, vd->mem[vd->buf.index],vd->buf.bytesused);
			
			if (yvu420_to_yuyv(vd->framebuffer, vd->tmpbuffer, vd->width,
				vd->height) < 0) 
			{
				g_printerr("error converting yvu420 to yuyv\n");
				ret = VDIN_DECODE_ERR;
				goto err;
			}
			break;
		
		case V4L2_PIX_FMT_YUYV:
			if(vd->isbayer>0) 
			{
				if (!(vd->tmpbuffer)) 
				{
					// rgb buffer for decoding bayer data
					vd->tmpbuffer = g_new0(unsigned char, 
						vd->width * vd->height * 3);
				}
				bayer_to_rgb24 (vd->mem[vd->buf.index],vd->tmpbuffer,vd->width,vd->height, vd->pix_order);
				// raw bayer is only available in logitech cameras in yuyv mode
				rgb2yuyv (vd->tmpbuffer,vd->framebuffer,vd->width,vd->height);
			} 
			else 
			{
				if (vd->buf.bytesused > vd->framesizeIn)
					memcpy(vd->framebuffer, vd->mem[vd->buf.index],
						(size_t) vd->framesizeIn);
				else
					memcpy(vd->framebuffer, vd->mem[vd->buf.index],
						(size_t) vd->buf.bytesused);
			}
			break;
		
		
		case V4L2_PIX_FMT_SGBRG8: //0
			bayer_to_rgb24 (vd->mem[vd->buf.index],vd->tmpbuffer,vd->width,vd->height, 0);
			rgb2yuyv (vd->tmpbuffer,vd->framebuffer,vd->width,vd->height);
			break;
		case V4L2_PIX_FMT_SGRBG8: //1
			bayer_to_rgb24 (vd->mem[vd->buf.index],vd->tmpbuffer,vd->width,vd->height, 1);
			rgb2yuyv (vd->tmpbuffer,vd->framebuffer,vd->width,vd->height);
			break;
		case V4L2_PIX_FMT_SBGGR8: //2
			bayer_to_rgb24 (vd->mem[vd->buf.index],vd->tmpbuffer,vd->width,vd->height, 2);
			rgb2yuyv (vd->tmpbuffer,vd->framebuffer,vd->width,vd->height);
			break;
		case V4L2_PIX_FMT_SRGGB8: //3
			bayer_to_rgb24 (vd->mem[vd->buf.index],vd->tmpbuffer,vd->width,vd->height, 3);
			rgb2yuyv (vd->tmpbuffer,vd->framebuffer,vd->width,vd->height);
			break;
		
		default:
			g_printerr("error grabbing (v4l2uvc.c) unknown format: %i\n", vd->formatIn);
			ret = VDIN_UNKNOWN_ERR;
			goto err;
			break;
	}
	if(vd->setFPS) 
	{
		video_disable(vd);
		input_set_framerate (vd);
		video_enable(vd);
		query_buff(vd,1);
		queue_buff(vd);
		vd->setFPS=0;
	} 
	else 
	{	
		ret = ioctl(vd->fd, VIDIOC_QBUF, &vd->buf);
		if (ret < 0) 
		{
			perror("VIDIOC_QBUF - Unable to requeue buffer");
			ret = VDIN_REQBUFS_ERR;
			goto err;
		}
	}

	return VDIN_OK;
err:
	vd->signalquit = 0;
	return ret;
}

/* cleans VdIn struct and allocations
 * args:
 * pointer to initiated vdIn struct
 *
 * returns: void
*/
void close_v4l2(struct vdIn *vd)
{
	int i=0;
	
	if (vd->isstreaming) video_disable(vd);
	g_free(vd->tmpbuffer);
	g_free(vd->framebuffer);
	g_free(vd->videodevice);
	g_free(vd->ImageFName);
	g_free(vd->AVIFName);
	// free format allocations
	freeFormats(vd->listFormats);
	// unmap queue buffers
	for (i = 0; i < NB_BUFFER; i++) 
	{
		if((vd->mem[i] != MAP_FAILED) && vd->buf.length)
			if(munmap(vd->mem[i],vd->buf.length)<0) 
			{
				perror("Failed to unmap buffer");
			}
	}
	
	vd->videodevice = NULL;
	vd->tmpbuffer = NULL;
	vd->framebuffer = NULL;
	vd->ImageFName = NULL;
	vd->AVIFName = NULL;
	freeDevices(vd->listDevices);
	// close device descriptor
	close(vd->fd);
	g_mutex_free( vd->mutex );
	// free struct allocation
	g_free(vd);
	vd=NULL;
}

/* sets video device frame rate
 * args:
 * vd: pointer to a VdIn struct ( must be allready initiated)
 *
 * returns: VIDIOC_S_PARM ioctl result value
*/
int
input_set_framerate (struct vdIn * device)
{  
	int fd;
	int ret=0;

	fd = device->fd;

	device->streamparm.parm.capture.timeperframe.numerator = device->fps_num;
	device->streamparm.parm.capture.timeperframe.denominator = device->fps;
	 
	ret = ioctl(fd,VIDIOC_S_PARM,&device->streamparm);
	if (ret < 0) 
	{
		g_printerr("Unable to set %d fps\n",device->fps);
		perror("VIDIOC_S_PARM error");
	} 

	return ret;
}

/* gets video device defined frame rate (not real - consider it a maximum value)
 * args:
 * vd: pointer to a VdIn struct ( must be allready initiated)
 *
 * returns: VIDIOC_G_PARM ioctl result value
*/
int
input_get_framerate (struct vdIn * device)
{
	int fd;
	int ret=0;

	fd = device->fd;

	ret = ioctl(fd,VIDIOC_G_PARM,&device->streamparm);
	if (ret < 0) 
	{
		perror("VIDIOC_G_PARM - Unable to get timeperframe");
	} 
	else 
	{
		// it seems numerator is allways 1 but we don't do assumptions here :-)
		device->fps = device->streamparm.parm.capture.timeperframe.denominator;
		device->fps_num = device->streamparm.parm.capture.timeperframe.numerator;
	}
	return ret;
}
