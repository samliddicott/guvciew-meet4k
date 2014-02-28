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

/*encoder modes*/
#define ENCODER_MODE_NONE   (0)
#define ENCODER_MODE_RAW    (1)

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

/*video buffer*/
typedef struct _video_buffer_t
{
	uint8_t *frame;  /*uncompressed*/
	int frame_size;
	int64_t timestamp;
	int flag;      /*VIDEO_BUFF_FREE | VIDEO_BUFF_USED*/
} video_buffer_t;

/*video codec properties*/
typedef struct _video_codec_t
{
	int valid;                //the encoding codec exists in libav
	const char *compressor;   //fourcc - upper case
	int mkv_4cc;              //fourcc WORD value
	const char *mkv_codec;    //mkv codecID
	void *mkv_codecPriv;      //mkv codec private data
	const char *description;  //codec description
	int fps;                  // encoder frame rate (used for time base)
	int bit_rate;             //lavc default bit rate
	int qmax;                 //lavc qmax
	int qmin;                 //lavc qmin
	int max_qdiff;            //lavc qmin
	int dia;                  //lavc dia_size
	int pre_dia;              //lavc pre_dia_size
	int pre_me;               //lavc pre_me
	int me_pre_cmp;           //lavc me_pre_cmp
	int me_cmp;               //lavc me_cmp
	int me_sub_cmp;           //lavc me_sub_cmp
	int last_pred;            //lavc last_predictor_count
	int gop_size;             //lavc gop_size
	float qcompress;          //lavc qcompress
	float qblur;              //lavc qblur
	int subq;                 //lavc subq
	int framerefs;            //lavc refs
	int codec_id;             //lavc codec_id
	const char* codec_name;   //lavc codec_name
	int mb_decision;          //lavc mb_decision
	int trellis;              //lavc trellis quantization
	int me_method;            //lavc motion estimation method
	int mpeg_quant;           //lavc mpeg quantization
	int max_b_frames;         //lavc max b frames
	int num_threads;          //lavc num threads
	int flags;                //lavc flags
	int monotonic_pts;		  //use monotonic pts instead of timestamp based
} video_codec_t;

/*audio codec properties*/
typedef struct _audio_codec_t
{
	int valid;                //the encoding codec exists in ffmpeg
	int bits;                 //bits per sample (pcm only)
	int monotonic_pts;
	uint16_t avi_4cc;         //fourcc value (4 bytes)
	const char *mkv_codec;    //mkv codecID
	const char *description;  //codec description
	int bit_rate;             //lavc default bit rate
	int codec_id;             //lavc codec_id
	const char *codec_name;   //lavc codec name
	int sample_format;        //lavc sample format
	int profile;              //for AAC only
	void *mkv_codpriv;        //pointer for codec private data
	int codpriv_size;         //size in bytes of private data
	int flags;                //lavc flags
} audio_codec_t;

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
	int outbuf_coded_size;

	int block_align;

	int64_t pts;
	int64_t dts;
	int flags;
	int duration;

} encoder_video_context_t;

/*Audio*/
typedef struct _encoder_audio_context_t
{
	AVCodec *codec;
	AVCodecContext *codec_context;
	AVFrame *frame;
	AVPacket *outpkt;

	int avi_4cc;

	int monotonic_pts;

	uint8_t* priv_data;
	int outbuf_size;
	uint8_t* outbuf;
	int outbuf_coded_size;

	int block_align;

	int64_t pts;
	int64_t dts;
	int flags;
	int duration;

} encoder_audio_context_t;


