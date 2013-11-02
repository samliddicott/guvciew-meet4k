/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
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
#include "create_video.h"
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <linux/videodev2.h>

//if not defined don't set any bits but prevent build error
#ifndef CODEC_FLAG2_INTRA_REFRESH
#define CODEC_FLAG2_INTRA_REFRESH 0
#endif

#define __FMUTEX &global->file_mutex

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
		.monotonic_pts= 0,
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
		.codec_id     = AV_CODEC_ID_MJPEG,
		.codec_name   = "mjpeg",
		.mb_decision  = 0,
		.trellis      = 0,
		.me_method    = 0,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.num_threads  = 0,
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
		.monotonic_pts= 0,
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
		.codec_id     = AV_CODEC_ID_NONE,
		.codec_name   = "none",
		.mb_decision  = 0,
		.trellis      = 0,
		.me_method    = 0,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.num_threads  = 0,
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
		.monotonic_pts= 0,
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
		.codec_id     = AV_CODEC_ID_NONE,
		.codec_name   = "none",
		.mb_decision  = 0,
		.trellis      = 0,
		.me_method    = 0,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.num_threads  = 0,
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
		.monotonic_pts= 1,
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
		.codec_id     = AV_CODEC_ID_MPEG1VIDEO,
		.codec_name   = "mpeg1video",
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 1,
		.me_method    = ME_EPZS,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.num_threads  = 1,
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
		.monotonic_pts= 1,
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
		.codec_id     = AV_CODEC_ID_FLV1,
		.codec_name   = "flv",
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 1,
		.me_method    = ME_EPZS,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.num_threads  = 1,
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
		.monotonic_pts= 1,
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
		.codec_id     = AV_CODEC_ID_WMV1,
		.codec_name   = "wmv1",
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 1,
		.me_method    = ME_EPZS,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.num_threads  = 1,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.compressor   = "MPG2",
		.mkv_4cc      = v4l2_fourcc('M','P','G','2'),
		.mkv_codec    = "V_MPEG2",
		.mkv_codecPriv= NULL,
		.description  = N_("MPG2 - MPG2 format"),
		.fps          = 30,
		.monotonic_pts= 1,
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
		.codec_id     = AV_CODEC_ID_MPEG2VIDEO,
		.codec_name   = "mpeg2video",
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 1,
		.me_method    = ME_EPZS,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.num_threads  = 1,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.compressor   = "MP43",
		.mkv_4cc      = v4l2_fourcc('M','P','4','3'),
		.mkv_codec    = "V_MPEG4/MS/V3",
		.mkv_codecPriv= NULL,
		.description  = N_("MS MP4 V3"),
		.fps          = 0,
		.monotonic_pts= 1,
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
		.codec_id     = AV_CODEC_ID_MSMPEG4V3,
		.codec_name   = "msmpeg4v3",
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 1,
		.me_method    = ME_EPZS,
		.mpeg_quant   = 0,
		.max_b_frames = 0,
		.num_threads  = 1,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.compressor   = "DX50",
		.mkv_4cc      = v4l2_fourcc('D','X','5','0'),
		.mkv_codec    = "V_MPEG4/ISO/ASP",
		.mkv_codecPriv= NULL,
		.description  = N_("MPEG4-ASP"),
		.fps          = 0,
		.monotonic_pts= 1,
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
		.codec_id     = AV_CODEC_ID_MPEG4,
		.codec_name   = "mpeg4",
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 1,
		.me_method    = ME_EPZS,
		.mpeg_quant   = 1,
		.max_b_frames = 0,
		.num_threads  = 1,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.compressor   = "H264",
		.mkv_4cc      = v4l2_fourcc('H','2','6','4'),
		.mkv_codec    = "V_MPEG4/ISO/AVC",
		.mkv_codecPriv= NULL,
		.description  = N_("MPEG4-AVC (H264)"),
		.fps          = 0,
		.monotonic_pts= 1,
		.bit_rate     = 1500000,
		.qmax         = 51,
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
		.qcompress    = 0.6,
		.qblur        = 0.5,
		.subq         = 5,
		.framerefs    = 0,
		.codec_id     = AV_CODEC_ID_H264,
		.codec_name   = "libx264",
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 0,
		.me_method    = ME_HEX,
		.mpeg_quant   = 1,
		.max_b_frames = 0,
		.num_threads  = 1,
