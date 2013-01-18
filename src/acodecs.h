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

#ifndef ACODECS_H
#define ACODECS_H

#include <glib.h>
#include "lavc_common.h"
#include "globals.h"
#include "sound.h"

#define MAX_ACODECS 6

#define CODEC_PCM   0
#define CODEC_MP2   1

typedef struct _acodecs_data
{
	gboolean avcodec;         //is a avcodec codec
	gboolean valid;           //the encoding codec exists in ffmpeg
	int bits;                 //bits per sample (pcm only)
	int monotonic_pts;
	WORD avi_4cc;             //fourcc WORD value
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
} acodecs_data;

/** must be called before all others
 *  sets the valid flag
 */
int setAcodecVal();

/** returns codec data array (all) index: 0 indexed */
int get_acodec_index(int codec_id);

/** returns codec list (available) index: 0 indexed */
int get_list_acodec_index(int codec_id);

/** codec_ind is the available codec list index
 *  --  refers to the dropdown list (1 indexed)
 */

WORD get_aud4cc(int codec_ind);

int get_aud_bit_rate(int codec_ind);

int get_aud_bits(int codec_ind);

int get_ind_by4cc(WORD avi_4cc);

const char *get_aud_desc4cc(int codec_ind);

const char *get_mkvACodec(int codec_ind);

int get_acodec_id(int codec_ind);

gboolean isLavcACodec(int codec_ind);

gboolean isAcodecValid(int codec_ind);

acodecs_data *get_aud_codec_defaults(int codec_ind);

void* get_mkvACodecPriv(int codec_ind);

int set_mkvACodecPriv(int codec_ind, int samprate, int channels, struct lavcAData* data);

int compress_audio_frame(void *data);

/**
 * Split a single extradata buffer into the three headers that most
 * Xiph codecs use. (e.g. Theora and Vorbis)
 * Works both with Matroska's packing and lavc's packing.
 *
 * @param[in] extradata The single chunk that combines all three headers
 * @param[in] extradata_size The size of the extradata buffer
 * @param[in] first_header_size The size of the first header, used to
 * differentiate between the Matroska packing and lavc packing.
 * @param[out] header_start Pointers to the start of the three separate headers.
 * @param[out] header_len The sizes of each of the three headers.
 * @return On error a negative value is returned, on success zero.
 */
int avpriv_split_xiph_headers(uint8_t *extradata, int extradata_size,
                               int first_header_size, uint8_t *header_start[3],
                               int header_len[3]);

#endif
