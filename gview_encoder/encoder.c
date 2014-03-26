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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <inttypes.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <linux/videodev2.h>
#include <errno.h>
#include <assert.h>
/* support for internationalization - i18n */
#include <locale.h>
#include <libintl.h>

#include "../config.h"
#include "gviewencoder.h"
#include "encoder.h"
#include "stream_io.h"
#include "gview.h"

#if LIBAVCODEC_VER_AT_LEAST(54,0)
#include <libavutil/channel_layout.h>
#endif

int verbosity = 0;

/*video buffer data mutex*/
static __MUTEX_TYPE mutex;
#define __PMUTEX &mutex

static int valid_video_codecs = 0;
static int valid_audio_codecs = 0;

static int64_t last_video_pts = 0;
static int64_t last_audio_pts = 0;
static int64_t reference_pts  = 0;

static int video_frame_max_size = 0;

static int video_ring_buffer_size = 0;
static video_buffer_t *video_ring_buffer = NULL;
static int video_read_index = 0;
static int video_write_index = 0;
static int video_scheduler = 0;

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
void encoder_set_verbosity(int value)
{
	verbosity = value;
}

/*
 * check that a given sample format is supported by the encoder
 * args:
 *    codec - pointer to AVCodec
 *    sample_fmt - audio sample format
 *
 * assertions:
 *    none
 *
 * returns: 1 - sample format is supported; 0 - is not supported
 */
static int encoder_check_audio_sample_fmt(AVCodec *codec, enum AVSampleFormat sample_fmt)
{
	const enum AVSampleFormat *p = codec->sample_fmts;

	while (*p != AV_SAMPLE_FMT_NONE)
	{
		if (*p == sample_fmt)
			return 1;
		p++;
	}
	return 0;
}

/*
 * allocate video ring buffer
 * args:
 *   video_width - video frame width (in pixels)
 *   video_height - video frame height (in pixels)
 *   fps_den - frames per sec (denominator)
 *   fps_num - frames per sec (numerator)
 *
 * asserts:
 *   none
 *
 * returns: none
 */
static void encoder_alloc_video_ring_buffer(
	int video_width,
	int video_height,
	int fps_den,
	int fps_num)
{
	video_ring_buffer_size = (fps_den * 3) / (fps_num * 2); /* 1.5 sec */
	if(video_ring_buffer_size < 15)
		video_ring_buffer_size = 15; /*at least 15 frames buffer*/
	video_ring_buffer = calloc(video_ring_buffer_size, sizeof(video_buffer_t));

	/*Max: (yuyv) 2 bytes per pixel*/
	video_frame_max_size = video_width * video_height * 2;
	int i = 0;
	for(i = 0; i < video_ring_buffer_size; ++i)
	{
		video_ring_buffer[i].frame = calloc(video_frame_max_size, sizeof(uint8_t));
		video_ring_buffer[i].flag = VIDEO_BUFF_FREE;
	}
}

/*
 * clean video ring buffer
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
static void encoder_clean_video_ring_buffer()
{
	if(!video_ring_buffer)
		return;

	int i = 0;
	for(i = 0; i < video_ring_buffer_size; ++i)
	{
		/*Max: (yuyv) 2 bytes per pixel*/
		free(video_ring_buffer[i].frame);
	}
	free(video_ring_buffer);
	video_ring_buffer = NULL;
}

/*
 * video encoder initialization
 * args:
 *   video_codec_ind - video codec list index
 *   video_width - video frame width
 *   video_height - video frame height
 *   fps_num - fps numerator
 *   fps_den - fps denominator
 *
 * asserts:
 *   none
 *
 * returns: pointer to encoder video context (NULL on none)
 */
