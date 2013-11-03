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

#ifndef UVC_H264_H
#define UVC_H264_H

#include <linux/videodev2.h>
#include <linux/uvcvideo.h>
#include <linux/media.h>

#include <gtk/gtk.h>

#include "defs.h"
#include "v4l2uvc.h"
#include "globals.h"
#include "guvcview.h"

// GUID of the UVC H.264 extension unit: {A29E7641-DE04-47E3-8B2B-F4341AFF003B}
#define GUID_UVCX_H264_XU {0x41, 0x76, 0x9E, 0xA2, 0x04, 0xDE, 0xE3, 0x47, 0x8B, 0x2B, 0xF4, 0x34, 0x1A, 0xFF, 0x00, 0x3B}

typedef struct
{
  int8_t bLength;
  int8_t bDescriptorType;
  int8_t bDescriptorSubType;
  int8_t bUnitID;
  uint8_t guidExtensionCode[16];
} __attribute__ ((__packed__)) xu_descriptor;

/* UVC H.264 control selectors */
#define UVCX_VIDEO_CONFIG_PROBE			0x01
#define	UVCX_VIDEO_CONFIG_COMMIT		0x02
#define	UVCX_RATE_CONTROL_MODE			0x03
#define	UVCX_TEMPORAL_SCALE_MODE		0x04
#define UVCX_SPATIAL_SCALE_MODE			0x05
#define	UVCX_SNR_SCALE_MODE				0x06
#define	UVCX_LTR_BUFFER_SIZE_CONTROL	0x07
#define	UVCX_LTR_PICTURE_CONTROL		0x08
#define	UVCX_PICTURE_TYPE_CONTROL		0x09
#define	UVCX_VERSION					0x0A
#define	UVCX_ENCODER_RESET				0x0B
#define	UVCX_FRAMERATE_CONFIG			0x0C
#define	UVCX_VIDEO_ADVANCE_CONFIG		0x0D
#define	UVCX_BITRATE_LAYERS				0x0E
#define	UVCX_QP_STEPS_LAYERS			0x0F

/* bmHints defines */
#define BMHINTS_RESOLUTION        0x0001
#define BMHINTS_PROFILE           0x0002
#define BMHINTS_RATECONTROL       0x0004
#define BMHINTS_USAGE             0x0008
#define BMHINTS_SLICEMODE         0x0010
#define BMHINTS_SLICEUNITS        0x0020
#define BMHINTS_MVCVIEW           0x0040
#define BMHINTS_TEMPORAL          0x0080
#define BMHINTS_SNR               0x0100
#define BMHINTS_SPATIAL           0x0200
#define BMHINTS_SPATIAL_RATIO     0x0400
#define BMHINTS_FRAME_INTERVAL    0x0800
#define BMHINTS_LEAKY_BKT_SIZE    0x1000
#define BMHINTS_BITRATE           0x2000
#define BMHINTS_ENTROPY           0x4000
#define BMHINTS_IFRAMEPERIOD      0x8000

/* wSliceMode defines */
#define SLICEMODE_BITSPERSLICE    0x0001
#define SLICEMODE_MBSPERSLICE     0x0002
#define SLICEMODE_SLICEPERFRAME   0x0003

/***********************************************************************************************************************
* bUsageType defines
* The bUsageType used in Probe/Commit structure. The UCCONFIG parameters are based on "UCConfig Modes v1.1".
* bUsageType  UCConfig   Description
*   4           0        Non-scalable single layer AVC bitstream with simulcast(number of simulcast streams>=1)
*   5           1        SVC temporal scalability with hierarchical P with simulcast(number of simulcast streams>=1)
*   6           2q       SVC temporal scalability + Quality/SNR scalability with simulcast(number of simulcast streams>=1)
*   7           2s       SVC temporal scalability + spatial scalability with simulcast(number of simulcast streams>=1)
*   8           3        Full SVC scalability (temporal scalability + SNR scalability + spatial scalability)
*                        with simulcast(number of simulcast streams>=1)
************************************************************************************************************************/

