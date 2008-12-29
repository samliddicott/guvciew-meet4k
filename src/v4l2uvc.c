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
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>

#include "v4l2uvc.h"
#include "utils.h"
#include "picture.h"
#include "colorspaces.h"
/* support for internationalization - i18n */
#include <glib/gi18n.h>

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


/* some Logitech webcams have pan/tilt/focus controls */
#define LENGTH_OF_XU_CTR (6)
#define LENGTH_OF_XU_MAP (10)

static struct uvc_xu_control_info xu_ctrls[] = 
{
	{
		.entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector = XU_MOTORCONTROL_PANTILT_RELATIVE,
		.index    = 0,
		.size     = 4,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_DEF
	},
	{
		.entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector = XU_MOTORCONTROL_PANTILT_RESET,
		.index    = 1,
		.size     = 1,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF
	},
	{
		.entity   = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector = XU_MOTORCONTROL_FOCUS,
		.index    = 2,
		.size     = 6,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_CUR | UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX |UVC_CONTROL_GET_DEF
	},
	{
		.entity   = UVC_GUID_LOGITECH_VIDEO_PIPE,
		.selector = XU_COLOR_PROCESSING_DISABLE,
		.index    = 4,
		.size     = 1,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_CUR |UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF
	},
	{
		.entity   = UVC_GUID_LOGITECH_VIDEO_PIPE,
		.selector = XU_RAW_DATA_BITS_PER_PIXEL,
		.index    = 7,
		.size     = 1,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_CUR |UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF
	},
	{
		.entity   = UVC_GUID_LOGITECH_USER_HW_CONTROL,
		.selector = XU_HW_CONTROL_LED1,
		.index    = 0,
		.size     = 3,
		.flags    = UVC_CONTROL_SET_CUR | UVC_CONTROL_GET_CUR |UVC_CONTROL_GET_MIN | UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | UVC_CONTROL_GET_DEF
	},
	
};

