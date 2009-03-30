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

#include "vcodecs.h"
#include "guvcview.h"
#include "colorspaces.h"
#include "mpeg.h"
#include "flv.h"
#include "wmv.h"
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <linux/videodev2.h>

#define CODEC_MJPEG 0
#define CODEC_YUV   1
#define CODEC_DIB   2
#define CODEC_MPEG  3
#define CODEC_FLV1  4
#define CODEC_WMV1  5

static vcodecs_data listSupVCodecs[] = //list of software supported formats
{
	{
		.avcodec     = FALSE,
		.compressor  = "MJPG",
		.description = N_("MJPG - compressed")
	},
	{
		.avcodec     = FALSE,
		.compressor  = "YUY2",
		.description = N_("YUY2 - uncomp YUV")
	},
	{
		.avcodec     = FALSE,
		.compressor  = "DIB ",
		.description = N_("RGB - uncomp BMP")
	},
	{
		.avcodec     = TRUE,
		.compressor  = "MPEG",
		.description = N_("MPEG video 1")
	},
	{
		.avcodec     = TRUE,
		.compressor  = "FLV1",
		.description = N_("FLV1 - flash video 1")
	},
	{
		.avcodec     = TRUE,
		.compressor  = "WMV1",
		.description = N_("WMV1 - win. med. video 7")
	}
};

const char *get_vid4cc(int codec_ind)
{
	return (listSupVCodecs[codec_ind].compressor);
}

const char *get_desc4cc(int codec_ind)
{
	return (listSupVCodecs[codec_ind].description);
}

int compress_frame(void *data, 
	void *jpeg_data, 
	void *lav_data,
	void *pavi_buff,
	int keyframe)
{
	struct JPEG_ENCODER_STRUCTURE **jpeg_struct = (struct JPEG_ENCODER_STRUCTURE **) jpeg_data;
	struct lavcData **lavc_data = (struct lavcData **) lav_data;
	BYTE **pavi = (BYTE **) pavi_buff;
		
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct avi_t *AviOut = all_data->AviOut;
	
	long framesize=0;
	int ret=0;
	switch (global->AVIFormat) 
	{
		case CODEC_MJPEG: /*MJPG*/
			/* save MJPG frame */   
			if((global->Frame_Flags==0) && (videoIn->formatIn==V4L2_PIX_FMT_MJPEG)) 
			{
				ret = AVI_write_frame (AviOut, videoIn->tmpbuffer, 
					videoIn->buf.bytesused, keyframe);
			} 
			else 
			{
				/* use built in encoder */ 
				if (!global->jpeg)
				{ 
					global->jpeg = g_new0(BYTE, global->jpeg_bufsize);
				}
				if(!*jpeg_struct) 
				{
					*jpeg_struct = g_new0(struct JPEG_ENCODER_STRUCTURE, 1);
					/* Initialization of JPEG control structure */
					initialization (*jpeg_struct,videoIn->width,videoIn->height);

					/* Initialization of Quantization Tables  */
					initialize_quantization_tables (*jpeg_struct);
				} 
				
				global->jpeg_size = encode_image(videoIn->framebuffer, global->jpeg, 
					*jpeg_struct,1, videoIn->width, videoIn->height);
			
				ret = AVI_write_frame (AviOut, global->jpeg, global->jpeg_size, keyframe);
			}
			break;

		case CODEC_YUV:
			framesize=(videoIn->width)*(videoIn->height)*2; /*YUY2-> 2 bytes per pixel */
			ret = AVI_write_frame (AviOut, videoIn->framebuffer, framesize, keyframe);
			break;
					
		case CODEC_DIB:
			framesize=(videoIn->width)*(videoIn->height)*3; /*DIB 24/32 -> 3/4 bytes per pixel*/ 
			if(*pavi==NULL)
			{
				*pavi = g_new0(BYTE, framesize);
			}
			yuyv2bgr(videoIn->framebuffer, *pavi, videoIn->width, videoIn->height);
					
			ret = AVI_write_frame (AviOut, *pavi, framesize, keyframe);
			break;
				
		case CODEC_MPEG:
			if(!(*lavc_data)) 
			{
				*lavc_data = init_mpeg(videoIn->width, videoIn->height, videoIn->fps);
			}
			if(*lavc_data)
			{
				framesize= encode_lavc_frame (videoIn->framebuffer, *lavc_data );
				ret = AVI_write_frame (AviOut, (*lavc_data)->outbuf, framesize, keyframe);
			}
			break;

		case CODEC_FLV1:
			if(!(*lavc_data)) 
			{
				*lavc_data = init_flv(videoIn->width, videoIn->height, videoIn->fps);
			}
			if(*lavc_data)
			{
				framesize= encode_lavc_frame (videoIn->framebuffer, *lavc_data );
				ret = AVI_write_frame (AviOut, (*lavc_data)->outbuf, framesize, keyframe);
			}
			break;

		case CODEC_WMV1:
			if(!(*lavc_data)) 
			{
				*lavc_data = init_wmv(videoIn->width, videoIn->height, videoIn->fps);
			}
			if(*lavc_data)
			{
				framesize= encode_lavc_frame (videoIn->framebuffer, *lavc_data );
				ret = AVI_write_frame (AviOut, (*lavc_data)->outbuf, framesize, keyframe);
			}
			break;
	}
	return (ret);
}
