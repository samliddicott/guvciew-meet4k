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
#include <glib/gprintf.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <linux/videodev2.h>

#include "guvcview.h"
#include "ms_time.h"
#include "mp2.h"
#include "colorspaces.h"
#include "lavc_common.h"
#include "video_format.h"
#include "vcodecs.h"
#include "defs.h"


static vformats_data listSupVFormats[] = //list of software supported formats
{
	{
		.avformat     = FALSE,
		.name         = "AVI",
		.description  = N_("AVI - avi format"),
		.extension    = "avi",
		.format_str   = "avi",
		.flags        = 0
	},
	{
		.avformat     = FALSE,
		.name         = "MATROSKA",
		.description  = N_("MKV - Matroska format"),
		.extension    = "mkv",
		.format_str   = "mkv",
		.flags        = 0
	}
};

const char *get_vformat_extension(int codec_ind)
{
	return (listSupVFormats[codec_ind].extension);
}

/*
static const char *get_vformat_str(int codec_ind)
{
	return (listSupVFormats[codec_ind].format_str);
}
*/

const char *get_vformat_desc(int codec_ind)
{
	return (listSupVFormats[codec_ind].description);
}

gboolean isLavfFormat(int codec_ind)
{
	return (listSupVFormats[codec_ind].avformat);
}

char *setVidExt(char *filename, int format_ind)
{
	int sname = strlen(filename)+1; /*include '\0' terminator*/
	char basename[sname];
	sscanf(filename,"%[^.]",basename);
	
	g_snprintf(filename, sname, "%s.%s", basename, get_vformat_extension(format_ind));
	
	return (filename);
}

int write_video_packet (BYTE *picture_buf, int size, int fps, struct VideoFormatData* videoF)
{
	int64_t t_stamp = (int64_t) videoF->vpts; 
	videoF->b_writing_frame = 0;
	mk_setFrameFlags( videoF->mkv_w, t_stamp, videoF->keyframe );
	
	if (!videoF->b_writing_frame) //sort of mutex?
	{
		if( mk_startFrame(videoF->mkv_w) < 0 )
			return -1;
		videoF->b_writing_frame = 1;
	}
	if( mk_addFrameData(videoF->mkv_w, picture_buf , size) < 0 )
		return -1;

	return (0);
}

int write_audio_packet (BYTE *audio_buf, int size, int samprate, struct VideoFormatData* videoF)
{
	int64_t t_stamp = (int64_t) videoF->apts ;
	videoF->b_writing_frame = 0;
	mk_setAudioFrameFlags( videoF->mkv_w, t_stamp, videoF->keyframe );

	if (!videoF->b_writing_frame) //sort of mutex?
	{
		if( mk_startAudioFrame(videoF->mkv_w) < 0 )
			return -1;
		videoF->b_writing_frame = 1;
	}
	if( mk_addAudioFrameData(videoF->mkv_w, audio_buf , size) < 0 )
		return -1;

	return (0);

}

int clean_FormatContext (void* data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	struct GLOBAL *global = all_data->global;
	struct paRecordData *pdata = all_data->pdata;
	struct VideoFormatData *videoF = all_data->videoF;
	int ret = 0;
	/*------------------- close audio stream and clean up -------------------*/
	if (global->Sound_enable > 0) 
	{
		if(global->debug) g_printf("closing sound...\n");
		if (close_sound (pdata)) g_printerr("Sound Close error\n");
		if(global->Sound_Format == ISO_FORMAT_MPEG12) close_MP2_encoder();
		if(global->debug) g_printf("sound closed\n");
	} 

	//int64_t def_duration = ((global->Vidstoptime - global->Vidstarttime)/global->framecount);
	//mk_setDef_Duration(videoF->mkv_w, def_duration);/* set real fps ( average frame duration)*/
	
	ret = mk_close(videoF->mkv_w);
	videoF->mkv_w = NULL;
	videoF->apts = 0;
	videoF->old_apts = 0;
	if(global->debug) g_printf("finished cleaning up\n");
	return (ret);
}

int init_FormatContext(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct VideoFormatData *videoF = all_data->videoF;
	struct paRecordData *pdata = all_data->pdata;
	
	char *AcodecID = NULL;
	int bitspersample = 0;
	float samprate = 16000.0; //reference
	int channels = 1;
	UINT64 duration = 0;
	
	if(videoF->mkv_w !=NULL )
	{
		g_printf("matroska: older mux ref not closed, cleaning now...\n");
		mk_close(videoF->mkv_w);
		videoF->mkv_w = NULL;
	}
	
	videoF->mkv_w = mk_createWriter( videoIn->VidFName );
	if(videoF->mkv_w == NULL)
	{
		g_printerr("matroska: failed to initialize file\n");
		return (-1);
	}
	
	if(global->Sound_enable > 0)
	{
		switch (global->Sound_Format)
		{
			case PA_FOURCC: //pcm codec
				AcodecID = "A_PCM/INT/LIT";
				bitspersample = 16;
				break;
			case ISO_FORMAT_MPEG12:
				AcodecID = "A_MPEG/L2";
				break;
		}
		if (pdata) 
		if(pdata->samprate > 0)
		{
			samprate = (float) pdata->samprate;
			channels = pdata->channels;
			if(pdata->api == PORT)
				duration = (UINT64) (1000*pdata->aud_numSamples)/(pdata->samprate * channels);
			else
				duration = (UINT64) (1000*pdata->aud_numSamples)/(pdata->samprate * channels);

		    	duration = duration * 1000000; //from milisec to nanosec
		}
	    
		if(global->debug) g_printf("audio frame: %i | %i | %i | %llu\n", 
			pdata->aud_numSamples, pdata->samprate, channels, duration);
	}
	else
		samprate = 0.0;
	
	videoF->apts = 0;
	videoF->old_apts = 0;
    
	set_mkvCodecPriv(global->VidCodec, videoIn->width, videoIn->height);
	int size = set_mkvCodecPriv(global->VidCodec, videoIn->width, videoIn->height);
	printf("writing header\n");
	
	/*gspca doesn't set the fps value so we don't set it in the file header    */
	/*this is OK acording to the standard (variable fps )but vlc seems to have */
	/*a problem with this in the case of mpeg codecs (mpg1 and mpg2)            */
	UINT64 v_def_dur = 0;
	if(videoIn->fps >= 5) v_def_dur = (UINT64) (videoIn->fps_num * 1000000000/videoIn->fps); //nano seconds
	if (global->debug) g_printf("video frame default duration =%llu for %i fps\n", v_def_dur, videoIn->fps);
	mk_writeHeader( videoF->mkv_w, "Guvcview",
                     get_mkvCodec(global->VidCodec),
                     AcodecID,
                     get_mkvCodecPriv(global->VidCodec), size,
                     v_def_dur, 
                     duration,
                     1000000,
                     videoIn->width, videoIn->height,
                     videoIn->width, videoIn->height,
                     samprate, channels, bitspersample );
	videoF->b_header_written = 1;
	return(0);
}