#define USAGETYPE_REALTIME        0x01
#define USAGETYPE_BROADCAST       0x02
#define USAGETYPE_STORAGE         0x03
#define USAGETYPE_UCCONFIG_0      0x04
#define USAGETYPE_UCCONFIG_1      0x05
#define USAGETYPE_UCCONFIG_2Q     0x06
#define USAGETYPE_UCCONFIG_2S     0x07
#define USAGETYPE_UCCONFIG_3      0x08

/* bRateControlMode defines */
#define RATECONTROL_CBR           0x01
#define RATECONTROL_VBR           0x02
#define RATECONTROL_CONST_QP      0x03
#define RATECONTROL_FIXED_FRM_FLG 0x10

/* bStreamFormat defines */
#define STREAMFORMAT_ANNEXB       0x00
#define STREAMFORMAT_NAL          0x01

/* bEntropyCABAC defines */
#define ENTROPY_CAVLC             0x00
#define ENTROPY_CABAC             0x01

/* bTimingstamp defines */
#define TIMESTAMP_SEI_DISABLE     0x00
#define TIMESTAMP_SEI_ENABLE      0x01

/* bPreviewFlipped defines */
#define PREFLIPPED_DISABLE        0x00
#define PREFLIPPED_HORIZONTAL     0x01

/* wPictureType defines */
#define PICTURE_TYPE_IFRAME 	0x0000 //Generate an IFRAME
#define PICTURE_TYPE_IDR		0x0001 //Generate an IDR
#define PICTURE_TYPE_IDR_FULL	0x0002 //Generate an IDR frame with new SPS and PPS

/* bStreamMuxOption defines */
#define STREAMMUX_H264          (1 << 0) | (1 << 1)
#define STREAMMUX_YUY2          (1 << 0) | (1 << 2)
#define STREAMMUX_YUYV          (1 << 0) | (1 << 2)
#define STREAMMUX_NV12          (1 << 0) | (1 << 3)

/* wLayerID Macro */

/*                              wLayerID
  |------------+------------+------------+----------------+------------|
  |  Reserved  |  StreamID  | QualityID  |  DependencyID  | TemporalID |
  |  (3 bits)  |  (3 bits)  | (3 bits)   |  (4 bits)      | (3 bits)   |
  |------------+------------+------------+----------------+------------|
  |15        13|12        10|9          7|6              3|2          0|
  |------------+------------+------------+----------------+------------|
*/

#define xLayerID(stream_id, quality_id, dependency_id, temporal_id) ((((stream_id)&7)<<10)|(((quality_id)&7)<<7)|(((dependency_id)&15)<<3)|((temporal_id)&7))

/* id extraction from wLayerID */
#define xStream_id(layer_id)      (((layer_id)>>10)&7)
#define xQuality_id(layer_id)     (((layer_id)>>7)&7)
#define xDependency_id(layer_id)  (((layer_id)>>3)&15)
#define xTemporal_id(layer_id)    ((layer_id)&7)

/* h264 probe commit struct (defined in v4l2uvc.h) */

/* rate control */
typedef struct _uvcx_rate_control_mode_t
{
	WORD	wLayerID;
	BYTE	bRateControlMode;
} __attribute__((__packed__)) uvcx_rate_control_mode_t;

/* temporal scale */
typedef struct _uvcx_temporal_scale_mode_t
{
	WORD	wLayerID;
	BYTE	bTemporalScaleMode;
} __attribute__((__packed__)) uvcx_temporal_scale_mode_t;

/* spatial scale mode */
typedef struct _uvcx_spatial_scale_mode_t
{
	WORD	wLayerID;
	BYTE	bSpatialScaleMode;
} __attribute__((__packed__)) uvcx_spatial_scale_mode_t;

/* snr scale mode */
typedef struct _uvcx_snr_scale_mode_t
{
	WORD	wLayerID;
	BYTE	bSNRScaleMode;
	BYTE	bMGSSublayerMode;
} __attribute__((__packed__)) uvcx_snr_scale_mode_t;

/* buffer size control*/
typedef struct _uvcx_ltr_buffer_size_control_t
{
	WORD	wLayerID;
	BYTE	bLTRBufferSize;
	BYTE	bLTREncoderControl;
} __attribute__((__packed__)) uvcx_ltr_buffer_size_control_t;

