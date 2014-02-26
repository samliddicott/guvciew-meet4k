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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
/* support for internationalization - i18n */
#include <locale.h>
#include <libintl.h>

#include "gview.h"
#include "encoder.h"
#include "gviewencoder.h"

extern int verbosity;

/* AAC object types index: MAIN = 1; LOW = 2; SSR = 3; LTP = 4*/
static int AAC_OBJ_TYPE[5] =
	{ FF_PROFILE_UNKNOWN, FF_PROFILE_AAC_MAIN, FF_PROFILE_AAC_LOW, FF_PROFILE_AAC_SSR, FF_PROFILE_AAC_LTP };
/*-1 = reserved; 0 = freq. is writen explictly (increases header by 24 bits)*/
static int AAC_SAMP_FREQ[16] =
	{ 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, -1, -1, 0};

/*NORMAL AAC HEADER*/
/*2 bytes: object type index(5 bits) + sample frequency index(4bits) + channels(4 bits) + flags(3 bit) */
/*default = MAIN(1)+44100(4)+stereo(2)+flags(0) = 0x0A10*/
static uint8_t AAC_ESDS[2] = {0x0A,0x10};
/* if samprate index == 15 AAC_ESDS[5]:
 * object type index(5 bits) + sample frequency index(4bits) + samprate(24bits) + channels(4 bits) + flags(3 bit)
 */


static audio_codec_t listSupCodecs[] = //list of software supported formats
{
	{
		.valid        = TRUE,
		.bits         = 16,
		.avi_4cc      = WAVE_FORMAT_PCM,
		.mkv_codec    = "A_PCM/INT/LIT",
		.description  = N_("PCM - uncompressed (16 bit)"),
		.bit_rate     = 0,
		.codec_id     = AV_CODEC_ID_PCM_S16LE,
		.codec_name   = "pcm_s16le",
		.sample_format = AV_SAMPLE_FMT_S16,
		.profile      = FF_PROFILE_UNKNOWN,
		.mkv_codpriv  = NULL,
		.codpriv_size = 0,
		.flags        = 0
	},
	{
		.valid        = TRUE,
		.bits         = 0,
		.monotonic_pts= 1,
		.avi_4cc      = WAVE_FORMAT_MPEG12,
		.mkv_codec    = "A_MPEG/L2",
		.description  = N_("MPEG2 - (lavc)"),
		.bit_rate     = 160000,
		.codec_id     = AV_CODEC_ID_MP2,
		.codec_name   = "mp2",
		.sample_format = AV_SAMPLE_FMT_S16,
		.profile      = FF_PROFILE_UNKNOWN,
		.mkv_codpriv  = NULL,
		.codpriv_size = 0,
		.flags        = 0
	},
	{
		.valid        = TRUE,
		.bits         = 0,
		.monotonic_pts= 1,
		.avi_4cc      = WAVE_FORMAT_MP3,
		.mkv_codec    = "A_MPEG/L3",
		.description  = N_("MP3 - (lavc)"),
		.bit_rate     = 160000,
		.codec_id     = AV_CODEC_ID_MP3,
		.codec_name   = "mp3",
		.sample_format = AV_SAMPLE_FMT_S16,
		.profile      = FF_PROFILE_UNKNOWN,
		.mkv_codpriv  = NULL,
		.codpriv_size = 0,
		.flags        = 0
	},
	{
		.valid        = TRUE,
		.bits         = 0,
		.monotonic_pts= 1,
		.avi_4cc      = WAVE_FORMAT_AC3,
		.mkv_codec    = "A_AC3",
		.description  = N_("Dolby AC3 - (lavc)"),
		.bit_rate     = 160000,
		.codec_id     = AV_CODEC_ID_AC3,
		.codec_name   = "ac3",
		.sample_format = AV_SAMPLE_FMT_FLT,
		.profile      = FF_PROFILE_UNKNOWN,
		.mkv_codpriv  = NULL,
		.codpriv_size = 0,
		.flags        = 0
	},
	{
		.valid        = TRUE,
		.bits         = 16,
		.monotonic_pts= 1,
		.avi_4cc      = WAVE_FORMAT_AAC,
		.mkv_codec    = "A_AAC",
		.description  = N_("ACC Low - (faac)"),
		.bit_rate     = 64000,
		.codec_id     = AV_CODEC_ID_AAC,
		.codec_name   = "aac",
		.sample_format = AV_SAMPLE_FMT_S16,
		.profile      = FF_PROFILE_AAC_LOW,
		.mkv_codpriv  = AAC_ESDS,
		.codpriv_size = 2,
		.flags        = 0
	},
	{
		.valid        = TRUE,
		.bits         = 16,
		.monotonic_pts= 1,
		.avi_4cc      = OGG_FORMAT_VORBIS,
		.mkv_codec    = "A_VORBIS",
		.description  = N_("Vorbis"),
		.bit_rate     = 64000,
		.codec_id     = AV_CODEC_ID_VORBIS,
		.codec_name   = "libvorbis",
		.sample_format = AV_SAMPLE_FMT_S16,
		.profile      = FF_PROFILE_UNKNOWN,
		.mkv_codpriv  =  NULL,
		.codpriv_size =  0,
		.flags        = 0
	}
};

