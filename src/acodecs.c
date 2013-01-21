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
#include "acodecs.h"
#include "guvcview.h"
#include "picture.h"
#include "colorspaces.h"
#include "create_video.h"
#include "sound.h"
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <linux/videodev2.h>

#define __FMUTEX &global->file_mutex

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
		.codec_id     = AV_CODEC_ID_PCM_S16LE,
		.codec_name   = "pcm_s16le",
		.sample_format = AV_SAMPLE_FMT_S16,
		.profile      = FF_PROFILE_UNKNOWN,
		.mkv_codpriv  = NULL,
		.codpriv_size = 0,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.bits         = 0,
		.monotonic_pts= 1,
		.avi_4cc      = WAVE_FORMAT_MPEG12,
		.mkv_codec    = "A_MPEG/L2",
		.description  = N_("MPEG2 - (lavc)"),
		.bit_rate     = 160000,
		.codec_id     = AV_CODEC_ID_MP2,
		.codec_name   = "mp2",
		.sample_format = AV_SAMPLE_FMT_S16,
		.profile      = FF_PROFILE_UNKNOWN,
		.mkv_codpriv  = NULL,
		.codpriv_size = 0,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.bits         = 0,
		.monotonic_pts= 1,
		.avi_4cc      = WAVE_FORMAT_MP3,
		.mkv_codec    = "A_MPEG/L3",
		.description  = N_("MP3 - (lavc)"),
		.bit_rate     = 160000,
		.codec_id     = AV_CODEC_ID_MP3,
		.codec_name   = "mp3",
		.sample_format = AV_SAMPLE_FMT_S16,
		.profile      = FF_PROFILE_UNKNOWN,
		.mkv_codpriv  = NULL,
		.codpriv_size = 0,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.bits         = 0,
		.monotonic_pts= 1,
		.avi_4cc      = WAVE_FORMAT_AC3,
		.mkv_codec    = "A_AC3",
		.description  = N_("Dolby AC3 - (lavc)"),
		.bit_rate     = 160000,
		.codec_id     = AV_CODEC_ID_AC3,
		.codec_name   = "ac3",
		.sample_format = AV_SAMPLE_FMT_FLT,
		.profile      = FF_PROFILE_UNKNOWN,
		.mkv_codpriv  = NULL,
		.codpriv_size = 0,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.bits         = 16,
		.monotonic_pts= 1,
		.avi_4cc      = WAVE_FORMAT_AAC,
		.mkv_codec    = "A_AAC",
		.description  = N_("ACC Low - (faac)"),
		.bit_rate     = 64000,
		.codec_id     = AV_CODEC_ID_AAC,
		.codec_name   = "aac",
		.sample_format = AV_SAMPLE_FMT_S16,
		.profile      = FF_PROFILE_AAC_LOW,
		.mkv_codpriv  = AAC_ESDS,
		.codpriv_size = 2,
		.flags        = 0
	},
	{
		.avcodec      = TRUE,
		.valid        = TRUE,
		.bits         = 16,
		.monotonic_pts= 1,
		.avi_4cc      = OGG_FORMAT_VORBIS,
		.mkv_codec    = "A_VORBIS",
		.description  = N_("Vorbis"),
		.bit_rate     = 64000,
		.codec_id     = AV_CODEC_ID_VORBIS,
		.codec_name   = "libvorbis",
		.sample_format = AV_SAMPLE_FMT_S16,
		.profile      = FF_PROFILE_UNKNOWN,
		.mkv_codpriv  =  NULL,
		.codpriv_size =  0,
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
		g_print("WARNING: invalid sample rate for AAC encoding\n");
		g_print("valid(96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350)\n");
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
		if(listSupACodecs[i].valid)
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
		real_index >= MAX_ACODECS ||
		!listSupACodecs[real_index].valid )
		return -1; //error: real index is not valid

	int i = 0;
	int ind = -1;
	for (i=0;i<=real_index; i++)
	{
		if(listSupACodecs[i].valid)
			ind++;
	}

	return (ind);
}

/** returns the real codec array index*/
int get_acodec_index(int codec_id)
{
	int i = 0;
	for(i=0; i<MAX_ACODECS; i++ )
	{
		if(codec_id == listSupACodecs[i].codec_id)
			return i;
	}

	return -1;
}

