/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "gviewv4l2core.h"
#include "uvc_h264.h"
#include "jpeg_decoder.h"
#include "colorspaces.h"

extern int verbosity;

/*
 * Alloc image buffers for decoding video stream
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: error code  (0- E_OK)
 */
int alloc_v4l2_frames(v4l2_dev *vd)
{
	/*assertions*/
	assert(vd != NULL);

	vd->raw_frame = NULL;

	if(vd->tmp_buffer)
	{
		free(vd->tmp_buffer);
		vd->tmp_buffer = NULL;
	}

	if(vd->h264_frame)
	{
		free(vd->h264_frame);
		vd->h264_frame = NULL;
	}

	if(vd->yuv_frame)
	{
		free(vd->yuv_frame);
		vd->yuv_frame = NULL;
	}

	if(vd->h264_last_IDR)
	{
		free(vd->h264_last_IDR);
		vd->h264_last_IDR = NULL;
	}

	int ret = E_OK;

	size_t framebuf_size = 0;
	size_t tmpbuf_size = 0;

	int width = vd->format.fmt.pix.width;
	int height = vd->format.fmt.pix.height;

	if(width <= 0 || height <= 0)
		return E_ALLOC_ERR;

	int framesizeIn = (width * height << 1); //2 bytes per pixel
	switch (vd->requested_fmt)
	{
		case V4L2_PIX_FMT_H264:
			/*init h264 context*/
			ret = h264_init_decoder(width, height);
			
			if(ret)
			{
				fprintf(stderr, "V4L2_CORE: couldn't init h264 decoder\n");
				return ret;
			}
			
			vd->h264_last_IDR = calloc(framesizeIn, sizeof(uint8_t));
			vd->h264_last_IDR_size = 0; /*reset (no frame stored)*/

			vd->h264_frame = calloc(framesizeIn, sizeof(uint8_t));

		case V4L2_PIX_FMT_JPEG:
		case V4L2_PIX_FMT_MJPEG:
			/* alloc a temp buffer to reconstruct the pict (MJPEG)*/
			tmpbuf_size= framesizeIn;
			vd->tmp_buffer = calloc(tmpbuf_size, sizeof(uint8_t));
			framebuf_size = width * (height + 8) * 2;
			vd->yuv_frame = calloc(framebuf_size, sizeof(uint8_t));
			break;

		case V4L2_PIX_FMT_UYVY:
		case V4L2_PIX_FMT_YVYU:
		case V4L2_PIX_FMT_YYUV:
		case V4L2_PIX_FMT_YUV420: /* only needs 3/2 bytes per pixel but we alloc 2 bytes per pixel*/
		case V4L2_PIX_FMT_YVU420: /* only needs 3/2 bytes per pixel but we alloc 2 bytes per pixel*/
		case V4L2_PIX_FMT_Y41P:   /* only needs 3/2 bytes per pixel but we alloc 2 bytes per pixel*/
		case V4L2_PIX_FMT_NV12:
		case V4L2_PIX_FMT_NV21:
		case V4L2_PIX_FMT_NV16:
		case V4L2_PIX_FMT_NV61:
		case V4L2_PIX_FMT_SPCA501:
		case V4L2_PIX_FMT_SPCA505:
		case V4L2_PIX_FMT_SPCA508:
			/* alloc a temp buffer for colorspace conversion*/
			tmpbuf_size= framesizeIn;
			vd->tmp_buffer = calloc(tmpbuf_size, sizeof(uint8_t));
			framebuf_size = framesizeIn;
			vd->yuv_frame = calloc(framebuf_size, sizeof(uint8_t));
			break;

		case V4L2_PIX_FMT_GREY:
			/* alloc a temp buffer for colorspace conversion*/
			tmpbuf_size= width * height; /* 1 byte per pixel*/
			vd->tmp_buffer = calloc(tmpbuf_size, sizeof(uint8_t));
			framebuf_size = framesizeIn;
			vd->yuv_frame = calloc(framebuf_size, sizeof(uint8_t));
			break;

	    case V4L2_PIX_FMT_Y10BPACK:
	    case V4L2_PIX_FMT_Y16:
			/* alloc a temp buffer for colorspace conversion*/
			tmpbuf_size= width * height * 2; /* 2 byte per pixel*/
			vd->tmp_buffer = calloc(tmpbuf_size, sizeof(uint8_t));
			framebuf_size = framesizeIn;
			vd->yuv_frame = calloc(framebuf_size, sizeof(uint8_t));
			break;

		case V4L2_PIX_FMT_YUYV:
			/*
			 * YUYV doesn't need a temp buffer but we will set it if/when
			 *  video processing disable is set (bayer processing).
			 *            (logitech cameras only)
			 */
			framebuf_size = framesizeIn;
			vd->yuv_frame = calloc(framebuf_size, sizeof(uint8_t));
			break;

		case V4L2_PIX_FMT_SGBRG8: /*0*/
		case V4L2_PIX_FMT_SGRBG8: /*1*/
		case V4L2_PIX_FMT_SBGGR8: /*2*/
		case V4L2_PIX_FMT_SRGGB8: /*3*/
			/*
			 * Raw 8 bit bayer
			 * when grabbing use:
			 *    bayer_to_rgb24(bayer_data, RGB24_data, width, height, 0..3)
			 *    rgb2yuyv(RGB24_data, vd->framebuffer, width, height)
			 */

			/* alloc a temp buffer for converting to YUYV*/
			/* rgb buffer for decoding bayer data*/
			tmpbuf_size = width * height * 3;
			vd->tmp_buffer = calloc(tmpbuf_size, sizeof(uint8_t));
			framebuf_size = framesizeIn;
			vd->yuv_frame = calloc(framebuf_size, sizeof(uint8_t));
			break;
		case V4L2_PIX_FMT_RGB24:
		case V4L2_PIX_FMT_BGR24:
			/*
			 * rgb or bgr (8-8-8)
			 * alloc a temp buffer for converting to YUYV
			 */
			tmpbuf_size = width * height * 3;
			vd->tmp_buffer = calloc(tmpbuf_size, sizeof(uint8_t));

			framebuf_size = framesizeIn;
			vd->yuv_frame = calloc(framebuf_size, sizeof(uint8_t));
			break;

		default:
			/*
			 * we check formats against a support formats list
			 * so we should never have to alloc for a unknown format
			 */
			fprintf(stderr, "V4L2_CORE: (v4l2uvc.c) should never arrive (1)- exit fatal !!\n");
			ret = E_UNKNOWN_ERR;
			vd->raw_frame = NULL;
			if(vd->h264_last_IDR)
				free(vd->h264_last_IDR);
			vd->h264_last_IDR = NULL;
			if(vd->yuv_frame)
				free(vd->yuv_frame);
			vd->yuv_frame = NULL;
			if(vd->tmp_buffer)
				free(vd->tmp_buffer);
			vd->tmp_buffer = NULL;
			if(vd->h264_frame)
				free(vd->h264_frame);
			vd->h264_frame = NULL;
			return (ret);
	}

	/*assertions*/
	assert(vd->yuv_frame != NULL);


	int i = 0;
	/* set framebuffer to black (y=0x00 u=0x80 v=0x80) by default*/
	for (i=0; i<(framebuf_size-4); i+=4)
	{
		vd->yuv_frame[i]=0x00;  //Y
		vd->yuv_frame[i+1]=0x80;//U
		vd->yuv_frame[i+2]=0x00;//Y
		vd->yuv_frame[i+3]=0x80;//V
	}

	return (ret);
}

