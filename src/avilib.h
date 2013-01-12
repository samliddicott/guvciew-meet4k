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
/*******************************************************************************#
#   Some utilities for writing and reading AVI files.                           #
#   These are not intended to serve for a full blown                            #
#   AVI handling software (this would be much too complex)                      #
#   The only intention is to write out MJPEG encoded                            #
#   AVIs with sound and to be able to read them back again.                     #
#   These utilities should work with other types of codecs too, however.        #
#                                                                               #
#   Copyright (C) 1999 Rainer Johanni <Rainer@Johanni.de>                       #
********************************************************************************/


#ifndef AVILIB_H
#define AVILIB_H

#include "defs.h"
#include <inttypes.h>
#include <sys/types.h>
#include <glib.h>
#include <pthread.h>

#define AVI_MAX_TRACKS 8
#define FRAME_RATE_SCALE 1000 //1000000

enum STREAM_TYPE
{
	STREAM_TYPE_VIDEO = 0,
	STREAM_TYPE_AUDIO = 1,
	STREAM_TYPE_SUB = 2 //not supported
};

typedef enum STREAM_TYPE STREAM_TYPE;

typedef struct _video_index_entry
{
	off_t key;
	off_t pos;
	off_t len;
} video_index_entry;

typedef struct _audio_index_entry
{
	off_t pos;
	off_t len;
	off_t tot;
} audio_index_entry;

typedef struct avi_Ientry {
    unsigned int flags, pos, len;
} avi_Ientry;

typedef struct avi_Index {
    int64_t     indx_start;
    int         entry;
    int         ents_allocated;
    avi_Ientry** cluster;
} avi_Index;

struct avi_Stream
{
	STREAM_TYPE type;          //stream type

	int32_t id;

	uint32_t packet_count;

	int32_t entry;

	int64_t rate_hdr_strm, frames_hdr_strm;

	avi_Index indexes;

	char   compressor[8];        /* Type of compressor, 4 bytes + padding for 0 byte */
	int32_t codec_id;

	//video
	int32_t   width;             /* Width  of a video frame */
	int32_t   height;            /* Height of a video frame */
	double fps;                  /* Frames per second */

	//audio
	int32_t   a_fmt;             /* Audio format, see #defines below */
	int32_t   a_chans;           /* Audio channels, 0 for no audio */
	int32_t   a_rate;            /* Rate in Hz */
	int32_t   a_bits;            /* bits per audio sample */
	int32_t   mpgrate;           /* mpg bitrate kbs*/
	int32_t   a_vbr;             /* 0 == no Variable BitRate */
	uint64_t audio_strm_length;  /* Total number of bytes of audio data */

	//stream private data (codec private data)
	BYTE*   extra_data;
	int32_t extra_data_size;

	struct avi_Stream *previous, *next;
};

typedef struct avi_Stream avi_Stream;

struct avi_RIFF {
    int64_t riff_start, movi_list;
    int64_t frames_hdr_all;
    int id;

    struct avi_RIFF *previous, *next;
};

typedef struct avi_RIFF avi_RIFF;

typedef struct avi_Writer
{
	FILE *fp;      /* file pointer     */

	BYTE *buffer;  /**< Start of the buffer. */
    int buffer_size;        /**< Maximum buffer size */
    BYTE *buf_ptr; /**< Current position in the buffer */
    BYTE *buf_end; /**< End of the buffer. */

	int64_t size; //file size (end of file position)
	int64_t position; //file pointer position (updates on buffer flush)
}avi_Writer;

struct avi_Context
{
	struct avi_Writer  *writer;
	__MUTEX_TYPE mutex;

	int flags; /* 0 - AVI is recordind;   1 - AVI is not recording*/
	
	int32_t time_base_num;       /* video time base numerator */
	int32_t time_base_den;       /* video time base denominator */    
	
	avi_RIFF* riff_list; // avi_riff list (NULL terminated)
	int riff_list_size;

	avi_Stream* stream_list;
	int stream_list_size;

	double fps;

	int64_t odml_list /*,time_delay_off*/ ; //some file offsets
};

typedef struct avi_Context avi_Context;

avi_Context* avi_create_context(const char * filename);

avi_Stream*
avi_add_video_stream(avi_Context *AVI,
					int32_t width,
					int32_t height,
					double fps,
					int32_t codec_id,
					char* compressor);

