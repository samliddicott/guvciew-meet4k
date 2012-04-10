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
#include "../config.h"

#ifdef PULSEAUDIO

#include <glib.h>
#include <string.h>
#include "audio_effects.h"
#include "ms_time.h"

#include <errno.h>

#define __AMUTEX &pdata->mutex

static void finish(struct paRecordData *pdata)
{
	g_print("audio thread exited\n");
	pdata->streaming = FALSE;
	if (pdata->pulse_simple)
		pa_simple_free(pdata->pulse_simple);
}

//run in separate thread
static void* pulse_read_audio(void *userdata) 
{
	int error = 0;
	int flag = 0;
	int skip_n = 0;
	struct paRecordData *pdata = (struct paRecordData*) userdata;
	UINT64 ts, nsec_per_frame;
	UINT64 framesPerBuffer;
	
	/* The sample type to use */
	pa_sample_spec ss;
	if (BIGENDIAN)
		ss.format = PA_SAMPLE_FLOAT32BE;
	else
		ss.format = PA_SAMPLE_FLOAT32LE;
	
	__LOCK_MUTEX(__AMUTEX);
		gboolean capVid = pdata->capVid;
		pdata->streaming = TRUE;
		ss.rate = pdata->samprate;
		ss.channels = pdata->channels;
	__UNLOCK_MUTEX(__AMUTEX);

	g_print("starting pulse audio thread: %d hz- %d ch\n",ss.rate, ss.channels);
	if (!(pdata->pulse_simple = pa_simple_new(NULL, "Guvcview Video Capture", PA_STREAM_RECORD, NULL, "pcm.record", &ss, NULL, NULL, &error))) 
	{
		g_printerr(": pa_simple_new() failed: %s\n", pa_strerror(error));
		finish(pdata);
		return (NULL);
	}
	
	/* buffer ends at timestamp "now", calculate beginning timestamp */
	nsec_per_frame = G_NSEC_PER_SEC / pdata->samprate;
	framesPerBuffer = (UINT64) pdata->aud_numSamples / pdata->channels;
	/* Record some data ... */
	while(capVid)
	{
		if (pa_simple_read(pdata->pulse_simple, pdata->recordedSamples, pdata->aud_numBytes, &error) < 0) 
		{
			g_printerr("pulse: pa_simple_read() failed: %s\n", pa_strerror(error));
			finish(pdata);
			return (NULL);
		}
		
		__LOCK_MUTEX(__AMUTEX);
			capVid = pdata->capVid;
			skip_n = pdata->skip_n;
			flag = pdata->audio_buff_flag[pdata->bw_ind];
		__UNLOCK_MUTEX(__AMUTEX);
		
		if(skip_n > 0)
		{
			pdata->snd_begintime = ns_time_monotonic(); /*reset first time stamp*/
			pdata->a_ts = 0;
			continue;
		}
		
		ts = ns_time_monotonic() - framesPerBuffer * nsec_per_frame;
		
		UINT64 buffer_length = (G_NSEC_PER_SEC * pdata->aud_numSamples)/(pdata->samprate * pdata->channels);
		/*first frame time stamp*/
		if(pdata->a_ts <= 0)
		{
			if((pdata->ts_ref > 0) && (pdata->ts_ref < pdata->snd_begintime)) 
				pdata->a_ts = pdata->snd_begintime - pdata->ts_ref;
			else pdata->a_ts = 1;
		}
		else /*increment time stamp for audio frame*/
			pdata->a_ts += buffer_length;
		
		/* check audio drift through timestamps */
		if (ts > pdata->snd_begintime)
			ts -= pdata->snd_begintime;
		else
			ts = 0;
		
		if (ts > buffer_length)
			ts -= buffer_length;
		else
			ts = 0;
		
		pdata->ts_drift = ts - pdata->a_ts;
		
		
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
						g_print("AUDIO: next buffer is not yet ready\n");
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
		}
	}

	finish(pdata);
	return (NULL);
}


int
pulse_init_audio(struct paRecordData* pdata)
{	
	//start audio capture thread
	if(__THREAD_CREATE(&pdata->pulse_read_th, (GThreadFunc) pulse_read_audio,pdata))  
	{
		g_printerr("Pulse thread creation failed\n");
		return (-1);
	}
	return (0);
} 

#endif
