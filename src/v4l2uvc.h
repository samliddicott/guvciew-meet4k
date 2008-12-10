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

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <linux/videodev2.h>
#include <gtk/gtk.h>

#define NB_BUFFER 4

/*timecode flag for unsetting UVC_QUEUE_DROP_INCOMPLETE*/
/*must patch UVC driver */
#define V4L2_TC_FLAG_NO_DROP		0x1000
/*
 * Private V4L2 control identifiers from UVC driver.  - this seems to change acording to driver version
 * all other User-class control IDs are defined by V4L2 (videodev.h)
 */
 
/*Debian Patch*/
#ifndef V4L2_PIX_FMT_SGBRG8
#define V4L2_PIX_FMT_SGBRG8  v4l2_fourcc('G', 'B', 'R', 'G') /*  8  GBGB.. RGRG.. */
#endif

#ifndef V4L2_PIX_FMT_JPEG
#define V4L2_PIX_FMT_JPEG  v4l2_fourcc('J', 'P', 'E', 'G') /*  JPEG stream */
#endif

/*------------------------- new camera class controls ---------------------*/
#define V4L2_CTRL_CLASS_USER_NEW		0x00980000
#define V4L2_CID_BASE_NEW			(V4L2_CTRL_CLASS_USER_NEW | 0x900)
#define V4L2_CID_POWER_LINE_FREQUENCY_NEW	(V4L2_CID_BASE_NEW+24)
#define V4L2_CID_HUE_AUTO_NEW			(V4L2_CID_BASE_NEW+25) 
#define V4L2_CID_WHITE_BALANCE_TEMPERATURE_NEW	(V4L2_CID_BASE_NEW+26) 
#define V4L2_CID_SHARPNESS_NEW			(V4L2_CID_BASE_NEW+27) 
#define V4L2_CID_BACKLIGHT_COMPENSATION_NEW 	(V4L2_CID_BASE_NEW+28)
#define V4L2_CID_LAST_NEW			(V4L2_CID_BASE_NEW+29)

#define V4L2_CTRL_CLASS_CAMERA_NEW 0x009A0000	/* Camera class controls */
#define V4L2_CID_CAMERA_CLASS_BASE_NEW 		(V4L2_CTRL_CLASS_CAMERA_NEW | 0x900)

#define V4L2_CID_EXPOSURE_AUTO_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+1)
#define V4L2_CID_EXPOSURE_ABSOLUTE_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+2)
#define V4L2_CID_EXPOSURE_AUTO_PRIORITY_NEW	(V4L2_CID_CAMERA_CLASS_BASE_NEW+3)

#define V4L2_CID_PAN_RELATIVE_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+4)
#define V4L2_CID_TILT_RELATIVE_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+5)
#define V4L2_CID_PAN_RESET_NEW			(V4L2_CID_CAMERA_CLASS_BASE_NEW+6)
#define V4L2_CID_TILT_RESET_NEW			(V4L2_CID_CAMERA_CLASS_BASE_NEW+7)

#define V4L2_CID_PAN_ABSOLUTE_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+8)
#define V4L2_CID_TILT_ABSOLUTE_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+9)

#define V4L2_CID_FOCUS_ABSOLUTE_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+10)
#define V4L2_CID_FOCUS_RELATIVE_NEW		(V4L2_CID_CAMERA_CLASS_BASE_NEW+11)
#define V4L2_CID_FOCUS_AUTO_NEW			(V4L2_CID_CAMERA_CLASS_BASE_NEW+12)
#define V4L2_CID_CAMERA_CLASS_LAST		(V4L2_CID_CAMERA_CLASS_BASE_NEW+13)

/*--------------- old private class controls ------------------------------*/

#define V4L2_CID_PRIVATE_BASE_OLD		0x08000000
#define V4L2_CID_BACKLIGHT_COMPENSATION_OLD	(V4L2_CID_PRIVATE_BASE_OLD+0)
#define V4L2_CID_POWER_LINE_FREQUENCY_OLD	(V4L2_CID_PRIVATE_BASE_OLD+1)
#define V4L2_CID_SHARPNESS_OLD			(V4L2_CID_PRIVATE_BASE_OLD+2)
#define V4L2_CID_HUE_AUTO_OLD			(V4L2_CID_PRIVATE_BASE_OLD+3)

#define V4L2_CID_FOCUS_AUTO_OLD			(V4L2_CID_PRIVATE_BASE_OLD+4)
#define V4L2_CID_FOCUS_ABSOLUTE_OLD		(V4L2_CID_PRIVATE_BASE_OLD+5)
#define V4L2_CID_FOCUS_RELATIVE_OLD		(V4L2_CID_PRIVATE_BASE_OLD+6)

