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
#include <string.h>
#include <math.h>
#include "vcodecs.h"
#include "avilib.h"
#include "acodecs.h"
#include "lavc_common.h"
#include "audio_effects.h"
#include "ms_time.h"

static int fill_audio_buffer(struct paRecordData *data, UINT64 ts)
{
	int ret =0;
	UINT64 buffer_length;

	if(data->sampleIndex >= data->aud_numSamples)
	{
		buffer_length = (G_NSEC_PER_SEC * data->aud_numSamples)/(data->samprate * data->channels);

		/*first frame time stamp*/
		if(data->a_ts <= 0)
		{
			/*if sound begin time > first video frame ts then sync audio to video
			* else set audio ts to aprox. the video ts */
			if((data->ts_ref > 0) && (data->ts_ref < data->snd_begintime)) 
				data->a_ts = data->snd_begintime - data->ts_ref;
			else data->a_ts = 1; /*make it > 0 otherwise we will keep getting the same ts*/
		}
		else /*increment time stamp for audio frame*/
			data->a_ts += buffer_length;

		/* check audio drift through timestamps */
		if (ts > data->snd_begintime)
			ts -= data->snd_begintime;
		else
			ts = 0;
		if (ts > buffer_length)
			ts -= buffer_length;
		else
			ts = 0;
		data->ts_drift = ts - data->a_ts;
		
		data->sampleIndex = 0; /*reset*/
		if(!data->audio_buff[data->w_ind].used)
		{
			/*copy data to audio buffer*/
			memcpy(data->audio_buff[data->w_ind].frame, data->recordedSamples, data->aud_numBytes);
			data->audio_buff[data->w_ind].time_stamp = data->a_ts + data->delay;
			data->audio_buff[data->w_ind].used = TRUE;
			NEXT_IND(data->w_ind, AUDBUFF_SIZE);
		}
		else
		{
			/*drop audio data*/
			ret = -1;
			g_printerr("AUDIO: droping audio data\n");
		}
	}
	
	return ret;
}

/*--------------------------- sound callback ------------------------------*/
int 
recordCallback (const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData )
{
	struct paRecordData *data = (struct paRecordData*)userData;

	const SAMPLE *rptr = (const SAMPLE*)inputBuffer;
	int i;
	UINT64 ts, nsec_per_frame;

	/* buffer ends at timestamp "now", calculate beginning timestamp */
	nsec_per_frame = G_NSEC_PER_SEC / data->samprate;
	ts = ns_time_monotonic() - (UINT64) framesPerBuffer * nsec_per_frame;

	g_mutex_lock( data->mutex );
		gboolean capVid = data->capVid;
		int channels = data->channels;
		int skip_n = data->skip_n;
	g_mutex_unlock( data->mutex );
	
	if (skip_n > 0) /*skip audio while were skipping video frames*/
	{
		
		if(capVid) 
		{
			g_mutex_lock( data->mutex );
				data->snd_begintime = ns_time_monotonic(); /*reset first time stamp*/
			g_mutex_unlock( data->mutex );
			return (paContinue); /*still capturing*/
		}
		else
		{	g_mutex_lock( data->mutex );
				data->streaming=FALSE;
			g_mutex_lock( data->mutex );
			return (paComplete);
		}
	}
	
	int numSamples= framesPerBuffer * channels;

	g_mutex_lock( data->mutex );
		/*set to FALSE on paComplete*/
		data->streaming=TRUE;

		for( i=0; i<numSamples; i++ )
		{
			data->recordedSamples[data->sampleIndex] = inputBuffer ? *rptr++ : 0;
			data->sampleIndex++;
		
			fill_audio_buffer(data, ts);

			/* increment timestamp accordingly while copying */
			if (i % channels == 0)
				ts += nsec_per_frame;
		}
		
	g_mutex_unlock( data->mutex );

	if(capVid) return (paContinue); /*still capturing*/
	else 
	{
		g_mutex_lock( data->mutex );
			data->streaming=FALSE;
		g_mutex_unlock( data->mutex );
		return (paComplete);
	}
	
}

