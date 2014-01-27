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
#include <string.h>
#include <math.h>
#include "sound.h"
#include "port_audio.h"
#include "pulse_audio.h"
#include "vcodecs.h"
#include "avilib.h"
#include "acodecs.h"
#include "lavc_common.h"
#include "audio_effects.h"
#include "ms_time.h"

#define __AMUTEX &pdata->mutex
#define MAX_FRAME_DRIFT 15000000 //15 ms
#define MAX_N_DRIFTS 3 //max allowed consecutive drifts

int n_drifts = 0; //number of consecutive delivered buffers with drift
int64_t last_drift = 0; //last calculated drift

/*
 * ts - timestamp of last frame based on monotonic time (nsec)
 * pdata->a_ts - timestamp of first buffer sample based on
 * sample count and sample rate (nsec)
 */
static int64_t
fill_audio_buffer(struct paRecordData *pdata, int64_t ts)
{
	int64_t ts_drift = 0;
	UINT64 frame_length = G_NSEC_PER_SEC / pdata->samprate; /*in nanosec*/
	UINT64 buffer_length = frame_length * (pdata->aud_numSamples / pdata->channels); /*in nanosec*/

	/*first frame time stamp*/
	if(pdata->a_ts < 0)
	{
		/*
		 * if sound begin time > first video frame ts then sync audio to video
		 * else set audio ts to aprox. the video ts (0)
		 */
		if((pdata->ts_ref > 0) && (pdata->ts_ref < pdata->snd_begintime))
			pdata->a_ts = pdata->snd_begintime - pdata->ts_ref;
		else
			pdata->a_ts = 0;
	}
	else /*increment time stamp for audio frame*/
		pdata->a_ts += buffer_length; /*add buffer time*/

	/* check audio drift through timestamps */

	/*
	 * this will introduce some drift unless
	 * snd_begintime is first buffer ts
	 */
	if (ts > pdata->snd_begintime)
		ts -= pdata->snd_begintime;
	else
		ts = 0;

	/*
	 * for accuracy this should be buffer_length - frame_length
	 * since ts is for last frame we should discount that one,
	 * in pratice one frame is irrelevant
	 */
	if (ts > buffer_length)
		ts -= buffer_length;
	else
		ts = 0;

	/*drift between monotonic ts and frame based ts*/
	ts_drift = ts - pdata->a_ts;

	pdata->sampleIndex = 0; /*reset*/

	__LOCK_MUTEX( __AMUTEX );
		int flag = pdata->audio_buff_flag[pdata->bw_ind];
	__UNLOCK_MUTEX( __AMUTEX );

	if(  flag == AUD_READY || flag == AUD_IN_USE )
	{
		if(flag == AUD_READY)
		{
			/*flag as IN_USE*/
			__LOCK_MUTEX( __AMUTEX );
				pdata->audio_buff_flag[pdata->bw_ind] = AUD_IN_USE;
			__UNLOCK_MUTEX( __AMUTEX );
		}
		/*copy data to audio buffer*/
		memcpy(pdata->audio_buff[pdata->bw_ind][pdata->w_ind].frame, pdata->recordedSamples, pdata->aud_numBytes);
		pdata->audio_buff[pdata->bw_ind][pdata->w_ind].time_stamp = pdata->a_ts + pdata->delay;
		pdata->audio_buff[pdata->bw_ind][pdata->w_ind].used = TRUE;

		pdata->blast_ind = pdata->bw_ind;
		pdata->last_ind  = pdata->w_ind;

		/*doesn't need locking as it's only used in the callback*/
		NEXT_IND(pdata->w_ind, AUDBUFF_SIZE);

		if(pdata->w_ind == 0)
		{
			/* reached end of current ring buffer
			 * flag it as AUD_PROCESS
			 * move to next one and flag it as AUD_IN_USE (if READY)
			 */
			pdata->audio_buff_flag[pdata->bw_ind] = AUD_PROCESS;

			__LOCK_MUTEX( __AMUTEX );
				NEXT_IND(pdata->bw_ind, AUDBUFF_NUM);

				if(pdata->audio_buff_flag[pdata->bw_ind] != AUD_READY)
				{
					g_printf("AUDIO: next buffer is not yet ready\n");
				}
				else
				{
					pdata->audio_buff_flag[pdata->bw_ind] = AUD_IN_USE;
				}
			__UNLOCK_MUTEX( __AMUTEX );

		}
	}
	else
	{
		/*drop audio data*/
		g_printerr("AUDIO: dropping audio data\n");
		ts_drift = buffer_length;
	}

	return ts_drift;
}

