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
#include <unistd.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
/* support for internationalization - i18n */
#include <locale.h>
#include <libintl.h>

#include "../config.h"
#include "gviewencoder.h"
#include "encoder.h"
#include "stream_io.h"
#include "matroska.h"
#include "gview.h"

static mkv_context_t *mkv_ctx = NULL;

static stream_io_t *video_stream = NULL;
static stream_io_t *audio_stream = NULL;

/*file mutex*/
static __MUTEX_TYPE mutex;
#define __PMUTEX &mutex

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
int write_video_data(encoder_context_t *encoder_ctx)
{
	/*assertions*/
	assert(encoder_ctx != NULL);

	encoder_video_context_t *enc_video_ctx = encoder_ctx->enc_video_ctx;

	if(enc_video_ctx->outbuf_coded_size <= 0)
		return -1;

	int ret =0;

	__LOCK_MUTEX( __PMUTEX );
	switch (encoder_ctx->muxer_id)
	{
		case ENCODER_MUX_AVI:
			//if(size > 0)
			//	ret = avi_write_packet(videoF->avi, 0, buff, size, videoF->vdts, videoF->vblock_align, videoF->vflags);
			break;

		case ENCODER_MUX_MKV:
		case ENCODER_MUX_WEBM:
			ret = mkv_write_packet(
					mkv_ctx,
					0,
					enc_video_ctx->outbuf,
					enc_video_ctx->outbuf_coded_size,
					enc_video_ctx->duration,
					enc_video_ctx->pts,
					enc_video_ctx->flags);
			break;

		default:

			break;
	}
	__UNLOCK_MUTEX( __PMUTEX );

	return (ret);
}

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
void encoder_muxer_init(encoder_context_t *encoder_ctx, const char *filename)
{
	switch (encoder_ctx->muxer_id)
	{
		case ENCODER_MUX_AVI:

			break;

		default:
		case ENCODER_MUX_MKV:
		case ENCODER_MUX_WEBM:
			if(mkv_ctx != NULL)
			{
				mkv_destroy_context(mkv_ctx);
				mkv_ctx = NULL;
			}
			mkv_ctx = mkv_create_context(filename, encoder_ctx->muxer_id);

			/*add video stream*/
			video_stream = mkv_add_video_stream(
				mkv_ctx,
				encoder_ctx->video_width,
				encoder_ctx->video_height,
				encoder_ctx->fps_den,
				encoder_ctx->fps_num,
				encoder_ctx->enc_video_ctx->codec_context->codec_id);

			if(encoder_ctx->input_format == V4L2_PIX_FMT_H264)
			{
				/*make sure we have sps and pps data*/

			}

			video_stream->extra_data_size = encoder_set_video_mkvCodecPriv(encoder_ctx);

			if(video_stream->extra_data_size > 0)
			{
				int vcodec_ind = get_video_codec_list_index(encoder_ctx->enc_video_ctx->codec_context->codec_id);
				video_stream->extra_data = (uint8_t *) encoder_get_video_mkvCodecPriv(vcodec_ind);
				if(encoder_ctx->input_format == V4L2_PIX_FMT_H264)
					video_stream->h264_process = 1; //we need to process NALU marker
			}

			/*add audio stream*/
			int acodec_ind = get_video_codec_list_index(encoder_ctx->enc_audio_ctx->codec_context->codec_id);
			/*sample size - only used for PCM*/
			int32_t a_bits = encoder_get_audio_bits(acodec_ind);
			/*bit rate (compressed formats)*/
			int32_t b_rate = encoder_get_audio_bit_rate(acodec_ind);

			audio_stream = mkv_add_audio_stream(
							mkv_ctx,
							encoder_ctx->audio_channels,
							encoder_ctx->audio_samprate,
							a_bits,
							b_rate,
							encoder_ctx->enc_audio_ctx->codec_context->codec_id,
							encoder_ctx->enc_audio_ctx->avi_4cc);

			audio_stream->extra_data_size = encoder_set_audio_mkvCodecPriv(encoder_ctx);

			if(audio_stream->extra_data_size > 0)
				audio_stream->extra_data = encoder_get_audio_mkvCodecPriv(acodec_ind);

			/* write the file header */
			mkv_write_header(mkv_ctx);

			break;

	}
}

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
void encoder_muxer_close(encoder_context_t *encoder_ctx)
{
	switch (encoder_ctx->muxer_id)
	{
		case ENCODER_MUX_AVI:

			break;

		default:
		case ENCODER_MUX_MKV:
		case ENCODER_MUX_WEBM:
			if(mkv_ctx != NULL)
			{
				mkv_destroy_context(mkv_ctx);
				mkv_ctx = NULL;
			}
			break;
	}
}