/*
 * free image buffers for decoding video stream
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: none
 */
void clean_v4l2_frames(v4l2_dev *vd)
{
	/*assertions*/
	assert(vd != NULL);

	vd->raw_frame = NULL;

	if(vd->tmp_buffer)
	{
		free(vd->tmp_buffer);
		vd->tmp_buffer = NULL;
	}

	if(vd->h264_frame)
	{
		free(vd->h264_frame);
		vd->h264_frame = NULL;
	}

	if(vd->yuv_frame)
	{
		free(vd->yuv_frame);
		vd->yuv_frame = NULL;
	}

	if(vd->h264_last_IDR)
	{
		free(vd->h264_last_IDR);
		vd->h264_last_IDR = NULL;
	}

	if(vd->h264_SPS)
	{
		free(vd->h264_SPS);
		vd->h264_SPS = NULL;
	}

	if(vd->h264_PPS)
	{
		free(vd->h264_PPS);
		vd->h264_PPS = NULL;
	}

	if(vd->requested_fmt == V4L2_PIX_FMT_H264)
		h264_close_decoder();

}

/*
 * check buff (*buff) of size (size) for NALU type (type)
 * args:
 *    type - NALU type
 *    buff - buffer with MJPG uvc frame containing h264 data
 *    size - buffer size
 *
 * asserts:
 *    buff is not null
 *
 * returns: buffer pointer to NALU type data if found
 *          NULL if not found
 */
