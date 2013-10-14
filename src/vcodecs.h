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

#ifndef VCODECS_H
#define VCODECS_H

#include <glib.h>
#include "jpgenc.h"
#include "lavc_common.h"
#include "globals.h"
#include "v4l2uvc.h"

#define MAX_VCODECS 12

#define CODEC_MJPEG 0
#define CODEC_YUV   1
#define CODEC_DIB   2
#define CODEC_LAVC  3

typedef struct _vcodecs_data
{
	gboolean avcodec;         //is a avcodec codec
	gboolean valid;           //the encoding codec exists in ffmpeg
	const char *compressor;   //fourcc - upper case
	int mkv_4cc;              //fourcc WORD value
	const char *mkv_codec;    //mkv codecID
	void *mkv_codecPriv;      //mkv codec private data
	const char *description;  //codec description
	//int frame_delay;          //frame delay
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
} vcodecs_data;

/** must be called before all others
 *  sets the valid flag
 */
int setVcodecVal();

/** returns codec data array (all) index: 0 indexed */
int get_vcodec_index(int codec_id);

/** returns codec list (available) index: 0 indexed */
int get_list_vcodec_index(int codec_id);

/** codec_ind is the available codec list index
 *  --  refers to the dropdown list
 */

const char *get_vid4cc(int codec_ind);

const char *get_desc4cc(int codec_ind);

const char *get_mkvCodec(int codec_ind);

int get_enc_fps(int codec_ind);

void *get_mkvCodecPriv(int codec_ind);

int set_mkvCodecPriv(struct vdIn *videoIn, struct GLOBAL *global, struct lavcData* data);

int get_vcodec_id(int codec_ind);

gboolean isLavcCodec(int codec_ind);

gboolean isVcodecValid(int codec_ind);

vcodecs_data *get_codec_defaults(int codec_ind);

int compress_frame(void *data,
	void *jpeg_data,
	struct lavcData *lav_data,
	VidBuff *proc_buff);

#endif
