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
#include "lavc_common.h"

static void yuv422to420p(BYTE* pic, struct lavcData* data, int isUYVY)
{
	int i,j;
	int width = data->codec_context->width;
	int height = data->codec_context->height;
	int linesize=width*2;
	int size = width * height;
	printf("XPTO is %d\n", XPTO);
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
				*u++ = (pic[i+1+j*linesize] + pic[i+1+(j+1)*linesize])>>1; // div by 2
				*v++ = (pic[i+3+j*linesize] + pic[i+3+(j+1)*linesize])>>1;
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
				*u++ = (pic[i+j*linesize] + pic[i+(j+1)*linesize])>>1;     // div by 2
				*v++ = (pic[i+2+j*linesize] + pic[i+2+(j+1)*linesize])>>1; // div by 2
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

int encode_lavc_frame (BYTE *picture_buf, struct lavcData* data, int isUYVY)
{
	int out_size = 0;
	//convert to 4:2:0
	yuv422to420p(picture_buf, data, isUYVY);
	/* encode the image */
	out_size = avcodec_encode_video(data->codec_context, data->outbuf, data->outbuf_size, data->picture);
	return (out_size);
}

void clean_lavc (struct lavcData* data)
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