#if LIBAVCODEC_VER_AT_LEAST(54,01)
		.flags        = CODEC_FLAG2_INTRA_REFRESH
#else
		.flags        = CODEC_FLAG2_BPYRAMID | CODEC_FLAG2_WPRED | CODEC_FLAG2_FASTPSKIP | CODEC_FLAG2_INTRA_REFRESH
#endif
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.compressor   = "VP80",
		.mkv_4cc      = v4l2_fourcc('V','P','8','0'),
		.mkv_codec    = "V_VP8",
		.mkv_codecPriv= NULL,
		.description  = N_("VP8 (VP8)"),
		.fps          = 0,
		.monotonic_pts= 1,
		.bit_rate     = 600000,
		.qmax         = 51,
		.qmin         = 11,
		.max_qdiff    = 4,
		.dia          = 2,
		.pre_dia      = 2,
		.pre_me       = 2,
		.me_pre_cmp   = 0,
		.me_cmp       = 3,
		.me_sub_cmp   = 3,
		.last_pred    = 2,
		.gop_size     = 120,
		.qcompress    = 0.8,
		.qblur        = 0.5,
		.subq         = 5,
		.framerefs    = 0,
		.codec_id     = AV_CODEC_ID_VP8,
		.codec_name   = "libvpx",
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 0,
		.me_method    = ME_HEX,
		.mpeg_quant   = 1,
		.max_b_frames = 0,
		.num_threads  = 4,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.compressor   = "theo",
		.mkv_4cc      = v4l2_fourcc('t','h','e','o'),
		.mkv_codec    = "V_THEORA",
		.mkv_codecPriv= NULL,
		.description  = N_("Theora (ogg theora)"),
		.fps          = 0,
		.monotonic_pts= 1,
		.bit_rate     = 600000,
		.qmax         = 51,
		.qmin         = 11,
		.max_qdiff    = 4,
		.dia          = 2,
		.pre_dia      = 2,
		.pre_me       = 2,
		.me_pre_cmp   = 0,
		.me_cmp       = 3,
		.me_sub_cmp   = 3,
		.last_pred    = 2,
		.gop_size     = 120,
		.qcompress    = 0.8,
		.qblur        = 0.5,
		.subq         = 5,
		.framerefs    = 0,
		.codec_id     = AV_CODEC_ID_THEORA,
		.codec_name   = "libtheora",
		.mb_decision  = FF_MB_DECISION_RD,
		.trellis      = 0,
		.me_method    = ME_HEX,
		.mpeg_quant   = 1,
		.max_b_frames = 0,
		.num_threads  = 1,
		.flags        = 0
	}
};

static int get_real_index (int codec_ind)
{
	int i = 0;
	int ind = -1;
	for (i=0;i<MAX_VCODECS; i++)
	{
		if(listSupVCodecs[i].valid)
			ind++;
		if(ind == codec_ind)
			return i;
	}
	return (codec_ind); //should never arrive
}

/** returns the valid list index (combobox)
 * or -1 if no valid codec*/
static int get_list_index (int real_index)
{
	if( real_index < 0 ||
		real_index >= MAX_VCODECS ||
		!listSupVCodecs[real_index].valid )
		return -1; //error: real index is not valid

	int i = 0;
	int ind = -1;
	for (i=0;i<=real_index; i++)
	{
		if(listSupVCodecs[i].valid)
			ind++;
	}

	return (ind);
}

/** returns the real codec array index*/
int get_vcodec_index(int codec_id)
{
	int i = 0;
	for(i=0; i<MAX_VCODECS; i++ )
	{
		if(codec_id == listSupVCodecs[i].codec_id)
			return i;
	}

	return -1;
}