static uint8_t* check_NALU(uint8_t type, uint8_t *buff, int size)
{
	/*asserts*/
	assert(buff != NULL);

	uint8_t *sp = buff;
	uint8_t *nal = NULL;
	//search for NALU of type
	for(sp = buff; sp < buff + size - 5; ++sp)
	{
		if(sp[0] == 0x00 &&
		   sp[1] == 0x00 &&
		   sp[2] == 0x00 &&
		   sp[3] == 0x01 &&
		   (sp[4] & 0x1F) == type)
		{
			/*found it*/
			nal = sp + 4;
			break;
		}
	}

	return nal;
}

/*
 * parses a buff (*buff) of size (size) for NALU type (type)
 * args:
 *    type - NALU type
 *    NALU - pointer to pointer to NALU data
 *    buff - pointer to buffer containing h264 data muxed in MJPG container
 *    size - buff size
 *
 * asserts:
 *    buff is not null
 *
 * returns: NALU size and sets pointer (NALU) to NALU data
 *          -1 if no NALU found
 */
static int parse_NALU(uint8_t type, uint8_t **NALU, uint8_t *buff, int size)
{
	/*asserts*/
	assert(buff != NULL);

	int nal_size = 0;
	uint8_t *sp = NULL;

	//search for NALU of type
	uint8_t *nal = check_NALU(type, buff, size);
	if(nal == NULL)
	{
		fprintf(stderr, "V4L2_CORE: (uvc H264) could not find NALU of type %i in buffer\n", type);
		return -1;
	}

	//search for end of NALU
	for(sp = nal; sp < buff + size - 4; ++sp)
	{
		if(sp[0] == 0x00 &&
		   sp[1] == 0x00 &&
		   sp[2] == 0x00 &&
		   sp[3] == 0x01)
		{
			nal_size = sp - nal;
			break;
		}
	}

	if(!nal_size)
		nal_size = buff + size - nal;

	*NALU = calloc(nal_size, sizeof(uint8_t));
	memcpy(*NALU, nal, nal_size);

	//char test_filename2[20];
	//snprintf(test_filename2, 20, "frame_nalu-%i.raw", type);
	//SaveBuff (test_filename2, nal_size, *NALU);

	return nal_size;
}

/*
 * demux a buff (*buff) of size (size) for NALU data
 * args:
 *    h264_data - pointer to h264 data
 *    buff pointer to buffer with h264 muxed in MJPG container
 *    size - buff size
 *
 * asserts:
 *    h264_data is not null
 *    buff is not null
 *
 * returns: data size and copies NALU data to h264 buffer
 */