/*--------------------------- sound callback ------------------------------*/
int
record_sound ( const void *inputBuffer, unsigned long numSamples, int64_t timestamp, void *userData )
{
	struct paRecordData *pdata = (struct paRecordData*)userData;

	__LOCK_MUTEX( __AMUTEX );
        gboolean capVid = pdata->capVid;
        int channels = pdata->channels;
        int skip_n = pdata->skip_n;
    __UNLOCK_MUTEX( __AMUTEX );

	const SAMPLE *rptr = (const SAMPLE*) inputBuffer;
    unsigned long i = 0;
    int64_t ts_drift = 0;

	//UINT64 numFrames = numSamples / channels;
    UINT64 nsec_per_frame = G_NSEC_PER_SEC / pdata->samprate;

	/*timestamp marks beginning of buffer*/
    int64_t ts = timestamp;

	if (skip_n > 0) /*skip audio while were skipping video frames*/
	{
		if(capVid)
		{
			__LOCK_MUTEX( __AMUTEX );
				pdata->snd_begintime = ns_time_monotonic(); /*reset first time stamp*/
			__UNLOCK_MUTEX( __AMUTEX );
			return (0); /*still capturing*/
		}
		else
		{	__LOCK_MUTEX( __AMUTEX );
				pdata->streaming=FALSE;
			__LOCK_MUTEX( __AMUTEX );
			return (-1); /*capture has stopped*/
		}
	}

    for( i=0; i<numSamples; i++ )
    {
        pdata->recordedSamples[pdata->sampleIndex] = inputBuffer ? *rptr++ : 0;
        pdata->sampleIndex++;

        if(pdata->sampleIndex >= pdata->aud_numSamples)
		{
			ts = timestamp + nsec_per_frame * (i/channels); /*timestamp for current frame*/
			ts_drift = fill_audio_buffer(pdata, ts);

			if(ts_drift > MAX_FRAME_DRIFT) /*audio delayed*/
			{
				/*
				 * wait for trend in the next couple of frames
				 * delay maybe compensated in the next buffers
				 */
				n_drifts++;

				/*drift has increased, increment n_drifts faster*/
				if(last_drift > MAX_FRAME_DRIFT && ts_drift > last_drift)
					n_drifts +=2;

			}
			else
				n_drifts = 0; /*we are good (if audio is faster compensate in video)*/

			last_drift = ts_drift;
		}
    }


    if(n_drifts > MAX_N_DRIFTS) /*Drift has been incresing for the last frames*/
	{
		n_drifts = 0; /*reset*/

		/* compensate drift (not all, only to MAX/2 ) */
		int n_samples = ((ts_drift - (MAX_FRAME_DRIFT/2)) / nsec_per_frame) * channels;

		printf("AUDIO: compensating ts drift of %" PRId64 " with %d samples (pa_ts=%" PRId64 " curr_ts=%" PRId64 ")\n",
			ts_drift, n_samples, timestamp, ts );

		int j=0;
		for( j=0; j<n_samples; j++ )
		{
			/*feed buffer with silence frames*/
			pdata->recordedSamples[pdata->sampleIndex] = 0;
			pdata->sampleIndex++;

			if(pdata->sampleIndex >= pdata->aud_numSamples)
			{
				ts += nsec_per_frame * (j/channels); /*timestamp for current frame*/
				ts_drift = fill_audio_buffer(pdata, ts);

				/*break if new drift is acceptable*/
				if(ts_drift < MAX_FRAME_DRIFT)
					break; /* already compensated */
			}
		}
	}

	pdata->ts_drift = ts_drift; /*reset*/

    if(capVid) return (0); /*still capturing*/
	else
    {
        __LOCK_MUTEX( __AMUTEX );
            pdata->streaming=FALSE;
            /* mark current buffer as ready to process */
            pdata->audio_buff_flag[pdata->bw_ind] = AUD_PROCESS;
        __UNLOCK_MUTEX( __AMUTEX );
    }

	return(-1); /* audio capture stopped*/
}

void
record_silence ( unsigned long numSamples, void *userData )
{
	struct paRecordData *pdata = (struct paRecordData*)userData;

    unsigned long i = 0;

    for( i=0; i<numSamples; i++ )
    {
        pdata->recordedSamples[pdata->sampleIndex] = 0;
        pdata->sampleIndex++;

        if(pdata->sampleIndex >= pdata->aud_numSamples)
		{
			/*we don't care about drift here*/
			fill_audio_buffer(pdata, pdata->a_last_ts);
		}
    }
}