int get_list_vcodec_index(int codec_id)
{
	return get_list_index(get_vcodec_index(codec_id));
}

const char *get_vid4cc(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_VCODECS)
		return (listSupVCodecs[real_index].compressor);
	else
	{
		fprintf(stderr, "VCODEC: (4cc) bad codec index\n");
		return NULL;
	}
}

const char *get_desc4cc(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_VCODECS)
		return (listSupVCodecs[real_index].description);
	else
	{
		fprintf(stderr, "VCODEC: (desc4cc) bad codec index\n");
		return NULL;
	}
}

int get_enc_fps(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_VCODECS)
		return (listSupVCodecs[real_index].fps);
	else
	{
		fprintf(stderr, "VCODEC: (enc_fps) bad codec index\n");
		return 0;
	}
}

const char *get_mkvCodec(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_VCODECS)
		return (listSupVCodecs[get_real_index (codec_ind)].mkv_codec);
	else
	{
		fprintf(stderr, "VCODEC: (mkvCodec) bad codec index\n");
		return NULL;
	}
}

void *get_mkvCodecPriv(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_VCODECS)
		return ((void *) listSupVCodecs[real_index].mkv_codecPriv);
	else
	{
		fprintf(stderr, "VCODEC: (mkvCodecPriv) bad codec index\n");
		return NULL;
	}
}

int set_mkvCodecPriv(struct vdIn *videoIn, struct GLOBAL *global, struct lavcData* data)
{
	int size = 0;
	int real_index = get_real_index (global->VidCodec);

	if(real_index < 0 || real_index >= MAX_VCODECS)
	{
		fprintf(stderr, "VCODEC: (set mkvCodecPriv) bad codec index\n");
		return 0;
	}

	if(listSupVCodecs[real_index].codec_id == AV_CODEC_ID_THEORA)
	{
		//get the 3 first header packets
		uint8_t *header_start[3];
		int header_len[3];
		int first_header_size;

		first_header_size = 42; //vorbis = 30
    	if (avpriv_split_xiph_headers(data->codec_context->extradata, data->codec_context->extradata_size,
				first_header_size, header_start, header_len) < 0)
        {
			fprintf(stderr, "VCODEC: theora codec - Extradata corrupt.\n");
			return -1;
		}

		//get the allocation needed for headers size
		int header_lace_size[2];
		header_lace_size[0]=0;
		header_lace_size[1]=0;
		int i;
		for (i = 0; i < header_len[0] / 255; i++)
			header_lace_size[0]++;
		header_lace_size[0]++;
		for (i = 0; i < header_len[1] / 255; i++)
			header_lace_size[1]++;
		header_lace_size[1]++;

		size = 1 + //number of packets -1
				header_lace_size[0] +  //first packet size
				header_lace_size[1] +  //second packet size
				header_len[0] + //first packet header
				header_len[1] + //second packet header
				header_len[2];  //third packet header

		//should check and clean before allocating ??
		data->priv_data = g_new0(BYTE, size);
		//write header
		BYTE* tmp = data->priv_data;
		*tmp++ = 0x02; //number of packets -1
		//size of head 1
		for (i = 0; i < header_len[0] / 0xff; i++)
			*tmp++ = 0xff;
		*tmp++ = header_len[0] % 0xff;
		//size of head 2
		for (i = 0; i < header_len[1] / 0xff; i++)
			*tmp++ = 0xff;
		*tmp++ = header_len[1] % 0xff;
		//add headers
		for(i=0; i<3; i++)
		{
			memcpy(tmp, header_start[i] , header_len[i]);
			tmp += header_len[i];
		}

		listSupVCodecs[real_index].mkv_codecPriv = data->priv_data;
	}
	else if(listSupVCodecs[real_index].codec_id == AV_CODEC_ID_H264)
	{
		if(global->format == V4L2_PIX_FMT_H264 && global->Frame_Flags==0)
		{
			//do we have SPS and PPS data ?
			if(videoIn->h264_SPS_size <= 0 || videoIn->h264_SPS == NULL)
			{
				fprintf(stderr,"can't store H264 codec private data: No SPS data\n");
				return 0;
			}
			if(videoIn->h264_PPS_size <= 0 || videoIn->h264_PPS == NULL)
			{
				fprintf(stderr,"can't store H264 codec private data: No PPS data\n");
				return 0;
			}

			//alloc the private data
			size = 6 + 2 + videoIn->h264_SPS_size + 1 + 2 + videoIn->h264_PPS_size;
			data->priv_data = g_new0(BYTE, size);

			//write the codec private data
			uint8_t *tp = data->priv_data;
			//header (6 bytes)
			tp[0] = 1; //version
			tp[1] = videoIn->h264_SPS[1]; /* profile */
			tp[2] = videoIn->h264_SPS[2]; /* profile compat */
			tp[3] = videoIn->h264_SPS[3]; /* level */
			tp[4] = 0xff; /* 6 bits reserved (111111) + 2 bits nal size length - 1 (11) */
			tp[5] = 0xe1; /* 3 bits reserved (111) + 5 bits number of sps (00001) */
			tp += 6;
			//SPS: size (2 bytes) + SPS data
			tp[0] = (uint8_t) (videoIn->h264_SPS_size >> 8);
			tp[1] = (uint8_t) videoIn->h264_SPS_size; //38 for logitech uvc 1.1
			tp += 2; //SPS size (16 bit)
			memcpy(tp, videoIn->h264_SPS , videoIn->h264_SPS_size);
			tp += videoIn->h264_SPS_size;
			//PPS number of pps (1 byte) + size (2 bytes) + PPS data
			tp[0] = 0x01; //number of pps
			tp[1] = (uint8_t) (videoIn->h264_PPS_size >> 8);
			tp[2] = (uint8_t) videoIn->h264_PPS_size; //4 for logitech uvc 1.1
			tp += 3; //PPS size (16 bit)
			memcpy(tp, videoIn->h264_PPS , videoIn->h264_PPS_size);

			listSupVCodecs[real_index].mkv_codecPriv = data->priv_data;
		}

	}
	else if(listSupVCodecs[real_index].mkv_codecPriv != NULL)
	{
		mkv_codecPriv.biWidth = global->width;
		mkv_codecPriv.biHeight = global->height;
		mkv_codecPriv.biCompression = listSupVCodecs[real_index].mkv_4cc;
		if(listSupVCodecs[real_index].codec_id != CODEC_DIB)
		{
			mkv_codecPriv.biSizeImage = global->width*global->height*2;
			mkv_codecPriv.biHeight =-global->height;
		}
		else mkv_codecPriv.biSizeImage = global->width*global->height*3; /*rgb*/
		size = 40; //40 bytes
	}

	return (size);
}