static int demux_NALU(uint8_t *h264_data, uint8_t *buff, int size)
{
	/*asserts*/
	assert(h264_data != NULL);
	assert(buff != NULL);

	uint8_t *sp = NULL;
	uint8_t *pl = NULL;
	uint8_t *spl = NULL;
	uint8_t *epl = NULL;
	uint8_t *ph264 = h264_data;
	int total_size = 0;
	//segments have a maximum of 64Kb
	int seg_size = 64*1024;

	//search for first APP4 marker
	for(sp = buff; sp < buff + size - 2; ++sp)
	{
		if(sp[0] == 0xFF &&
		   sp[1] == 0xE4)
		{
			spl = sp; //payload start
			break;
		}
	}

	if(spl == buff)
	{
		fprintf(stderr, "V4L2_CORE: (uvc H264) could not find a APP4 marker in buffer\n");
		return -1;
	}

	pl = spl + 4; //marker(2) + length(2)

	uint32_t frame_PTS = 0;
	frame_PTS =  ((uint32_t) pl[18]) << 0;
	frame_PTS |= ((uint32_t) pl[19]) << 8;
	frame_PTS |= ((uint32_t) pl[20]) << 16;
	frame_PTS |= ((uint32_t) pl[21]) << 24;

	pl += 22; //header(22)

	//get payload size
	uint32_t payload_size = 0;
	payload_size =  ((uint32_t) pl[0]) << 0;
	payload_size |= ((uint32_t) pl[1]) << 8;
	payload_size |= ((uint32_t) pl[2]) << 16;
	payload_size |= ((uint32_t) pl[3]) << 24;

	pl +=4;                  //start of the payload data
	epl = pl + payload_size; //end of payload data
	if(epl > buff + size)
		epl = buff + size;

	//for the first segment remove header size
	//marker(2) + length(2) + header(22) + payload size(4)
	seg_size = 64*1024 - (pl-spl);

	while(pl + seg_size <= epl)
	{
		//copy segment to h264 data buffer
		memcpy(ph264, pl, seg_size);
		ph264 += seg_size;
		total_size += seg_size;

		pl += seg_size; //reset to the next segment data

		if(pl[0] != 0xFF ||
		   pl[1] != 0xE4)
		{
		   fprintf(stderr, "V4L2_CORE: (uvc H264) no APP4 marker at end of 64K segment\n");
		   break;
		}
		else
			pl += 4; //skip marker(2) + length(2)

		seg_size = 64*1024 - 4; //skip marker + length
	}

	seg_size = epl - pl; //last segment
	if(seg_size > 0)
	{
		//copy segment to h264 data buffer
		memcpy(ph264, pl, seg_size);
		ph264 += seg_size;
		total_size += seg_size;
	}

	//char test_filename2[22];
	//snprintf(test_filename2, 22, "frame-%u.raw", frame_PTS);
	//SaveBuff (test_filename2, total_size, h264_data);

	//printf("V4L2_CORE: (uvc H264) PTS: %u; payload size: %u, total size: %u\n", frame_PTS, payload_size, total_size);
	return total_size;
}

/*
 * Store the SPS and PPS NALUs of uvc H264 stream
 * args:
 *    vd - pointer to device data
 *
 * asserts:
 *    vd is not null
 *
 * returns: error code (0 - E_OK)
 */
