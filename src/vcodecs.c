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

static BITMAPINFOHEADER mkv_codecPriv =
{
	.biSize = 0x00000028, //40 bytes 
	.biWidth = 640, //default values (must be set before use)
	.biHeight = 480, 
	.biPlanes = 1, 
	.biBitCount = 24, 
	.biCompression = V4L2_PIX_FMT_MJPEG, 
	.biSizeImage = 640*480*2, //2 bytes per pixel (max buffer - use x3 for RGB)
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
		.fps          = 0,
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
		.subq         = 0,
		.framerefs    = 0,
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
		.fps          = 0,
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
		.subq         = 0,
		.framerefs    = 0,
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
		.compressor   = "RGB ",
		.mkv_4cc      = v4l2_fourcc('R','G','B',' '),
		.mkv_codec    = "V_MS/VFW/FOURCC",
		.mkv_codecPriv= &mkv_codecPriv,
		.description = N_("RGB - uncomp BMP"),
		.fps          = 0,
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
		.subq         = 0,
		.framerefs    = 0,
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
        .mkv_4cc      = v4l2_fourcc('M','P','E','G'),
		.mkv_codec    = "V_MPEG1",
		.mkv_codecPriv= NULL,
		.description  = N_("MPEG video 1"),
		.fps          = 30,
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
		.subq         = 0,
		.framerefs    = 0,
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
		.fps          = 0,
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
		.subq         = 0,
		.framerefs    = 0,
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
		.fps          = 0,
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
		.subq         = 0,
		.framerefs    = 0,
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
		.mkv_4cc      = v4l2_fourcc('M','P','G','2'),
		.mkv_codec    = "V_MPEG2",
		.mkv_codecPriv= NULL,
		.description  = N_("MPG2 - MPG2 format"),
		.fps          = 30,
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
		.subq         = 0,
		.framerefs    = 0,
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
		.mkv_4cc      = v4l2_fourcc('M','P','4','3'),
		.mkv_codec    = "V_MPEG4/MS/V3",
		.mkv_codecPriv= NULL,
		.description  = N_("MS MP4 V3"),
		.fps          = 0,
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
		.subq         = 0,
		.framerefs    = 0,
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
		.compressor   = "DX50",
		.mkv_4cc      = v4l2_fourcc('D','X','5','0'),
		.mkv_codec    = "V_MPEG4/ISO/ASP",
		.mkv_codecPriv= NULL,
		.description  = N_("MPEG4-ASP"),
		.fps          = 0,
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
		.subq         = 0,
		.framerefs    = 0,
		.codec_id     = CODEC_ID_MPEG4,
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 1,
		.me_method    = ME_EPZS,
		.mpeg_quant   = 1,
		.max_b_frames = 0,
		.flags        = 0
	},
	{       //only available in libavcodec-unstriped
		.avcodec      = TRUE,
		.valid        = TRUE,
		.compressor   = "H264",
		.mkv_4cc      = v4l2_fourcc('H','2','6','4'),
		.mkv_codec    = "V_MPEG4/ISO/AVC",
		.mkv_codecPriv= NULL,
		.description  = N_("MPEG4-AVC (H264)"),
		.fps          = 0,
		.bit_rate     = 1500000,
		.qmax         = 21,
		.qmin         = 10,
		.max_qdiff    = 4,
		.dia          = 2,
		.pre_dia      = 2,
		.pre_me       = 2,
		.me_pre_cmp   = 0,
		.me_cmp       = 3,
		.me_sub_cmp   = 3,
		.last_pred    = 2,
		.gop_size     = 100,
		.qcompress    = 0.4,
		.qblur        = 0.2,
		.subq         = 5,
		.framerefs    = 2,
		.codec_id     = CODEC_ID_H264,
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 0,
		.me_method    = ME_HEX,
		.mpeg_quant   = 1,
		.max_b_frames = 0,
		.flags        = CODEC_FLAG2_BPYRAMID | CODEC_FLAG2_WPRED | CODEC_FLAG2_FASTPSKIP
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

int get_enc_fps(int codec_ind)
{
	return (listSupVCodecs[get_real_index (codec_ind)].fps);
}

const char *get_mkvCodec(int codec_ind)
{
	return (listSupVCodecs[get_real_index (codec_ind)].mkv_codec);
}

void *get_mkvCodecPriv(int codec_ind)
{
	return ((void *) listSupVCodecs[get_real_index (codec_ind)].mkv_codecPriv);
}

int set_mkvCodecPriv(int codec_ind, int width, int height)
{
	int size = 0;
	int index = get_real_index (codec_ind);
	if(listSupVCodecs[index].mkv_codecPriv != NULL)
	{
		mkv_codecPriv.biWidth = width;
		mkv_codecPriv.biHeight = height; 
		mkv_codecPriv.biCompression = listSupVCodecs[get_real_index (codec_ind)].mkv_4cc; 
		if(index != CODEC_DIB) 
		{
			mkv_codecPriv.biSizeImage = width*height*2;
			mkv_codecPriv.biHeight =-height;
		}
		else mkv_codecPriv.biSizeImage = width*height*3; /*rgb*/
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

static int write_video_data(struct ALL_DATA *all_data, BYTE *buff, int size, QWORD v_ts)
{
	struct VideoFormatData *videoF = all_data->videoF;
	struct GLOBAL *global = all_data->global;
	
	int ret =0;
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			if(size > 0)
				ret = AVI_write_frame (videoF->AviOut, buff, size, videoF->keyframe);
			break;
		
		case MKV_FORMAT:
			videoF->vpts = v_ts;
			if(size > 0)
				ret = write_video_packet (buff, size, global->fps, videoF);
			break;
			
		default:
			
			break;
	}
	
	return (ret);
}

static int encode_lavc (struct lavcData *lavc_data, 
	struct ALL_DATA *all_data, 
	VidBuff *proc_buff)
{
	//struct vdIn *videoIn = all_data->videoIn;
	struct VideoFormatData *videoF = all_data->videoF;
	struct GLOBAL *global = all_data->global;
	
	int framesize = 0;
	int ret = 0;
	
	if(lavc_data)
	{
		/* 
		 * if no video filter applied take advantage 
		 * of possible raw formats nv12 and nv21
		 * else use internal format (yuyv)
		 */
		if ( !(global->Frame_Flags) )
			framesize= encode_lavc_frame (proc_buff->frame, lavc_data, global->format);
		else 
			framesize= encode_lavc_frame (proc_buff->frame, lavc_data, V4L2_PIX_FMT_YUYV);
			
		videoF->keyframe = lavc_data->codec_context->coded_frame->key_frame;
		
		ret = write_video_data (all_data, lavc_data->outbuf, framesize, proc_buff->time_stamp);
	}
	return (ret);
}
		   
int compress_frame(void *data, 
	void *jpeg_data, 
	void *lav_data,
	VidBuff *proc_buff)
{
	struct JPEG_ENCODER_STRUCTURE **jpeg_struct = (struct JPEG_ENCODER_STRUCTURE **) jpeg_data;
	struct lavcData **lavc_data = (struct lavcData **) lav_data;
	BYTE *prgb =NULL;
	
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	struct GLOBAL *global = all_data->global;
	
	long framesize = 0;
	int jpeg_size = 0;
	int ret = 0;
	
	switch (global->VidCodec) 
	{
		case CODEC_MJPEG: /*MJPG*/
			/* save MJPG frame */   
			if((global->Frame_Flags==0) && (global->format==V4L2_PIX_FMT_MJPEG)) 
			{
				ret = write_video_data (all_data, proc_buff->frame, proc_buff->bytes_used, proc_buff->time_stamp);
			} 
			else 
			{
				/* use built in encoder */ 
				if (!global->jpeg)
				{ 
					global->jpeg = g_new0(BYTE, ((global->width)*(global->height))>>1);
				}
				if(!(*jpeg_struct))
				{
					*jpeg_struct = g_new0(struct JPEG_ENCODER_STRUCTURE, 1);
					/* Initialization of JPEG control structure */
					initialization (*jpeg_struct,global->width,global->height);

					/* Initialization of Quantization Tables  */
					initialize_quantization_tables (*jpeg_struct);
				} 
				
				jpeg_size = encode_image(proc_buff->frame, global->jpeg, 
					*jpeg_struct, 0, global->width, global->height);
			
				ret = write_video_data (all_data, global->jpeg, jpeg_size, proc_buff->time_stamp);
			}
			break;

		case CODEC_YUV:
			ret = write_video_data (all_data, proc_buff->frame, proc_buff->bytes_used, proc_buff->time_stamp);
			break;
					
		case CODEC_DIB:
			framesize=(global->width)*(global->height)*3; /*DIB 24/32 -> 3/4 bytes per pixel*/ 
			prgb = g_new0(BYTE, framesize);
			switch (global->VidFormat)
			{
				case AVI_FORMAT: /* lines upside down     */
					yuyv2bgr(proc_buff->frame, prgb, global->width, global->height);
					break;
				case MKV_FORMAT: /* lines in correct order*/
					yuyv2bgr1(proc_buff->frame, prgb, global->width, global->height);
					break;
			}
			ret = write_video_data (all_data, prgb, framesize, proc_buff->time_stamp);
			g_free(prgb);
			prgb=NULL;
			break;
				
		default:
			if(!(*lavc_data)) 
			{
				*lavc_data = init_lavc(global->width, global->height, global->fps_num, global->fps, global->VidCodec);
			}
			
			ret = encode_lavc (*lavc_data, all_data, proc_buff);
			break;
	}
	return (ret);
}
