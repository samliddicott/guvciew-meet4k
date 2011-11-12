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

#if GLIB_MINOR_VERSION < 31
	#define __AMUTEX pdata->mutex
#else
	#define __AMUTEX &pdata->mutex
#endif

//run in separate thread
static void* pulse_read_audio(void *userdata) 
{
	int error;
	
	struct paRecordData *pdata = (struct paRecordData*) userdata;
	/* The sample type to use */
	pa_sample_spec ss;
	if (BIGENDIAN)
		ss.format = PA_SAMPLE_FLOAT32BE;
	else
		ss.format = PA_SAMPLE_FLOAT32LE;
	
	__LOCK_MUTEX(__AMUTEX);
		gboolean capVid = pdata->capVid;
		int skip_n = pdata->skip_n;
		pdata->streaming = TRUE;
		ss.rate = pdata->samprate;
		ss.channels = pdata->channels;
	__UNLOCK_MUTEX(__AMUTEX);

	printf("starting pulse audio thread: %d hz- %d ch\n",ss.rate, ss.channels);
	if (!(pdata->pulse_simple = pa_simple_new(NULL, "Guvcview Video Capture", PA_STREAM_RECORD, NULL, "pcm.record", &ss, NULL, NULL, &error))) 
	{
		g_printerr(": pa_simple_new() failed: %s\n", pa_strerror(error));
		goto finish;
	}
	
	/* Record some data ... */
	while(capVid)
	{
		if (pa_simple_read(pdata->pulse_simple, pdata->recordedSamples, pdata->aud_numBytes, &error) < 0) 
		{
			g_printerr("pulse: pa_simple_read() failed: %s\n", pa_strerror(error));
			goto finish;
		}

		__LOCK_MUTEX(__AMUTEX);
			capVid = pdata->capVid;
			/*first frame time stamp*/
			if(pdata->a_ts <= 0)
			{
				if((pdata->ts_ref > 0) && (pdata->ts_ref < pdata->snd_begintime)) 
					pdata->a_ts = pdata->snd_begintime - pdata->ts_ref;
				else pdata->a_ts = 1;
			}
			else /*increment time stamp for audio frame*/
				pdata->a_ts += (G_NSEC_PER_SEC * pdata->aud_numSamples)/(pdata->samprate * pdata->channels);
	
			skip_n = pdata->skip_n;
		__UNLOCK_MUTEX(__AMUTEX);
		
		if (!skip_n) //skip audio while were skipping video frames
		{
			__LOCK_MUTEX( __AMUTEX );
				if(!pdata->audio_buff[pdata->w_ind].used)
				{
					
					/*copy data to audio buffer*/
					memcpy(pdata->audio_buff[pdata->w_ind].frame, pdata->recordedSamples, pdata->aud_numBytes);
					pdata->audio_buff[pdata->w_ind].time_stamp = pdata->a_ts + pdata->delay;
					pdata->audio_buff[pdata->w_ind].used = TRUE;
					NEXT_IND(pdata->w_ind, AUDBUFF_SIZE);
				}
				else
				{
					//drop audio data
					g_printerr("AUDIO: droping audio data\n");
				}
			__UNLOCK_MUTEX( __AMUTEX );
		}
		else 
		{
			pdata->snd_begintime = ns_time_monotonic();
			pdata->a_ts = 0;
		}
	}

finish:
	printf("audio thread exited\n");
	pdata->streaming = FALSE;
	if (pdata->pulse_simple)
		pa_simple_free(pdata->pulse_simple);
	return (NULL);
}


int
pulse_init_audio(struct paRecordData* pdata)
{	
	//start audio capture thread
#if GLIB_MINOR_VERSION < 31 
	if( (pdata->pulse_read_th = g_thread_create(
		(GThreadFunc) pulse_read_audio,
		pdata, //data
		FALSE,    //joinable - no need waiting for thread to finish
		NULL)    //error
		) == NULL)
#else
	if( (pdata->pulse_read_th = g_thread_new("pulse thread",
		(GThreadFunc) pulse_read_audio,
		pdata)
		) == NULL)
#endif  
	{
		g_printerr("Pulse thread creation failed\n");
		return (-1);
	}
	return (0);
} 

#endif