/* mapping for Pan/Tilt/Focus */
static struct uvc_xu_control_mapping xu_mappings[] = 
{
	{
		.id        = V4L2_CID_PAN_RELATIVE_NEW,
		.name      = N_("Pan (relative)"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_PANTILT_RELATIVE,
		.size      = 16,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_SIGNED
	},
	{
		.id        = V4L2_CID_TILT_RELATIVE_NEW,
		.name      = N_("Tilt (relative)"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_PANTILT_RELATIVE,
		.size      = 16,
		.offset    = 16,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_SIGNED
	},
	{
		.id        = V4L2_CID_PAN_RESET_NEW,
		.name      = N_("Pan (reset)"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_PANTILT_RESET,
		.size      = 1,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
	},
	{
		.id        = V4L2_CID_TILT_RESET_NEW,
		.name      = N_("Tilt (reset)"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_PANTILT_RESET,
		.size      = 1,
		.offset    = 1,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
	},
	{
		.id        = V4L2_CID_PANTILT_RESET_LOGITECH,
		.name      = N_("Pan/Tilt (reset)"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_PANTILT_RESET,
		.size      = 8,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
	},
	{
		.id        = V4L2_CID_FOCUS_LOGITECH,
		.name      = N_("Focus (absolute)"),
		.entity    = UVC_GUID_LOGITECH_MOTOR_CONTROL,
		.selector  = XU_MOTORCONTROL_FOCUS,
		.size      = 8,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
	},
	{
		.id        = V4L2_CID_LED1_MODE_LOGITECH,
		.name      = N_("LED1 Mode"),
		.entity    = UVC_GUID_LOGITECH_USER_HW_CONTROL,
		.selector  = XU_HW_CONTROL_LED1,
		.size      = 8,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
	},
	{
		.id        = V4L2_CID_LED1_FREQUENCY_LOGITECH,
		.name      = N_("LED1 Frequency"),
		.entity    = UVC_GUID_LOGITECH_USER_HW_CONTROL,
		.selector  = XU_HW_CONTROL_LED1,
		.size      = 8,
		.offset    = 16,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
	},
	{
		.id        = V4L2_CID_DISABLE_PROCESSING_LOGITECH,
		.name      = N_("Disable video processing"),
		.entity    = UVC_GUID_LOGITECH_VIDEO_PIPE,
		.selector  = XU_COLOR_PROCESSING_DISABLE,
		.size      = 8,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_BOOLEAN,
		.data_type = UVC_CTRL_DATA_TYPE_BOOLEAN
	},
	{
		.id        = V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH,
		.name      = N_("Raw bits per pixel"),
		.entity    = UVC_GUID_LOGITECH_VIDEO_PIPE,
		.selector  = XU_RAW_DATA_BITS_PER_PIXEL,
		.size      = 8,
		.offset    = 0,
		.v4l2_type = V4L2_CTRL_TYPE_INTEGER,
		.data_type = UVC_CTRL_DATA_TYPE_UNSIGNED
	},
	
};


#define SUP_PIX_FMT 6 /*number of supported formats*/

static SupFormats listSupFormats[] =
{
	{
		.format   = V4L2_PIX_FMT_MJPEG,
		.mode     = "mjpg",
		.hardware = 0
	},
	{
		.format   = V4L2_PIX_FMT_JPEG,
		.mode     ="jpeg",
		.hardware = 0
	},
	{
		.format   = V4L2_PIX_FMT_YUYV,
		.mode     = "yuv",
		.hardware = 0
	},
	{
		.format   = V4L2_PIX_FMT_UYVY,
		.mode     = "uyv",
		.hardware = 0
	},
	{
		.format   = V4L2_PIX_FMT_YYUV,
		.mode     = "yyu",
		.hardware = 0
	},
	{
		.format   = V4L2_PIX_FMT_YUV420,
		.mode     = "yup",
		.hardware = 0
	},
	{
		.format   = V4L2_PIX_FMT_SGBRG8,
		.mode     = "gbr",
		.hardware = 0
	},
};

/*check if format is supported by guvcview*/
static int check_PixFormat(int pixfmt)
{
	int i=0;
	for (i=0; i<SUP_PIX_FMT; i++)
	{
		if (pixfmt == listSupFormats[i].format)
		{
			return (i); /*return index from supported formats list*/
		}
		else i++;
	}
	return (-1);
} 

/*check if format is supported by hardware*/
static int check_SupPixFormat(int pixfmt)
{
	int i=0;
	for (i=0; i<SUP_PIX_FMT; i++)
	{
		if (pixfmt == listSupFormats[i].format)
		{
			if(listSupFormats[i].hardware > 0) return (i); /*supported by hardware*/
		}
		else i++;
	}
	return (-1);

}

int get_PixMode(int pixfmt, char *mode)
{
	int i=0;
	for (i=0; i<SUP_PIX_FMT; i++)
	{
		if (pixfmt == listSupFormats[i].format)
		{
			g_snprintf(mode, 5, "%s", listSupFormats[i].mode);
			return (1);
		}
		else i++;
	}
	return (-1);
}

int get_PixFormat(char *mode)
{
	int i=0;
	for (i=0; i<SUP_PIX_FMT; i++)
	{
		if (g_strcmp0(mode, listSupFormats[i].mode)==0)
		{
			return (listSupFormats[i].format);
		}
		else i++;
	}
	return (listSupFormats[0].format); /*default is- MJPG*/
}

static int enum_devices(struct vdIn *vd)
{
	int ret=0;
	int fd=0;
	if (vd == NULL)
		return -1;
	struct v4l2_capability v4l2_cap;
	GDir *v4l2_dir=NULL;
	GError *error=NULL;
	v4l2_dir = g_dir_open("/sys/class/video4linux",0,&error);
	if(v4l2_dir == NULL)
	{
		g_printerr ("opening '/sys/class/video4linux' failed: %s\n", 
			 error->message);
		g_error_free ( error );
		return -1;
	}
	const gchar *v4l2_device;
	vd->num_devices = 0;
	while((v4l2_device = g_dir_read_name(v4l2_dir)) != NULL)
	{
		if(g_strrstr(v4l2_device, "video") != v4l2_device)
			continue;
		vd->num_devices++;
		vd->listVidDevices = g_renew(VidDevice, 
			vd->listVidDevices, 
			vd->num_devices);
		vd->listVidDevices[vd->num_devices-1].device = g_strjoin("/","/dev",v4l2_device,NULL);
		
		if ((fd = open(vd->listVidDevices[vd->num_devices-1].device, O_RDWR )) == -1) 
		{
			g_printerr("ERROR opening V4L interface for %s\n",
				vd->listVidDevices[vd->num_devices-1].device);
			close(fd);
			vd->listVidDevices[vd->num_devices-1].name = g_strdup(v4l2_device);
			vd->listVidDevices[vd->num_devices-1].driver = NULL;
			vd->listVidDevices[vd->num_devices-1].location = NULL;
			vd->listVidDevices[vd->num_devices-1].valid = 0;
			vd->listVidDevices[vd->num_devices-1].current = 0;
			continue;
		} 
		else
		{
			ret = ioctl(fd, VIDIOC_QUERYCAP, &v4l2_cap);
			if (ret < 0) 
			{
				perror("VIDIOC_QUERYCAP error");
				g_printerr("   couldn't query device %s\n",
					vd->listVidDevices[vd->num_devices-1].device);
				close(fd);
				vd->listVidDevices[vd->num_devices-1].name = g_strdup(v4l2_device);
				vd->listVidDevices[vd->num_devices-1].driver = NULL;
				vd->listVidDevices[vd->num_devices-1].location = NULL;
				vd->listVidDevices[vd->num_devices-1].valid = 0;
				vd->listVidDevices[vd->num_devices-1].current = 0;
				continue;
			}
			else
			{
				vd->listVidDevices[vd->num_devices-1].name = g_strdup((gchar *) v4l2_cap.card);
				vd->listVidDevices[vd->num_devices-1].driver = g_strdup((gchar *) v4l2_cap.driver);
				vd->listVidDevices[vd->num_devices-1].location = g_strdup((gchar *) v4l2_cap.bus_info);
				vd->listVidDevices[vd->num_devices-1].valid = 1;
				if(g_strcmp0(vd->videodevice,vd->listVidDevices[vd->num_devices-1].device)==0) 
					vd->listVidDevices[vd->num_devices-1].current = 1;
				else
					vd->listVidDevices[vd->num_devices-1].current = 0;
			}
			
		}
		
		close(fd);
	}
	
	g_dir_close(v4l2_dir);
	return(vd->num_devices);
}

static void freeDevices(struct vdIn *vd)
{
	int i=0;
	
	for(i=0;i<(vd->num_devices);i++)
	{
		g_free(vd->listVidDevices[i].device);
		g_free(vd->listVidDevices[i].name);
		g_free(vd->listVidDevices[i].driver);
		g_free(vd->listVidDevices[i].location);
	}
	g_free(vd->listVidDevices);
}

int check_videoIn(struct vdIn *vd)
{
	int ret=0;
	if (vd == NULL)
		return -1;
	
	
	memset(&vd->cap, 0, sizeof(struct v4l2_capability));
	ret = ioctl(vd->fd, VIDIOC_QUERYCAP, &vd->cap);
	if (ret < 0) 
	{
		perror("VIDIOC_QUERYCAP error");
		goto fatal;
	}

	if ((vd->cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0) {
		g_printerr("Error opening device %s: video capture not supported.\n",
				vd->videodevice);
		goto fatal;;
	}
	if (vd->grabmethod) {
		if (!(vd->cap.capabilities & V4L2_CAP_STREAMING)) {
			g_printerr("%s does not support streaming i/o\n", 
				vd->videodevice);
			goto fatal;
		}
	} else {
		if (!(vd->cap.capabilities & V4L2_CAP_READWRITE)) {
			g_printerr("%s does not support read i/o\n", 
				vd->videodevice);
			goto fatal;
		}
	}
	g_printf("Init. %s (driver: %s location: %s)\n",vd->cap.card, vd->cap.driver,
		vd->cap.bus_info);
	enum_frame_formats(vd);
	return 0;
fatal:
	return -1;
}



int
init_videoIn(struct vdIn *vd, char *device, int width, int height,
	int format, int grabmethod, int fps, int fps_num)
{
	int ret=0;
	int i=0;
	
	if (vd == NULL || device == NULL)
		return -4;
	if (width == 0 || height == 0)
		return -4;
	if (grabmethod < 0 || grabmethod > 1)
		grabmethod = 1;		//mmap by default;
	vd->videodevice = NULL;
	
	
	vd->videodevice = g_strdup(device);
	
	g_printf("video device: %s \n", vd->videodevice);
	
	/*flag to video thread*/
	vd->capAVI = FALSE;
	/*flag from video thread*/
	vd->AVICapStop=TRUE;
	
	vd->AVIFName = g_strdup(DEFAULT_AVI_FNAME);
	
	vd->fps = fps;
	vd->fps_num = fps_num;
	vd->signalquit = 1;
	vd->PanTilt=0;
	vd->isbayer = 0; /*bayer mode off*/
	vd->pix_order=0; /* pix order for bayer mode def: gbgbgb..|rgrgrg..*/
	vd->setFPS=0;
	vd->width = width;
	vd->height = height;
	vd->formatIn = format;
	vd->grabmethod = grabmethod;
	vd->capImage=FALSE;
	vd->cap_raw=0;
	vd->tmpbuf_size=0;
	vd->framebuf_size=0;
	vd->numb_formats=0;
	
	vd->ImageFName = g_strdup(DEFAULT_IMAGE_FNAME);
	
	vd->timecode.type = V4L2_TC_TYPE_25FPS;
	vd->timecode.flags = V4L2_TC_FLAG_DROPFRAME | V4L2_TC_FLAG_NO_DROP;
	
	vd->available_exp[0]=-1;
	vd->available_exp[1]=-1;
	vd->available_exp[2]=-1;
	vd->available_exp[3]=-1;
	
	vd->tmpbuffer = NULL;
	vd->framebuffer = NULL;
	
	if(enum_devices(vd)<1)
		g_printerr("unable to detect video devices on your system\n");
	
	if ((ret=init_v4l2(vd)) < 0) 
	{
		g_printerr("Init v4L2 failed !! \n");
		goto error;
	}
	vd->framesizeIn = (vd->width * vd->height << 1);
	switch (vd->formatIn) 
	{
		case V4L2_PIX_FMT_JPEG:
		case V4L2_PIX_FMT_MJPEG:
			/* alloc a temp buffer to reconstruct the pict (MJPEG)*/
			vd->tmpbuf_size= vd->framesizeIn;
			vd->tmpbuffer = g_new0(unsigned char, vd->tmpbuf_size);
			
			vd->framebuf_size = vd->width * (vd->height + 8) * 2;
			vd->framebuffer = g_new0(unsigned char, vd->framebuf_size); 
			break;
			
		case V4L2_PIX_FMT_YYUV:
		case V4L2_PIX_FMT_YUV420:
			/* alloc a temp buffer for converting to YUYV*/
			vd->tmpbuf_size= vd->framesizeIn;
			vd->tmpbuffer = g_new0(unsigned char, vd->tmpbuf_size);
			
			vd->framebuf_size = vd->framesizeIn;
			vd->framebuffer = g_new0(unsigned char, vd->framebuf_size);
			break;
			
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_UYVY:
			/*YUYV doesn't need a temp buffer but we set if      */
			/*video processing disable control  is set           */
			/*            (logitech cameras only)                */
			vd->framebuf_size = vd->framesizeIn;
			vd->framebuffer = g_new0(unsigned char, vd->framebuf_size);
			break;
			
		case V4L2_PIX_FMT_SGBRG8:
			/*Raw 8 bit GBGB... RGRG...*/
			/*when grabbing */
			/*use: bayer_to_rgb24(bayer_data, RGB24_data, width, height, 0)*/
			/*use: rgb2yuyv(RGB24_data, vd->framebuffer, width, height)*/
		
			/* alloc a temp buffer for converting to YUYV*/
			/* rgb buffer for decoding bayer data*/
			vd->tmpbuf_size = vd->width * vd->height * 3;
			vd->tmpbuffer = g_new0(unsigned char, vd->tmpbuf_size);
			
			vd->framebuf_size = vd->framesizeIn;
			vd->framebuffer = g_new0(unsigned char, vd->framebuf_size);
			break;
			
		default:
			g_printerr("(v4l2uvc.c) should never arrive (1)- exit fatal !!\n");
			ret=-7;
			goto error;
			break;
	}
	
	if ((!vd->framebuffer) || (vd->framebuf_size <=0)) 
	{
		g_printerr("couldn't calloc %d bytes of memory for frame buffer\n",
			vd->framebuf_size);
		ret=-6;
		goto error;
	} 
	else
	{
		/*set framebuffer to black (y=0x00 u=0x80 v=0x80) by default*/
		switch (vd->formatIn) {
			case V4L2_PIX_FMT_JPEG:
			case V4L2_PIX_FMT_MJPEG:
			case V4L2_PIX_FMT_SGBRG8: /*converted to YUYV*/
			case V4L2_PIX_FMT_YUV420: /*converted to YUYV*/
			case V4L2_PIX_FMT_YYUV:   /*converted to YUYV*/ 
			case V4L2_PIX_FMT_YUYV:
				for (i=0; i<(vd->framebuf_size-4); i+=4)
				{
					vd->framebuffer[i]=0x00;  //Y
					vd->framebuffer[i+1]=0x80;//U
					vd->framebuffer[i+2]=0x00;//Y
					vd->framebuffer[i+3]=0x80;//V
				}
			break;
			case V4L2_PIX_FMT_UYVY:
				for (i=0; i<(vd->framebuf_size-4); i+=4)
				{
					vd->framebuffer[i]=0x80;  //U
					vd->framebuffer[i+1]=0x00;//Y
					vd->framebuffer[i+2]=0x80;//V
					vd->framebuffer[i+3]=0x00;//Y
				}
				break;
			default:
				g_printerr("(v4l2uvc.c) should never arrive (2)- exit fatal !!\n");
				ret=-7;
				goto error;
			break;
	}	
	}
	
	return 0;
	/*error: clean up allocs*/
error:
	g_free(vd->framebuffer);
	g_free(vd->tmpbuffer);
	close(vd->fd);
	vd->fd=0;
	g_free(vd->videodevice);
	g_free(vd->AVIFName);
	g_free(vd->ImageFName);
	return ret;
}


/* map the buffers */
static int query_buff(struct vdIn *vd, const int setUNMAP) 
{
	int i=0;
	int ret=0;
	for (i = 0; i < NB_BUFFER; i++) 
	{
		
		/* unmap old buffer */
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
			return 1;
		}
		if (vd->buf.length <= 0) 
			g_printerr("WARNING VIDIOC_QUERYBUF - buffer length is %d\n",
					       vd->buf.length);
		/* map new buffer */
		vd->mem[i] = mmap(0 /* start anywhere */ ,
			  vd->buf.length, PROT_READ, MAP_SHARED, vd->fd,
			  vd->buf.m.offset);
		if (vd->mem[i] == MAP_FAILED) 
		{
			perror("Unable to map buffer");
			return 2;
		}
	}

	return 0;
}

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
			return 1;
		}
	}
	return 0;
}


int init_v4l2(struct vdIn *vd)
{
	int ret = 0;
	if (vd->fd <=0 )
	{
		if ((vd->fd = open(vd->videodevice, O_RDWR )) == -1) 
		{
			perror("ERROR opening V4L interface");
			ret = -1;
			goto fatal;
		}
	}
    
	/*reset v4l2_format */
	memset(&vd->fmt, 0, sizeof(struct v4l2_format));
	/* populate video capabilities structure array           */
	/* should only be called after all vdIn struct elements  */
	/* have been initialized                                 */
	check_videoIn(vd);
	
	/*make sure we set a valid format*/
	if ((ret=check_SupPixFormat(vd->formatIn)) < 0)
	{
		/*not available - Fail so we can check other formats (don't bother trying it)*/
		g_printerr("Format unavailable: %d.\n",vd->formatIn);
		ret = -2;
		goto fatal;
	}
    
	/* set format in */
	vd->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vd->fmt.fmt.pix.width = vd->width;
	vd->fmt.fmt.pix.height = vd->height;
	vd->fmt.fmt.pix.pixelformat = vd->formatIn;
	vd->fmt.fmt.pix.field = V4L2_FIELD_ANY;
	
	ret = ioctl(vd->fd, VIDIOC_S_FMT, &vd->fmt);
	if (ret < 0) 
	{
		perror("VIDIOC_S_FORMAT - Unable to set format");
		ret = -2;
		goto fatal;
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
	/* request buffers */
	memset(&vd->rb, 0, sizeof(struct v4l2_requestbuffers));
	vd->rb.count = NB_BUFFER;
	vd->rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	vd->rb.memory = V4L2_MEMORY_MMAP;

	ret = ioctl(vd->fd, VIDIOC_REQBUFS, &vd->rb);
	if (ret < 0) 
	{
		perror("VIDIOC_REQBUFS - Unable to allocate buffers");
		ret = -3;
		goto fatal;
	}
	/* map the buffers */
	if (query_buff(vd,0)) goto fatal;

	/* Queue the buffers. */
	if (queue_buff(vd)) goto fatal;
	
	return 0;
fatal:
	return ret;

}



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


int uvcGrab(struct vdIn *vd)
{
#define HEADERFRAME1 0xaf
	int ret=0;
	fd_set rdset;
	struct timeval timeout;
	/*make sure streaming is on*/
	if (!vd->isstreaming)
		if (video_enable(vd)) goto err;
			
	FD_ZERO(&rdset);
	FD_SET(vd->fd, &rdset);
	timeout.tv_sec = 1; /* 1 sec timeout */
	timeout.tv_usec = 0;
	ret = select(vd->fd + 1, &rdset, NULL, NULL, &timeout);
	if (ret < 0) 
	{
		perror(" Could not grab image (select error)");
		return (1);
	} 
	else if (ret == 0)
	{
		perror(" Could not grab image (select timeout)");
		return (2);
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
			goto err;
		}
	}

	/*save raw frame */
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
				/* Prevent crash on empty image */
				g_printf("Ignoring empty buffer ...\n");
				return 0;
			}
			memcpy(vd->tmpbuffer, vd->mem[vd->buf.index],vd->buf.bytesused);
			
			if (jpeg_decode(&vd->framebuffer, vd->tmpbuffer, &vd->width,
				&vd->height) < 0) 
			{
				g_printerr("jpeg decode errors\n");
				goto err;
			}
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
				goto err;
			}
			break;
			
