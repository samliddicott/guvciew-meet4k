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
#include "audio_effects.h"
#include "ms_time.h"

static int fill_audio_buffer(struct paRecordData *data, int64_t tstamp)
{
	int ret =0;
	/* first frame time stamp*/
	if((data->a_ts == 0) && (data->ts_ref < data->snd_begintime)) data->a_ts= data->snd_begintime - data->ts_ref;
	if(data->sampleIndex >= data->aud_numSamples)
	{
		data->sampleIndex = 0; //reset
		if(!data->audio_buff[data->w_ind].used)
		{
			/*copy data to audio buffer*/
			memcpy(data->audio_buff[data->w_ind].frame, data->recordedSamples, data->aud_numSamples*sizeof(SAMPLE));
			data->audio_buff[data->w_ind].time_stamp = data->a_ts;
			data->audio_buff[data->w_ind].used = TRUE;
			NEXT_IND(data->w_ind, AUDBUFF_SIZE);
		}
		else
		{
			//drop audio data
			ret = -1;
			g_printerr("AUDIO: droping audio data\n");
		}
		if((data->ts_ref > 0) && (data->ts_ref < tstamp)) data->a_ts= tstamp - data->ts_ref; //timestamp for next callback
		else data->a_ts = tstamp - data->snd_begintime;
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
	
	//time stamps
	int64_t tstamp = ns_time();
	
	if (data->skip_n > 0) //skip audio while were skipping video frames
	{
		
		if(data->capVid) return (paContinue); /*still capturing*/
		else
		{
			data->streaming=FALSE;
			return (paComplete);
		}
	}
	
	int numSamples= framesPerBuffer * data->channels;

	/*set to FALSE on paComplete*/    
	data->streaming=TRUE;

	g_mutex_lock( data->mutex );
		if( inputBuffer == NULL )
		{
			for( i=0; i<numSamples; i++ )
			{
				data->recordedSamples[data->sampleIndex] = 0;/*silence*/
				data->sampleIndex++;
			
				fill_audio_buffer(data, tstamp);
			}
		}
		else
		{
			for( i=0; i<numSamples; i++ )
			{
				data->recordedSamples[data->sampleIndex] = *rptr++;
				data->sampleIndex++;
			
				fill_audio_buffer(data, tstamp);
			}
		}
		
	g_mutex_unlock( data->mutex );

	if(data->capVid) return (paContinue); /*still capturing*/
	else 
	{
		data->streaming=FALSE;
		return (paComplete);
	}
	
}

void
set_sound (struct GLOBAL *global, struct paRecordData* data) 
{
	//int totalFrames;
	//int MP2Frames=0;
	int i=0;

	if(global->Sound_SampRateInd==0)
		global->Sound_SampRate=global->Sound_IndexDev[global->Sound_UseDev].samprate;/*using default*/
	
	if(global->Sound_NumChanInd==0) 
	{
		/*using default if channels <3 or stereo(2) otherwise*/
		global->Sound_NumChan=(global->Sound_IndexDev[global->Sound_UseDev].chan < 3) ? 
			global->Sound_IndexDev[global->Sound_UseDev].chan : 2;
	}
	
	data->samprate = global->Sound_SampRate;
	data->channels = global->Sound_NumChan;
	data->skip_n = global->skip_n; //inital video frames to skip
	//data->ts_ref = global->Vidstarttime; //maybe not set yet
	int mfactor = round(data->samprate/16000);
	if( mfactor < 1 ) mfactor = 1;
	data->aud_numSamples = mfactor * MPG_NUM_FRAMES * (MPG_NUM_SAMP * data->channels); //  MPG frames
	
	data->input_type = PA_SAMPLE_TYPE;
	data->mp2Buff = NULL;
	
	data->aud_numBytes = data->aud_numSamples * sizeof(SAMPLE);
	
	data->recordedSamples = g_new0(SAMPLE, data->aud_numSamples);
	data->sampleIndex = 0;
	
	data->flush = 0;
	data->a_ts= 0;
	
	data->stream = NULL;
	
	/*alloc audio ring buffer*/
	data->audio_buff = g_new0(AudBuff, AUDBUFF_SIZE);
	for(i=0; i<AUDBUFF_SIZE; i++)
		data->audio_buff[i].frame = g_new0(SAMPLE, data->aud_numSamples);
	
	//reset the indexes	
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
    
	switch(data->api)
	{
#ifdef PULSEAUDIO
		case PULSE:
			if(pulse_init_audio(data))
				goto error;
			break;
#endif
		case PORT:
		default:
			//err = Pa_Initialize();
			//if( err != paNoError ) goto error;
			if(data->stream)
			{
				if( !(Pa_IsStreamStopped( data->stream )))
				{
					Pa_AbortStream( data->stream );
					Pa_CloseStream( data->stream );
					data->stream = NULL;
				}
			}
				
			/* Record for a few seconds. */
			data->inputParameters.channelCount = data->channels;
			data->inputParameters.sampleFormat = PA_SAMPLE_TYPE;
			if (Pa_GetDeviceInfo( data->inputParameters.device ))
				data->inputParameters.suggestedLatency = Pa_GetDeviceInfo( data->inputParameters.device )->defaultHighInputLatency;
			else
				data->inputParameters.suggestedLatency = DEFAULT_LATENCY_DURATION/1000.0;
			data->inputParameters.hostApiSpecificStreamInfo = NULL; 
	
			/*---------------------------- Record some audio. ----------------------------- */
	
			err = Pa_OpenStream(
				&data->stream,
				&data->inputParameters,
				NULL,                  /* &outputParameters, */
				data->samprate,
				MPG_NUM_SAMP,            // buffer in frames => Mpeg frame size (samples = 1152 samples * channels)
				//paFramesPerBufferUnspecified,       // buffer Size - set by portaudio
				//paClipOff | paDitherOff, 
				paNoFlag,      /* PaNoFlag - clip and dhiter*/
				recordCallback, /* sound callback */
				data ); /* callback userData */
	
			if( err != paNoError ) goto error;
	
			err = Pa_StartStream( data->stream );
			if( err != paNoError ) goto error; /*should close the stream if error ?*/
			break;
	}
	
	/*sound start time - used to sync with video*/
	data->snd_begintime = ns_time();

	return (0);
error:
	g_printerr("An error occured while starting portaudio\n" );
	g_printerr("Error number: %d\n", err );
	g_printerr("Error message: %s\n", Pa_GetErrorText( err ) ); 
	data->streaming=0;
	data->flush=0;
	if(data->api < 1)
	{
		if(data->stream) Pa_AbortStream( data->stream );
	}
	g_free( data->recordedSamples );
	data->recordedSamples=NULL;
	if(data->audio_buff)
	{
		for(i=0; i<AUDBUFF_SIZE; i++)
			g_free(data->audio_buff[i].frame);
		g_free(data->audio_buff);
	}
	data->audio_buff = NULL;

	return(-1);
} 

int
close_sound (struct paRecordData *data) 
{
	int err =0;
	int i=0;
    
	data->capVid = 0;
	/*make sure we have stoped streaming - IO thread also checks for this*/
	int stall = wait_ms( &data->streaming, FALSE, 10, 50 );
	if(!(stall)) 
	{
		g_printerr("WARNING:sound capture stall (still streaming(%d)) \n",
			data->streaming);
			data->streaming = FALSE;
	}
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
		if( err != paNoError ) goto error;
	}
	if(data->api < 1)
	{
		g_printf("Closing audio stream...\n");
		err = Pa_CloseStream( data->stream );
		if( err != paNoError ) goto error; 
	}
	data->stream = NULL;
	data->flush = 0;

	/*---------------------------------------------------------------------*/
	/*make sure no operations are performed on the buffers*/
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
	
		if(data->mp2Buff) g_free(data->mp2Buff);
		data->mp2Buff = NULL;
		if(data->pcm_sndBuff) g_free(data->pcm_sndBuff);
		data->pcm_sndBuff = NULL;
	g_mutex_unlock(data->mutex);
	
	return (0);
error:  
	g_printerr("An error occured while closing the portaudio stream\n" );
	g_printerr("Error number: %d\n", err );
	g_printerr("Error message: %s\n", Pa_GetErrorText( err ) );
	data->flush=0;
	data->streaming=FALSE;
	g_mutex_lock( data->mutex);
		g_free( data->recordedSamples );
		data->recordedSamples=NULL;
		if(data->api < 1) 
		{
			Pa_CloseStream( data->stream );
		}
		data->stream = NULL;
		
		if(data->audio_buff)
		{
			for(i=0; i<AUDBUFF_SIZE; i++)
				g_free(data->audio_buff[i].frame);
			g_free(data->audio_buff);
		}
		data->audio_buff = NULL;
	
		if(data->mp2Buff) g_free(data->mp2Buff);
		data->mp2Buff = NULL;
		if(data->pcm_sndBuff) g_free(data->pcm_sndBuff);
		data->pcm_sndBuff = NULL;
	g_mutex_unlock( data->mutex );
	return(-1);
}

/* saturate float samples to int16 limits*/
static gint16 clip_int16 (float in)
{
	in = (in < -32768) ? -32768 : (in > 32767) ? 32767 : in;
	
	return ((gint16) in);
}

void Float2Int16 (struct paRecordData* data, AudBuff *proc_buff)
{
	if (data->pcm_sndBuff == NULL) 
		data->pcm_sndBuff = g_new0(gint16, data->aud_numSamples);
	
	float res = 0.0;
	int samp = 0;
	
	for(samp=0; samp < data->aud_numSamples; samp++)
	{
		res = proc_buff->frame[samp] * 32768 + 385;
		/*clip*/
		data->pcm_sndBuff[samp] = clip_int16(res);
	}
}

