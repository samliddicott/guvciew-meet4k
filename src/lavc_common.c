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

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include "lavc_common.h"
#include "vcodecs.h"

static void yuv422to420p(BYTE* pic, struct lavcData* data )
{
	int i,j;
	int width = data->codec_context->width;
	int height = data->codec_context->height;
	int linesize=width*2;
	int size = width * height;

	BYTE *y;
	BYTE *y1;
	BYTE *u;
	BYTE* v;
	y = data->tmpbuf;
	y1 = data->tmpbuf + width;
	u = data->tmpbuf + size;
	v = u + size/4;
	
	for(j=0;j<(height-1);j+=2)
	{
		for(i=0;i<(linesize-3);i+=4)
		{ 
			*y++ = pic[i+j*linesize];
			*y++ = pic[i+2+j*linesize]; 
			*y1++ = pic[i+(j+1)*linesize]; 
			*y1++ = pic[i+2+(j+1)*linesize]; 
			*u++ = (pic[i+1+j*linesize] + pic[i+1+(j+1)*linesize])>>1; // div by 2 
			*v++ = (pic[i+3+j*linesize] + pic[i+3+(j+1)*linesize])>>1; 
		} 
		y += width; 
		y1 += width;//2 lines
	}
	
	data->picture->data[0] = data->tmpbuf;                    //Y
	data->picture->data[1] = data->tmpbuf + size;             //U
	data->picture->data[2] = data->picture->data[1] + size/4; //V
	data->picture->linesize[0] = data->codec_context->width;
	data->picture->linesize[1] = data->codec_context->width / 2;
	data->picture->linesize[2] = data->codec_context->width / 2;
}

int encode_lavc_frame (BYTE *picture_buf, struct lavcData* data)
{
	int out_size = 0;
	//convert to 4:2:0
	yuv422to420p(picture_buf, data );
	/* encode the image */
	out_size = avcodec_encode_video(data->codec_context, data->outbuf, data->outbuf_size, data->picture);
	return (out_size);
}

int clean_lavc (void* arg)
{
	struct lavcData** data= (struct lavcData**) arg;
	int enc_frames =0;
	if(*data)
	{
		enc_frames = (*data)->codec_context->real_pict_num;
		avcodec_flush_buffers((*data)->codec_context);
		//close codec 
		avcodec_close((*data)->codec_context);
		//free codec context
		g_free((*data)->codec_context);
		(*data)->codec_context = NULL;
		g_free((*data)->tmpbuf);
		g_free((*data)->outbuf);
		g_free((*data)->picture);
		g_free(*data);
		*data = NULL;
	}
	return (enc_frames);
}

struct lavcData* init_lavc(int width, int height, int fps, int codec_ind)
{
	//allocate
	struct lavcData* data = g_new0(struct lavcData, 1);
	
	data->codec_context = NULL;
	
	data->codec_context = avcodec_alloc_context();
	
	vcodecs_data *defaults = get_codec_defaults(codec_ind);
	
	// find the mpeg video encoder
	data->codec = avcodec_find_encoder(defaults->codec_id);
	if (!data->codec) 
	{
		fprintf(stderr, "mpeg codec not found\n");
		return(NULL);
	}
	
	//alloc picture
	data->picture= avcodec_alloc_frame();
	// define bit rate (lower = more compression but lower quality)
	data->codec_context->bit_rate = defaults->bit_rate;
	// resolution must be a multiple of two
	data->codec_context->width = width; 
	data->codec_context->height = height;
	
	data->codec_context->flags |= defaults->flags;
	
	/* 
	* mb_decision
	*0 (FF_MB_DECISION_SIMPLE) Use mbcmp (default).
	*1 (FF_MB_DECISION_BITS)   Select the MB mode which needs the fewest bits (=vhq).
	*2 (FF_MB_DECISION_RD)     Select the MB mode which has the best rate distortion.
	*/
	data->codec_context->mb_decision = defaults->mb_decision;
	/*use trellis quantization*/
	data->codec_context->trellis = defaults->trellis;
	
	//motion estimation method epzs
	data->codec_context->me_method = defaults->me_method; 
	
	data->codec_context->dia_size = defaults->dia;
	data->codec_context->pre_dia_size = defaults->pre_dia;
	data->codec_context->pre_me = defaults->pre_me;
	data->codec_context->me_pre_cmp = defaults->me_pre_cmp;
	data->codec_context->me_cmp = defaults->me_cmp;
	data->codec_context->me_sub_cmp = defaults->me_sub_cmp;
	data->codec_context->last_predictor_count = defaults->last_pred;
	
	data->codec_context->mpeg_quant = defaults->mpeg_quant; //h.263
	data->codec_context->qmin = defaults->qmin;             // best detail allowed - worst compression
	data->codec_context->qmax = defaults->qmax;             // worst detail allowed - best compression
	data->codec_context->max_qdiff = defaults->max_qdiff;
	data->codec_context->max_b_frames = defaults->max_b_frames;
	data->codec_context->gop_size = defaults->gop_size;
	
	data->codec_context->qcompress = defaults->qcompress;
	data->codec_context->qblur = defaults->qblur;
	data->codec_context->strict_std_compliance = FF_COMPLIANCE_NORMAL;
	data->codec_context->codec_id = defaults->codec_id;
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