#define V4L2_CID_PAN_RELATIVE_OLD		(V4L2_CID_PRIVATE_BASE_OLD+7)
#define V4L2_CID_TILT_RELATIVE_OLD		(V4L2_CID_PRIVATE_BASE_OLD+8)
#define V4L2_CID_PANTILT_RESET_OLD		(V4L2_CID_PRIVATE_BASE_OLD+9)

#define V4L2_CID_EXPOSURE_AUTO_OLD		(V4L2_CID_PRIVATE_BASE_OLD+10)
#define V4L2_CID_EXPOSURE_ABSOLUTE_OLD		(V4L2_CID_PRIVATE_BASE_OLD+11)

#define V4L2_CID_WHITE_BALANCE_TEMPERATURE_AUTO_OLD	(V4L2_CID_PRIVATE_BASE_OLD+12)
#define V4L2_CID_WHITE_BALANCE_TEMPERATURE_OLD		(V4L2_CID_PRIVATE_BASE_OLD+13)

#define V4L2_CID_PRIVATE_LAST			(V4L2_CID_WHITE_BALANCE_TEMPERATURE_OLD+1)



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

	                      
#define UVC_DYN_CONTROLS
/*
 * Dynamic controls
 */

#define UVC_CTRL_DATA_TYPE_RAW		0
#define UVC_CTRL_DATA_TYPE_SIGNED	1
#define UVC_CTRL_DATA_TYPE_UNSIGNED	2
#define UVC_CTRL_DATA_TYPE_BOOLEAN	3
#define UVC_CTRL_DATA_TYPE_ENUM		4
#define UVC_CTRL_DATA_TYPE_BITMASK	5


#ifdef UVC_DYN_CONTROLS

#define V4L2_CID_BASE_EXTCTR				0x0A046D01
#define V4L2_CID_BASE_LOGITECH				V4L2_CID_BASE_EXTCTR
//#define V4L2_CID_PAN_RELATIVE_LOGITECH  		V4L2_CID_BASE_LOGITECH
//#define V4L2_CID_TILT_RELATIVE_LOGITECH 		V4L2_CID_BASE_LOGITECH+1
#define V4L2_CID_PANTILT_RESET_LOGITECH 		V4L2_CID_BASE_LOGITECH+2
#define V4L2_CID_FOCUS_LOGITECH         		V4L2_CID_BASE_LOGITECH+3
#define V4L2_CID_LED1_MODE_LOGITECH     		V4L2_CID_BASE_LOGITECH+4
#define V4L2_CID_LED1_FREQUENCY_LOGITECH 		V4L2_CID_BASE_LOGITECH+5
#define V4L2_CID_DISABLE_PROCESSING_LOGITECH 		V4L2_CID_BASE_LOGITECH+0x70
#define V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH 		V4L2_CID_BASE_LOGITECH+0x71
#define V4L2_CID_LAST_EXTCTR				V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH

#define UVC_GUID_LOGITECH_VIDEO_PIPE		{0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x50}
#define UVC_GUID_LOGITECH_MOTOR_CONTROL 	{0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x56}
#define UVC_GUID_LOGITECH_USER_HW_CONTROL 	{0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x1f}

#define XU_HW_CONTROL_LED1			1
#define XU_MOTORCONTROL_PANTILT_RELATIVE	1
#define XU_MOTORCONTROL_PANTILT_RESET		2
#define XU_MOTORCONTROL_FOCUS			3
#define XU_COLOR_PROCESSING_DISABLE		5
#define XU_RAW_DATA_BITS_PER_PIXEL		8

#define UVC_CONTROL_SET_CUR	(1 << 0)
#define UVC_CONTROL_GET_CUR	(1 << 1)
#define UVC_CONTROL_GET_MIN	(1 << 2)
#define UVC_CONTROL_GET_MAX	(1 << 3)
#define UVC_CONTROL_GET_RES	(1 << 4)
#define UVC_CONTROL_GET_DEF	(1 << 5)
/* Control should be saved at suspend and restored at resume. */
#define UVC_CONTROL_RESTORE	(1 << 6)
/* Control can be updated by the camera. */
#define UVC_CONTROL_AUTO_UPDATE	(1 << 7)

#define UVC_CONTROL_GET_RANGE   (UVC_CONTROL_GET_CUR | UVC_CONTROL_GET_MIN | \
                                 UVC_CONTROL_GET_MAX | UVC_CONTROL_GET_RES | \
                                 UVC_CONTROL_GET_DEF)


