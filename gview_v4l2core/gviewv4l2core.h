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

#ifndef GVIEWV4L2CORE_H
#define GVIEWV4L2CORE_H

#include <features.h>

#include <linux/videodev2.h>
#include <linux/uvcvideo.h>
#include <linux/media.h>
#include <libudev.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/types.h>

/*make sure we support c++*/
__BEGIN_DECLS

/*
 * LOGITECH Dynamic controls defs
 */

#define V4L2_CID_BASE_EXTCTR					0x0A046D01
#define V4L2_CID_BASE_LOGITECH					V4L2_CID_BASE_EXTCTR
//#define V4L2_CID_PAN_RELATIVE_LOGITECH		V4L2_CID_BASE_LOGITECH
//#define V4L2_CID_TILT_RELATIVE_LOGITECH		V4L2_CID_BASE_LOGITECH+1
#define V4L2_CID_PANTILT_RESET_LOGITECH			V4L2_CID_BASE_LOGITECH+2

/*this should realy be replaced by V4L2_CID_FOCUS_ABSOLUTE in libwebcam*/
#define V4L2_CID_FOCUS_LOGITECH					V4L2_CID_BASE_LOGITECH+3
#define V4L2_CID_LED1_MODE_LOGITECH				V4L2_CID_BASE_LOGITECH+4
#define V4L2_CID_LED1_FREQUENCY_LOGITECH		V4L2_CID_BASE_LOGITECH+5
#define V4L2_CID_DISABLE_PROCESSING_LOGITECH	V4L2_CID_BASE_LOGITECH+0x70
#define V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH	V4L2_CID_BASE_LOGITECH+0x71
#define V4L2_CID_LAST_EXTCTR					V4L2_CID_RAW_BITS_PER_PIXEL_LOGITECH

/*
 * Error Codes
 */
#define E_OK					  (0)
#define E_ALLOC_ERR				  (-1)
#define E_QUERYCAP_ERR  		  (-2)
#define E_READ_ERR     		 	  (-3)
#define E_MMAP_ERR      		  (-4)
#define E_QUERYBUF_ERR   		  (-5)
#define E_QBUF_ERR       		  (-6)
#define E_DQBUF_ERR				  (-7)
#define E_STREAMON_ERR   		  (-8)
#define E_STREAMOFF_ERR  		  (-9)
#define E_FORMAT_ERR    		  (-10)
#define E_REQBUFS_ERR    		  (-11)
#define E_DEVICE_ERR     		  (-12)
#define E_SELECT_ERR     		  (-13)
#define E_SELECT_TIMEOUT_ERR	  (-14)
#define E_FBALLOC_ERR			  (-15)
#define E_NO_STREAM_ERR           (-16)
#define E_NO_DATA                 (-17)
#define E_NO_CODEC                (-18)
#define E_DECODE_ERR              (-19)
#define E_BAD_TABLES_ERR          (-20)
#define E_NO_SOI_ERR              (-21)
#define E_NOT_8BIT_ERR            (-22)
#define E_BAD_WIDTH_OR_HEIGHT_ERR (-23)
#define E_TOO_MANY_COMPPS_ERR     (-24)
#define E_ILLEGAL_HV_ERR          (-25)
#define E_QUANT_TBL_SEL_ERR       (-26)
#define E_NOT_YCBCR_ERR           (-27)
#define E_UNKNOWN_CID_ERR         (-28)
#define E_WRONG_MARKER_ERR        (-29)
#define E_NO_EOI_ERR              (-30)
#define E_FILE_IO_ERR             (-31)
#define E_UNKNOWN_ERR    		  (-40)

/*
 * stream status codes
 */
#define STRM_STOP        (0)
#define STRM_REQ_STOP    (1)
#define STRM_OK          (2)

/*
 * IO methods
 */
#define IO_MMAP 1
#define IO_READ 2

/*
 * software autofocus sort method
 * quick sort
 * shell sort
 * insert sort
 * bubble sort
 */
#define AUTOF_SORT_QUICK  1
#define AUTOF_SORT_SHELL  2
#define AUTOF_SORT_INSERT 3
#define AUTOF_SORT_BUBBLE 4

/*
 * Image Formats
 */