static encoder_video_context_t *encoder_video_init(
	int video_codec_ind,
	int input_format,
	int video_width,
	int video_height,
	int fps_num,
	int fps_den)
{
	if(video_codec_ind < 0)
	{
		if(verbosity > 0)
			printf("ENCODER: no video codec set - using raw (direct input)\n");

		video_codec_ind = 0;
	}

	video_codec_t *video_defaults = encoder_get_video_codec_defaults(video_codec_ind);

	if(!video_defaults)
	{
		fprintf(stderr, "ENCODER: defaults for video codec index %i not found: using raw (direct input)\n",
			video_codec_ind);
		video_codec_ind = 0;
		video_defaults = encoder_get_video_codec_defaults(video_codec_ind);
		if(!video_defaults)
		{
			/*should never happen*/
			fprintf(stderr, "ENCODER: defaults for raw video not found\n");
			return NULL;
		}
	}

	encoder_video_context_t *enc_video_ctx = calloc(1, sizeof(encoder_video_context_t));

	/*raw input - don't set a codec but set the proper codec 4cc*/
	if(video_codec_ind == 0)
	{
		switch(input_format)
		{
			case V4L2_PIX_FMT_MJPEG:
				strncpy(video_defaults->compressor, "MJPG", 5);
				video_defaults->mkv_4cc = v4l2_fourcc('M','J','P','G');
				strncpy(video_defaults->mkv_codec, "V_MS/VFW/FOURCC", 25);
				enc_video_ctx->outbuf_size = video_width * video_height / 2;
				enc_video_ctx->outbuf = calloc(enc_video_ctx->outbuf_size, sizeof(uint8_t));
				break;

			case V4L2_PIX_FMT_H264:
				strncpy(video_defaults->compressor, "H264", 5);
				video_defaults->mkv_4cc = v4l2_fourcc('H','2','6','4');
				strncpy(video_defaults->mkv_codec, "V_MPEG4/ISO/AVC", 25);
				enc_video_ctx->outbuf_size = video_width * video_height / 2;
				enc_video_ctx->outbuf = calloc(enc_video_ctx->outbuf_size, sizeof(uint8_t));
				break;

			default:
				strncpy(video_defaults->compressor, "YUY2", 5);
				video_defaults->mkv_4cc = v4l2_fourcc('Y','U','Y','2');
				strncpy(video_defaults->mkv_codec, "V_MS/VFW/FOURCC", 25);
				enc_video_ctx->outbuf_size = video_width * video_height * 2;
				enc_video_ctx->outbuf = calloc(enc_video_ctx->outbuf_size, sizeof(uint8_t));
				break;
		}

		return (enc_video_ctx);
	}

	/*
	 * find the video encoder
	 *   try specific codec (by name)
	 */
	enc_video_ctx->codec = avcodec_find_encoder_by_name(video_defaults->codec_name);
	/*if it fails try any codec with matching AV_CODEC_ID*/
	if(!enc_video_ctx->codec)
		enc_video_ctx->codec = avcodec_find_encoder(video_defaults->codec_id);

	if(!enc_video_ctx->codec)
	{
		fprintf(stderr, "ENCODER: libav video codec (%i) not found\n",video_defaults->codec_id);
		free(enc_video_ctx);
		return(NULL);
	}

#if LIBAVCODEC_VER_AT_LEAST(53,6)

	enc_video_ctx->codec_context = avcodec_alloc_context3(enc_video_ctx->codec);

	avcodec_get_context_defaults3 (
			enc_video_ctx->codec_context,
			enc_video_ctx->codec);
#else
	enc_video_ctx->codec_context = avcodec_alloc_context();
#endif

	if(!enc_video_ctx->codec_context)
	{
		fprintf(stderr, "ENCODER: couldn't allocate video codec context\n");
		free(enc_video_ctx);
		return(NULL);
	}

	/*set codec defaults*/
	enc_video_ctx->codec_context->bit_rate = video_defaults->bit_rate;
	enc_video_ctx->codec_context->width = video_width;
	enc_video_ctx->codec_context->height = video_height;

	enc_video_ctx->codec_context->flags |= video_defaults->flags;
	if (video_defaults->num_threads > 0)
		enc_video_ctx->codec_context->thread_count = video_defaults->num_threads;
	/*
	 * mb_decision:
	 * 0 (FF_MB_DECISION_SIMPLE) Use mbcmp (default).
	 * 1 (FF_MB_DECISION_BITS)   Select the MB mode which needs the fewest bits (=vhq).
	 * 2 (FF_MB_DECISION_RD)     Select the MB mode which has the best rate distortion.
	 */
	enc_video_ctx->codec_context->mb_decision = video_defaults->mb_decision;
	/*use trellis quantization*/
	enc_video_ctx->codec_context->trellis = video_defaults->trellis;

	/*motion estimation method (epzs)*/
	enc_video_ctx->codec_context->me_method = video_defaults->me_method;

	enc_video_ctx->codec_context->dia_size = video_defaults->dia;
	enc_video_ctx->codec_context->pre_dia_size = video_defaults->pre_dia;
	enc_video_ctx->codec_context->pre_me = video_defaults->pre_me;
	enc_video_ctx->codec_context->me_pre_cmp = video_defaults->me_pre_cmp;
	enc_video_ctx->codec_context->me_cmp = video_defaults->me_cmp;
	enc_video_ctx->codec_context->me_sub_cmp = video_defaults->me_sub_cmp;
	enc_video_ctx->codec_context->me_subpel_quality = video_defaults->subq; //NEW
	enc_video_ctx->codec_context->refs = video_defaults->framerefs;         //NEW
	enc_video_ctx->codec_context->last_predictor_count = video_defaults->last_pred;

	enc_video_ctx->codec_context->mpeg_quant = video_defaults->mpeg_quant; //h.263
	enc_video_ctx->codec_context->qmin = video_defaults->qmin;             // best detail allowed - worst compression
	enc_video_ctx->codec_context->qmax = video_defaults->qmax;             // worst detail allowed - best compression
	enc_video_ctx->codec_context->max_qdiff = video_defaults->max_qdiff;
	enc_video_ctx->codec_context->max_b_frames = video_defaults->max_b_frames;

	enc_video_ctx->codec_context->qcompress = video_defaults->qcompress;
	enc_video_ctx->codec_context->qblur = video_defaults->qblur;
	enc_video_ctx->codec_context->strict_std_compliance = FF_COMPLIANCE_NORMAL;
	enc_video_ctx->codec_context->codec_id = video_defaults->codec_id;

#if !LIBAVCODEC_VER_AT_LEAST(53,0)
#define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
#endif
	enc_video_ctx->codec_context->codec_type = AVMEDIA_TYPE_VIDEO;

	enc_video_ctx->codec_context->pix_fmt = PIX_FMT_YUV420P; //only yuv420p available for mpeg
	if(video_defaults->fps)
		enc_video_ctx->codec_context->time_base = (AVRational){1, video_defaults->fps}; //use codec properties fps
	else if (fps_den >= 5)
		enc_video_ctx->codec_context->time_base = (AVRational){fps_num, fps_den}; //default fps (for gspca this is 1/1)
	else
		enc_video_ctx->codec_context->time_base = (AVRational){1,15}; //fallback to 15 fps (e.g gspca)

    if(video_defaults->gop_size > 0)
        enc_video_ctx->codec_context->gop_size = video_defaults->gop_size;
    else
        enc_video_ctx->codec_context->gop_size = enc_video_ctx->codec_context->time_base.den;

	if(video_defaults->codec_id == AV_CODEC_ID_H264)
	{
	   enc_video_ctx->codec_context->me_range = 16;
	    //the first compressed frame will be empty (1 frame out of sync)
	    //but avoids x264 warning on lookaheadless mb-tree
#if LIBAVCODEC_VER_AT_LEAST(53,6)
	    av_dict_set(&enc_video_ctx->private_options, "rc_lookahead", "1", 0);
#else
	    enc_video_ctx->codec_context->rc_lookahead=1;
#endif
	}

	/* open codec*/
#if LIBAVCODEC_VER_AT_LEAST(53,6)
	if (avcodec_open2(
		enc_video_ctx->codec_context,
		enc_video_ctx->codec,
		&enc_video_ctx->private_options) < 0)
#else
	if (avcodec_open(
		enc_video_ctx->codec_context,
		enc_video_ctx->codec) < 0)
#endif
	{
		fprintf(stderr, "ENCODER: could not open video codec (%s)\n", video_defaults->codec_name);
		free(enc_video_ctx->codec_context);
		free(enc_video_ctx);
		return(NULL);
	}

	enc_video_ctx->picture= avcodec_alloc_frame();
	enc_video_ctx->picture->pts = 0;

	//alloc tmpbuff (yuv420p)
	enc_video_ctx->tmpbuf = calloc((video_width * video_height * 3)/2, sizeof(uint8_t));
	//alloc outbuf
	enc_video_ctx->outbuf_size = 240000;//1792
	enc_video_ctx->outbuf = calloc(enc_video_ctx->outbuf_size, sizeof(uint8_t));

	enc_video_ctx->delayed_frames = 0;
	enc_video_ctx->index_of_df = -1;

	enc_video_ctx->flushed_buffers = 0;
	enc_video_ctx->flush_delayed_frames = 0;
	enc_video_ctx->flush_done = 0;

	return (enc_video_ctx);
}