		case V4L2_PIX_FMT_YUYV:
		case V4L2_PIX_FMT_UYVY:
			if(vd->isbayer>0) 
			{
				if (!(vd->tmpbuffer)) 
				{
					/* rgb buffer for decoding bayer data*/
					vd->tmpbuffer = g_new0(unsigned char, 
						vd->width * vd->height * 3);
				}
				bayer_to_rgb24 (vd->mem[vd->buf.index],vd->tmpbuffer,vd->width,vd->height, vd->pix_order);
				/*raw bayer is only available in logitech cameras so no uyvy mode, only yuyv*/
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
			
		case V4L2_PIX_FMT_SGBRG8:
			/*pixel order is 0 gb.. rg..*/
			bayer_to_rgb24 (vd->mem[vd->buf.index],vd->tmpbuffer,vd->width,vd->height, 0);
			/*convert to yuyv*/
			rgb2yuyv (vd->tmpbuffer,vd->framebuffer,vd->width,vd->height);
			break;
			
		default:
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
			goto err;
		}
	}

	return 0;
err:
	vd->signalquit = 0;
	return -1;
}

static void freeFormats(struct vdIn *vd)
{
	int i=0;
	int j=0;
	for(i=0;i<vd->numb_formats;i++)
	{
		for(j=0;j<vd->listVidFormats[i].numb_res;j++)
		{
			g_free(vd->listVidFormats[i].listVidCap[j].framerate_num);
			g_free(vd->listVidFormats[i].listVidCap[j].framerate_denom);
		
		}
		g_free(vd->listVidFormats[i].listVidCap);
	}
	g_free(vd->listVidFormats);
}

