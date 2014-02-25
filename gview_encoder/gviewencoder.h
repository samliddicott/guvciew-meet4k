/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
#           Flemming Frandsen <dren.dk@gmail.com>                               #
#                             Add VU meter OSD                                  #
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
#  Encoder library                                                                #
#                                                                               #
********************************************************************************/

#ifndef GVIEWENCODER_H
#define GVIEWENCODER_H

#include <inttypes.h>
#include <sys/types.h>

#include "../config.h"

#ifdef HAS_AVCODEC_H
  #include <avcodec.h>
#else
  #ifdef HAS_LIBAVCODEC_AVCODEC_H
    #include <libavcodec/avcodec.h>
  #else
    #ifdef HAS_FFMPEG_AVCODEC_H
      #include <ffmpeg/avcodec.h>
    #else
      #include <libavcodec/avcodec.h>
    #endif
  #endif
#endif


/*Muxer defs*/
#define ENCODER_MUX_MKV        (0)
#define ENCODER_MUX_WEBM       (1)
#define ENCODER_MUX_AVI        (2)

#define LIBAVCODEC_VER_AT_LEAST(major,minor)  (LIBAVCODEC_VERSION_MAJOR > major || \
                                              (LIBAVCODEC_VERSION_MAJOR == major && \
                                               LIBAVCODEC_VERSION_MINOR >= minor))

#if !LIBAVCODEC_VER_AT_LEAST(53,0)
  #define AV_SAMPLE_FMT_S16 SAMPLE_FMT_S16
  #define AV_SAMPLE_FMT_FLT SAMPLE_FMT_FLT
#endif


#if !LIBAVCODEC_VER_AT_LEAST(54,25)
	#define AV_CODEC_ID_NONE CODEC_ID_NONE
	#define AV_CODEC_ID_MJPEG CODEC_ID_MJPEG
	#define AV_CODEC_ID_MPEG1VIDEO CODEC_ID_MPEG1VIDEO
	#define AV_CODEC_ID_FLV1 CODEC_ID_FLV1
	#define AV_CODEC_ID_WMV1 CODEC_ID_WMV1
	#define AV_CODEC_ID_MPEG2VIDEO CODEC_ID_MPEG2VIDEO
	#define AV_CODEC_ID_MSMPEG4V3 CODEC_ID_MSMPEG4V3
	#define AV_CODEC_ID_MPEG4 CODEC_ID_MPEG4
	#define AV_CODEC_ID_H264 CODEC_ID_H264
	#define AV_CODEC_ID_VP8 CODEC_ID_VP8
	#define AV_CODEC_ID_THEORA CODEC_ID_THEORA

	#define AV_CODEC_ID_PCM_S16LE CODEC_ID_PCM_S16LE
	#define AV_CODEC_ID_MP2 CODEC_ID_MP2
	#define AV_CODEC_ID_MP3 CODEC_ID_MP3
	#define AV_CODEC_ID_AC3 CODEC_ID_AC3
	#define AV_CODEC_ID_AAC CODEC_ID_AAC
	#define AV_CODEC_ID_VORBIS CODEC_ID_VORBIS
#endif

#define MAX_DELAYED_FRAMES 50  /*Maximum supported delayed frames*/

/*video*/
typedef struct _encoder_video_context_t
{
	AVCodec *codec;

#if LIBAVCODEC_VER_AT_LEAST(53,6)
	AVDictionary *private_options;
#endif

	AVCodecContext *codec_context;
	AVFrame *picture;
	AVPacket *outpkt;

	int monotonic_pts;

	int delayed_frames;
	int index_of_df; /*index of delayed frame pts in use;*/
	int64_t delayed_pts[MAX_DELAYED_FRAMES]; /*delayed frames pts*/
	int flush_delayed_frames;
	int flushed_buffers;
	int flush_done;

	uint8_t* tmpbuf;

	uint8_t* priv_data;

	int outbuf_size;
	uint8_t* outbuf;
} encoder_video_context_t;

/*Audio*/
typedef struct _encoder_audio_context_t
{
	AVCodec *codec;
	AVCodecContext *codec_context;
	AVFrame *frame;
	AVPacket *outpkt;

	int monotonic_pts;

	uint8_t* priv_data;
	int outbuf_size;
	uint8_t* outbuf;

	int64_t pts;

} encoder_audio_context_t;


typedef struct _encoder_context_t
{
	int muxer_id;

	int video_width;
	int video_height;
	int audio_channels;
	int audio_samprate;

	encoder_video_context_t *enc_video_ctx;

	encoder_audio_context_t *enc_audio_ctx;

} encoder_context_t;

/*
 * set verbosity
 * args:
 *   value - verbosity value
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void encoder_set_verbosity(int value);

/*
 * encoder initialization
 * args:
 *   video_codec_ind - video codec list index
 *   audio_codec_ind - audio codec list index
 *   muxer_id - file muxer:
 *        ENCODER_MUX_MKV; ENCODER_MUX_WEBM; ENCODER_MUX_AVI
 *   video_width - video frame width
 *   video_height - video frame height
 *   fps_num - fps numerator
 *   fps_den - fps denominator
 *   audio_channels- audio channels
 *   audio_samprate- audio sample rate
 *
 * asserts:
 *   none
 *
 * returns: pointer to encoder context (NULL on error)
 */
encoder_context_t *encoder_init(
	int video_codec_ind,
	int audio_codec_ind,
	int muxer_id,
	int video_width,
	int video_height,
	int fps_num,
	int fps_den,
	int audio_channels,
	int audio_samprate);

/*
 * encode video frame
 * args:
 *   encoder_ctx - pointer to encoder context
 *   yuv_frame - yuyv input frame
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int encoder_encode_video(encoder_context_t *encoder_ctx, void *yuv_frame);

/*
 * encode audio
 * args:
 *   encoder_ctx - pointer to encoder context
 *   pcm_data - audio pcm data
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int encoder_encode_audio(encoder_context_t *encoder_ctx, void *pcm);

/*
 * close and clean encoder context
 * args:
 *   encoder_ctx - pointer to encoder context data
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void encoder_close(encoder_context_t *encoder_ctx);

#endif