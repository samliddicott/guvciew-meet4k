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
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <linux/videodev2.h>

/* AAC object types index: MAIN = 1; LOW = 2; SSR = 3; LTP = 4*/
static int AAC_OBJ_TYPE[5] = 
	{ FF_PROFILE_UNKNOWN, FF_PROFILE_AAC_MAIN, FF_PROFILE_AAC_LOW, FF_PROFILE_AAC_SSR, FF_PROFILE_AAC_LTP };
/*-1 = reserved; 0 = freq. is writen explictly (increases header by 24 bits)*/
static int AAC_SAMP_FREQ[16] = 
	{ 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, -1, -1, 0};

/*NORMAL AAC HEADER*/
/*2 bytes: object type index(5 bits) + sample frequency index(4bits) + channels(4 bits) + flags(3 bit) */
/*default = MAIN(1)+44100(4)+stereo(2)+flags(0) = 0x0A10*/
static BYTE AAC_ESDS[2] = {0x0A,0x10};
/* if samprate index == 15 AAC_ESDS[5]: 
 * object type index(5 bits) + sample frequency index(4bits) + samprate(24bits) + channels(4 bits) + flags(3 bit)
 */

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
		.codec_id     = CODEC_ID_PCM_S16LE,
		.profile      = FF_PROFILE_UNKNOWN,
		.mkv_codpriv  = NULL,
		.codpriv_size = 0,
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
		.mkv_codpriv  = NULL,
		.codpriv_size = 0,
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
		.mkv_codpriv  = NULL,
		.codpriv_size = 0,
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
		.mkv_codpriv  = NULL,
		.codpriv_size = 0,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.bits         = 16,
		.avi_4cc      = WAVE_FORMAT_AAC,
		.mkv_codec    = "A_AAC",
		.description  = N_("ACC Low - (faac)"),
		.bit_rate     = 64000,
		.codec_id     = CODEC_ID_AAC,
		.profile      = FF_PROFILE_AAC_LOW,
		.mkv_codpriv  = AAC_ESDS,
		.codpriv_size = 2,
		.flags        = 0
	}
};

static int get_aac_obj_ind(int profile)
{
	int i = 0;

	for (i=0; i<4; i++)
	 if(AAC_OBJ_TYPE[i] == profile) break;
	 
	 return i;
}

static int get_aac_samp_ind(int samprate)
{
	int i = 0;

	for (i=0; i<13; i++)
	 if(AAC_SAMP_FREQ[i] == samprate) break;
	 
	 if (i>12) 
	 {
		g_printf("WARNING: invalid sample rate for AAC encoding\n");
		g_printf("valid(96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350)\n");
		i=4; /*default 44100*/
	 }
	 return i;
}

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

const void *get_mkvACodecPriv(int codec_ind)
{
	return ((void *) listSupACodecs[get_real_index (codec_ind)].mkv_codpriv);
}

int set_mkvACodecPriv(int codec_ind, int samprate, int channels)
{
	int index = get_real_index (codec_ind);
	if (listSupACodecs[index].mkv_codpriv != NULL)
	{
		if (listSupACodecs[index].codec_id == CODEC_ID_AAC)
		{
			int obj_type = get_aac_obj_ind(listSupACodecs[index].profile);
			int sampind  = get_aac_samp_ind(samprate);
			AAC_ESDS[0] = (BYTE) ((obj_type & 0x1F) << 3 ) + ((sampind & 0x0F) >> 1);
			AAC_ESDS[1] = (BYTE) ((sampind & 0x0F) << 7 ) + ((channels & 0x0F) << 3);
		
			return listSupACodecs[index].codpriv_size; /*return size = 2 */
		}
	}

	return 0;
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