void close_v4l2(struct vdIn *vd)
{
	int i=0;
	
	if (vd->isstreaming) video_disable(vd);
	g_free(vd->tmpbuffer);
	g_free(vd->framebuffer);
	g_free(vd->videodevice);
	g_free(vd->ImageFName);
	g_free(vd->AVIFName);
	/*free format allocations*/
	freeFormats(vd);
	/*unmap queue buffers*/	
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
	freeDevices(vd);
	/*close device descriptor*/
	close(vd->fd);
	/*free struct allocation*/
	g_free(vd);
	vd=NULL;
}

int
input_get_control (struct vdIn * device, InputControl * control, int * val)
{
	int fd;
	int ret=0;
	struct v4l2_control c;
	memset(&c,0,sizeof(struct v4l2_control));
	
	fd = device->fd;
 
	c.id  = control->id;
	ret = ioctl (fd, VIDIOC_G_CTRL, &c);
	if (ret == 0) *val = c.value;
	else perror("VIDIOC_G_CTRL - Unable to get control");
	
	return ret;
}

int
input_set_control (struct vdIn * device, InputControl * control, int val)
{
	int fd;
	int ret=0;
	struct v4l2_control c;

	fd = device->fd;

	c.id  = control->id;
	c.value = val;
	ret = ioctl (fd, VIDIOC_S_CTRL, &c);
	if (ret < 0) perror("VIDIOC_S_CTRL - Unable to set control");

	return ret;
}

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

