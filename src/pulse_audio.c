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
#include "../config.h"

#ifdef PULSEAUDIO

#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>
#include "audio_effects.h"
#include "ms_time.h"

#include <errno.h>

//run in separate thread
static void* pulse_read_audio(void *userdata) 
{
	int error;
	
	struct paRecordData *data = (struct paRecordData*) userdata;
	//time stamps
	int64_t tstamp = 0;
	
	/* The sample type to use */
	pa_sample_spec ss;
	if (BIGENDIAN)
		ss.format = PA_SAMPLE_FLOAT32BE;
	else
		ss.format = PA_SAMPLE_FLOAT32LE;
	ss.rate = data->samprate;
	ss.channels = data->channels;

	printf("starting pulse audio thread: %d hz- %d ch\n",ss.rate, ss.channels);
	if (!(data->pulse_simple = pa_simple_new(NULL, "Guvcview Video Capture", PA_STREAM_RECORD, NULL, "pcm.record", &ss, NULL, NULL, &error))) 
	{
		g_printerr(": pa_simple_new() failed: %s\n", pa_strerror(error));
		goto finish;
	}
	
	data->streaming = TRUE;
	/* Record some data ... */
	while(data->capVid)
	{
		if (pa_simple_read(data->pulse_simple, data->recordedSamples, data->aud_numBytes, &error) < 0) 
		{
			g_printerr("pulse: pa_simple_read() failed: %s\n", pa_strerror(error));
			goto finish;
		}
		//time stamps
		tstamp = ns_time();
		/*first frame time stamp*/
		if((data->a_ts == 0) && (data->ts_ref > 0) && (data->ts_ref < data->snd_begintime)) 
			data->a_ts= data->snd_begintime - data->ts_ref;
		
		if (!data->skip_n) //skip audio while were skipping video frames
		{
			g_mutex_lock( data->mutex );
			if(!data->audio_buff[data->w_ind].used)
			{
				/*copy data to audio buffer*/
				memcpy(data->audio_buff[data->w_ind].frame, data->recordedSamples, data->aud_numBytes);
				data->audio_buff[data->w_ind].time_stamp = data->a_ts;
				data->audio_buff[data->w_ind].used = TRUE;
				NEXT_IND(data->w_ind, AUDBUFF_SIZE);
			}
			else
			{
				//drop audio data
				g_printerr("AUDIO: droping audio data\n");
			}
			if((data->ts_ref > 0) && (data->ts_ref < tstamp)) data->a_ts= tstamp - data->ts_ref; //timestamp for next callback
			else data->a_ts = tstamp - data->snd_begintime;
			g_mutex_unlock( data->mutex );
		}
		else data->snd_begintime = tstamp;
	}

finish:
	printf("audio thread exited\n");
	data->streaming = FALSE;
	if (data->pulse_simple)
		pa_simple_free(data->pulse_simple);
	return (NULL);
}


int
pulse_init_audio(struct paRecordData* data)
{
	GError *err1 = NULL;
	
	//start audio capture thread 
	if( (data->pulse_read_th = g_thread_create((
		GThreadFunc) pulse_read_audio,
		data, //data
		FALSE,    //joinable - no need waiting for thread to finish
		&err1)    //error
		) == NULL)  
	{
		g_printerr("Thread create failed: %s!!\n", err1->message );
		g_error_free ( err1 ) ;
		return (-1);
	}
	return (0);
} 

#endif