typedef struct _encoder_context_t
{
	int muxer_id;

	int input_format;

	int video_width;
	int video_height;

	int fps_num;
	int fps_den;

	int audio_channels;
	int audio_samprate;

	encoder_video_context_t *enc_video_ctx;

	encoder_audio_context_t *enc_audio_ctx;

	/*external h264 encoder data*/
	int h264_pps_size;
	uint8_t *h264_pps;
	int h264_sps_size;
	uint8_t *h264_sps;

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
 * convert yuyv to yuv420p
 * args:
 *    encoder_ctx - pointer to encoder context
 *    inp - input data (yuyv)
 *
 * asserts:
 *    encoder_ctx is not null
 *    encoder_ctx->enc_video_ctx is not null
 *
 * returns: none
 */
void yuv422to420p(encoder_context_t *encoder_ctx, uint8_t *inp);

/*
 * encoder initaliztion (first function to get called)
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void encoder_init();

/*
 * get valid video codec count
 * args:
 *   none
 *
 * asserts:
 *    none
 *
 * returns: number of valid video codecs
 */
int encoder_get_valid_video_codecs();

/*
 * get valid audio codec count
 * args:
 *   none
 *
 * asserts:
 *    none
 *
 * returns: number of valid audio codecs
 */
int encoder_get_valid_audio_codecs();

/*
 * sets the valid flag in the video codecs list
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: number of valid video codecs in list
 */
int encoder_set_valid_video_codec_list ();

/*
 * sets the valid flag in the audio codecs list
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: number of valid audio codecs in list
 */
int encoder_set_valid_audio_codec_list ();

/*
 * get video list codec description
 * args:
 *   codec_ind - codec list index
 *
 * asserts:
 *   none
 *
 * returns: list codec entry or NULL if none
 */
const char *encoder_get_video_codec_description(int codec_ind);

/*
 * get audio list codec description
 * args:
 *   codec_ind - codec list index
 *
 * asserts:
 *   none
 *
 * returns: list codec entry or NULL if none
 */
const char *encoder_get_audio_codec_description(int codec_ind);

/*
 * initialize and get the encoder context
 * args:
 *   input_format - input v4l2 format (yuyv for encoding)
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
encoder_context_t *encoder_get_context(
	int input_format,
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
 * initialization of the file muxer
 * args:
 *   encoder_ctx - pointer to encoder context
 *   filename - video filename
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void encoder_muxer_init(encoder_context_t *encoder_ctx, const char *filename);

/*
 * close the file muxer
 * args:
 *   encoder_ctx - pointer to encoder context
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void encoder_muxer_close(encoder_context_t *encoder_ctx);

/*
 * get video list codec entry for codec index
 * args:
 *   codec_ind - codec list index
 *
 * asserts:
 *   none
 *
 * returns: list codec entry or NULL if none
 */
video_codec_t *encoder_get_video_codec_defaults(int codec_ind);

/*
 * get audio list codec entry for codec index
 * args:
 *   codec_ind - codec list index
 *
 * asserts:
 *   none
 *
 * returns: audio list codec entry or NULL if none
 */
audio_codec_t *encoder_get_audio_codec_defaults(int codec_ind);

/*
 * get the mkv codec private data
 * args:
 *    codec_ind - codec list index
 *
 * asserts:
 *    none
 *
 * returns: pointer to mkvCodecPriv data
 */
void *encoder_get_video_mkvCodecPriv(int codec_ind);

/*
 * set the video codec mkv private data
 * args:
 *    encoder_ctx - pointer to encoder context
 *
 * asserts:
 *    encoder_ctx is not null
 *
 * returns: mkvCodecPriv size
 */
int encoder_set_video_mkvCodecPriv(encoder_context_t *encoder_ctx);

/*
 * get the mkv codec private data
 * args:
 *    codec_ind - codec list index
 *
 * asserts:
 *    none
 *
 * returns: pointer to mkvCodecPriv data
 */
void *encoder_get_audio_mkvCodecPriv(int codec_ind);

/*
 * set the audio codec mkv private data
 * args:
 *    encoder_ctx - pointer to encoder context
 *
 * asserts:
 *    encoder_ctx is not null
 *
 * returns: mkvCodecPriv size
 */
int encoder_set_audio_mkvCodecPriv(encoder_context_t *encoder_ctx);

/*
 * store unprocessed input video frame
 * args:
 *   frame - pointer to unprocessed frame data
 *   size - frame size (in bytes)
 *   timestamp - frame timestamp (in nanosec)
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int encoder_store_input_frame(uint8_t *frame, int size, int64_t timestamp);

/*
 * process next video frame on the ring buffer (encode and mux to file)
 * args:
 *   encoder_ctx - pointer to encoder context
 *   mode - encoder mode (ENCODER_MODE_[NONE | RAW])
 *
 * asserts:
 *   encoder_ctx is not null
 *
 * returns: error code
 */
int encoder_process_next_video_buffer(encoder_context_t *encoder_ctx, int mode);

/*
 * process audio buffer (encode and mux to file)
 * args:
 *   encoder_ctx - pointer to encoder context
 *   data - audio buffer
 *   mode - encoder mode (ENCODER_MODE_[NONE | RAW])
 *
 * asserts:
 *   encoder_ctx is not null
 *
 * returns: error code
 */
int encoder_process_audio_buffer(encoder_context_t *encoder_ctx, void *data, int mode);

/*
 * encode video frame
 * args:
 *   encoder_ctx - pointer to encoder context
 *   yuv_frame - yuyv input frame
 *
 * asserts:
 *   none
 *
 * returns: encoded buffer size
 */
int encoder_encode_video(encoder_context_t *encoder_ctx, void *yuv_frame);

/*
 * encode audio
 * args:
 *   encoder_ctx - pointer to encoder context
 *   pcm - pointer to audio pcm data
 *
 * asserts:
 *   none
 *
 * returns: encoded buffer size
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

/*
 * mux a video frame
 * args:
 *   encoder_ctx - pointer to encoder context
 *
 * asserts:
 *   encoder_ctx is not null;
 *
 * returns: none
 */
int encoder_write_video_data(encoder_context_t *encoder_ctx);

/*
 * mux a audio frame
 * args:
 *   encoder_ctx - pointer to encoder context
 *
 * asserts:
 *   encoder_ctx is not null;
 *
 * returns: none
 */
int encoder_write_audio_data(encoder_context_t *encoder_ctx);

/*
 * function to determine if enought free space is available
 * args:
 *   treshold: limit treshold in Kbytes (min. free space)
 *
 * asserts:
 *   none
 *
 * returns: 1 if still enough free space left on disk
 *          0 otherwise
 */
int encoder_disk_supervisor(int treshold, const char *path);

#endif