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

#include "flv.h"

struct lavcData* init_flv (int width, int height, int fps)
{
	//allocate
	struct lavcData* data = g_new0(struct lavcData, 1);
	// must be called before using avcodec lib
	avcodec_init();

	// register all the codecs (you can also register only the codec
	//you wish to have smaller code
	avcodec_register_all();
	
	data->codec_context = NULL;
	
	data->codec_context = avcodec_alloc_context();
	
	// find the flv1 video encoder
	data->codec = avcodec_find_encoder(CODEC_ID_FLV1);
	if (!data->codec) 
	{
		fprintf(stderr, "FLV1 codec not found\n");
		return(NULL);
	}
	
	//alloc picture
	data->picture= avcodec_alloc_frame();
	
	data->codec_context->bit_rate = 3000000;
	
	// resolution must be a multiple of two
	data->codec_context->width = width; 
	data->codec_context->height = height;
	
	data->codec_context->gop_size = 100;
	data->codec_context->codec_id = CODEC_ID_FLV1;
	data->codec_context->pix_fmt = PIX_FMT_YUV420P;
	data->codec_context->time_base = (AVRational){1,fps};
	
	/* open it */
	if (avcodec_open(data->codec_context, data->codec) < 0) 
	{
		fprintf(stderr, "could not open codec\n");
		return(NULL);
	}
	//alloc tmpbuff (yuv420p)
	data->tmpbuf = g_new0(BYTE, (width*height*3)/2);
	//alloc outbuf
	data->outbuf_size = 200000;
	data->outbuf = g_new0(BYTE, data->outbuf_size);
	
	return(data);
}
