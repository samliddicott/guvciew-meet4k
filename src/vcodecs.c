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

#include <glib/gprintf.h>
#include "vcodecs.h"
#include "guvcview.h"
#include "picture.h"
#include "colorspaces.h"
#include "lavc_common.h"
#include "create_video.h"
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <linux/videodev2.h>

#define CODEC_MJPEG 0
#define CODEC_YUV   1
#define CODEC_DIB   2

static BITMAPINFOHEADER mkv_codecPriv =
{
	.biSize = 0x00000028, //40 bytes 
	.biWidth = 640, //default values (must be set before use)
	.biHeight = 480, 
	.biPlanes = 1, 
	.biBitCount = 1, 
	.biCompression = V4L2_PIX_FMT_MJPEG, 
	.biSizeImage = 640*480*2, //2 bytes per pixel 
	.biXPelsPerMeter = 0, 
	.biYPelsPerMeter = 0, 
	.biClrUsed = 0, 
	.biClrImportant = 0
};

static vcodecs_data listSupVCodecs[] = //list of software supported formats
{
	{
		.avcodec      = FALSE,
		.valid        = TRUE,
		.compressor   = "MJPG",
		.mkv_4cc      = v4l2_fourcc('M','J','P','G'),
		.mkv_codec    = "V_MS/VFW/FOURCC",
		.mkv_codecPriv= &mkv_codecPriv,
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
		.codec_id     = CODEC_ID_MJPEG,
		.mb_decision  = 0,
		.trellis      = 0,
		.me_method    = 0,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.flags        = 0
	},
	{
		.avcodec      = FALSE,
		.valid        = TRUE,
		.compressor   = "YUY2",
		.mkv_4cc      = v4l2_fourcc('Y','U','Y','2'),
		.mkv_codec    = "V_MS/VFW/FOURCC",
		.mkv_codecPriv= &mkv_codecPriv,
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
		.codec_id     = CODEC_ID_NONE,
		.mb_decision  = 0,
		.trellis      = 0,
		.me_method    = 0,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.flags        = 0
	},
	{
		.avcodec      = FALSE,
		.valid        = TRUE,
		.compressor   = "DIB ",
		.mkv_4cc      = v4l2_fourcc('D','I','B',' '),
		.mkv_codec    = "V_MS/VFW/FOURCC",
		.mkv_codecPriv= &mkv_codecPriv,
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
		.codec_id     = CODEC_ID_NONE,
		.mb_decision  = 0,
		.trellis      = 0,
		.me_method    = 0,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.compressor   = "MPEG",
		.mkv_codec    = "V_MPEG1",
		.mkv_codecPriv= NULL,
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
		.valid        = TRUE,
		.compressor   = "FLV1",
		.mkv_4cc      = v4l2_fourcc('F','L','V','1'),
		.mkv_codec    = "V_MS/VFW/FOURCC",
		.mkv_codecPriv= &mkv_codecPriv,
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
		.valid        = TRUE,
		.compressor   = "WMV1",
		.mkv_4cc      = v4l2_fourcc('W','M','V','1'),
		.mkv_codec    = "V_MS/VFW/FOURCC",
		.mkv_codecPriv= &mkv_codecPriv,
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
	},
	{       //only available in libavcodec-unstriped
		.avcodec      = TRUE,
		.valid        = TRUE,
		.compressor   = "MPG2",
		.mkv_codec    = "V_MPEG2",
		.mkv_codecPriv= NULL,
		.description  = N_("MPG2 - MPG2 format"),
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
		.gop_size     = 12,
		.qcompress    = 0.5,
		.qblur        = 0.5,
		.codec_id     = CODEC_ID_MPEG2VIDEO,
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 1,
		.me_method    = ME_EPZS,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.flags        = 0
	},
	{       //only available in libavcodec-unstriped
		.avcodec      = TRUE,
		.valid        = TRUE,
		.compressor   = "MP43",
		.mkv_codec    = "V_MPEG4/MS/V3",
		.mkv_codecPriv= NULL,
		.description  = N_("MS MP4 V3"),
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
		.codec_id     = CODEC_ID_MSMPEG4V3,
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 1,
		.me_method    = ME_EPZS,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.flags        = 0
	},
	{       //only available in libavcodec-unstriped
		.avcodec      = TRUE,
		.valid        = TRUE,
		.compressor   = "MP4V",
		.mkv_codec    = "V_MPEG4/ISO/ASP",
		.mkv_codecPriv= NULL,
		.description  = N_("MPEG4 - MPEG4 format"),
		.bit_rate     = 1500000,
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
		.codec_id     = CODEC_ID_MPEG4,
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 1,
		.me_method    = ME_EPZS,
		.mpeg_quant   = 1,
		.max_b_frames = 0,
		.flags        = 0
	}
};

static int get_real_index (int codec_ind)
{
	int i = 0;
	int ind = -1;
	for (i=0;i<MAX_VCODECS; i++)
	{
		if(isVcodecValid(i))
			ind++;
		if(ind == codec_ind)
			return i;
	}
	return (codec_ind); //should never arrive
}