#define IMG_FMT_RAW     (0)
#define IMG_FMT_JPG     (1)
#define IMG_FMT_PNG     (2)
#define IMG_FMT_BMP     (3)


/*
 * buffer number (for driver mmap ops)
 */
#define NB_BUFFER 4

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
typedef struct _v4l2_stream_cap_t
{
	int width;            //width
	int height;           //height
	int *framerate_num;   //list of numerator values - should be 1 in almost all cases
	int *framerate_denom; //list of denominator values - gives fps
	int numb_frates;      //number of frame rates (numerator and denominator lists size)
} v4l2_stream_cap_t;

/*
 * v4l2 stream format data
 */
typedef struct _v4l2_stream_format_t
{
	uint8_t dec_support; //decoder support (1-supported; 0-not supported)
	int format;          //v4l2 pixel format
	char fourcc[5];      //corresponding fourcc (mode)
	int numb_res;        //available number of resolutions for format (v4l2_stream_cap_t list size)
	v4l2_stream_cap_t *list_stream_cap;  //list of stream capabilities for format
} v4l2_stream_formats_t;

/*
 * v4l2 control data
 */
typedef struct _v4l2_ctrl_t
{
    struct v4l2_queryctrl control;
    struct v4l2_querymenu *menu;
    int32_t class;
    int32_t value; //also used for string max size
    int64_t value64;
    char *string;

    /*localization*/
    char *name; /*gettext translated name*/
    int menu_entries;
    char **menu_entry; /*gettext translated menu entry name*/

    //next control in the list
    struct _v4l2_ctrl_t *next;
} v4l2_ctrl_t;

/*
 * v4l2 device system data
 */
typedef struct _v4l2_dev_sys_data_t
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
} v4l2_dev_sys_data_t;

/*
 * v4l2 devices list data
 */
typedef struct _v4l2_device_list_t
{
	struct udev *udev;                  // pointer to a udev struct (lib udev)
    struct udev_monitor *udev_mon;      // udev monitor
    int udev_fd;                        // udev monitor file descriptor
    v4l2_dev_sys_data_t* list_devices;  // list of available v4l2 devices
    int num_devices;                    // number of available v4l2 devices
} v4l2_device_list;

/*
 * video device data
 */
typedef struct _v4l2_dev_t
{
	int fd;                             // device file descriptor
	char *videodevice;                  // video device string (default "/dev/video0)"

	int cap_meth;                       // capture method: IO_READ or IO_MMAP
	v4l2_stream_formats_t* list_stream_formats; //list of available stream formats
	int numb_formats;                   //list size
	//int current_format_index;           //index of current stream format
	//int current_resolution_index;       //index of current resolution for current format

	struct v4l2_capability cap;         // v4l2 capability struct
	struct v4l2_format format;          // v4l2 format struct
	struct v4l2_buffer buf;             // v4l2 buffer struct
	struct v4l2_requestbuffers rb;      // v4l2 request buffers struct
	struct v4l2_streamparm streamparm;  // v4l2 stream parameters struct

	int requested_fmt;                  //requested format (may differ from format.fmt.pix.pixelformat)

	int fps_num;                        //fps numerator
	int fps_denom;                      //fps denominator

	uint8_t streaming;                  // flag device stream : STRM_STOP ; STRM_REQ_STOP; STRM_OK
	uint64_t frame_index;               // captured frame index from 0 to max(uint64_t)
	uint64_t timestamp;                 // captured frame timestamp
	void *mem[NB_BUFFER];               // memory buffers for mmap driver frames
	uint32_t buff_length[NB_BUFFER];    // memory buffers length as set by VIDIOC_QUERYBUF
	uint32_t buff_offset[NB_BUFFER];    // memory buffers offset as set by VIDIOC_QUERYBUF

	/*
	 * raw_frame is a pointer to the
	 * current frame buffer
	 * (DO NOT FREE)
	 */
	uint8_t *raw_frame;                 // pointer to raw frame (as captured from device)
	size_t raw_frame_size;              // size of raw frame (in bytes)
	size_t raw_frame_max_size;          // maximum size for raw frame

	uint8_t *tmp_buffer;                // temp buffer for decoding compressed data
	size_t tmp_buffer_max_size;         // max size (allocated size) for temp buffer
	uint8_t *yuv_frame;                 // frame buffer (YUYV), for rendering
	uint8_t *h264_frame;                // h264 frame data (after demuxing) can be a copy of raw_frame
	size_t  h264_frame_size;            // h264 frame data size (in bytes)
	size_t  h264_frame_max_size;        // h264 frame max size (allocated size)

	uint8_t h264_unit_id;  				// uvc h264 unit id, if <= 0 then uvc h264 is not supported
	uint8_t h264_no_probe_default;      // flag core to use the preset h264_config_probe_req data (don't reset to default before commit)
	uvcx_video_config_probe_commit_t h264_config_probe_req; //probe commit struct for h264 streams
	uint8_t *h264_last_IDR;             // last IDR frame retrieved from uvc h264 stream
	int h264_last_IDR_size;             // last IDR frame size
	uint8_t *h264_SPS;                  // h264 SPS info
	uint16_t h264_SPS_size;             // SPS size
	uint8_t *h264_PPS;                  // h264 PPS info
	uint16_t h264_PPS_size;             // PPS size
	uint8_t isKeyframe;                 // current buffer contains a keyframe (h264 IDR)

    int this_device;                    // index of this device in device list

    v4l2_ctrl_t* list_device_controls;    //null terminated linked list of available device controls
    int num_controls;                   //number of controls in list

    uint8_t isbayer;                    //flag if we are streaming bayer data in yuyv frame (logitech only)
    uint8_t bayer_pix_order;            //bayer pixel order

    int pan_step;                       //pan step for relative pan tilt controls (logitech sphere/orbit/BCC950)
    int tilt_step;                      //tilt step for relative pan tilt controls (logitech sphere/orbit/BCC950)

	int has_focus_control_id;           //it's set to control id if a focus control is available (enables software autofocus)
	int has_pantilt_control_id;         //it's set to 1 if a pan/tilt control is available
} v4l2_dev_t;

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
void v4l2core_set_verbosity(int level);

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
void v4l2core_disable_libv4l2();

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
void v4l2core_enable_libv4l2();