int
input_get_framerate (struct vdIn * device)
{
	int fd;
	int ret=0;
	int fps=0;
	int fps_num=0;

	fd = device->fd;

	ret = ioctl(fd,VIDIOC_G_PARM,&device->streamparm);
	if (ret < 0) 
	{
		perror("VIDIOC_G_PARM - Unable to get timeperframe");
	} 
	else 
	{
		/*it seems numerator is allways 1*/
		fps = device->streamparm.parm.capture.timeperframe.denominator;
		fps_num = device->streamparm.parm.capture.timeperframe.numerator;
		device->fps=fps;
		device->fps_num=fps_num;
	}
	return ret;
}

InputControl *
input_enum_controls (struct vdIn * device, int *num_controls)
{
	int ret=0;
	int fd;
	InputControl * control = NULL;
	int n = 0;
	struct v4l2_queryctrl queryctrl;
	memset(&queryctrl,0,sizeof(struct v4l2_queryctrl));
	int i=0;
	fd = device->fd;

	initDynCtrls(device);
	i = V4L2_CID_BASE; /* as defined by V4L2 */
	while (i <= V4L2_CID_LAST_EXTCTR) 
	{ 
		queryctrl.id = i;
		if ((ret=ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) == 0 &&
			!(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)) 
		{
			control = g_renew(InputControl, control, n+1);
			control[n].i = n;
			control[n].id = queryctrl.id;
			control[n].type = queryctrl.type;
			control[n].name = strdup ((char *)queryctrl.name);
			control[n].min = queryctrl.minimum;
			control[n].max = queryctrl.maximum;
			control[n].step = queryctrl.step;
			control[n].default_val = queryctrl.default_value;
			control[n].enabled = (queryctrl.flags & V4L2_CTRL_FLAG_GRABBED) ? 0 : 1;
			control[n].entries = NULL;
			if (queryctrl.type == V4L2_CTRL_TYPE_BOOLEAN)
			{
				control[n].min = 0;
				control[n].max = 1;
				control[n].step = 1;
				/*get the first bit*/
				control[n].default_val=(queryctrl.default_value & 0x0001);
			} 
			else if (queryctrl.type == V4L2_CTRL_TYPE_MENU) 
			{
				struct v4l2_querymenu querymenu;
				memset(&querymenu,0,sizeof(struct v4l2_querymenu));
				control[n].min = 0;

				querymenu.id = queryctrl.id;
				querymenu.index = 0;
				while (ioctl (fd, VIDIOC_QUERYMENU, &querymenu) == 0) 
				{
					control[n].entries = g_renew(pchar, control[n].entries, querymenu.index+1);
					control[n].entries[querymenu.index] = g_strdup ((char *) querymenu.name);
					querymenu.index++;
				}
				control[n].max = querymenu.index - 1;
			}
			n++;
		} 
		else 
		{
			if (errno != EINVAL) g_printerr("Failed to query control id=%d: %s\n"
					, i, strerror(errno));
		}
		i++;
		if (i == V4L2_CID_LAST_NEW)  /* jump between CIDs*/
			i = V4L2_CID_CAMERA_CLASS_BASE_NEW;
		if (i == V4L2_CID_CAMERA_CLASS_LAST)
			i = V4L2_CID_PRIVATE_BASE_OLD;
		if (i == V4L2_CID_PRIVATE_LAST)
			i = V4L2_CID_BASE_EXTCTR;
	}
	*num_controls = n;
	return control;
}

