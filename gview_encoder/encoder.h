/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           George Sedov <radist.morse@gmail.com>                               #
#                  - Threaded encoding                                          #
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

#ifndef ENCODER_H
#define ENCODER_H

#include <inttypes.h>
#include <sys/types.h>

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

typedef struct _bmp_info_header_t
{
	uint32_t   biSize;  /*size of this header 40 bytes*/
	int32_t    biWidth;
	int32_t    biHeight;
	uint16_t   biPlanes; /*color planes - set to 1*/
	uint16_t   biBitCount; /*bits per pixel - color depth (use 24)*/
	uint32_t   biCompression; /*BI_RGB = 0*/
	uint32_t   biSizeImage;
	uint32_t   biXPelsPerMeter;
	uint32_t   biYPelsPerMeter;
	uint32_t   biClrUsed;
	uint32_t   biClrImportant;
}  __attribute__ ((packed)) bmp_info_header_t;

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

/*
 * split xiph headers from libav private data
 * args:
 *    extradata - libav codec private data
 *    extradata_size - codec private data size
 *    first_header_size - first header size
 *    header_start - first 3 bytes of header
 *    header_len - header length
 *
 * asserts:
 *    none
 *
 * returns: error code
 */
int avpriv_split_xiph_headers(
		uint8_t *extradata,
		int extradata_size,
		int first_header_size,
		uint8_t *header_start[3],
        int header_len[3]);

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
int get_video_codec_index(int codec_id);

/*
 * returns the list codec index
 * args:
 *   codec_id - codec id
 *
 * asserts:
 *   none
 *
 * returns: list index or -1 if none
 */
int get_video_codec_list_index(int codec_id);

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
video_codec_t *get_video_codec_defaults(int codec_ind);

/*
 * get audio codec list size
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: listSupVCodecs size (number of elements)
 */
int get_audio_codec_list_size();

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
int get_audio_codec_index(int codec_id);

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
int get_audio_codec_list_index(int codec_id);

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
audio_codec_t *get_audio_codec_defaults(int codec_ind);

#endif