/*
 * get pixelformat from fourcc
 * args:
 *    fourcc - fourcc code for format
 *
 * asserts:
 *    none
 *
 * returns: v4l2 pixel format
 */
int v4l2core_fourcc_2_v4l2_pixelformat(const char *fourcc);

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
double v4l2core_get_realfps();

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
void v4l2core_set_capture_method(v4l2_dev_t *vd, int method);

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
v4l2_dev_t *v4l2core_init_dev(const char *device);

/*
 * Initiate the device list (with udev monitoring)
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void v4l2core_init_device_list();

/*
 * get the device list data
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: pointer to device list data
 */
v4l2_device_list *v4l2core_get_device_list();

/*
 * get the device index in device list
 * args:
 *   videodevice - string with videodevice node (e.g: /dev/video0)
 *
 * asserts:
 *   none
 *
 * returns:
 *   videodevice index in device list [0 - num_devices[ or -1 on error
 */
int v4l2core_get_device_index(const char *videodevice);

/*
 * check for new devices
 * args:
 *   vd - pointer to device data (can be null)
 *
 * asserts:
 *   my_device_list.udev is not null
 *   my_device_list.udev_fd is valid (> 0)
 *   my_device_list.udev_mon is not null
 *
 * returns: true(1) if device list was updated, false(0) otherwise
 */
int v4l2core_check_device_list_events(v4l2_dev_t *vd);

/*
 * free v4l2 devices list
 * args:
 *   none
 *
 * asserts:
 *   my_device_list.list_devices is not null
 *
 * returns: void
 */
void v4l2core_close_v4l2_device_list();

/* get frame format index from format list
 * args:
 *   vd - pointer to video device data
 *   format - v4l2 pixel format
 *
 * asserts:
 *   vd is not null
 *   vd->list_stream_formats is not null
 *
 * returns: format list index or -1 if not available
 */
int v4l2core_get_frame_format_index(v4l2_dev_t *vd, int format);

/* get resolution index for format index from format list
 * args:
 *   vd - pointer to video device data
 *   format - format index from format list
 *   width - requested width
 *   height - requested height
 *
 * asserts:
 *   vd is not null
 *   vd->list_stream_formats is not null
 *
 * returns: resolution list index for format index or -1 if not available
 */
