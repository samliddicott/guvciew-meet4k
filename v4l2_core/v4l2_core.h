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

#ifndef V4L2_CORE_H
#define V4L2_CORE_H

#include <linux/videodev2.h>
#include <linux/uvcvideo.h>
#include <linux/media.h>
#include <libudev.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/types.h>

#include "defs.h"

/*
 * buffer number (for driver mmap ops)
 */
#define NB_BUFFER 8

/*jpeg header def*/
#define HEADERFRAME1 0xaf

/*
 * set ioctl retries to 4
 */
#define IOCTL_RETRY 4

/* A.8. Video Class-Specific Request Codes */
#define UVC_RC_UNDEFINED                                0x00
#define UVC_SET_CUR                                     0x01
#define UVC_GET_CUR                                     0x81
#define UVC_GET_MIN                                     0x82
#define UVC_GET_MAX                                     0x83
#define UVC_GET_RES                                     0x84
#define UVC_GET_LEN                                     0x85
#define UVC_GET_INFO                                    0x86
#define UVC_GET_DEF                                     0x87

/*
 * h264 probe commit struct (uvc 1.1)
 */
typedef struct _uvcx_video_config_probe_commit_t
{
	uint32_t  dwFrameInterval;
	uint32_t  dwBitRate;
	uint16_t  bmHints;
	uint16_t  wConfigurationIndex;
	uint16_t  wWidth;
	uint16_t  wHeight;
	uint16_t  wSliceUnits;
	uint16_t  wSliceMode;
	uint16_t  wProfile;
	uint16_t  wIFramePeriod;
	uint16_t  wEstimatedVideoDelay;
	uint16_t  wEstimatedMaxConfigDelay;
	uint8_t   bUsageType;
	uint8_t   bRateControlMode;
	uint8_t   bTemporalScaleMode;
	uint8_t   bSpatialScaleMode;
	uint8_t   bSNRScaleMode;
	uint8_t   bStreamMuxOption;
	uint8_t   bStreamFormat;
	uint8_t   bEntropyCABAC;
	uint8_t   bTimestamp;
	uint8_t   bNumOfReorderFrames;
	uint8_t   bPreviewFlipped;
	uint8_t   bView;
	uint8_t   bReserved1;
	uint8_t   bReserved2;
	uint8_t   bStreamID;
	uint8_t   bSpatialLayerRatio;
	uint16_t  wLeakyBucketSize;
} __attribute__((__packed__)) uvcx_video_config_probe_commit_t;


/*
 * v4l2 stream capability data
 */
typedef struct _v4l2_stream_cap
{
	int width;            //width
	int height;           //height
	int *framerate_num;   //list of numerator values - should be 1 in almost all cases
	int *framerate_denom; //list of denominator values - gives fps
	int numb_frates;      //number of frame rates (numerator and denominator lists size)
} v4l2_stream_cap;

/*
 * v4l2 stream format data
 */
typedef struct _v4l2_stream_format
{
	int format;          //v4l2 pixel format
	char fourcc[5];      //corresponding fourcc (mode)
	int numb_res;        //available number of resolutions for format (v4l2_stream_cap list size)
	v4l2_stream_cap *list_stream_cap;  //list of stream capabilities for format
} v4l2_stream_formats;

/*
 * v4l2 control data
 */
typedef struct _v4l2_ctrl
{
    struct v4l2_queryctrl control;
    struct v4l2_querymenu *menu;
    int32_t class;
    int32_t value; //also used for string max size
    int64_t value64;
    char *string;

    //next control in the list
    struct _v4l2_ctrl *next;
} v4l2_ctrl;

/*
 * v4l2 device system data
 */
typedef struct _v4l2_dev_sys_data
{
	char *device;
	char *name;
	char *driver;
	char *location;
	uint32_t vendor;
	uint32_t product;
	int valid;
	int current;
	uint64_t busnum;
	uint64_t devnum;
} v4l2_dev_sys_data;

/*
 * video device data
 */
