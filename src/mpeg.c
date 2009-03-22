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

#define INBUF_SIZE 4096

struct mpegData* init_mpeg (int width, int height, int fps)
{
	//allocate
	struct mpegData* data = g_new0(struct mpegData, 1);
	// must be called before using avcodec lib
	avcodec_init();

	// register all the codecs (you can also register only the codec
	//you wish to have smaller code
	avcodec_register_all();
	
	data->codec_context = NULL;
	//data->codec = NULL;
	
	data->codec_context = avcodec_alloc_context();
	
	/* find the mpeg4 video encoder */
	data->codec = avcodec_find_encoder(CODEC_ID_MPEG1VIDEO);
	if (!data->codec) 
	{
		fprintf(stderr, "mpeg4 codec not found\n");
		return(NULL);
	}
	
	//alloc picture
	data->picture= avcodec_alloc_frame();
	//data->codec_context = &stream->codec;
	/* put sample parameters */
	data->codec_context->bit_rate = 400000;
	
	/* resolution must be a multiple of two */
	data->codec_context->width = width; 
	data->codec_context->height = height;
	/* frames per second */
	data->codec_context->gop_size = 10; /* emit one intra frame every ten frames */
	data->codec_context->max_b_frames = 1;
	data->codec_context->me_method = 1;
	data->codec_context->mpeg_quant = 0;
	data->codec_context->qmin = 6;
	data->codec_context->qmax = 6;
	data->codec_context->max_qdiff = 1;
	data->codec_context->qblur = 0.01;
	data->codec_context->strict_std_compliance = 1;
	data->codec_context->codec_id = CODEC_ID_MPEG1VIDEO;
	data->codec_context->pix_fmt = PIX_FMT_YUV420P;
	data->codec_context->time_base = (AVRational){1,fps};
	//data->codec_context->time_base.num = 1;
	//data->codec_context->time_base.den = fps;
	
	/* open it */
	if (avcodec_open(data->codec_context, data->codec) < 0) 
	{
		fprintf(stderr, "could not open codec\n");
		return(NULL);
	}
	//alloc tmpbuff
	data->tmpbuf = g_new0(BYTE, (width*height*3)/2);
	//alloc outbuf
	data->outbuf_size = 200000;
	data->outbuf = g_new0(BYTE, data->outbuf_size);
	
	return(data);
}

static void yuv422to420p(BYTE* pic, struct mpegData* data, int isUYVY)
{
	int i,j;
	int k = 0;
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
	if (!isUYVY) //YUYV
	{
		for(j=0;j<(height-1);j+=2)
		{
			for(i=0;i<(linesize-3);i+=4)
			{
				*y++ = pic[i+j*linesize];
				*y++ = pic[i+2+j*linesize];
				*y1++ = pic[i+(j+1)*linesize];
				*y1++ = pic[i+2+(j+1)*linesize];
				*u++ = (pic[i+1+j*linesize] + pic[i+1+(j+1)*linesize])/2;
				*v++ = (pic[i+3+j*linesize] + pic[i+3+(j+1)*linesize])/2;
			}
			y += width;
			y1 += width;//2 lines
		}
	}
	else //UYVY
	{
		for(j=0;j<(height-1);j+=2)
		{
			for(i=0;i<(linesize-3);i+=4)
			{
				*y++ = pic[i+1+j*linesize];
				*y++ = pic[i+3+j*linesize];
				*y1++ = pic[i+1+(j+1)*linesize];
				*y1++ = pic[i+3+(j+1)*linesize];
				*u++ = (pic[i+j*linesize] + pic[i+(j+1)*linesize])/2;
				*v++ = (pic[i+2+j*linesize] + pic[i+2+(j+1)*linesize])/2;
			}
			y += width;
			y1 += width;//2 lines
		}
	}
	data->picture->data[0] = data->tmpbuf;                    //Y
	data->picture->data[1] = data->tmpbuf + size;             //U
	data->picture->data[2] = data->picture->data[1] + size/4; //V
	data->picture->linesize[0] = data->codec_context->width;
	data->picture->linesize[1] = data->codec_context->width / 2;
	data->picture->linesize[2] = data->codec_context->width / 2;
}



int encode_mpeg_frame (BYTE *picture_buf, struct mpegData* data, int isUYVY)
{
	int out_size = 0;
	//convert to 4:2:0
	yuv422to420p(picture_buf, data, isUYVY);
	/* encode the image */
	out_size = avcodec_encode_video(data->codec_context, data->outbuf, data->outbuf_size, data->picture);
	return (out_size);
}

void clean_mpeg (struct mpegData* data)
{
	if(data)
	{
		avcodec_close(data->codec_context); //free(codec_context)
		g_free(data->tmpbuf);
		g_free(data->outbuf);
		g_free(data->picture);
		g_free(data);
		data = NULL;
	}
}