int v4l2core_get_format_resolution_index(v4l2_dev_t *vd, int format, int width, int height);

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
void v4l2core_prepare_valid_format(v4l2_dev_t *vd);

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
void v4l2core_prepare_new_format(v4l2_dev_t *vd, int new_format);

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
void v4l2core_prepare_valid_resolution(v4l2_dev_t *vd);

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
void v4l2core_prepare_new_resolution(v4l2_dev_t *vd, int new_width, int new_height);

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
int v4l2core_update_current_format(v4l2_dev_t *vd);

/*
 * gets the next video frame and decodes it if necessary
 * args:
 * vd: pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code (E_OK)
 */
int v4l2core_get_frame(v4l2_dev_t *vd);

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
int v4l2core_release_frame(v4l2_dev_t *vd);

/*
 * decode video stream ( from raw_frame to frame buffer (yuyv format))
 * args:
 *    vd - pointer to device data
 *
 * asserts:
 *    vd is not null
 *
 * returns: error code ( 0 - E_OK)
*/
int v4l2core_frame_decode(v4l2_dev_t *vd);

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
void v4l2core_clean_buffers(v4l2_dev_t *vd);

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
void v4l2core_close_dev(v4l2_dev_t *vd);

/*
 * request a fps update
 * args:
 *    vd - pointer to video device data
 *
 * asserts:
 *    vd is not null
 *
 * returns: none
 */
void v4l2core_request_framerate_update(v4l2_dev_t *vd);

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
int v4l2core_get_framerate (v4l2_dev_t *vd);

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
int v4l2core_start_stream(v4l2_dev_t *vd);

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
int v4l2core_request_stop_stream(v4l2_dev_t *vd);

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
int v4l2core_stop_stream(v4l2_dev_t *vd);

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
v4l2_ctrl_t* v4l2core_get_control_by_id(v4l2_dev_t *vd, int id);

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
int v4l2core_set_control_value_by_id(v4l2_dev_t *vd, int id);

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
int v4l2core_get_control_value_by_id(v4l2_dev_t *vd, int id);

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
void v4l2core_set_control_defaults(v4l2_dev_t *vd);

/*
 * set autofocus sort method
 * args:
 *    method - sort method
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void v4l2core_soft_autofocus_set_sort(int method);

/*
 * initiate software autofocus
 * args:
 *    vd - pointer to device data
 *
 * asserts:
 *    vd is not null
 *
 * returns: error code (0 - E_OK)
 */
int v4l2core_soft_autofocus_init (v4l2_dev_t *vd);

/*
 * run the software autofocus
 * args:
 *    vd - pointer to device data
 *
 * asserts:
 *    vd is not null
 *
 * returns: 1 - running  0- focused
 * 	(only matters for non-continue focus)
 */
int v4l2core_soft_autofocus_run(v4l2_dev_t *vd);

/*
 * sets a focus loop while autofocus is on
 * args:
 *    none
 *
 * asserts:
 *    focus_ctx is not null
 *
 * returns: none
 */
void v4l2core_soft_autofocus_set_focus();

/*
 * close and clean software autofocus
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void v4l2core_soft_autofocus_close();

/*
 * save the device control values into a profile file
 * args:
 *   vd - pointer to video device data
 *   filename - profile filename
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code (0 -E_OK)
 */
int v4l2core_save_control_profile(v4l2_dev_t *vd, const char *filename);

/*
 * load the device control values from a profile file
 * args:
 *   vd - pointer to video device data
 *   filename - profile filename
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code (0 -E_OK)
 */
int v4l2core_load_control_profile(v4l2_dev_t *vd, const char *filename);

/*
 * ########### H264 controls ###########
 */

/*
 * resets the h264 encoder
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: 0 on success or error code on fail
 */
int v4l2core_reset_h264_encoder(v4l2_dev_t *vd);

/*
 * request a IDR frame from the H264 encoder
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: none
 */
void v4l2core_h264_request_idr(v4l2_dev_t *vd);

/*
 * query the frame rate config
 * args:
 *   vd - pointer to video device data
 *   query - query type
 *
 * asserts:
 *   vd is not null
 *
 * returns: frame rate config (FIXME: 0xffffffff on error)
 */