void
set_sound (struct GLOBAL *global, struct paRecordData* data, void *lav_aud_data) 
{
	struct lavcAData **lavc_data = (struct lavcAData **) lav_aud_data;
	
	if(global->Sound_SampRateInd==0)
		global->Sound_SampRate=global->Sound_IndexDev[global->Sound_UseDev].samprate;/*using default*/
	
	if(global->Sound_NumChanInd==0) 
	{
		/*using default if channels <3 or stereo(2) otherwise*/
		global->Sound_NumChan=(global->Sound_IndexDev[global->Sound_UseDev].chan < 3) ? 
			global->Sound_IndexDev[global->Sound_UseDev].chan : 2;
	}
	
	data->audio_buff = NULL;
	data->recordedSamples = NULL;
	
	data->samprate = global->Sound_SampRate;
	data->channels = global->Sound_NumChan;
	g_mutex_lock( data->mutex );
		data->skip_n = global->skip_n; /*initial video frames to skip*/
	g_mutex_unlock( data->mutex );
	if(global->debug) g_printf("using audio codec: 0x%04x\n",global->Sound_Format );
	switch (global->Sound_Format)
	{
		case PA_FOURCC:
		{
			data->aud_numSamples = MPG_NUM_SAMP * data->channels;
			break;
		}
		default:
		{
			/*initialize lavc data*/
			if(!(*lavc_data)) 
			{
				*lavc_data = init_lavc_audio(data, get_ind_by4cc(global->Sound_Format));
			}
			/*use lavc audio codec frame size to determine samples*/
			data->aud_numSamples = (*lavc_data)->codec_context->frame_size * data->channels;
			break;
		}
	}
	
	data->aud_numBytes = data->aud_numSamples * sizeof(SAMPLE);
	data->input_type = PA_SAMPLE_TYPE;
	data->mp2Buff = NULL;
	
	data->sampleIndex = 0;
	
	data->flush = 0;
	data->a_ts= 0;
	data->ts_ref = 0;
	
	data->stream = NULL;
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
		data->delay = (UINT64) 2*(fps_num * G_NSEC_PER_SEC / fps_den); /*2 frame delay in nanosec*/
	data->delay += global->Sound_delay; /*add predefined delay - def = 0*/
	
	/*reset the indexes*/	
	data->r_ind = 0;
	data->w_ind = 0;
	/*buffer for video PCM 16 bits*/
	data->pcm_sndBuff=NULL;
	/*set audio device to use*/
	data->inputParameters.device = global->Sound_IndexDev[global->Sound_UseDev].id; /* input device */
}

int
init_sound(struct paRecordData* data)
{
	PaError err = paNoError;
	int i=0;
	
	/*alloc audio ring buffer*/
    	if(!(data->audio_buff))
    	{
		data->audio_buff = g_new0(AudBuff, AUDBUFF_SIZE);
		for(i=0; i<AUDBUFF_SIZE; i++)
			data->audio_buff[i].frame = g_new0(SAMPLE, data->aud_numSamples);
	}
	
	/*alloc the callback buffer*/
	data->recordedSamples = g_new0(SAMPLE, data->aud_numSamples);
	
	switch(data->api)
	{
#ifdef PULSEAUDIO
		case PULSE:
			if(err = pulse_init_audio(data))
				goto error;
			break;
#endif
		case PORT:
		default:
			if(data->stream)
			{
				if( !(Pa_IsStreamStopped( data->stream )))
				{
					Pa_AbortStream( data->stream );
					Pa_CloseStream( data->stream );
					data->stream = NULL;
				}
			}
				
			data->inputParameters.channelCount = data->channels;
			data->inputParameters.sampleFormat = PA_SAMPLE_TYPE;
			if (Pa_GetDeviceInfo( data->inputParameters.device ))
				data->inputParameters.suggestedLatency = Pa_GetDeviceInfo( data->inputParameters.device )->defaultHighInputLatency;
			else
				data->inputParameters.suggestedLatency = DEFAULT_LATENCY_DURATION/1000.0;
			data->inputParameters.hostApiSpecificStreamInfo = NULL; 
	
			/*---------------------------- start recording Audio. ----------------------------- */
	
			err = Pa_OpenStream(
				&data->stream,
				&data->inputParameters,
				NULL,                  /* &outputParameters, */
				data->samprate,        /* sample rate        */
				MPG_NUM_SAMP,          /* buffer in frames => Mpeg frame size (samples = 1152 samples * channels)*/
				paNoFlag,              /* PaNoFlag - clip and dhiter*/
				recordCallback,        /* sound callback     */
				data );                /* callback userData  */
	
			if( err != paNoError ) goto error;
	
			err = Pa_StartStream( data->stream );
			if( err != paNoError ) goto error; /*should close the stream if error ?*/
			break;
	}
	
	/*sound start time - used to sync with video*/
	data->snd_begintime = ns_time_monotonic();

	return (0);
error:
	g_printerr("An error occured while starting the audio API\n" );
	g_printerr("Error number: %d\n", err );
	if(data->api == PORT) g_printerr("Error message: %s\n", Pa_GetErrorText( err ) ); 
	data->streaming=FALSE;
	data->flush=0;
	data->delay=0;
	if(data->api == PORT)
	{
		if(data->stream) Pa_AbortStream( data->stream );
	}
	if(data->recordedSamples) g_free( data->recordedSamples );
	data->recordedSamples=NULL;
	if(data->audio_buff)
	{
		for(i=0; i<AUDBUFF_SIZE; i++)
			g_free(data->audio_buff[i].frame);
		g_free(data->audio_buff);
	}
	data->audio_buff = NULL;
	/*lavc is allways checked and cleaned when finishing worker thread*/
	return(-1);
} 