static int store_extra_data(v4l2_dev* vd)
{
	/*asserts*/
	assert(vd != NULL);

	if(vd->h264_SPS == NULL)
	{
		vd->h264_SPS_size = parse_NALU( 7, &vd->h264_SPS, vd->h264_frame, (int) vd->h264_frame_size);

		if(vd->h264_SPS_size <= 0 || vd->h264_SPS == NULL)
		{
			fprintf(stderr, "V4L2_CORE: (uvc H264) Could not find SPS (NALU type: 7)\n");
			return E_NO_DATA;
		}
		else if(verbosity > 0)
			printf("V4L2_CORE: (uvc H264) stored SPS %i bytes of data\n", vd->h264_SPS_size);
	}

	if(vd->h264_PPS == NULL)
	{
		vd->h264_PPS_size = parse_NALU( 8, &vd->h264_PPS, vd->h264_frame, (int) vd->h264_frame_size);

		if(vd->h264_PPS_size <= 0 || vd->h264_PPS == NULL)
		{
			fprintf(stderr, "Could not find PPS (NALU type: 8)\n");
			return E_NO_DATA;
		}
		else if(verbosity > 0)
			printf("V4L2_CORE: (uvc H264) stored PPS %i bytes of data\n", vd->h264_PPS_size);
	}

	return E_OK;
}

/*
 * check/store the last IDR frame
 * args:
 *    vd - pointer to device data
 *
 * asserts:
 *    vd is not NULL
 *
 * return: TRUE (1) if IDR frame
 *         FALSE(0) if non IDR frame
 */
static uint8_t is_h264_keyframe (v4l2_dev* vd)
{
	//check for a IDR frame type
	if(check_NALU(5, vd->h264_frame, vd->h264_frame_size) != NULL)
	{
		memcpy(vd->h264_last_IDR, vd->h264_frame, vd->h264_frame_size);
		vd->h264_last_IDR_size = vd->h264_frame_size;
		if(verbosity > 1)
			printf("V4L2_CORE: (uvc H264) IDR frame found in frame %" PRIu64 "\n", vd->frame_index);
		return TRUE;
	}

	return FALSE;
}

/*
 * demux h264 data from muxed frame
 * args:
 *    h264_data - pointer to demuxed h264 data
 *    buffer - pointer to muxed h264 data
 *    size - buffer size
 *
 * asserts:
 *    h264_data is not null
 *    buffer is not null
 *
 * return: demuxed h264 frame data size
 */