const char *get_vid4cc(int codec_ind)
{
	return (listSupVCodecs[get_real_index (codec_ind)].compressor);
}

const char *get_desc4cc(int codec_ind)
{
	return (listSupVCodecs[get_real_index (codec_ind)].description);
}

const char *get_mkvCodec(int codec_ind)
{
	return (listSupVCodecs[get_real_index (codec_ind)].mkv_codec);
}

void *get_mkvCodecPriv(int codec_ind)
{
	return (listSupVCodecs[get_real_index (codec_ind)].mkv_codecPriv);
}

int set_mkvCodecPriv(int codec_ind, int width, int height)
{
	int size = 0;
	if(listSupVCodecs[get_real_index (codec_ind)].mkv_codecPriv != NULL)
	{
		mkv_codecPriv.biWidth = width;
		mkv_codecPriv.biHeight = height; 
		mkv_codecPriv.biCompression = listSupVCodecs[get_real_index (codec_ind)].mkv_4cc; 
		mkv_codecPriv.biSizeImage = width*height*2;
		size = 40; //40 bytes
	}
	
	return (size);
}

int get_vcodec_id(int codec_ind)
{
	return (listSupVCodecs[get_real_index (codec_ind)].codec_id);
}

gboolean isLavcCodec(int codec_ind)
{
	return (listSupVCodecs[get_real_index (codec_ind)].avcodec);
}

void setVcodecVal ()
{
	AVCodec *codec;
	int ind = 0;
	for (ind=0;ind<MAX_VCODECS;ind++)
	{
		if (isLavcCodec(ind))
		{
			codec = avcodec_find_encoder(get_vcodec_id(ind));
			if (!codec) 
			{
				g_printf("no codec detected for %s\n", listSupVCodecs[ind].compressor);
				listSupVCodecs[ind].valid = FALSE;
			}
		}
	}
}

gboolean isVcodecValid(int codec_ind)
{
	return (listSupVCodecs[codec_ind].valid);
}

vcodecs_data *get_codec_defaults(int codec_ind)
{
	return (&(listSupVCodecs[get_real_index (codec_ind)]));
}

static int encode_lavc (struct lavcData *lavc_data, struct ALL_DATA *all_data)
{
	struct vdIn *videoIn = all_data->videoIn;
	struct VideoFormatData *videoF = all_data->videoF;
	
	int framesize = 0;
	int ret = 0;
	
	if(lavc_data)
	{
		framesize= encode_lavc_frame (videoIn->framebuffer, lavc_data );
		
		if(lavc_data->codec_context->coded_frame->pts != AV_NOPTS_VALUE)
			videoF->vpts = lavc_data->codec_context->coded_frame->pts;
		if(lavc_data->codec_context->coded_frame->key_frame)
			videoF->keyframe = 1;
			
		ret = write_video_data (all_data, lavc_data->outbuf, framesize);
	}
	return (ret);
}
		   
int compress_frame(void *data, 
	void *jpeg_data, 
	void *lav_data,
	void *pvid_buff)
{
	struct JPEG_ENCODER_STRUCTURE **jpeg_struct = (struct JPEG_ENCODER_STRUCTURE **) jpeg_data;
	struct lavcData **lavc_data = (struct lavcData **) lav_data;
	BYTE **pvid = (BYTE **) pvid_buff;
		
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	long framesize=0;
	int ret=0;
	
	switch (global->VidCodec) 
	{
		case CODEC_MJPEG: /*MJPG*/
			/* save MJPG frame */   
			if((global->Frame_Flags==0) && (videoIn->formatIn==V4L2_PIX_FMT_MJPEG)) 
			{
				ret = write_video_data (all_data, videoIn->tmpbuffer, videoIn->buf.bytesused);
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
			
				ret = write_video_data (all_data, global->jpeg, global->jpeg_size);
			}
			break;

		case CODEC_YUV:
			framesize=(videoIn->width)*(videoIn->height)*2; /*YUY2-> 2 bytes per pixel */
			ret = write_video_data (all_data, videoIn->framebuffer, framesize);
			break;
					
		case CODEC_DIB:
			framesize=(videoIn->width)*(videoIn->height)*3; /*DIB 24/32 -> 3/4 bytes per pixel*/ 
			if(*pvid==NULL)
			{
				*pvid = g_new0(BYTE, framesize);
			}
			yuyv2bgr(videoIn->framebuffer, *pvid, videoIn->width, videoIn->height);
					
			ret = write_video_data (all_data, *pvid, framesize);
			break;
				
		default:
			if(!(*lavc_data)) 
			{
				*lavc_data = init_lavc(videoIn->width, videoIn->height, videoIn->fps, global->VidCodec);
			}
			ret = encode_lavc (*lavc_data, all_data);
			break;
	}
	return (ret);
}