void
set_sound (struct GLOBAL *global, struct paRecordData* pdata)
{
	if(global->Sound_SampRateInd==0)
		global->Sound_SampRate=global->Sound_IndexDev[global->Sound_UseDev].samprate;/*using default*/

	if(global->Sound_NumChanInd==0)
	{
		/*using default if channels <3 or stereo(2) otherwise*/
		global->Sound_NumChan=(global->Sound_IndexDev[global->Sound_UseDev].chan < 3) ?
			global->Sound_IndexDev[global->Sound_UseDev].chan : 2;
	}

	pdata->api = global->Sound_API;
	pdata->audio_buff[0] = NULL;
	pdata->recordedSamples = NULL;

	pdata->samprate = global->Sound_SampRate;
	pdata->channels = global->Sound_NumChan;
	__LOCK_MUTEX( __AMUTEX );
		pdata->skip_n = global->skip_n; /*initial video frames to skip*/
	__UNLOCK_MUTEX( __AMUTEX );
	if(global->debug)
	{
		g_print("using audio codec: 0x%04x\n",global->Sound_Format );
		g_print("\tchannels: %d  samplerate: %d\n", pdata->channels, pdata->samprate);
	}

	switch (global->Sound_Format)
	{
		case PA_FOURCC:
		{
			pdata->aud_numSamples = MPG_NUM_SAMP * pdata->channels;
			//outbuffer size in bytes (max value is for pcm 2 bytes per sample)
			pdata->outbuf_size = pdata->aud_numSamples * 2; //a good value is 240000;
			break;
		}
		default:
		{
			//outbuffer size in bytes (max value is for pcm 2 bytes per sample)
			pdata->outbuf_size = MPG_NUM_SAMP * pdata->channels * 2; //a good value is 240000;

			/*initialize lavc data*/
			if(!(pdata->lavc_data))
			{
				pdata->lavc_data = init_lavc_audio(pdata, global->AudCodec);
			}
			/*use lavc audio codec frame size to determine samples*/
			pdata->aud_numSamples = (pdata->lavc_data)->codec_context->frame_size * pdata->channels;
			if(pdata->aud_numSamples <= 0)
			{
				pdata->aud_numSamples = MPG_NUM_SAMP * pdata->channels;
			}
			break;
		}
	}

	pdata->aud_numBytes = pdata->aud_numSamples * sizeof(SAMPLE);
	pdata->input_type = PA_SAMPLE_TYPE;
	pdata->mp2Buff = NULL;

	pdata->sampleIndex = 0;

	fprintf(stderr, "AUDIO: samples(%d)\n", pdata->aud_numSamples);
	pdata->flush = 0;
	pdata->a_ts= -1;
	pdata->ts_ref = 0;
	pdata->a_last_ts = 0;
	last_drift = 0;

	pdata->stream = NULL;
	/* some drivers, e.g. GSPCA, don't set fps( guvcview sets it to 1/1 )
	 * so we can't obtain the proper delay for H.264 (2 video frames)
	 * if set, use the codec properties fps value */
	int fps_num = 1;
	int fps_den = get_enc_fps(global->VidCodec); /*if set use encoder fps */
	if(!fps_den) /*if not set use video combobox fps*/
	{
		fps_num = global->fps_num;
		fps_den = global->fps;
	}
	if((get_vcodec_id(global->VidCodec) == CODEC_ID_H264) && (fps_den >= 5))
		pdata->delay = (UINT64) 2*(fps_num * G_NSEC_PER_SEC / fps_den); /*2 frame delay in nanosec*/
	pdata->delay += global->Sound_delay; /*add predefined delay - def = 0*/

	/*reset the indexes*/
	pdata->r_ind     = 0;
	pdata->w_ind     = 0;
	pdata->bw_ind    = 0;
	pdata->br_ind    = 0;
	pdata->blast_ind = 0;
	pdata->last_ind  = 0;
	/*buffer for video PCM 16 bits*/
	pdata->pcm_sndBuff=NULL;


	/*set audio device id to use (portaudio)*/
	pdata->device_id = global->Sound_IndexDev[global->Sound_UseDev].id; /* input device */


	/*set audio device to use (pulseudio)*/
	strncpy(pdata->device_name, global->Sound_IndexDev[global->Sound_UseDev].name, 511);
}