/* ltr picture control */
typedef struct _uvcx_ltr_picture_control
{
	WORD	wLayerID;
	BYTE	bPutAtPositionInLTRBuffer;
	BYTE	bEncodeUsingLTR;
} __attribute__((__packed__)) uvcx_ltr_picture_control;

/* picture type control */
typedef struct _uvcx_picture_type_control_t
{
	WORD	wLayerID;
	WORD	wPicType;
} __attribute__((__packed__)) uvcx_picture_type_control_t;

/* version */
typedef struct _uvcx_version_t
{
	WORD	wVersion;
} __attribute__((__packed__)) uvcx_version_t;

/* encoder reset */
typedef struct _uvcx_encoder_reset
{
	WORD	wLayerID;
} __attribute__((__packed__)) uvcx_encoder_reset;

/* frame rate */
typedef struct _uvcx_framerate_config_t
{
	WORD	wLayerID;
	DWORD	dwFrameInterval;
} __attribute__((__packed__)) uvcx_framerate_config_t;

/* advance config */
typedef struct _uvcx_video_advance_config_t
{
	WORD	wLayerID;
	DWORD	dwMb_max;
	BYTE	blevel_idc;
	BYTE	bReserved;
} __attribute__((__packed__)) uvcx_video_advance_config_t;

/* bit rate */
typedef struct _uvcx_bitrate_layers_t
{
	WORD	wLayerID;
	DWORD	dwPeakBitrate;
	DWORD	dwAverageBitrate;
} __attribute__((__packed__)) uvcx_bitrate_layers_t;

/* qp steps */
typedef struct _uvcx_qp_steps_layers_t
{
	WORD	wLayerID;
	BYTE	bFrameType;
	BYTE	bMinQp;
	BYTE	bMaxQp;
} __attribute__((__packed__)) uvcx_qp_steps_layers_t;


/*
 * creates the control widgets for uvc H264
 */
void add_uvc_h264_controls_tab (struct ALL_DATA* all_data);

uint8_t xu_get_unit_id (uint64_t busnum, uint64_t devnum);
int has_h264_support(int hdevice, uint8_t unit_id);
/*
 * determines and sets a resonable framerate
 * for the h264 stream depending on the video capture
 * circular buffer usage
 */
int h264_framerate_balance(struct ALL_DATA *all_data);

void check_uvc_h264_format(struct vdIn *vd, struct GLOBAL *global);
void set_muxed_h264_format(struct vdIn *vd, struct GLOBAL *global);
/*
 * probe/commit h264 controls (video stream must be off)
 */
void h264_probe(struct ALL_DATA *data);
void h264_commit(struct vdIn *vd, struct GLOBAL *global);

int uvcx_video_probe(int hdevice, uint8_t unit_id, uint8_t query, uvcx_video_config_probe_commit_t *uvcx_video_config);
int uvcx_video_commit(int hdevice, uint8_t unit_id, uvcx_video_config_probe_commit_t *uvcx_video_config);
int uvcx_video_encoder_reset(int hdevice, uint8_t unit_id);
uint8_t uvcx_get_video_rate_control_mode(int hdevice, uint8_t unit_id, uint8_t query);
int uvcx_set_video_rate_control_mode(int hdevice, uint8_t unit_id, uint8_t rate_mode);
uint8_t uvcx_get_temporal_scale_mode(int hdevice, uint8_t unit_id, uint8_t query);
int uvcx_set_temporal_scale_mode(int hdevice, uint8_t unit_id, uint8_t scale_mode);
uint8_t uvcx_get_spatial_scale_mode(int hdevice, uint8_t unit_id, uint8_t query);
int uvcx_set_spatial_scale_mode(int hdevice, uint8_t unit_id, uint8_t scale_mode);
int uvcx_request_frame_type(int hdevice, uint8_t unit_id, uint16_t type);
uint32_t uvcx_get_frame_rate_config(int hdevice, uint8_t unit_id, uint8_t query);
int uvcx_set_frame_rate_config(int hdevice, uint8_t unit_id, uint32_t framerate);
#endif /*UVC_H264_H*/