/*
 * audio encoder initialization
 * args:
 *   audio_codec_ind - audio codec list index
 *   audio_channels - audio channels
 *   audio_samprate - audio sample rate
 *
 * asserts:
 *   none
 *
 * returns: pointer to encoder audio context (NULL on none)
 */
static encoder_audio_context_t *encoder_audio_init(
	int audio_codec_ind,
	int audio_channels,
	int audio_samprate)
{

	if(audio_codec_ind < 0)
	{
		if(verbosity > 0)
			printf("ENCODER: no audio codec set\n");

		return NULL;
	}

	audio_codec_t *audio_defaults = encoder_get_audio_codec_defaults(audio_codec_ind);

	if(!audio_defaults)
	{
		fprintf(stderr, "ENCODER: defaults for audio codec index %i not found\n", audio_codec_ind);
		return NULL;
	}

	encoder_audio_context_t *enc_audio_ctx = calloc(1, sizeof(encoder_audio_context_t));
	/*
	 * find the audio encoder
	 *   try specific codec (by name)
	 */
	enc_audio_ctx->codec = avcodec_find_encoder_by_name(audio_defaults->codec_name);
	/*if it fails try any codec with matching AV_CODEC_ID*/
	if(!enc_audio_ctx->codec)
		enc_audio_ctx->codec = avcodec_find_encoder(audio_defaults->codec_id);

	if(!enc_audio_ctx->codec)
	{
		fprintf(stderr, "ENCODER: audio codec (%i) not found\n",audio_defaults->codec_id);
		free(enc_audio_ctx);
		return NULL;
	}

#if LIBAVCODEC_VER_AT_LEAST(53,6)
	enc_audio_ctx->codec_context = avcodec_alloc_context3(enc_audio_ctx->codec);
	avcodec_get_context_defaults3 (enc_audio_ctx->codec_context, enc_audio_ctx->codec);
#else
	enc_audio_ctx->codec_context = avcodec_alloc_context();
#endif

	if(!enc_audio_ctx->codec_context)
	{
		fprintf(stderr, "ENCODER: couldn't allocate audio codec context\n");
		free(enc_audio_ctx);
		return(NULL);
	}

	/*defaults*/
	enc_audio_ctx->avi_4cc = audio_defaults->avi_4cc;

	enc_audio_ctx->codec_context->bit_rate = audio_defaults->bit_rate;
	enc_audio_ctx->codec_context->profile = audio_defaults->profile; /*for AAC*/

	enc_audio_ctx->codec_context->flags |= audio_defaults->flags;

	enc_audio_ctx->codec_context->sample_rate = audio_samprate;
	enc_audio_ctx->codec_context->channels = audio_channels;

#if LIBAVCODEC_VER_AT_LEAST(53,34)
	if(audio_channels < 2)
		enc_audio_ctx->codec_context->channel_layout = AV_CH_LAYOUT_MONO;
	else
		enc_audio_ctx->codec_context->channel_layout = AV_CH_LAYOUT_STEREO;
#endif

	enc_audio_ctx->codec_context->cutoff = 0; /*automatic*/

    enc_audio_ctx->codec_context->codec_id = audio_defaults->codec_id;

#if !LIBAVCODEC_VER_AT_LEAST(53,0)
#define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#endif
	enc_audio_ctx->codec_context->codec_type = AVMEDIA_TYPE_AUDIO;

	/*check if codec supports sample format*/
	if (!encoder_check_audio_sample_fmt(enc_audio_ctx->codec, audio_defaults->sample_format))
	{
		switch(audio_defaults->sample_format)
		{
			case AV_SAMPLE_FMT_S16:
				if (encoder_check_audio_sample_fmt(enc_audio_ctx->codec, AV_SAMPLE_FMT_S16P))
				{
					fprintf(stderr, "ENCODER: changing sample format (S16 -> S16P)\n");
					audio_defaults->sample_format = AV_SAMPLE_FMT_S16P;
				}
				else if (encoder_check_audio_sample_fmt(enc_audio_ctx->codec, AV_SAMPLE_FMT_FLT))
				{
					fprintf(stderr, "ENCODER: changing sample format (S16 -> FLT)\n");
					audio_defaults->sample_format = AV_SAMPLE_FMT_FLT;
				}
				else if (encoder_check_audio_sample_fmt(enc_audio_ctx->codec, AV_SAMPLE_FMT_FLTP))
				{
					fprintf(stderr, "ENCODER: changing sample format (S16 -> FLTP)\n");
					audio_defaults->sample_format = AV_SAMPLE_FMT_FLTP;
				}
				else
				{
					fprintf(stderr, "ENCODER: could not open audio codec: no supported sample format\n");
					free(enc_audio_ctx->codec_context);
					free(enc_audio_ctx);
					return NULL;
				}
				break;

			case AV_SAMPLE_FMT_FLT:
				if (encoder_check_audio_sample_fmt(enc_audio_ctx->codec, AV_SAMPLE_FMT_S16))
				{
					fprintf(stderr, "ENCODER: changing sample format (FLT -> S16)\n");
					audio_defaults->sample_format = AV_SAMPLE_FMT_S16;
				}
				else if (encoder_check_audio_sample_fmt(enc_audio_ctx->codec, AV_SAMPLE_FMT_S16P))
				{
					fprintf(stderr, "ENCODER: changing sample format (FLT -> S16P)\n");
					audio_defaults->sample_format = AV_SAMPLE_FMT_S16P;
				}
				else if (encoder_check_audio_sample_fmt(enc_audio_ctx->codec, AV_SAMPLE_FMT_FLTP))
				{
					fprintf(stderr, "ENCODER: changing sample format (FLT -> FLTP)\n");
					audio_defaults->sample_format = AV_SAMPLE_FMT_FLTP;
				}
				else
				{
					fprintf(stderr, "ENCODER: could not open audio codec: no supported sample format\n");
					free(enc_audio_ctx->codec_context);
					free(enc_audio_ctx);
					return NULL;
				}
				break;

			case AV_SAMPLE_FMT_FLTP:
				if (encoder_check_audio_sample_fmt(enc_audio_ctx->codec, AV_SAMPLE_FMT_S16))
				{
					fprintf(stderr, "ENCODER: changing sample format (FLTP -> S16)\n");
					audio_defaults->sample_format = AV_SAMPLE_FMT_S16;
				}
				else if (encoder_check_audio_sample_fmt(enc_audio_ctx->codec, AV_SAMPLE_FMT_S16P))
				{
					fprintf(stderr, "ENCODER: changing sample format (FLTP -> S16P)\n");
					audio_defaults->sample_format = AV_SAMPLE_FMT_S16P;
				}
				else if (encoder_check_audio_sample_fmt(enc_audio_ctx->codec, AV_SAMPLE_FMT_FLT))
				{
					fprintf(stderr, "ENCODER: changing sample format (FLTP -> FLT)\n");
					audio_defaults->sample_format = AV_SAMPLE_FMT_FLT;
				}
				else
				{
					fprintf(stderr, "ENCODER: could not open audio codec: no supported sample format\n");
					free(enc_audio_ctx->codec_context);
					free(enc_audio_ctx);
					return NULL;
				}
				break;
		}
	}

	enc_audio_ctx->codec_context->sample_fmt = audio_defaults->sample_format;

	/* open codec*/
#if LIBAVCODEC_VER_AT_LEAST(53,6)
	if (avcodec_open2(
		enc_audio_ctx->codec_context,
		enc_audio_ctx->codec, NULL) < 0)
#else
	if (avcodec_open(
		enc_audio_ctx->codec_context,
		enc_audio_ctx->codec) < 0)
#endif
	{
		fprintf(stderr, "ENCODER: could not open audio codec\n");
		free(enc_audio_ctx->codec_context);
		free(enc_audio_ctx);
		return NULL;
	}

	/* the codec gives us the frame size, in samples */
	int frame_size = enc_audio_ctx->codec_context->frame_size;
	if(frame_size <= 0)
	{
		frame_size = 1152; /*default value*/
		enc_audio_ctx->codec_context->frame_size = frame_size;
	}
	if(verbosity > 0)
		printf("ENCODER: Audio frame size is %d frames for selected codec\n", frame_size);

	enc_audio_ctx->monotonic_pts = audio_defaults->monotonic_pts;

	/*alloc outbuf*/
	enc_audio_ctx->outbuf_size = 240000;
	enc_audio_ctx->outbuf = calloc(enc_audio_ctx->outbuf_size, sizeof(uint8_t));

#if LIBAVCODEC_VER_AT_LEAST(53,34)
	enc_audio_ctx->frame= avcodec_alloc_frame();
	avcodec_get_frame_defaults(enc_audio_ctx->frame);

	enc_audio_ctx->frame->nb_samples = frame_size;
	enc_audio_ctx->frame->format = audio_defaults->sample_format;

#if LIBAVCODEC_VER_AT_LEAST(54,0)
	enc_audio_ctx->frame->channel_layout = enc_audio_ctx->codec_context->channel_layout;
#endif

#endif

	return (enc_audio_ctx);
}