/*
 * get audio codec list size
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: listSupCodecs size (number of elements)
 */
int encoder_get_audio_codec_list_size()
{
	int size = sizeof(listSupCodecs)/sizeof(audio_codec_t);

	if(verbosity > 0)
		printf("ENCODER: audio codec list size:%i\n", size);

	return size;
}

/*
 * get audio codec valid list size
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: listSupCodecs valid number of elements
 */
int encoder_get_audio_codec_valid_list_size()
{
	int valid_size = 0;

	int i = 0;
	for(i = 0;  i < encoder_get_audio_codec_list_size(); ++i)
		if(listSupCodecs[i].valid)
			valid_size++;

	if(verbosity > 0)
		printf("ENCODER: audio codec valid list size:%i\n", valid_size);

	return valid_size;
}

/*
 * return the real (valid only) codec index
 * args:
 *   codec_ind - codec list index (with non valid removed)
 *
 * asserts:
 *   none
 *
 * returns: matching listSupCodecs index
 */
static int get_real_index (int codec_ind)
{
	int i = 0;
	int ind = -1;
	for (i = 0; i < encoder_get_audio_codec_list_size(); ++i)
	{
		if(listSupCodecs[i].valid)
			ind++;
		if(ind == codec_ind)
			return i;
	}
	return (codec_ind); //should never arrive
}

/*
 * return the list codec index
 * args:
 *   real_ind - listSupCodecs index
 *
 * asserts:
 *   none
 *
 * returns: matching list index (with non valid removed)
 */
static int get_list_index (int real_index)
{
	if( real_index < 0 ||
		real_index >= encoder_get_audio_codec_list_size() ||
		!listSupCodecs[real_index].valid )
		return -1; //error: real index is not valid

	int i = 0;
	int ind = -1;
	for (i = 0; i<= real_index; ++i)
	{
		if(listSupCodecs[i].valid)
			ind++;
	}

	return (ind);
}

/*
 * returns the real codec array index
 * args:
 *   codec_id - codec id
 *
 * asserts:
 *   none
 *
 * returns: real index or -1 if none
 */
int get_audio_codec_index(int codec_id)
{
	int i = 0;
	for(i = 0; i < encoder_get_audio_codec_list_size(); ++i )
	{
		if(codec_id == listSupCodecs[i].codec_id)
			return i;
	}

	return -1;
}

/*
 * returns the list codec index
 * args:
 *   codec_id - codec id
 *
 * asserts:
 *   none
 *
 * returns: real index or -1 if none
 */
int get_audio_codec_list_index(int codec_id)
{
	return get_list_index(get_audio_codec_index(codec_id));
}

/*
 * get audio list codec entry for codec index
 * args:
 *   codec_ind - codec list index
 *
 * asserts:
 *   none
 *
 * returns: list codec entry or NULL if none
 */
audio_codec_t *encoder_get_audio_codec_defaults(int codec_ind)
{
	int real_index = get_real_index (codec_ind);

	if(real_index >= 0 && real_index < encoder_get_audio_codec_list_size())
		return (&(listSupCodecs[real_index]));
	else
	{
		fprintf(stderr, "ENCODER: (audio codec defaults) bad codec index\n");
		return NULL;
	}
}

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
int encoder_set_valid_audio_codec_list ()
{
	AVCodec *codec;
	int ind = 0;
	int num_codecs = 0;
	for ( ind = 0; ind < encoder_get_audio_codec_list_size(); ++ind)
	{
		codec = avcodec_find_encoder(listSupCodecs[ind].codec_id);
		if (!codec)
		{
			printf("ENCODER: no audio codec detected for %s\n", listSupCodecs[ind].description);
			listSupCodecs[ind].valid = 0;
		}
		else num_codecs++;
	}

	return num_codecs;
}

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
const char *encoder_get_audio_codec_description(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < encoder_get_audio_codec_list_size())
		return (listSupCodecs[real_index].description);
	else
	{
		fprintf(stderr, "ENCODER: (video codec description) bad codec index\n");
		return NULL;
	}
}

/*
 * get audio mkv codec
 * args:
 *   codec_ind - codec list index
 *
 * asserts:
 *   none
 *
 * returns: mkv codec entry or NULL if none
 */
const char *encoder_get_audio_mkv_codec(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < encoder_get_audio_codec_list_size())
		return (listSupCodecs[real_index].mkv_codec);
	else
	{
		fprintf(stderr, "ENCODER: (audio mkv codec) bad codec index\n");
		return NULL;
	}
}