static int demux_h264(uint8_t* h264_data, uint8_t* buffer, int size)
{
	/*asserts*/
	assert(h264_data != NULL);
	assert(buffer != NULL);

	/*
	 * if h264 is not supported return 0 (empty frame)
	 */
	if(h264_get_support() == H264_NONE)
		return 0;

	/*
	 * if it's a muxed stream we must demux it first
	 */
	if(h264_get_support() == H264_MUXED)
	{
		return demux_NALU(h264_data, buffer, size);
	}

	/*
	 * (H264_FRAME) store the raw frame in h264 frame buffer
	 */
	memcpy(h264_data, buffer, size);
	return size;

}

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
int frame_decode(v4l2_dev* vd)
{
	/*asserts*/
	assert(vd != NULL);

	int ret = E_OK;

	int width = vd->format.fmt.pix.width;
	int height = vd->format.fmt.pix.height;

	/*
	 * use the requested format since it may differ
	 * from format.fmt.pix.pixelformat (muxed H264)
	 */
	int format = vd->requested_fmt;

	int framesizeIn =(width * height << 1);//2 bytes per pixel
	switch (format)
	{
		case V4L2_PIX_FMT_H264:
			/*
			 * get the h264 frame in the tmp_buffer
			 */
			vd->h264_frame_size = demux_h264(vd->h264_frame, vd->raw_frame, vd->raw_frame_size);

			/*
			 * store SPS and PPS info (usually the first two NALU)
			 * and check/store the last IDR frame
			 */
			store_extra_data(vd);

			/*
			 * check for keyframe and store it
			 */
			vd->isKeyframe = is_h264_keyframe(vd);

			//decode if we already have a IDR frame
			if(vd->h264_last_IDR_size > 0)
			{
				/* decode (h264) to vd->tmp_buffer (yuv420p)*/
				h264_decode(vd->tmp_buffer, vd->h264_frame, vd->h264_frame_size);
				/* convert to yuyv*/
				yuv420_to_yuyv (vd->yuv_frame, vd->tmp_buffer, width, height);
			}
			break;

		case V4L2_PIX_FMT_JPEG:
		case V4L2_PIX_FMT_MJPEG:
			if(vd->raw_frame_size <= HEADERFRAME1)
			{
				// Prevent crash on empty image
				printf("V4L2_CORE: (jpeg decoder) Ignoring empty buffer ...\n");
				return (ret);
			}
			/*FIXME: do we need this ?*/
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);

			if (jpeg_decode(&vd->yuv_frame, vd->tmp_buffer, width, height) < 0)
			{
				fprintf(stderr, "V4L2_CORE: jpeg decoder exit with an error\n");
				ret = E_DECODE_ERR;
				return ret;
			}
			break;

		case V4L2_PIX_FMT_UYVY:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			uyvy_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_YVYU:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			yvyu_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_YYUV:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			yyuv_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_YUV420:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			yuv420_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_YVU420:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			yvu420_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_NV12:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			nv12_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_NV21:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			nv21_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_NV16:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			nv16_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_NV61:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			nv61_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_Y41P:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			y41p_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_GREY:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			grey_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_Y10BPACK:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			y10b_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

	    case V4L2_PIX_FMT_Y16:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			y16_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_SPCA501:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			s501_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_SPCA505:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			s505_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_SPCA508:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			s508_to_yuyv(vd->yuv_frame, vd->tmp_buffer, width, height);
			break;

		case V4L2_PIX_FMT_YUYV:
			if(vd->isbayer>0)
			{
				if (!(vd->tmp_buffer))
				{
					// rgb buffer for decoding bayer data
					vd->tmp_buffer = calloc(width * height * 3, sizeof(uint8_t));
				}
				bayer_to_rgb24 (vd->raw_frame, vd->tmp_buffer, width, height, vd->pix_order);
				// raw bayer is only available in logitech cameras in yuyv mode
				rgb2yuyv (vd->tmp_buffer, vd->yuv_frame, width, height);
			}
			else
			{
				if (vd->raw_frame_size > framesizeIn)
					memcpy(vd->yuv_frame, vd->raw_frame,
						(size_t) framesizeIn);
				else
					memcpy(vd->yuv_frame, vd->raw_frame, vd->raw_frame_size);
			}
			break;

		case V4L2_PIX_FMT_SGBRG8: //0
			bayer_to_rgb24 (vd->raw_frame, vd->tmp_buffer, width, height, 0);
			rgb2yuyv (vd->tmp_buffer, vd->yuv_frame, width, height);
			break;

		case V4L2_PIX_FMT_SGRBG8: //1
			bayer_to_rgb24 (vd->raw_frame, vd->tmp_buffer, width, height, 1);
			rgb2yuyv (vd->tmp_buffer, vd->yuv_frame, width, height);
			break;

		case V4L2_PIX_FMT_SBGGR8: //2
			bayer_to_rgb24 (vd->raw_frame, vd->tmp_buffer, width, height, 2);
			rgb2yuyv (vd->tmp_buffer, vd->yuv_frame, width, height);
			break;
		case V4L2_PIX_FMT_SRGGB8: //3
			bayer_to_rgb24 (vd->raw_frame, vd->tmp_buffer, width, height, 3);
			rgb2yuyv (vd->tmp_buffer, vd->yuv_frame, width, height);
			break;

		case V4L2_PIX_FMT_RGB24:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			rgb2yuyv(vd->tmp_buffer, vd->yuv_frame, width, height);
			break;
		case V4L2_PIX_FMT_BGR24:
			memcpy(vd->tmp_buffer, vd->raw_frame, vd->raw_frame_size);
			bgr2yuyv(vd->tmp_buffer, vd->yuv_frame, width, height);
			break;

		default:
			fprintf(stderr, "V4L2_CORE: error decoding frame: unknown format: %i\n", format);
			ret = E_UNKNOWN_ERR;
			break;
	}

	return ret;
}