typedef struct _v4l2_dev
{
	int fd;                             // device file descriptor
	char *videodevice;                  // video device string (default "/dev/video0)"

	int cap_meth;                       // capture method: IO_READ or IO_MMAP
	v4l2_stream_formats* list_stream_formats; //list of available stream formats
	int numb_formats;                   //list size
	int current_format;                 //index of current stream format

	struct v4l2_capability cap;         // v4l2 capability struct
	struct v4l2_format format;          // v4l2 format struct
	struct v4l2_buffer buf;             // v4l2 buffer struct
	struct v4l2_requestbuffers rb;      // v4l2 request buffers struct
	struct v4l2_streamparm streamparm;  // v4l2 stream parameters struct

	int fps_num;                        //fps numerator
	int fps_denom;                      //fps denominator

	uint8_t streaming;                  // flag if device is streaming (1) or not (0)
	uint64_t timestamp;                 // captured frame timestamp
	void *mem[NB_BUFFER];               // memory buffers for mmap driver frames
	uint32_t buff_length[NB_BUFFER];    // memory buffers length as set by VIDIOC_QUERYBUF
	uint32_t buff_offset[NB_BUFFER];    // memory buffers offset as set by VIDIOC_QUERYBUF
	uint8_t *tmpbuffer;                 // temp buffer for decoding compressed data
	uint8_t *framebuffer;               // frame buffer (YUYV), for rendering in SDL overlay

	uint8_t *h264_frame;                // current uvc h264 frame retrieved from video stream
	uint8_t h264_unit_id;  				// uvc h264 unit id, if <= 0 then uvc h264 is not supported
	uvcx_video_config_probe_commit_t h264_config_probe_req; //probe commit struct for h264 streams
	//struct h264_decoder_context* h264_ctx; //h264 decoder context
	uint8_t *h264_last_IDR;             // last IDR frame retrieved from uvc h264 stream
	int h264_last_IDR_size;             // last IDR frame size
	uint8_t *h264_SPS;                  // h264 SPS info
	uint16_t h264_SPS_size;             // SPS size
	uint8_t *h264_PPS;                  // h264 PPS info
	uint16_t h264_PPS_size;             // PPS size
	uint8_t isKeyframe;                 // current buffer contains a keyframe (h264 IDR)

	struct udev *udev;                  // pointer to a udev struct (lib udev)
    struct udev_monitor *udev_mon;      // udev monitor
    int udev_fd;                        // udev monitor file descriptor
    v4l2_dev_sys_data* list_devices;    // list of available v4l2 devices
    int num_devices;                    // number of available v4l2 devices
    int this_device;                    // index of this device in device list

    v4l2_ctrl* list_device_controls;    //null terminated linked list of available device controls
    int num_controls;                   //number of controls in list
    //v4l2_device *list_video_devices;
} v4l2_dev;

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
int xioctl(int fd, int IOCTL_X, void *arg);

/*
 * set verbosity level
 * args:
 *   level - verbosity level (def = 0)
 *
 * asserts:
 *   none
 *
 * returns - void
 */
void set_v4l2_verbosity(int level);

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
v4l2_dev* init_v4l2_dev(const char *device);


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
int try_video_stream(v4l2_dev* vd);

/*
 * gets the next video frame and decodes it if necessary
 * args:
 * vd: pointer to video device data
 *
 * returns: error code (E_OK)
 */
//int get_v4l2_frame(v4l2_dev* vd);

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
void close_v4l2_dev(v4l2_dev* vd);

/*
 * request v4l2 device with new format
 * args:
 *   vd: pointer to video device data
 *
 * returns: error code ( 0 - VDIN_OK)
 */
//int reset_v4l2_dev(v4l2_dev* vd, int format, int width, int height);

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
int set_v4l2_framerate (v4l2_dev* vd);

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
int get_v4l2_framerate (v4l2_dev* vd);

/*
 * Starts the video stream
 * args:
 *   vd: pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: VIDIOC_STREAMON ioctl result (0- E_OK)
 */
int start_video_stream(v4l2_dev* vd);

/*
 * Stops the video stream
 * args:
 *   vd: pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: VIDIOC_STREAMOFF ioctl result (0- E_OK)
 */
int stop_video_stream(v4l2_dev* vd);

/*
 *  ######### CONTROLS ##########
 */

/*
 * return the control associated to id from device list
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *   vd->list_device_controls is not null
 *
 * returns: pointer to v4l2_control if succeded or null otherwise
 */
v4l2_ctrl* get_v4l2_control_by_id(v4l2_dev* vd, int id);

/*
 * sets the value of control id in device
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *   vd->fd is valid
 *
 * returns: ioctl result
 */
int set_v4l2_control_id_value(v4l2_dev* vd, int id);

/*
 * updates the value for control id from the device
 * also updates control flags
 * args:
 *   vd - pointer to video device data
 *   vd->fd is valid
 *
 * asserts:
 *   vd is not null
 *
 * returns: ioctl result
 */
int get_v4l2_control_id_value (v4l2_dev* vd, int id);

/*
 * goes trough the control list and sets values in device to default
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *   vd->list_device_controls is not null
 *
 * returns: void
 */
void set_v4l2_control_defaults(v4l2_dev* vd);

/*
 *  ######### XU CONTROLS ##########
 */

/*
 * get lenght of xu control defined by unit id and selector
 * args:
 *   vd - pointer to video device data
 *   unit - unit id of xu control
 *   selector - selector for control
 *
 * asserts:
 *   vd is not null
 *   vd->fd is valid ( > 0 )
 *
 * returns: length of xu control
 */
uint16_t get_length_xu_control(v4l2_dev* vd, uint8_t unit, uint8_t selector);

/*
 * get uvc info for xu control defined by unit id and selector
 * args:
 *   vd - pointer to video device data
 *   unit - unit id of xu control
 *   selector - selector for control
 *
 * asserts:
 *   vd is not null
 *   vd->fd is valid ( > 0 )
 *
 * returns: info of xu control
 */
uint8_t get_info_xu_control(v4l2_dev* vd, uint8_t unit, uint8_t selector);

/*
 * runs a query on xu control defined by unit id and selector
 * args:
 *   vd - pointer to video device data
 *   unit - unit id of xu control
 *   selector - selector for control
 *   query - query type
 *   data - pointer to query data
 *
 * asserts:
 *   vd is not null
 *   vd->fd is valid ( > 0 )
 *
 * returns: 0 if query succeded or errno otherwise
 */
int query_xu_control(v4l2_dev* vd, uint8_t unit, uint8_t selector, uint8_t query, void *data);

#endif

