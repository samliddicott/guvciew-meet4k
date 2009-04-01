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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <glib.h>

#include "mpeg.h"

struct lavcData* init_mpeg (int width, int height, int fps)
{
	//allocate
	struct lavcData* data = g_new0(struct lavcData, 1);
	
	data->codec_context = NULL;
	
	data->codec_context = avcodec_alloc_context();
	
	// find the mpeg video encoder
	data->codec = avcodec_find_encoder(CODEC_ID_MPEG1VIDEO);
	if (!data->codec) 
	{
		fprintf(stderr, "mpeg codec not found\n");
		return(NULL);
	}
	
	//alloc picture
	data->picture= avcodec_alloc_frame();
	// define bit rate (lower = more compression but lower quality)
	data->codec_context->bit_rate = 3000000;
	// resolution must be a multiple of two
	data->codec_context->width = width; 
	data->codec_context->height = height;
	
	data->codec_context->flags = CODEC_FLAG_4MV;
	/* 
	* mb_decision
	*0 (FF_MB_DECISION_SIMPLE) Use mbcmp (default).
	*1 (FF_MB_DECISION_BITS)   Select the MB mode which needs the fewest bits (=vhq).
	*2 (FF_MB_DECISION_RD)     Select the MB mode which has the best rate distortion.
	*/
	data->codec_context->mb_decision = FF_MB_DECISION_RD;
	/*use trellis quantization*/
	data->codec_context->trellis = 1;
	
	data->codec_context->me_method = ME_EPZS; 
	data->codec_context->mpeg_quant = 0; //h.263
	data->codec_context->qmin = 2; // best detail allowed - worst compression
	data->codec_context->qmax = 8; // worst detail allowed - best compression
	data->codec_context->max_qdiff = 2;
	data->codec_context->max_b_frames = 0;
	data->codec_context->gop_size = 12;
	data->codec_context->qcompress = 0.5;
	data->codec_context->qblur = 0.5;
	data->codec_context->strict_std_compliance = FF_COMPLIANCE_NORMAL;
	data->codec_context->codec_id = CODEC_ID_MPEG1VIDEO;
	data->codec_context->pix_fmt = PIX_FMT_YUV420P; //only yuv420p available for mpeg
	data->codec_context->time_base = (AVRational){1,fps};
	
	// open codec
	if (avcodec_open(data->codec_context, data->codec) < 0) 
	{
		fprintf(stderr, "could not open codec\n");
		return(NULL);
	}
	//alloc tmpbuff (yuv420p)
	data->tmpbuf = g_new0(BYTE, (width*height*3)/2);
	//alloc outbuf
	data->outbuf_size = 240000;
	data->outbuf = g_new0(BYTE, data->outbuf_size);
	
	return(data);
}