int get_list_acodec_index(int codec_id)
{
	return get_list_index (get_acodec_index(codec_id));
}


WORD get_aud4cc(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_ACODECS)
		return (listSupACodecs[real_index].avi_4cc);
	else
	{
		fprintf(stderr, "ACODEC: (4cc) bad codec index\n");
		return 0;
	}
}

int get_aud_bits(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_ACODECS)
		return (listSupACodecs[real_index].bits);
	else
	{
		fprintf(stderr, "ACODEC: (bits) bad codec index\n");
		return 0;
	}
}

int get_aud_bit_rate(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_ACODECS)
		return (listSupACodecs[real_index].bit_rate);
	else
	{
		fprintf(stderr, "ACODEC: (bit_rate) bad codec index\n");
		return 0;
	}
}

int get_ind_by4cc(WORD avi_4cc)
{
	int i = 0;
	int ind = -1;
	for (i=0;i<MAX_ACODECS; i++)
	{
		if(listSupACodecs[i].avcodec)
		{
			ind++;
			if (listSupACodecs[i].avi_4cc == avi_4cc)
				return (ind);
		}
	}
	g_printerr("ACODEC: audio codec (0x%x) not supported\n", avi_4cc);
	return(0);
}

const char *get_aud_desc4cc(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_ACODECS)
		return (listSupACodecs[real_index].description);
	else
	{
		fprintf(stderr, "ACODEC: (desc4cc) bad codec index\n");
		return NULL;
	}
}


const char *get_mkvACodec(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_ACODECS)
		return (listSupACodecs[real_index].mkv_codec);
	else
	{
		fprintf(stderr, "ACODEC: (mkvACodec) bad codec index\n");
		return NULL;
	}
}

void *get_mkvACodecPriv(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_ACODECS)
		return ((void *) listSupACodecs[real_index].mkv_codpriv);
	else
	{
		fprintf(stderr, "ACODEC: (mkvACodecPriv) bad codec index\n");
		return NULL;
	}
}

int set_mkvACodecPriv(int codec_ind, int samprate, int channels, struct lavcAData* data)
{
	int real_index = get_real_index (codec_ind);

	if(real_index < 0 || real_index >= MAX_ACODECS)
	{
		fprintf(stderr, "ACODEC: (set mkvCodecPriv) bad codec index\n");
		return 0;
	}

	if (listSupACodecs[real_index].codec_id == AV_CODEC_ID_AAC)
	{
		int obj_type = get_aac_obj_ind(listSupACodecs[real_index].profile);
		int sampind  = get_aac_samp_ind(samprate);
		AAC_ESDS[0] = (BYTE) ((obj_type & 0x1F) << 3 ) + ((sampind & 0x0F) >> 1);
		AAC_ESDS[1] = (BYTE) ((sampind & 0x0F) << 7 ) + ((channels & 0x0F) << 3);

		return listSupACodecs[real_index].codpriv_size; /*return size = 2 */
	}
	else if(listSupACodecs[real_index].codec_id == AV_CODEC_ID_VORBIS)
	{
		//get the 3 first header packets
		uint8_t *header_start[3];
		int header_len[3];
		int first_header_size;

		first_header_size = 30; //theora = 42
    	if (avpriv_split_xiph_headers(data->codec_context->extradata, data->codec_context->extradata_size,
				first_header_size, header_start, header_len) < 0)
        {
			fprintf(stderr, "ACODEC: vorbis codec - Extradata corrupt.\n");
			return -1;
		}

		//printf("Vorbis: header1: %i  header2: %i  header3:%i \n", header_len[0], header_len[1], header_len[2]);

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

		int priv_data_size = 1 + //number of packets -1
						header_lace_size[0] +  //first packet size
						header_lace_size[1] +  //second packet size
						header_len[0] + //first packet header
						header_len[1] + //second packet header
						header_len[2];  //third packet header

		//should check and clean before allocating ??
		data->priv_data = g_new0(BYTE, priv_data_size);
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

		listSupACodecs[real_index].mkv_codpriv = data->priv_data;
		listSupACodecs[real_index].codpriv_size = priv_data_size;
		return listSupACodecs[real_index].codpriv_size;
	}


	return 0;
}
int get_acodec_id(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_ACODECS)
		return (listSupACodecs[real_index].codec_id);
	else
	{
		fprintf(stderr, "ACODEC: (id) bad codec index\n");
		return 0;
	}
}