void
input_free_controls (struct VidState *s)
{
	int i=0;
	for (i = 0; i < s->num_controls; i++) 
	{
		ControlInfo * ci = s->control_info + i;
		if (ci->widget)
			gtk_widget_destroy (ci->widget);
		if (ci->label)
			gtk_widget_destroy (ci->label);
		if (ci->spinbutton)
			gtk_widget_destroy (ci->spinbutton);
		g_free (s->control[i].name);
		if (s->control[i].type == INPUT_CONTROL_TYPE_MENU) 
		{
			int j;
			for (j = 0; j <= s->control[i].max; j++) 
			{
				g_free (s->control[i].entries[j]);
			}
			g_free (s->control[i].entries);
		}
	}
	g_free (s->control_info);
	s->control_info = NULL;
	g_free (s->control);
	s->control = NULL;
}
/******************************* enumerations *********************************/
/*list_form < 0   => format not supported */
int enum_frame_intervals(struct vdIn *vd, __u32 pixfmt, __u32 width, __u32 height, 
			int fmtind, int fsizeind)
{
	int ret=0;
	struct v4l2_frmivalenum fival;
	int list_fps=0;
	memset(&fival, 0, sizeof(fival));
	fival.index = 0;
	fival.pixel_format = pixfmt;
	fival.width = width;
	fival.height = height;
	vd->listVidFormats[fmtind-1].listVidCap[fsizeind-1].framerate_num=NULL;
	vd->listVidFormats[fmtind-1].listVidCap[fsizeind-1].framerate_denom=NULL;
	
	g_printf("\tTime interval between frame: ");
	while ((ret = ioctl(vd->fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0) 
	{
		fival.index++;
		if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE) 
		{
			g_printf("%u/%u, ", fival.discrete.numerator, fival.discrete.denominator);
				
			list_fps++;
			vd->listVidFormats[fmtind-1].listVidCap[fsizeind-1].framerate_num = g_renew(
				int, vd->listVidFormats[fmtind-1].listVidCap[fsizeind-1].framerate_num, list_fps);
			vd->listVidFormats[fmtind-1].listVidCap[fsizeind-1].framerate_denom = g_renew(
				int, vd->listVidFormats[fmtind-1].listVidCap[fsizeind-1].framerate_denom, list_fps);
			
			vd->listVidFormats[fmtind-1].listVidCap[fsizeind-1].framerate_num[list_fps-1] = fival.discrete.numerator;
			vd->listVidFormats[fmtind-1].listVidCap[fsizeind-1].framerate_denom[list_fps-1] = fival.discrete.denominator;
		} 
		else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) 
		{
			g_printf("{min { %u/%u } .. max { %u/%u } }, ",
				fival.stepwise.min.numerator, fival.stepwise.min.numerator,
				fival.stepwise.max.denominator, fival.stepwise.max.denominator);
			break;
		} 
		else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE) 
		{
			g_printf("{min { %u/%u } .. max { %u/%u } / "
				"stepsize { %u/%u } }, ",
				fival.stepwise.min.numerator, fival.stepwise.min.denominator,
				fival.stepwise.max.numerator, fival.stepwise.max.denominator,
				fival.stepwise.step.numerator, fival.stepwise.step.denominator);
			break;
		}
	}
	
	vd->listVidFormats[fmtind-1].listVidCap[fsizeind-1].numb_frates = list_fps;
	
	g_printf("\n");
	if (ret != 0 && errno != EINVAL) 
	{
		perror("VIDIOC_ENUM_FRAMEINTERVALS - Error enumerating frame intervals");
		return errno;
	}
	return 0;
}