int
init_sound(struct paRecordData* pdata)
{
	int err = paNoError;
	int i = 0;
	int j = 0;

	/*alloc audio ring buffers*/
    if(!(pdata->audio_buff[0]))
    {
		for(j=0; j< AUDBUFF_NUM; j++)
		{
			pdata->audio_buff[j] = g_new0(AudBuff, AUDBUFF_SIZE);
			for(i=0; i<AUDBUFF_SIZE; i++)
			{
				pdata->audio_buff[j][i].frame = g_new0(SAMPLE, pdata->aud_numSamples);
				pdata->audio_buff[j][i].used = FALSE;
				pdata->audio_buff[j][i].time_stamp = 0;
			}
			pdata->audio_buff_flag[j] = AUD_READY;
		}
	}

	/*alloc the callback buffer*/
	pdata->recordedSamples = g_new0(SAMPLE, pdata->aud_numSamples);

	switch(pdata->api)
	{
#ifdef PULSEAUDIO
		case PULSE:
			err = pulse_init_audio(pdata);
			if(err)
				goto error;
			break;
#endif
		case PORT:
		default:
			err = port_init_audio(pdata);
			if(err)
				goto error;
			break;
	}

	/*sound start time - used to sync with video*/
	pdata->snd_begintime = ns_time_monotonic();

	return (0);

error:
	pdata->streaming=FALSE;
	pdata->flush=0;
	pdata->delay=0;

	if(pdata->recordedSamples) g_free( pdata->recordedSamples );
	pdata->recordedSamples=NULL;
	if(pdata->audio_buff)
	{

		for(j=0; j< AUDBUFF_NUM; j++)
		{
			for(i=0; i<AUDBUFF_SIZE; i++)
			{
				g_free(pdata->audio_buff[j][i].frame);
			}
			g_free(pdata->audio_buff[j]);
			pdata->audio_buff[j] = NULL;
		}
	}
	/*lavc is allways checked and cleaned when finishing worker thread*/
	return(-1);
}

int
close_sound (struct paRecordData *pdata)
{
	int err = 0;
	int i= 0, j= 0;

	pdata->capVid = 0;

	switch(pdata->api)
	{
#ifdef PULSEAUDIO
		case PULSE:
			err = pulse_close_audio(pdata);
			break;
#endif
		case PORT:
		default:
			err = port_close_audio(pdata);
			break;
	}

	pdata->flush = 0;
	pdata->delay = 0; /*reset the audio delay*/

	/* ---------------------------------------------------------------------
	 * make sure no operations are performed on the buffers  */
	__LOCK_MUTEX(__AMUTEX);
		if(pdata->lavc_data)
			clean_lavc_audio(&(pdata->lavc_data));
		pdata->lavc_data = NULL;
		/*free primary buffer*/
		g_free( pdata->recordedSamples );
		pdata->recordedSamples=NULL;
		if(pdata->audio_buff)
		{
			for(j=0; j< AUDBUFF_NUM; j++)
			{
				for(i=0; i<AUDBUFF_SIZE; i++)
				{
					g_free(pdata->audio_buff[j][i].frame);
				}
				g_free(pdata->audio_buff[j]);
				pdata->audio_buff[j] = NULL;
			}
		}
		if(pdata->pcm_sndBuff) g_free(pdata->pcm_sndBuff);
		pdata->pcm_sndBuff = NULL;
	__UNLOCK_MUTEX(__AMUTEX);

	return (err);
}

/* saturate float samples to int16 limits*/
static gint16 clip_int16 (float in)
{
	in = (in < -32768) ? -32768 : (in > 32767) ? 32767 : in;

	return ((gint16) in);
}

void SampleConverter (struct paRecordData* pdata)
{
	if(pdata->lavc_data && pdata->lavc_data->codec_context->sample_fmt == AV_SAMPLE_FMT_FLT)
	{
		if(!(pdata->float_sndBuff))
			pdata->float_sndBuff = g_new0(float, pdata->aud_numSamples);

		int samp = 0;

		for(samp=0; samp < pdata->aud_numSamples; samp++)
		{
			pdata->float_sndBuff[samp] = pdata->audio_buff[pdata->br_ind][pdata->r_ind].frame[samp];
		}
	}
	else
	{
		if (!(pdata->pcm_sndBuff))
			pdata->pcm_sndBuff = g_new0(gint16, pdata->aud_numSamples);

		int samp = 0;

		for(samp=0; samp < pdata->aud_numSamples; samp++)
		{
			pdata->pcm_sndBuff[samp] = clip_int16(pdata->audio_buff[pdata->br_ind][pdata->r_ind].frame[samp] * 32767.0); //* 32768 + 385;
		}
	}
}