int
close_sound (struct paRecordData *data) 
{
	int err = 0;
	int ret = 0;
	int i   = 0;
    
	data->capVid = 0;
	
	/*stops and closes the audio stream*/
	if(data->stream)
	{
		if(Pa_IsStreamActive( data->stream ) > 0)
		{
			g_printf("Aborting audio stream\n");
			err = Pa_AbortStream( data->stream );
		}
		else
		{
			g_printf("Stoping audio stream\n");
			err = Pa_StopStream( data->stream );
		}
		if( err != paNoError )
		{
			g_printerr("An error occured while stoping the audio stream\n" );
			g_printerr("Error number: %d\n", err );
			g_printerr("Error message: %s\n", Pa_GetErrorText( err ) );
			ret = -1;
		}
	}
	
	if(data->api == PORT)
	{
		g_printf("Closing audio stream...\n");
		err = Pa_CloseStream( data->stream );
		if( err != paNoError )
		{
			g_printerr("An error occured while closing the audio stream\n" );
			g_printerr("Error number: %d\n", err );
			g_printerr("Error message: %s\n", Pa_GetErrorText( err ) );
			ret = -1;
		}
	}
	data->stream = NULL;
	data->flush = 0;
	data->delay = 0; /*reset the audio delay*/
	
	/* ---------------------------------------------------------------------
	 * make sure no operations are performed on the buffers  */
	g_mutex_lock(data->mutex);
		/*free primary buffer*/
		g_free( data->recordedSamples );
		data->recordedSamples=NULL;
		if(data->audio_buff)
		{
			for(i=0; i<AUDBUFF_SIZE; i++)
				g_free(data->audio_buff[i].frame);
			g_free(data->audio_buff);
		}
		data->audio_buff = NULL;
		if(data->pcm_sndBuff) g_free(data->pcm_sndBuff);
		data->pcm_sndBuff = NULL;
	g_mutex_unlock(data->mutex);
	
	return (ret);
}

/* saturate float samples to int16 limits*/
static gint16 clip_int16 (float in)
{
	in = (in < -32768) ? -32768 : (in > 32767) ? 32767 : in;
	
	return ((gint16) in);
}

void Float2Int16 (struct paRecordData* data, AudBuff *proc_buff)
{
	if (!(data->pcm_sndBuff)) 
		data->pcm_sndBuff = g_new0(gint16, data->aud_numSamples);
		
	int samp = 0;
	
	for(samp=0; samp < data->aud_numSamples; samp++)
	{
		data->pcm_sndBuff[samp] = clip_int16(proc_buff->frame[samp] * 32767.0); //* 32768 + 385;
	}
}

