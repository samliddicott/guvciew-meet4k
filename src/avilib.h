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
#define FRAME_RATE_SCALE 1000000

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

	int64_t frames_hdr_strm;

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

	avi_RIFF* riff_list; // avi_riff list (NULL terminated)
	int riff_list_size;

	avi_Stream* stream_list;
	int stream_list_size;

	double fps;

	int64_t time_delay_off, odml_list; //some file offsets
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






struct track_t
{

	long   a_fmt;             /* Audio format, see #defines below */
	long   a_chans;           /* Audio channels, 0 for no audio */
	long   a_rate;            /* Rate in Hz */
	long   a_bits;            /* bits per audio sample */
	long   mpgrate;           /* mpg bitrate kbs*/
	long   a_vbr;             /* 0 == no Variable BitRate */
	long   padrate;	      /* byte rate used for zero padding */

	long   audio_strn;        /* Audio stream number */
	off_t  audio_bytes;       /* Total number of bytes of audio data */
	long   audio_chunks;      /* Chunks of audio data in the file */

	char   audio_tag[4];      /* Tag of audio data */
	long   audio_posc;        /* Audio position: chunk */
	long   audio_posb;        /* Audio position: byte within chunk */

	off_t  a_codech_off;       /* absolut offset of audio codec information */
	off_t  a_codecf_off;       /* absolut offset of audio codec information */

	audio_index_entry *audio_index;

	struct track_t *previous, *next;

};

typedef struct track_t track_t;

/*
typedef struct
{
  DWORD  bi_size;
  DWORD  bi_width;
  DWORD  bi_height;
  DWORD  bi_planes;
  DWORD  bi_bit_count;
  DWORD  bi_compression;
  DWORD  bi_size_image;
  DWORD  bi_x_pels_per_meter;
  DWORD  bi_y_pels_per_meter;
  DWORD  bi_clr_used;
  DWORD  bi_clr_important;
} __attribute__ ((packed)) alBITMAPINFOHEADER;

typedef struct
{
  WORD  w_format_tag;
  WORD  n_channels;
  DWORD  n_samples_per_sec;
  DWORD  n_avg_bytes_per_sec;
  WORD  n_block_align;
  WORD  w_bits_per_sample;
  WORD  cb_size;
} __attribute__ ((packed)) alWAVEFORMATEX;

typedef struct
{
  DWORD fcc_type;
  DWORD fcc_handler;
  DWORD dw_flags;
  DWORD dw_caps;
  WORD w_priority;
  WORD w_language;
  DWORD dw_scale;
  DWORD dw_rate;
  DWORD dw_start;
  DWORD dw_length;
  DWORD dw_initial_frames;
  DWORD dw_suggested_buffer_size;
  DWORD dw_quality;
  DWORD dw_sample_size;
  DWORD dw_left;
  DWORD dw_top;
  DWORD dw_right;
  DWORD dw_bottom;
  DWORD dw_edit_count;
  DWORD dw_format_change_count;
  char     sz_name[64];
} __attribute__ ((packed)) alAVISTREAMINFO;

*/

struct avi_t
{
	long   fdes;              /* File descriptor of AVI file */
	long   mode;              /* 0 for reading, 1 for writing */
	__MUTEX_TYPE mutex;

	long   width;             /* Width  of a video frame */
	long   height;            /* Height of a video frame */
	double fps;               /* Frames per second */
	char   compressor[8];     /* Type of compressor, 4 bytes + padding for 0 byte */
	char   compressor2[8];     /* Type of compressor, 4 bytes + padding for 0 byte */
	long   video_strn;        /* Video stream number */
	long   video_frames;      /* Number of video frames */
	char   video_tag[4];      /* Tag of video data */
	long   video_pos;         /* Number of next frame to be read
	                             (if index present) */

	DWORD max_len;    /* maximum video chunk present */

	track_t track[AVI_MAX_TRACKS];  // up to AVI_MAX_TRACKS audio tracks supported

	off_t  pos;               /* position in file */
	long   n_idx;             /* number of index entries actually filled */
	long   max_idx;           /* number of index entries actually allocated */