int get_vcodec_id(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_VCODECS)
		return (listSupVCodecs[real_index].codec_id);
	else
	{
		fprintf(stderr, "VCODEC: (id) bad codec index\n");
		return 0;
	}
}

gboolean isLavcCodec(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_VCODECS)
		return (listSupVCodecs[real_index].avcodec);
	else
	{
		fprintf(stderr, "VCODEC: (isLavcCodec) bad codec index\n");
		return FALSE;
	}
}

int setVcodecVal ()
{
	AVCodec *codec;
	int ind = 0;
	int num_codecs = 0;
	for (ind=0;ind<MAX_VCODECS;ind++)
	{
		if(!listSupVCodecs[ind].avcodec)
		{
			//its a guvcview internal codec (allways valid)
			num_codecs++;
		}
		else
		{
			codec = avcodec_find_encoder(listSupVCodecs[ind].codec_id);
			if (!codec)
			{
				g_print("no codec detected for %s\n", listSupVCodecs[ind].compressor);
				listSupVCodecs[ind].valid = FALSE;
			}
			else num_codecs++;
		}
	}

	return num_codecs;
}

gboolean isVcodecValid(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_VCODECS)
		return (listSupVCodecs[real_index].valid);
	else
	{
		fprintf(stderr, "VCODEC: (isValid) bad codec index\n");
		return FALSE;
	}
}