/*
 * get an estimated write loop sleep time to avoid a ring buffer overrun
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: estimate sleep time (nanosec)
 */
uint32_t encoder_buff_scheduler()
{
	int diff_ind = 0;
	uint32_t sched_time = 0; /*in milisec*/

	__LOCK_MUTEX( __PMUTEX );
	/* try to balance buffer overrun in read/write operations */
	if(video_write_index >= video_read_index)
		diff_ind = video_write_index - video_read_index;
	else
		diff_ind = (video_ring_buffer_size - video_read_index) + video_write_index;
	__UNLOCK_MUTEX( __PMUTEX );

	int th = (int) lround((double) video_ring_buffer_size * 0.65); /*65% full*/
	int th1 =(int) lround((double) video_ring_buffer_size * 0.85); /*85% full*/

	/**/
	if(diff_ind >= th1)
		sched_time = (uint32_t) lround((double) ((diff_ind * 320) / video_ring_buffer_size) - 192);
	else if (diff_ind >= th)
		sched_time = (uint32_t) lround((double) (diff_ind * 64) / video_ring_buffer_size);


	/*clip*/
	if(sched_time < 0) sched_time = 0; /*clip to positive values just in case*/
	if(sched_time > 1000)
		sched_time = 1000; /*1 sec*/

	return (sched_time * 1E6); /*return in nanosec*/
}