	off_t  v_codech_off;      /* absolut offset of video codec (strh) info */
	off_t  v_codecf_off;      /* absolut offset of video codec (strf) info */

	BYTE (*idx)[16]; /* index entries (AVI idx1 tag) */

	video_index_entry *video_index;

	//int is_opendml;           /* set to 1 if this is an odml file with multiple index chunks */

	off_t  last_pos;          /* Position of last frame written */
	DWORD last_len;   /* Length of last frame written */
	int must_use_index;       /* Flag if frames are duplicated */
	off_t  movi_start;
	int total_frames;         /* total number of frames if dmlh is present */

	int anum;            // total number of audio tracks
	int aptr;            // current audio working track
	// int comment_fd;      // Read avi header comments from this fd
	// char *index_file;    // read the avi index from this file

	//alBITMAPINFOHEADER *bitmap_info_header;
	//alWAVEFORMATEX *wave_format_ex[AVI_MAX_TRACKS];

	void*	extradata;
	ULONG	extradata_size;

	void* audio_priv_data;
	ULONG audio_priv_data_size;

	int closed; /* 0 - AVI is opened(recordind) 1 -AVI is closed (not recording)*/

};

#define AVI_MODE_WRITE  0
#define AVI_MODE_READ   1

/* The error codes delivered by avi_open_input_file */

#define AVI_ERR_SIZELIM      1     /* The write of the data would exceed
                                      the maximum size of the AVI file.
                                      This is more a warning than an error
                                      since the file may be closed safely */

#define AVI_ERR_OPEN         2     /* Error opening the AVI file - wrong path
                                      name or file nor readable/writable */

#define AVI_ERR_READ         3     /* Error reading from AVI File */

#define AVI_ERR_WRITE        4     /* Error writing to AVI File,
                                      disk full ??? */

#define AVI_ERR_WRITE_INDEX  5     /* Could not write index to AVI file
                                      during close, file may still be
                                      usable */

#define AVI_ERR_CLOSE        6     /* Could not write header to AVI file
                                      or not truncate the file during close,
                                      file is most probably corrupted */

#define AVI_ERR_NOT_PERM     7     /* Operation not permitted:
                                      trying to read from a file open
                                      for writing or vice versa */

#define AVI_ERR_NO_MEM       8     /* malloc failed */

#define AVI_ERR_NO_AVI       9     /* Not an AVI file */

#define AVI_ERR_NO_HDRL     10     /* AVI file has no has no header list,
                                      corrupted ??? */

#define AVI_ERR_NO_MOVI     11     /* AVI file has no has no MOVI list,
                                      corrupted ??? */

#define AVI_ERR_NO_VIDS     12     /* AVI file contains no video data */

#define AVI_ERR_NO_IDX      13     /* The file has been opened with
                                      getIndex==0, but an operation has been
                                      performed that needs an index */

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

int AVI_open_output_file(struct avi_t *AVI, const char * filename);
void AVI_set_video(struct avi_t *AVI, int width, int height, double fps, const char *compressor);
void AVI_set_audio(struct avi_t *AVI, int channels, long rate, int mpgrate, int bits, int format);
int  AVI_write_frame(struct avi_t *AVI, BYTE *data, long bytes, int keyframe);
int  AVI_dup_frame(struct avi_t *AVI);
int  AVI_write_audio(struct avi_t *AVI, BYTE *data, long bytes);
int  AVI_append_audio(struct avi_t *AVI, BYTE *data, long bytes);
ULONG AVI_bytes_remain(struct avi_t *AVI);
int  AVI_close(struct avi_t *AVI);

//int avi_update_header(struct avi_t *AVI);
int AVI_set_audio_track(struct avi_t *AVI, int track);
void AVI_set_audio_vbr(struct avi_t *AVI, long is_vbr);


ULONG AVI_set_MAX_LEN(ULONG len);

int AVI_getErrno();

void AVI_print_error(char *str);
char *AVI_strerror();
char *AVI_syserror();

#endif