struct uvc_xu_control_info 
{
	__u8 entity[16];
	__u8 index;
	__u8 selector;
	__u16 size;
	__u32 flags;
};

struct uvc_xu_control_mapping 
{
	__u32 id;
	__u8 name[32];
	__u8 entity[16];
	__u8 selector;
  
	__u8 size;
	__u8 offset;
	enum v4l2_ctrl_type v4l2_type;
	__u32 data_type;
};

struct uvc_xu_control 
{
	__u8 unit;
	__u8 selector;
	__u16 size;
	//__u8 __user *data;
	__u8 *data;
};

#define UVCIOC_CTRL_ADD		_IOW  ('U', 1, struct uvc_xu_control_info)
#define UVCIOC_CTRL_MAP		_IOWR ('U', 2, struct uvc_xu_control_mapping)
#define UVCIOC_CTRL_GET		_IOWR ('U', 3, struct uvc_xu_control)
#define UVCIOC_CTRL_SET		_IOW  ('U', 4, struct uvc_xu_control)

#endif  
  
#define MAX_LIST_FPS (10)
#define MAX_LIST_VIDCAP (20)

typedef struct _VidCap 
{
	int width;
	int height;
	int framerate_num[MAX_LIST_FPS];/*numerator - should be 1 in almost all cases*/
	int framerate_denom[MAX_LIST_FPS];/*denominator - gives fps*/
	int numb_frates;
} VidCap;

struct vdIn 
{
	int fd;
	char *videodevice;
	char *status;
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
	int grabmethod;
	int width;
	int height;
	int numb_resol;
	int SupYuv;
	int SupUyv;
	int SupYup;
	int SupGbr;
	int SupMjpg;
	int SupJpeg;
	int formatIn;
	int formatOut;
	int framesizeIn;
	int signalquit;
	int capAVI;
	char *AVIFName;
	int fps;
	int fps_num;
	int capImage;
	char *ImageFName;
	int cap_raw;
	int available_exp[4];
	/* supported formats: 0-MJPG | 1-YUYV, UYVY, YUV420, SGBRG8*/
	/* 20 settings for each format              */
	VidCap listVidCap[2][MAX_LIST_VIDCAP];
};


typedef enum 
{
	INPUT_CONTROL_TYPE_INTEGER = 1,
	INPUT_CONTROL_TYPE_BOOLEAN = 2,
	INPUT_CONTROL_TYPE_MENU = 3,
	INPUT_CONTROL_TYPE_BUTTON = 4,
} InputControlType;

typedef struct _InputControl 
{
	unsigned int i;
	unsigned int id;
	InputControlType type;
	char * name;
	int min, max, step, default_val, enabled;
	char ** entries;
} InputControl;

typedef struct _ControlInfo 
{
	GtkWidget * widget;
	GtkWidget * label;
	//GtkWidget * labelval;
	GtkWidget *spinbutton; /*used in integer (slider) controls*/
	unsigned int idx;
	int maxchars;
} ControlInfo;

struct VidState 
{
	GtkWidget * table;
	//GtkWidget * device_combo;
	//GtkWidget * device_label;
	//GtkWidget * input_combo;
	//GtkWidget * input_label;

	int width_req;
	int height_req;

	InputControl * control;
	int num_controls;
	ControlInfo * control_info;
};

int check_videoIn(struct vdIn *vd);

int 
init_videoIn(struct vdIn *vd, char *device, int width, int height,
	int format, int grabmethod, int fps, int fps_num);

int uvcGrab(struct vdIn *vd);

void close_v4l2(struct vdIn *vd);

int input_get_control (struct vdIn * device, InputControl * control, int * val);

int input_set_control (struct vdIn * device, InputControl * control, int val);

int input_set_framerate (struct vdIn * device);

int input_get_framerate (struct vdIn * device);

InputControl * input_enum_controls (struct vdIn * device, int *num_controls);

void input_free_controls (struct VidState *s);

int init_v4l2(struct vdIn *vd);

int enum_frame_intervals(struct vdIn *vd, __u32 pixfmt, __u32 width, __u32 height,
	int list_form, int list_ind);

int enum_frame_sizes(struct vdIn *vd, __u32 pixfmt);

int enum_frame_formats(struct vdIn *vd);

int initDynCtrls(struct vdIn *vd);

int uvcPanTilt(struct vdIn *vd, int pan, int tilt, int reset);

int get_focus (struct vdIn *videoIn);

int set_focus (struct vdIn *videoIn, int val);

#endif