gboolean isLavcACodec(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_ACODECS)
		return (listSupACodecs[real_index].avcodec);
	else
	{
		fprintf(stderr, "ACODEC: (isLavc) bad codec index\n");
		return FALSE;
	}
}

int setAcodecVal()
{
	AVCodec *codec;
	int ind = 0;
	int num_codecs = 0;
	for (ind=0;ind<MAX_ACODECS;ind++)
	{
		if(!listSupACodecs[ind].avcodec)
		{
			//its a guvcview internal codec (allways valid)
			num_codecs++;
		}
		else
		{
			codec = avcodec_find_encoder(listSupACodecs[ind].codec_id);
			if (!codec)
			{
				g_print("no codec detected for %s\n", listSupACodecs[ind].description);
				listSupACodecs[ind].valid = FALSE;
			}
			else
				num_codecs++;
		}
	}

	return num_codecs;
}

gboolean isAcodecValid(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_ACODECS)
		return (listSupACodecs[real_index].valid);
	else
	{
		fprintf(stderr, "ACODEC: (isValid) bad codec index\n");
		return FALSE;
	}
}

acodecs_data *get_aud_codec_defaults(int codec_ind)
{
	int real_index = get_real_index (codec_ind);
	if(real_index >= 0 && real_index < MAX_ACODECS)
		return (&(listSupACodecs[real_index]));
	else
	{
		fprintf(stderr, "ACODEC: (defaults) bad codec index\n");
		return NULL;
	}
}

static int write_audio_data(struct ALL_DATA *all_data, BYTE *buff, int size, QWORD a_ts)
{
	struct VideoFormatData *videoF = all_data->videoF;
	struct GLOBAL *global = all_data->global;

	int ret =0;

	__LOCK_MUTEX( __FMUTEX );

	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			if(size > 0)
				ret = avi_write_packet(videoF->avi, 1, buff, size, videoF->adts, videoF->ablock_align, videoF->aflags);
			break;

		case WEBM_FORMAT:
		case MKV_FORMAT:
			videoF->apts = a_ts;
			if(size > 0)
				ret = mkv_write_packet(videoF->mkv, 1, buff, size, videoF->aduration, videoF->apts, videoF->aflags);
				//ret = write_audio_packet (buff, size, videoF);
			break;

		default:

			break;
	}

	__UNLOCK_MUTEX( __FMUTEX );

	return (ret);
}

static int encode_lavc_audio ( struct ALL_DATA *all_data )
{
	struct paRecordData *pdata = all_data->pdata;
	struct VideoFormatData *videoF = all_data->videoF;

	int framesize = 0;
	int ret = 0;

	videoF->old_apts = videoF->apts;

	if(pdata->lavc_data)
	{
		if(pdata->lavc_data->codec_context->sample_fmt == AV_SAMPLE_FMT_FLT)
			framesize= encode_lavc_audio_frame (pdata->float_sndBuff, pdata->lavc_data, videoF);
		else
			framesize= encode_lavc_audio_frame (pdata->pcm_sndBuff, pdata->lavc_data, videoF);

		ret = write_audio_data (all_data, pdata->lavc_data->outbuf, framesize, pdata->audio_buff[pdata->br_ind][pdata->r_ind].time_stamp);
	}
	return (ret);
}

int compress_audio_frame(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	struct GLOBAL *global = all_data->global;
	struct paRecordData *pdata = all_data->pdata;

	int ret = 0;

	switch (global->Sound_Format)
	{
		/*write audio chunk*/
		case PA_FOURCC:
		{
			SampleConverter(pdata); /*convert from float sample to 16 bit PCM*/
			ret = write_audio_data (all_data, (BYTE *) pdata->pcm_sndBuff, pdata->aud_numSamples*2, pdata->audio_buff[pdata->br_ind][pdata->r_ind].time_stamp);
			break;
		}
		default:
		{
			SampleConverter(pdata);
			ret = encode_lavc_audio (all_data);
			break;
		}
	}
	return (ret);
}