int enum_frame_sizes(struct vdIn *vd, __u32 pixfmt, int fmtind)
{
	int ret=0;
	int fsizeind=0; /*index for supported sizes*/
	vd->listVidFormats[fmtind-1].listVidCap = NULL;
	struct v4l2_frmsizeenum fsize;

	memset(&fsize, 0, sizeof(fsize));
	fsize.index = 0;
	fsize.pixel_format = pixfmt;
	while ((ret = ioctl(vd->fd, VIDIOC_ENUM_FRAMESIZES, &fsize)) == 0) 
	{
		fsize.index++;
		if (fsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) 
		{
			g_printf("{ discrete: width = %u, height = %u }\n",
				fsize.discrete.width, fsize.discrete.height);
			
			fsizeind++;
			vd->listVidFormats[fmtind-1].listVidCap = g_renew(VidCap, 
				vd->listVidFormats[fmtind-1].listVidCap,
				fsizeind);
			
			vd->listVidFormats[fmtind-1].listVidCap[fsizeind-1].width = fsize.discrete.width;
			vd->listVidFormats[fmtind-1].listVidCap[fsizeind-1].height = fsize.discrete.height;
			
			ret = enum_frame_intervals(vd, pixfmt,
				fsize.discrete.width, fsize.discrete.height,
				fmtind,fsizeind);
			
			if (ret != 0) perror("  Unable to enumerate frame sizes");
		} 
		else if (fsize.type == V4L2_FRMSIZE_TYPE_CONTINUOUS) 
		{
			g_printf("{ continuous: min { width = %u, height = %u } .. "
				"max { width = %u, height = %u } }\n",
				fsize.stepwise.min_width, fsize.stepwise.min_height,
				fsize.stepwise.max_width, fsize.stepwise.max_height);
			g_printf("  will not enumerate frame intervals.\n");
		} 
		else if (fsize.type == V4L2_FRMSIZE_TYPE_STEPWISE) 
		{
			g_printf("{ stepwise: min { width = %u, height = %u } .. "
				"max { width = %u, height = %u } / "
				"stepsize { width = %u, height = %u } }\n",
				fsize.stepwise.min_width, fsize.stepwise.min_height,
				fsize.stepwise.max_width, fsize.stepwise.max_height,
				fsize.stepwise.step_width, fsize.stepwise.step_height);
			g_printf("  will not enumerate frame intervals.\n");
		} 
		else 
		{
			g_printerr("  fsize.type not supported: %d\n", fsize.type);
			g_printerr("     (Discrete: %d   Continuous: %d  Stepwise: %d)\n",
				V4L2_FRMSIZE_TYPE_DISCRETE,
				V4L2_FRMSIZE_TYPE_CONTINUOUS,
				V4L2_FRMSIZE_TYPE_STEPWISE);
		}
	}
	if (ret != 0 && errno != EINVAL) 
	{
		perror("VIDIOC_ENUM_FRAMESIZES - Error enumerating frame sizes");
		return errno;
	} 
	else if ((ret != 0) && (fsizeind == 0)) 
	{
		/* ------ gspca doesn't enumerate frame sizes ------ */
		/*       negotiate with VIDIOC_TRY_FMT instead       */
		
		fsizeind++;
		vd->fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		vd->fmt.fmt.pix.width = vd->width;
		vd->fmt.fmt.pix.height = vd->height;
		vd->fmt.fmt.pix.pixelformat = pixfmt;
		vd->fmt.fmt.pix.field = V4L2_FIELD_ANY;
		ret = ioctl(vd->fd, VIDIOC_TRY_FMT, &vd->fmt);
		/*use the returned values*/
		vd->width = vd->fmt.fmt.pix.width;
		vd->height = vd->fmt.fmt.pix.height;
		g_printf("{ ?GSPCA? : width = %u, height = %u }\n", vd->width, vd->height);
		
		if(vd->listVidFormats[fmtind-1].listVidCap == NULL) 
		{
			vd->listVidFormats[fmtind-1].listVidCap = g_renew( VidCap,
				vd->listVidFormats[fmtind-1].listVidCap,
				fsizeind);
			vd->listVidFormats[fmtind-1].listVidCap[0].framerate_num = g_renew( int,
				vd->listVidFormats[fmtind-1].listVidCap[0].framerate_num,
				1);
			vd->listVidFormats[fmtind-1].listVidCap[0].framerate_denom = g_renew( int,
				vd->listVidFormats[fmtind-1].listVidCap[0].framerate_denom,
				1);
		} 
		else
		{
			g_printerr("assert failed: listVidCap not Null\n");
			return (-2);
		}
		vd->listVidFormats[fmtind-1].listVidCap[0].width = vd->width;
		vd->listVidFormats[fmtind-1].listVidCap[0].height = vd->height;
		vd->listVidFormats[fmtind-1].listVidCap[0].framerate_num[0] = vd->fps_num;
		vd->listVidFormats[fmtind-1].listVidCap[0].framerate_denom[0] = vd->fps;
	}
	
	vd->listVidFormats[fmtind-1].numb_res=fsizeind;
	return 0;
}