/*
 * encoder initaliztion
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void encoder_init()
{

#if !LIBAVCODEC_VER_AT_LEAST(53,34)
	avcodec_init();
#endif
	/* register all the codecs (you can also register only the codec
	 * you wish to have smaller code)
	 */
	avcodec_register_all();

	valid_video_codecs = encoder_set_valid_video_codec_list ();
	valid_audio_codecs = encoder_set_valid_audio_codec_list ();

}

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
int encoder_get_valid_video_codecs()
{
	return valid_video_codecs;
}

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
int encoder_get_valid_audio_codecs()
{
	return valid_audio_codecs;
}

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
	int audio_samprate)
{
	encoder_context_t *encoder_ctx = calloc(1, sizeof(encoder_context_t));

	if(!encoder_ctx)
	{
		fprintf(stderr, "ENCODER: couldn't allocate encoder context\n");
		return NULL;
	}

	encoder_ctx->input_format = input_format;

	encoder_ctx->video_codec_ind = video_codec_ind;
	encoder_ctx->audio_codec_ind = audio_codec_ind;

	encoder_ctx->muxer_id = muxer_id;

	encoder_ctx->video_width = video_width;
	encoder_ctx->video_height = video_height;

	encoder_ctx->fps_num = fps_num;
	encoder_ctx->fps_den = fps_den;

	encoder_ctx->audio_channels = audio_channels;
	encoder_ctx->audio_samprate = audio_samprate;

	/******************* video **********************/
	encoder_ctx->enc_video_ctx = encoder_video_init(
		video_codec_ind,
		input_format,
		video_width,
		video_height,
		fps_num,
		fps_den);

	/******************* audio **********************/
	encoder_ctx->enc_audio_ctx = encoder_audio_init(
		audio_codec_ind,
		audio_channels,
		audio_samprate);

	if(!encoder_ctx->enc_audio_ctx)
		encoder_ctx->audio_channels = 0; /*no audio*/

	/****************** ring buffer *****************/
	encoder_alloc_video_ring_buffer(
		video_width,
		video_height,
		fps_den,
		fps_num);

	return encoder_ctx;
}

/*
 * store unprocessed input video frame in video ring buffer
 * args:
 *   frame - pointer to unprocessed frame data
 *   size - frame size (in bytes)
 *   timestamp - frame timestamp (in nanosec)
 *   isKeyframe - flag if it's a key(IDR) frame
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int encoder_add_video_frame(uint8_t *frame, int size, int64_t timestamp, int isKeyframe)
{
	if(!video_ring_buffer)
		return -1;

	int ret = 0;

	if (reference_pts == 0)
	{
		reference_pts = timestamp; /*first frame ts*/
		if(verbosity > 0)
			printf("ENCODER: ref ts = %" PRId64 "\n", timestamp);
	}

	int64_t pts = timestamp - reference_pts;

	__LOCK_MUTEX( __PMUTEX );
	int flag = video_ring_buffer[video_write_index].flag;
	__UNLOCK_MUTEX( __PMUTEX );

	if(flag != VIDEO_BUFF_FREE)
	{
		fprintf(stderr, "ENCODER: video ring buffer full - dropping frame\n");
		return -1;
	}

	/*clip*/
	if(size > video_frame_max_size)
	{
		fprintf(stderr, "ENCODER: frame (%i bytes) larger than buffer (%i bytes): clipping\n",
			size, video_frame_max_size);

		size = video_frame_max_size;
	}
	memcpy(video_ring_buffer[video_write_index].frame, frame, size);
	video_ring_buffer[video_write_index].frame_size = size;
	video_ring_buffer[video_write_index].timestamp = pts;
	video_ring_buffer[video_write_index].keyframe = isKeyframe;

	__LOCK_MUTEX( __PMUTEX );
	video_ring_buffer[video_write_index].flag = VIDEO_BUFF_USED;
	NEXT_IND(video_write_index, video_ring_buffer_size);
	__UNLOCK_MUTEX( __PMUTEX );

	return 0;
}

/*
 * process next video frame on the ring buffer (encode and mux to file)
 * args:
 *   encoder_ctx - pointer to encoder context
 *
 * asserts:
 *   encoder_ctx is not null
 *
 * returns: error code
 */
