/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
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

#define MAX_VCODECS 6 

typedef struct _vcodecs_data
{
	gboolean avcodec;         //is a avcodec codec
	const char *compressor;   //fourcc - upper case
	const char *description;  //codec description
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
	int codec_id;             //lavc codec_id
	int mb_decision;          //lavc mb_decision
	int trellis;              //lavc trellis quantization
	int me_method;            //lavc motion estimation method
	int mpeg_quant;           //lavc mpeg quantization
	int max_b_frames;         //lavc max b frames
	int flags;                //lavc flags
} vcodecs_data;


const char *get_vid4cc(int codec_ind);

const char *get_desc4cc(int codec_ind);

gboolean isLavcCodec(int codec_ind);

vcodecs_data *get_codec_defaults(int codec_ind);

int compress_frame(void *data, 
	void *jpeg_data, 
	void *lav_data,
	void *pavi_buff,
	int keyframe);

#endif