avi_Stream*
avi_add_audio_stream(avi_Context *AVI,
					int32_t   channels,
					int32_t   rate,
					int32_t   bits,
					int32_t   mpgrate,
					int32_t   codec_id,
					int32_t   format);

avi_Stream*
avi_get_stream(avi_Context* AVI, int index);


int avi_write_packet(avi_Context* AVI,
					int stream_index,
					BYTE *data,
					uint32_t size,
					int64_t dts,
					int block_align,
					int32_t flags);

int avi_close(avi_Context* AVI);


void avi_destroy_context(avi_Context* AVI);

/* Possible Audio formats */

#define WAVE_FORMAT_UNKNOWN             (0x0000)
#define WAVE_FORMAT_PCM                 (0x0001)
#define WAVE_FORMAT_ADPCM               (0x0002)
#define WAVE_FORMAT_IEEE_FLOAT          (0x0003)
#define WAVE_FORMAT_IBM_CVSD            (0x0005)
#define WAVE_FORMAT_ALAW                (0x0006)
#define WAVE_FORMAT_MULAW               (0x0007)
#define WAVE_FORMAT_OKI_ADPCM           (0x0010)
#define WAVE_FORMAT_DVI_ADPCM           (0x0011)
#define WAVE_FORMAT_DIGISTD             (0x0015)
#define WAVE_FORMAT_DIGIFIX             (0x0016)
#define WAVE_FORMAT_YAMAHA_ADPCM        (0x0020)
#define WAVE_FORMAT_DSP_TRUESPEECH      (0x0022)
#define WAVE_FORMAT_GSM610              (0x0031)
#define WAVE_FORMAT_MP3                 (0x0055)
#define WAVE_FORMAT_MPEG12              (0x0050)
#define WAVE_FORMAT_AAC                 (0x00ff)
#define WAVE_FORMAT_IBM_MULAW           (0x0101)
#define WAVE_FORMAT_IBM_ALAW            (0x0102)
#define WAVE_FORMAT_IBM_ADPCM           (0x0103)
#define WAVE_FORMAT_AC3                 (0x2000)
/*extra audio formats (codecs)*/
#define ANTEX_FORMAT_ADPCME		(0x0033)
#define AUDIO_FORMAT_APTX		(0x0025)
#define AUDIOFILE_FORMAT_AF10		(0x0026)
#define AUDIOFILE_FORMAT_AF36		(0x0024)
#define BROOKTREE_FORMAT_BTVD		(0x0400)
#define CANOPUS_FORMAT_ATRAC		(0x0063)
#define CIRRUS_FORMAT_CIRRUS		(0x0060)
#define CONTROL_FORMAT_CR10		(0x0037)
#define CONTROL_FORMAT_VQLPC		(0x0034)
#define CREATIVE_FORMAT_ADPCM		(0x0200)
#define CREATIVE_FORMAT_FASTSPEECH10	(0x0203)
#define CREATIVE_FORMAT_FASTSPEECH8	(0x0202)
#define IMA_FORMAT_ADPCM		(0x0039)
#define CONSISTENT_FORMAT_CS2		(0x0260)
#define HP_FORMAT_CU			(0x0019)
#define DEC_FORMAT_G723			(0x0123)
#define DF_FORMAT_G726			(0x0085)
#define DSP_FORMAT_ADPCM		(0x0036)
#define DOLBY_FORMAT_AC2		(0x0030)
#define DOLBY_FORMAT_AC3_SPDIF		(0x0092)
#define ESS_FORMAT_ESPCM		(0x0061)
#define IEEE_FORMAT_FLOAT		(0x0003)
#define MS_FORMAT_MSAUDIO1_DIVX		(0x0160)
#define MS_FORMAT_MSAUDIO2_DIVX		(0x0161)
#define OGG_FORMAT_VORBIS		(0x566f)
#define OGG_FORMAT_VORBIS1		(0x674f)
#define OGG_FORMAT_VORBIS1P		(0x676f)
#define OGG_FORMAT_VORBIS2		(0x6750)
#define OGG_FORMAT_VORBIS2P		(0x6770)
#define OGG_FORMAT_VORBIS3		(0x6751)
#define OGG_FORMAT_VORBIS3P		(0x6771)
#define MS_FORMAT_WMA9			(0x0163)
#define MS_FORMAT_WMA9_PRO		(0x0162)

#endif