int encoder_process_next_video_buffer(encoder_context_t *encoder_ctx)
{
	/*assertions*/
	assert(encoder_ctx != NULL);

	__LOCK_MUTEX( __PMUTEX );

	int flag = video_ring_buffer[video_read_index].flag;

	__UNLOCK_MUTEX ( __PMUTEX );

	if(flag == VIDEO_BUFF_FREE)
		return 1; /*all done*/

	/*timestamp is zero indexed*/
	encoder_ctx->enc_video_ctx->pts = video_ring_buffer[video_read_index].timestamp;

	/*raw (direct input)*/
	if(encoder_ctx->video_codec_ind == 0)
	{
		/*outbuf_coded_size must already be set*/
		encoder_ctx->enc_video_ctx->outbuf_coded_size = video_ring_buffer[video_read_index].frame_size;
		if(video_ring_buffer[video_read_index].keyframe)
			encoder_ctx->enc_video_ctx->flags |= AV_PKT_FLAG_KEY;
	}

	encoder_encode_video(encoder_ctx, video_ring_buffer[video_read_index].frame);


	/*mux the frame*/
	__LOCK_MUTEX( __PMUTEX );

	video_ring_buffer[video_read_index].flag = VIDEO_BUFF_FREE;
	NEXT_IND(video_read_index, video_ring_buffer_size);

	__UNLOCK_MUTEX ( __PMUTEX );

	encoder_write_video_data(encoder_ctx);

	return 0;
}

/*
 * process all used video frames from buffer
  * args:
 *   encoder_ctx - pointer to encoder context
 *
 * asserts:
 *   encoder_ctx is not null
 *
 * returns: error code
 */
int encoder_flush_video_buffer(encoder_context_t *encoder_ctx)
{
	/*assertions*/
	assert(encoder_ctx != NULL);

	__LOCK_MUTEX( __PMUTEX );
	int flag = video_ring_buffer[video_read_index].flag;
	__UNLOCK_MUTEX ( __PMUTEX );

	int buffer_count = video_ring_buffer_size;

	while(flag != VIDEO_BUFF_FREE && buffer_count > 0)
	{
		buffer_count--;

		/*timestamp is zero indexed*/
		encoder_ctx->enc_video_ctx->pts = video_ring_buffer[video_read_index].timestamp;

		/*raw (direct input)*/
		if(encoder_ctx->video_codec_ind == 0)
		{
			/*outbuf_coded_size must already be set*/
			encoder_ctx->enc_video_ctx->outbuf_coded_size = video_ring_buffer[video_read_index].frame_size;
			if(video_ring_buffer[video_read_index].keyframe)
				encoder_ctx->enc_video_ctx->flags |= AV_PKT_FLAG_KEY;
		}

		encoder_encode_video(encoder_ctx, video_ring_buffer[video_read_index].frame);

		/*mux the frame*/
		__LOCK_MUTEX( __PMUTEX );

		video_ring_buffer[video_read_index].flag = VIDEO_BUFF_FREE;
		NEXT_IND(video_read_index, video_ring_buffer_size);

		__UNLOCK_MUTEX ( __PMUTEX );

		encoder_write_video_data(encoder_ctx);

		/*get next buffer flag*/
		__LOCK_MUTEX( __PMUTEX );
		flag = video_ring_buffer[video_read_index].flag;
		__UNLOCK_MUTEX ( __PMUTEX );
	}

	if(!buffer_count)
	{
		fprintf(stderr, "ENCODER: (flush video buffer) max processed buffers reached\n");
		return -1;
	}

	return 0;
}

/*
 * process audio frame (encode and mux to file)
 * args:
 *   encoder_ctx - pointer to encoder context
 *   data - audio buffer
 *
 * asserts:
 *   encoder_ctx is not null
 *
 * returns: error code
 */
int encoder_process_audio_buffer(encoder_context_t *encoder_ctx, void *data)
{
	/*assertions*/
	assert(encoder_ctx != NULL);

	if(encoder_ctx->enc_audio_ctx == NULL ||
		encoder_ctx->audio_channels <= 0)
		return -1;

	encoder_encode_audio(encoder_ctx, data);

	int ret = encoder_write_audio_data(encoder_ctx);

	return ret;
}

/*
 * encode video frame
 * args:
 *   encoder_ctx - pointer to encoder context
 *   input_frame - pointer to frame data
 *
 * asserts:
 *   encoder_ctx is not null
 *
 * returns: encoded buffer size
 */