vcodecs_data *get_codec_defaults(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_VCODECS)
		return (&(listSupVCodecs[real_index]));
	else
	{
		fprintf(stderr, "VCODEC: (defaults) bad codec index\n");
		return NULL;
	}
}

static int write_video_data(struct ALL_DATA *all_data, BYTE *buff, int size)
{
	struct VideoFormatData *videoF = all_data->videoF;
	struct GLOBAL *global = all_data->global;

	int ret =0;

	__LOCK_MUTEX( __FMUTEX );
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			if(size > 0)
				ret = avi_write_packet(videoF->avi, 0, buff, size, videoF->vdts, videoF->vblock_align, videoF->vflags);
				//ret = AVI_write_frame (videoF->AviOut, buff, size, videoF->keyframe);
			break;

		case WEBM_FORMAT:
		case MKV_FORMAT:
			if(size > 0)
			{
				ret = mkv_write_packet(videoF->mkv, 0, buff, size, videoF->vduration, videoF->vpts, videoF->vflags);
				//ret = write_video_packet (buff, size, global->fps, videoF);
			}
			break;

		default:

			break;
	}
	__UNLOCK_MUTEX( __FMUTEX );

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
			framesize= encode_lavc_frame (proc_buff->frame, lavc_data, global->format, videoF);
		else
			framesize= encode_lavc_frame (proc_buff->frame, lavc_data, V4L2_PIX_FMT_YUYV, videoF);

		ret = write_video_data (all_data, lavc_data->outbuf, framesize);
	}
	else
		fprintf(stderr, "encode_lavc: encoder not initiated\n");
		
	return (ret);
}

int compress_frame(void *data,
	void *jpeg_data,
	struct lavcData *lavc_data,
	VidBuff *proc_buff)
{
	struct JPEG_ENCODER_STRUCTURE **jpeg_struct = (struct JPEG_ENCODER_STRUCTURE **) jpeg_data;
	BYTE *prgb =NULL;

	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	struct GLOBAL *global = all_data->global;
	struct VideoFormatData *videoF = all_data->videoF;

	long framesize = 0;
	int jpeg_size = 0;
	int ret = 0;

	videoF->vpts = proc_buff->time_stamp;

	switch (global->VidCodec)
	{
		case CODEC_MJPEG: /*MJPG*/
			/* save MJPG frame */
			if((global->Frame_Flags==0) && (global->format==V4L2_PIX_FMT_MJPEG))
			{
				ret = write_video_data (all_data, proc_buff->frame, proc_buff->bytes_used);
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

				ret = write_video_data (all_data, global->jpeg, jpeg_size);
			}
			break;

		case CODEC_YUV:
			ret = write_video_data (all_data, proc_buff->frame, proc_buff->bytes_used);
			break;

		case CODEC_DIB:
			framesize=(global->width)*(global->height)*3; /*DIB 24/32 -> 3/4 bytes per pixel*/
			prgb = g_new0(BYTE, framesize);
			switch (global->VidFormat)
			{
				case AVI_FORMAT: /* lines upside down     */
					yuyv2bgr(proc_buff->frame, prgb, global->width, global->height);
					break;

				case WEBM_FORMAT:
				case MKV_FORMAT: /* lines in correct order*/
					yuyv2bgr1(proc_buff->frame, prgb, global->width, global->height);
					break;
			}
			ret = write_video_data (all_data, prgb, framesize);
			g_free(prgb);
			prgb=NULL;
			break;

		default:
			if( global->format == V4L2_PIX_FMT_H264 && 
				global->VidCodec_ID == AV_CODEC_ID_H264 &&
				global->Frame_Flags == 0)
			{
				videoF->vflags = 0;
				if(proc_buff->keyframe)
					videoF->vflags |= AV_PKT_FLAG_KEY;
				ret = write_video_data (all_data, proc_buff->frame, proc_buff->bytes_used);
				videoF->old_vpts = videoF->vpts;
			}
			else
				ret = encode_lavc (lavc_data, all_data, proc_buff);
			break;
	}
	return (ret);
}
