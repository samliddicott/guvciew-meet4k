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
#include "acodecs.h"
#include "guvcview.h"
#include "picture.h"
#include "colorspaces.h"
#include "lavc_common.h"
#include "create_video.h"
#include "sound.h"
#include "mp2.h"
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <linux/videodev2.h>

static acodecs_data listSupACodecs[] = //list of software supported formats
{
	{
		.avcodec      = FALSE,
		.valid        = TRUE,
		.bits         = 16,
		.avi_4cc      = WAVE_FORMAT_PCM,
		.mkv_codec    = "A_PCM/INT/LIT",
		.description  = N_("PCM - uncompressed (16 bit)"),
		.bit_rate     = 0,
		.codec_id     = CODEC_ID_NONE,
		.profile      = FF_PROFILE_UNKNOWN,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.bits         = 0,
		.avi_4cc      = WAVE_FORMAT_MPEG12,
		.mkv_codec    = "A_MPEG/L2",
		.description  = N_("MPEG2 - (lavc)"),
		.bit_rate     = 160000,
		.codec_id     = CODEC_ID_MP2,
		.profile      = FF_PROFILE_UNKNOWN,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.bits         = 0,
		.avi_4cc      = WAVE_FORMAT_MP3,
		.mkv_codec    = "A_MPEG/L3",
		.description  = N_("MP3 - (lavc)"),
		.bit_rate     = 160000,
		.codec_id     = CODEC_ID_MP3,
		.profile      = FF_PROFILE_UNKNOWN,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.bits         = 0,
		.avi_4cc      = WAVE_FORMAT_AC3,
		.mkv_codec    = "A_AC3",
		.description  = N_("Dolby AC3 - (lavc)"),
		.bit_rate     = 160000,
		.codec_id     = CODEC_ID_AC3,
		.profile      = FF_PROFILE_UNKNOWN,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.bits         = 0,
		.avi_4cc      = WAVE_FORMAT_AAC,
		.mkv_codec    = "A_AAC/MPEG4/MAIN",
		.description  = N_("ACC - (lavc)"),
		.bit_rate     = 160000,
		.codec_id     = CODEC_ID_AAC,
		.profile      = FF_PROFILE_AAC_MAIN,
		.flags        = 0
	}
};

static int get_real_index (int codec_ind)
{
	int i = 0;
	int ind = -1;
	for (i=0;i<MAX_ACODECS; i++)
	{
		if(isAcodecValid(i))
			ind++;
		if(ind == codec_ind)
			return i;
	}
	return (codec_ind); //should never arrive
}

WORD get_aud4cc(int codec_ind)
{
	return (listSupACodecs[get_real_index (codec_ind)].avi_4cc);
}

int get_aud_bits(int codec_ind)
{
	return (listSupACodecs[get_real_index (codec_ind)].bits);
}

int get_aud_bit_rate(int codec_ind)
{
	return (listSupACodecs[get_real_index (codec_ind)].bit_rate);
}

int get_ind_by4cc(WORD avi_4cc)
{
	int i = 0;
	int ind = -1;
	for (i=0;i<MAX_ACODECS; i++)
	{
		if(isAcodecValid(i))
		{
			ind++;
			if (listSupACodecs[i].avi_4cc == avi_4cc)
				return (ind);
		}
	}
	g_printerr("WARNING: audio codec (%d) not supported\n", avi_4cc);
	return(0);
}

const char *get_aud_desc4cc(int codec_ind)
{
	return (listSupACodecs[get_real_index (codec_ind)].description);
}


const char *get_mkvACodec(int codec_ind)
{
	return (listSupACodecs[get_real_index (codec_ind)].mkv_codec);
}


int get_acodec_id(int codec_ind)
{
	return (listSupACodecs[get_real_index (codec_ind)].codec_id);
}

gboolean isLavcACodec(int codec_ind)
{
	return (listSupACodecs[get_real_index (codec_ind)].avcodec);
}

void setAcodecVal ()
{
	AVCodec *codec;
	int ind = 0;
	for (ind=0;ind<MAX_ACODECS;ind++)
	{
		if (isLavcACodec(ind))
		{
			codec = avcodec_find_encoder(get_acodec_id(ind));
			if (!codec) 
			{
				g_printf("no codec detected for %s\n", listSupACodecs[ind].description);
				listSupACodecs[ind].valid = FALSE;
			}
		}
	}
}

gboolean isAcodecValid(int codec_ind)
{
	return (listSupACodecs[codec_ind].valid);
}

acodecs_data *get_aud_codec_defaults(int codec_ind)
{
	return (&(listSupACodecs[get_real_index (codec_ind)]));
}

static int write_audio_data(struct ALL_DATA *all_data, BYTE *buff, int size, QWORD a_ts)
{
	struct VideoFormatData *videoF = all_data->videoF;
	struct GLOBAL *global = all_data->global;
	
	int ret =0;
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			if(size > 0)
				ret = AVI_write_audio (videoF->AviOut, buff, size);
			break;
		
		case MKV_FORMAT:
			videoF->apts = a_ts;
			if(size > 0)
				ret = write_audio_packet (buff, size, videoF);
			break;
			
		default:
			
			break;
	}
	
	return (ret);
}

static int encode_lavc_audio (struct lavcAData *lavc_data, 
	struct ALL_DATA *all_data, 
	AudBuff *proc_buff)
{
	struct paRecordData *pdata = all_data->pdata;
	
	int framesize = 0;
	int ret = 0;
	
	if(lavc_data)
	{
		/*lavc is initialized when setting sound*/
		Float2Int16(pdata, proc_buff); /*convert from float sample to 16 bit PCM*/
		framesize= encode_lavc_audio_frame (pdata->pcm_sndBuff, lavc_data);
		//framesize= encode_lavc_audio_frame (pdata->frame, lavc_data);
		
		ret = write_audio_data (all_data, lavc_data->outbuf, framesize, proc_buff->time_stamp);
	}
	return (ret);
}
		   
int compress_audio_frame(void *data, 
	void *lav_aud_data,
	AudBuff *proc_buff)
{
	struct lavcAData **lavc_data = (struct lavcAData **) lav_aud_data;
	
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	struct GLOBAL *global = all_data->global;
	struct paRecordData *pdata = all_data->pdata;
	
	int ret = 0;
	
	switch (global->Sound_Format) 
	{
		/*write audio chunk*/
		case PA_FOURCC: 
		{
			Float2Int16(pdata, proc_buff); /*convert from float sample to 16 bit PCM*/
			ret = write_audio_data (all_data, (BYTE *) pdata->pcm_sndBuff, pdata->aud_numSamples*2, proc_buff->time_stamp);
			break;
		}
		default:
		{
			ret = encode_lavc_audio (*lavc_data, all_data, proc_buff);
			break;
		}
	}
	return (ret);
}