int encoder_encode_video(encoder_context_t *encoder_ctx, void *input_frame)
{
	/*assertions*/
	assert(encoder_ctx != NULL);

	encoder_video_context_t *enc_video_ctx = encoder_ctx->enc_video_ctx;

	int outsize = 0;

	if(!enc_video_ctx)
	{
		if(verbosity > 1)
			printf("ENCODER: video encoder not set\n");
		encoder_ctx->enc_video_ctx->outbuf_coded_size = outsize;
		return outsize;
	}

	/*raw - direct input no software encoding*/
	if(encoder_ctx->video_codec_ind == 0)
	{
		/*outbuf_coded_size must already be set*/
		outsize = enc_video_ctx->outbuf_coded_size;
		memcpy(enc_video_ctx->outbuf, input_frame, outsize);
		enc_video_ctx->flags = 0;
		/*enc_video_ctx->flags must be set*/
		enc_video_ctx->dts = AV_NOPTS_VALUE;
		enc_video_ctx->duration = enc_video_ctx->pts - last_video_pts;
		last_video_pts = enc_video_ctx->pts;
		return (outsize);
	}

	/*convert default yuyv to y420p (libav input format)*/
	yuv422to420p(encoder_ctx, input_frame);

	if(!enc_video_ctx->monotonic_pts) //generate a real pts based on the frame timestamp
		enc_video_ctx->picture->pts += ((enc_video_ctx->pts - last_video_pts)/1000) * 90;
	else  /*generate a true monotonic pts based on the codec fps*/
		enc_video_ctx->picture->pts +=
			(enc_video_ctx->codec_context->time_base.num * 1000 / enc_video_ctx->codec_context->time_base.den) * 90;

	if(enc_video_ctx->flush_delayed_frames)
	{
		//pkt.size = 0;
		if(!enc_video_ctx->flushed_buffers)
		{
			avcodec_flush_buffers(enc_video_ctx->codec_context);
			enc_video_ctx->flushed_buffers = 1;
		}
 	}
#if LIBAVCODEC_VER_AT_LEAST(54,01)
	AVPacket pkt;
    int got_packet = 0;
    av_init_packet(&pkt);
	pkt.data = enc_video_ctx->outbuf;
	pkt.size = enc_video_ctx->outbuf_size;

	int ret = 0;
    //if(enc_video_ctx->outbuf_size < FF_MIN_BUFFER_SIZE)
    //{
	//	av_log(avctx, AV_LOG_ERROR, "buffer smaller than minimum size\n");
    //    return -1;
    //}
 	if(!enc_video_ctx->flush_delayed_frames)
    	ret = avcodec_encode_video2(
			enc_video_ctx->codec_context,
			&pkt,
			enc_video_ctx->picture,
			&got_packet);
   	else
   		ret = avcodec_encode_video2(
			enc_video_ctx->codec_context,
			&pkt, NULL, /*NULL flushes the encoder buffers*/
			&got_packet);

    if (!ret && got_packet && enc_video_ctx->codec_context->coded_frame)
    {
		/* Do we really need to set this ???*/
    	enc_video_ctx->codec_context->coded_frame->pts = pkt.pts;
        enc_video_ctx->codec_context->coded_frame->key_frame = !!(pkt.flags & AV_PKT_FLAG_KEY);
    }

	enc_video_ctx->dts = pkt.dts;
	enc_video_ctx->flags = pkt.flags;
	enc_video_ctx->duration = pkt.duration;

    /* free any side data since we cannot return it */
    if (pkt.side_data_elems > 0)
    {
    	int i;
        for (i = 0; i < pkt.side_data_elems; i++)
        	av_free(pkt.side_data[i].data);
        av_freep(&pkt.side_data);
        pkt.side_data_elems = 0;
    }

    outsize = pkt.size;
#else
	if(!enc_video_ctx->flush_delayed_frames)
		outsize = avcodec_encode_video(
			enc_video_ctx->codec_context,
			enc_video_ctx->outbuf,
			enc_video_ctx->outbuf_size,
			enc_video_ctx->picture);
	else
		outsize = avcodec_encode_video(
			enc_video_ctx->codec_context,
			enc_video_ctx->outbuf,
			enc_video_ctx->outbuf_size,
			NULL); /*NULL flushes the encoder buffers*/

	enc_video_ctx->flags = 0;
	if (enc_video_ctx->codec_context->coded_frame->key_frame)
		enc_video_ctx->flags |= AV_PKT_FLAG_KEY;
	enc_video_ctx->dts = AV_NOPTS_VALUE;
	enc_video_ctx->duration = enc_video_ctx->pts - last_video_pts;
#endif

	last_video_pts = enc_video_ctx->pts;

	if(enc_video_ctx->flush_delayed_frames && outsize == 0)
    	enc_video_ctx->flush_done = 1;

	if(outsize == 0 && enc_video_ctx->index_of_df < 0)
	{
	    enc_video_ctx->delayed_pts[enc_video_ctx->delayed_frames] = enc_video_ctx->pts;
	    enc_video_ctx->delayed_frames++;
	    if(enc_video_ctx->delayed_frames > MAX_DELAYED_FRAMES)
	    {
	    	enc_video_ctx->delayed_frames = MAX_DELAYED_FRAMES;
	    	printf("ENCODER: Maximum of %i delayed video frames reached...\n", MAX_DELAYED_FRAMES);
	    }
	}
	else
	{
		if(enc_video_ctx->delayed_frames > 0)
		{
			if(enc_video_ctx->index_of_df < 0)
			{
				enc_video_ctx->index_of_df = 0;
				printf("ENCODER: video codec is using %i delayed video frames\n", enc_video_ctx->delayed_frames);
			}
			int64_t my_pts = enc_video_ctx->pts;
			enc_video_ctx->pts = enc_video_ctx->delayed_pts[enc_video_ctx->index_of_df];
			enc_video_ctx->delayed_pts[enc_video_ctx->index_of_df] = my_pts;
			enc_video_ctx->index_of_df++;
			if(enc_video_ctx->index_of_df >= enc_video_ctx->delayed_frames)
				enc_video_ctx->index_of_df = 0;
		}
	}

	encoder_ctx->enc_video_ctx->outbuf_coded_size = outsize;
	return (outsize);
}

/*
 * encode audio
 * args:
 *   encoder_ctx - pointer to encoder context
 *   audio_data - pointer to audio pcm data
 *
 * asserts:
 *   encoder_ctx is not null
 *
 * returns: encoded buffer size
 */