uint32_t v4l2core_query_h264_frame_rate_config(v4l2_dev_t *vd, uint8_t query);

/*
 * get the frame rate config
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: frame rate config (FIXME: 0xffffffff on error)
 */
uint32_t v4l2core_get_h264_frame_rate_config(v4l2_dev_t *vd);

/*
 * set the frame rate config
 * args:
 *   vd - pointer to video device data
 *   framerate - framerate
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code ( 0 -OK)
 */
int v4l2core_set_h264_frame_rate_config(v4l2_dev_t *vd, uint32_t framerate);

/*
 * updates the h264_probe_commit_req field
 * args:
 *   vd - pointer to video device data
 *   query - (UVC_GET_CUR; UVC_GET_MAX; UVC_GET_MIN)
 *   config_probe_req - pointer to uvcx_video_config_probe_commit_t:
 *     if null vd->h264_config_probe_req will be used
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code ( 0 -OK)
 */
int v4l2core_probe_h264_config_probe_req(
			v4l2_dev_t *vd,
			uint8_t query,
			uvcx_video_config_probe_commit_t *config_probe_req);

/*
 * get the video rate control mode
 * args:
 *   vd - pointer to video device data
 *   query - query type
 *
 * asserts:
 *   vd is not null
 *
 * returns: video rate control mode (FIXME: 0xff on error)
 */
uint8_t v4l2core_get_h264_video_rate_control_mode(v4l2_dev_t *vd, uint8_t query);

/*
 * set the video rate control mode
 * args:
 *   vd - pointer to video device data
 *   mode - rate mode
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code ( 0 -OK)
 */
int v4l2core_set_h264_video_rate_control_mode(v4l2_dev_t *vd, uint8_t mode);

/*
 * get the temporal scale mode
 * args:
 *   vd - pointer to video device data
 *   query - query type
 *
 * asserts:
 *   vd is not null
 *
 * returns: temporal scale mode (FIXME: 0xff on error)
 */
uint8_t v4l2core_get_h264_temporal_scale_mode(v4l2_dev_t *vd, uint8_t query);

/*
 * set the temporal scale mode
 * args:
 *   vd - pointer to video device data
 *   mode - temporal scale mode
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code ( 0 -OK)
 */
int v4l2core_set_h264_temporal_scale_mode(v4l2_dev_t *vd, uint8_t mode);

/*
 * get the spatial scale mode
 * args:
 *   vd - pointer to video device data
 *   query - query type
 *
 * asserts:
 *   vd is not null
 *
 * returns: temporal scale mode (FIXME: 0xff on error)
 */
uint8_t v4l2core_get_h264_spatial_scale_mode(v4l2_dev_t *vd, uint8_t query);

/*
 * set the spatial scale mode
 * args:
 *   vd - pointer to video device data
 *   mode - spatial scale mode
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code ( 0 -OK)
 */
int v4l2core_set_h264_spatial_scale_mode(v4l2_dev_t *vd, uint8_t mode);

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
uint16_t v4l2core_get_length_xu_control(v4l2_dev_t *vd, uint8_t unit, uint8_t selector);

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
uint8_t v4l2core_get_info_xu_control(v4l2_dev_t *vd, uint8_t unit, uint8_t selector);

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
int v4l2core_query_xu_control(v4l2_dev_t *vd, uint8_t unit, uint8_t selector, uint8_t query, void *data);

/*
 *  ########### FILE IO ###############
 */
/*
 * save data to file
 * args:
 *   filename - string with filename
 *   data - pointer to data
 *   size - data size in bytes = sizeof(uint8_t)
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int v4l2core_save_data_to_file(const char *filename, uint8_t *data, int size);

/*
 * save the current frame to file
 * args:
 *    vd - pointer to device data
 *    filename - output file name
 *    format - image type
 *           (IMG_FMT_RAW, IMG_FMT_JPG, IMG_FMT_PNG, IMG_FMT_BMP)
 *
 * asserts:
 *    vd is not null
 *
 * returns: error code
 */
int v4l2core_save_image(v4l2_dev_t *vd, const char *filename, int format);

/*
 * ############### TIME DATA ##############
 */

/*
 * get current timestamp
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: monotonic time in nanoseconds
 */
uint64_t v4l2core_time_get_timestamp();

__END_DECLS

#endif

