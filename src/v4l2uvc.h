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
#  MJpeg decoding and frame capture taken from luvcview                         #
#                                                                               # 
#                                                                               #
********************************************************************************/

#ifndef V4L2UVC_H
#define V4L2UVC_H

#include <linux/videodev2.h>
#include "globals.h"
#include "v4l2_devices.h"
#include "v4l2_formats.h"
#include "v4l2_controls.h"

#define NB_BUFFER 4

/*timecode flag for unsetting UVC_QUEUE_DROP_INCOMPLETE*/
/*must patch UVC driver */
#define V4L2_TC_FLAG_NO_DROP		0x1000

enum  v4l2_uvc_exposure_auto_type 
{
	V4L2_UVC_EXPOSURE_MANUAL = 1,
	V4L2_UVC_EXPOSURE_AUTO = 2,
	V4L2_UVC_EXPOSURE_SHUTTER_PRIORITY = 4,
	V4L2_UVC_EXPOSURE_APERTURE_PRIORITY = 8
};

static const int exp_vals[]=
{
	V4L2_UVC_EXPOSURE_MANUAL,
	V4L2_UVC_EXPOSURE_AUTO,
	V4L2_UVC_EXPOSURE_SHUTTER_PRIORITY, 
	V4L2_UVC_EXPOSURE_APERTURE_PRIORITY
};

typedef struct _PanTiltInfo
{
	gint pan;
	gint tilt;
	gint reset;
} PanTiltInfo;

struct vdIn 
{
	GMutex *mutex;
	int fd;
	char *videodevice;
	//char *status;
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	struct v4l2_buffer buf;
	struct v4l2_requestbuffers rb;
	struct v4l2_timecode timecode;
	struct v4l2_streamparm streamparm;
	void *mem[NB_BUFFER];
	unsigned char *tmpbuffer;
	unsigned char *framebuffer;
	size_t tmpbuf_size;
	size_t framebuf_size;
	int isstreaming;
	int isbayer;
	int pix_order;
	int setFPS;
	int PanTilt; /*1-if PanTilt Camera; 0-otherwise*/
	PanTiltInfo *Pantilt_info;
	int grabmethod;
	int width;
	int height;
	//int numb_resol;
	int numb_formats;
	int formatIn;
	int formatOut;
	int framesizeIn;
	int signalquit;
	int capAVI;
	int AVICapStop;
	char *AVIFName;
	int fps;
	int fps_num;
	int capImage;
	char *ImageFName;
	int cap_raw;
	int available_exp[4]; //backward compatible (old v4l2 exposure menu interface)
	VidFormats *listVidFormats;
	VidDevice *listVidDevices;
	int num_devices;
	int  current_device;
};

int check_videoIn(struct vdIn *vd);

int init_videoIn(struct vdIn *vd, struct GLOBAL *global);

int uvcGrab(struct vdIn *vd);

void close_v4l2(struct vdIn *vd);

int get_FormatIndex(struct vdIn *vd, int format);

int input_set_framerate (struct vdIn * device);

int input_get_framerate (struct vdIn * device);

int init_v4l2(struct vdIn *vd);

#endif