int encoder_encode_audio(encoder_context_t *encoder_ctx, void *audio_data)
{
	/*assertions*/
	assert(encoder_ctx != NULL);

	encoder_audio_context_t *enc_audio_ctx = encoder_ctx->enc_audio_ctx;

	int outsize = 0;

	if(!enc_audio_ctx)
	{
		if(verbosity > 1)
			printf("ENCODER: audio encoder not set\n");
		enc_audio_ctx->outbuf_coded_size = outsize;
		return outsize;
	}

	/* encode the audio */
#if LIBAVCODEC_VER_AT_LEAST(53,34)
	AVPacket pkt;
	int got_packet;
	av_init_packet(&pkt);
	pkt.data = enc_audio_ctx->outbuf;
	pkt.size = enc_audio_ctx->outbuf_size;

	int ret = 0;

	/*number of samples per channel*/
	enc_audio_ctx->frame->nb_samples  = enc_audio_ctx->codec_context->frame_size;
	int buffer_size = av_samples_get_buffer_size(
		NULL,
		enc_audio_ctx->codec_context->channels,
		enc_audio_ctx->frame->nb_samples,
		enc_audio_ctx->codec_context->sample_fmt,
		0);

	/*set the data pointers in frame*/
    avcodec_fill_audio_frame(
		enc_audio_ctx->frame,
		enc_audio_ctx->codec_context->channels,
		enc_audio_ctx->codec_context->sample_fmt,
		(const uint8_t *) audio_data,
		buffer_size,
		0);

	if(!enc_audio_ctx->monotonic_pts) /*generate a real pts based on the frame timestamp*/
		enc_audio_ctx->frame->pts += ((enc_audio_ctx->pts - last_audio_pts)/1000) * 90;
	else  /*generate a true monotonic pts based on the codec fps*/
		enc_audio_ctx->frame->pts +=
			(enc_audio_ctx->codec_context->time_base.num*1000/enc_audio_ctx->codec_context->time_base.den) * 90;

	ret = avcodec_encode_audio2(
			enc_audio_ctx->codec_context,
			&pkt,
			enc_audio_ctx->frame,
			&got_packet);

	if (!ret && got_packet && enc_audio_ctx->codec_context->coded_frame)
    {
    	enc_audio_ctx->codec_context->coded_frame->pts = pkt.pts;
        enc_audio_ctx->codec_context->coded_frame->key_frame = !!(pkt.flags & AV_PKT_FLAG_KEY);
    }

	enc_audio_ctx->dts = pkt.dts;
	enc_audio_ctx->flags = pkt.flags;
	enc_audio_ctx->duration = pkt.duration;

	/* free any side data since we cannot return it */
	//ff_packet_free_side_data(&pkt);
	if (enc_audio_ctx->frame &&
		enc_audio_ctx->frame->extended_data != enc_audio_ctx->frame->data)
		av_freep(enc_audio_ctx->frame->extended_data);

	outsize = pkt.size;
#else
	outsize = avcodec_encode_audio(
			enc_audio_ctx->codec_context,
			enc_audio_ctx->outbuf,
			enc_audio_ctx->outbuf_size,
			audio_data);

	enc_audio_ctx->dts = AV_NOPTS_VALUE;
	enc_audio_ctx->flags = 0;
	if (enc_audio_ctx->codec_context->coded_frame->key_frame)
		enc_audio_ctx->flags |= AV_PKT_FLAG_KEY;

	enc_audio_ctx->duration = enc_audio_ctx->pts - last_audio_pts;
#endif

	last_audio_pts = enc_audio_ctx->pts;

	enc_audio_ctx->outbuf_coded_size = outsize;
	return (outsize);
}

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
void encoder_close(encoder_context_t *encoder_ctx)
{
	encoder_clean_video_ring_buffer();

	if(!encoder_ctx);
		return;

	encoder_video_context_t *enc_video_ctx = encoder_ctx->enc_video_ctx;
	encoder_audio_context_t *enc_audio_ctx = encoder_ctx->enc_audio_ctx;

	reference_pts = 0;

	if(encoder_ctx->enc_video_ctx->priv_data)
		free(encoder_ctx->enc_video_ctx->priv_data);

	if(encoder_ctx->h264_pps)
		free(encoder_ctx->h264_pps);

	if(encoder_ctx->h264_sps)
		free(encoder_ctx->h264_sps);

	/*close video codec*/
	if(enc_video_ctx && encoder_ctx->video_codec_ind > 0)
	{
		if(!(enc_video_ctx->flushed_buffers))
		{
			avcodec_flush_buffers(enc_video_ctx->codec_context);
			enc_video_ctx->flushed_buffers = 1;
		}

		avcodec_close(enc_video_ctx->codec_context);
		free(enc_video_ctx->codec_context);

#if LIBAVCODEC_VER_AT_LEAST(53,6)
		av_dict_free(&(enc_video_ctx->private_options));
#endif

		if(enc_video_ctx->picture)
			free(enc_video_ctx->picture);
	}
	if(enc_video_ctx)
	{
		if(enc_video_ctx->tmpbuf)
			free(enc_video_ctx->tmpbuf);
		if(enc_video_ctx->outbuf)
			free(enc_video_ctx->outbuf);

		free(enc_video_ctx);
	}

	if(encoder_ctx->enc_audio_ctx->priv_data)
		free(encoder_ctx->enc_audio_ctx->priv_data);
	/*close audio codec*/
	if(enc_audio_ctx)
	{
		avcodec_flush_buffers(enc_audio_ctx->codec_context);

		avcodec_close(enc_audio_ctx->codec_context);
		free(enc_audio_ctx->codec_context);

		if(enc_audio_ctx->outbuf)
			free(enc_audio_ctx->outbuf);
		if(enc_audio_ctx->frame)
			free(enc_audio_ctx->frame);

		free(enc_audio_ctx);
	}

	free(encoder_ctx);
}
