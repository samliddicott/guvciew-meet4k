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
#include "lavc_common.h"
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
		.avcodec      = FALSE,
		.compressor   = "MJPG",
		.description  = N_("MJPG - compressed"),
		.bit_rate     = 0,
		.qmax         = 0,
		.qmin         = 0,
		.max_qdiff    = 0,
		.dia          = 0,
		.pre_dia      = 0,
		.pre_me       = 0,
		.me_pre_cmp   = 0,
		.me_cmp       = 0,
		.me_sub_cmp   = 0,
		.last_pred    = 0,
		.gop_size     = 0,
		.qcompress    = 0,
		.qblur        = 0,
		.codec_id     = 0,
		.mb_decision  = 0,
		.trellis      = 0,
		.me_method    = 0,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.flags        = 0
	},
	{
		.avcodec     = FALSE,
		.compressor  = "YUY2",
		.description = N_("YUY2 - uncomp YUV"),
		.bit_rate     = 0,
		.qmax         = 0,
		.qmin         = 0,
		.max_qdiff    = 0,
		.dia          = 0,
		.pre_dia      = 0,
		.pre_me       = 0,
		.me_pre_cmp   = 0,
		.me_cmp       = 0,
		.me_sub_cmp   = 0,
		.last_pred    = 0,
		.gop_size     = 0,
		.qcompress    = 0,
		.qblur        = 0,
		.codec_id     = 0,
		.mb_decision  = 0,
		.trellis      = 0,
		.me_method    = 0,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.flags        = 0
	},
	{
		.avcodec     = FALSE,
		.compressor  = "DIB ",
		.description = N_("RGB - uncomp BMP"),
		.bit_rate     = 0,
		.qmax         = 0,
		.qmin         = 0,
		.max_qdiff    = 0,
		.dia          = 0,
		.pre_dia      = 0,
		.pre_me       = 0,
		.me_pre_cmp   = 0,
		.me_cmp       = 0,
		.me_sub_cmp   = 0,
		.last_pred    = 0,
		.gop_size     = 0,
		.qcompress    = 0,
		.qblur        = 0,
		.codec_id     = 0,
		.mb_decision  = 0,
		.trellis      = 0,
		.me_method    = 0,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.compressor   = "MPEG",
		.description  = N_("MPEG video 1"),
		.bit_rate     = 3000000,
		.qmax         = 8,
		.qmin         = 2,
		.max_qdiff    = 2,
		.dia          = 2,
		.pre_dia      = 2,
		.pre_me       = 2,
		.me_pre_cmp   = 0,
		.me_cmp       = 3,
		.me_sub_cmp   = 3,
		.last_pred    = 2,
		.gop_size     = 12,
		.qcompress    = 0.5,
		.qblur        = 0.5,
		.codec_id     = CODEC_ID_MPEG1VIDEO,
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 1,
		.me_method    = ME_EPZS,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.compressor   = "FLV1",
		.description  = N_("FLV1 - flash video 1"),
		.bit_rate     = 3000000,
		.qmax         = 31,
		.qmin         = 2,
		.max_qdiff    = 3,
		.dia          = 2,
		.pre_dia      = 2,
		.pre_me       = 2,
		.me_pre_cmp   = 0,
		.me_cmp       = 3,
		.me_sub_cmp   = 3,
		.last_pred    = 2,
		.gop_size     = 100,
		.qcompress    = 0.5,
		.qblur        = 0.5,
		.codec_id     = CODEC_ID_FLV1,
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 1,
		.me_method    = ME_EPZS,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.flags        = CODEC_FLAG_4MV
	},
	{
		.avcodec      = TRUE,
		.compressor   = "WMV1",
		.description  = N_("WMV1 - win. med. video 7"),
		.bit_rate     = 3000000,
		.qmax         = 8,
		.qmin         = 2,
		.max_qdiff    = 2,
		.dia          = 2,
		.pre_dia      = 2,
		.pre_me       = 2,
		.me_pre_cmp   = 0,
		.me_cmp       = 3,
		.me_sub_cmp   = 3,
		.last_pred    = 2,
		.gop_size     = 100,
		.qcompress    = 0.5,
		.qblur        = 0.5,
		.codec_id     = CODEC_ID_WMV1,
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 1,
		.me_method    = ME_EPZS,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.flags        = 0
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

gboolean isLavcCodec(int codec_ind)
{
	return (listSupVCodecs[codec_ind].avcodec);
}

vcodecs_data *get_codec_defaults(int codec_ind)
{
	return (&(listSupVCodecs[codec_ind]));
}

static int encode_lavc (struct lavcData *lavc_data, struct ALL_DATA *all_data, int keyframe)
{
	struct vdIn *videoIn = all_data->videoIn;
	struct avi_t *AviOut = all_data->AviOut;
	int framesize = 0;
	int ret = 0;
	
	if(lavc_data)
	{
		framesize= encode_lavc_frame (videoIn->framebuffer, lavc_data );
		ret = AVI_write_frame (AviOut, lavc_data->outbuf, framesize, keyframe);
	}
	return (ret);
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
		case CODEC_FLV1:
		case CODEC_WMV1:
			if(!(*lavc_data)) 
			{
				*lavc_data = init_lavc(videoIn->width, videoIn->height, videoIn->fps, global->AVIFormat);
			}
			ret = encode_lavc (*lavc_data, all_data, keyframe);
			break;
	}
	return (ret);
}