int enum_frame_formats(struct vdIn *vd)
{
	int ret=0;
	int fmtind=0;
	struct v4l2_fmtdesc fmt;
	vd->listVidFormats = NULL;
	memset(&fmt, 0, sizeof(fmt));
	fmt.index = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	while ((ret = ioctl(vd->fd, VIDIOC_ENUM_FMT, &fmt)) == 0) 
	{
		fmt.index++;
		g_printf("{ pixelformat = '%c%c%c%c', description = '%s' }\n",
				fmt.pixelformat & 0xFF, (fmt.pixelformat >> 8) & 0xFF,
				(fmt.pixelformat >> 16) & 0xFF, (fmt.pixelformat >> 24) & 0xFF,
				fmt.description);
		/*check if format is supported by guvcview and allocate it*/
		if((ret=check_PixFormat(fmt.pixelformat)) >= 0) 
		{
			fmtind++;
			listSupFormats[ret].hardware = fmtind; /*supported by hardware*/
			vd->listVidFormats = g_renew(VidFormats, vd->listVidFormats, fmtind);
			vd->listVidFormats[fmtind-1].format=fmt.pixelformat;
			g_snprintf(vd->listVidFormats[fmtind-1].fourcc,5,"%c%c%c%c",
				fmt.pixelformat & 0xFF, (fmt.pixelformat >> 8) & 0xFF,
				(fmt.pixelformat >> 16) & 0xFF, (fmt.pixelformat >> 24) & 0xFF);
			ret = enum_frame_sizes(vd, fmt.pixelformat, fmtind);
			if (ret != 0)
				perror("  Unable to enumerate frame sizes.\n");
		}
		else
		{
			g_printerr("   { not supported - request format support at http://guvcview.berlios.de }\n");
		}
	}
	if (errno != EINVAL) {
		perror("VIDIOC_ENUM_FMT - Error enumerating frame formats");
		return errno;
	}
	vd->numb_formats=fmtind;
	return 0;
}

int get_FormatIndex(struct vdIn *vd, int format)
{
	int i=0;
	for(i=0;i<vd->numb_formats;i++)
	{
		if(format == vd->listVidFormats[i].format) 
			return (i);
	}
	return (-1);
}

int initDynCtrls(struct vdIn *vd) 
{
	int i=0;
	int err=0;
	/* try to add all controls listed above */
	for ( i=0; i<LENGTH_OF_XU_CTR; i++ ) 
	{
		g_printf("Adding control for %s\n", xu_mappings[i].name);
		if ((err=ioctl(vd->fd, UVCIOC_CTRL_ADD, &xu_ctrls[i])) < 0 ) 
		{
			if (errno != EEXIST) perror("UVCIOC_CTRL_ADD - Error");
			//else perror("Control exists");
		}
	}
	/* after adding the controls, add the mapping now */
	for ( i=0; i<LENGTH_OF_XU_MAP; i++ ) 
	{
		g_printf("mapping control for %s\n", xu_mappings[i].name);
		if ((err=ioctl(vd->fd, UVCIOC_CTRL_MAP, &xu_mappings[i])) < 0) 
		{
			if (errno!=EEXIST) perror("UVCIOC_CTRL_MAP - Error");
			//else perror("Mapping exists");
		}
	} 
	return 0;
}

/*
SRC: https://lists.berlios.de/pipermail/linux-uvc-devel/2007-July/001888.html
- dev: the device file descriptor
- pan: pan angle in 1/64th of degree
- tilt: tilt angle in 1/64th of degree
- reset: set 1 to reset Pan, set 2 to reset tilt, set to 3 to reset pan/tilt to the device origin, set to 0 otherwise
*/
int uvcPanTilt(struct vdIn *vd, int pan, int tilt, int reset) 
{
	struct v4l2_ext_control xctrls[2];
	struct v4l2_ext_controls ctrls;
	
	if (reset) 
	{
		switch (reset) 
		{
			case 1:
				xctrls[0].id = V4L2_CID_PAN_RESET_NEW;
				xctrls[0].value = 1;
				break;
			
			case 2:
				xctrls[0].id = V4L2_CID_TILT_RESET_NEW;
				xctrls[0].value = 1;
				break;
			
			case 3:
				xctrls[0].value = 3;
				xctrls[0].id = V4L2_CID_PANTILT_RESET_LOGITECH;
				break;
		}
		ctrls.count = 1;
		ctrls.controls = xctrls;
	} 
	else 
	{
		xctrls[0].id = V4L2_CID_PAN_RELATIVE_NEW;
		xctrls[0].value = pan;
		xctrls[1].id = V4L2_CID_TILT_RELATIVE_NEW;
		xctrls[1].value = tilt;
	
		ctrls.count = 2;
		ctrls.controls = xctrls;
	}
	
	if ( ioctl(vd->fd, VIDIOC_S_EXT_CTRLS, &ctrls) < 0 ) 
	{
		perror("VIDIOC_S_EXT_CTRLS - Pan/Tilt error\n");
			return -1;
	}
	
	return 0;
}

/*--------------------------- focus control ----------------------------------*/
int 
get_focus (struct vdIn *videoIn)
{
	int ret=0;
	struct v4l2_control c;
	int val=0;

	c.id  = V4L2_CID_FOCUS_LOGITECH;
	ret = ioctl (videoIn->fd, VIDIOC_G_CTRL, &c);
	if (ret < 0) 
	{
		perror("VIDIOC_G_CTRL - get focus error");
		val = -1;
	}
	else val = c.value;
	
	return val;

}

int 
set_focus (struct vdIn *videoIn, int val) 
{
	int ret=0;
	struct v4l2_control c;

	c.id  = V4L2_CID_FOCUS_LOGITECH;
	c.value = val;
	ret = ioctl (videoIn->fd, VIDIOC_S_CTRL, &c);
	if (ret < 0) perror("VIDIOC_S_CTRL - set focus error");

	return ret;
}
