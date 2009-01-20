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

#include "sound.h"
#include "ms_time.h"
#include "globals.h"
#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <portaudio.h>

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

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
	int numSamples=framesPerBuffer*data->channels;

	/*set to zero on paComplete*/    
	data->streaming=1;

	if( inputBuffer == NULL )
	{
		for( i=0; i<numSamples; i++ )
		{
			data->recordedSamples[data->sampleIndex] = 0;/*silence*/
			data->sampleIndex++;
		}
	}
	else
	{
		for( i=0; i<numSamples; i++ )
		{
			data->recordedSamples[data->sampleIndex] = *rptr++;
			data->sampleIndex++;
		}
	}

	data->numSamples += numSamples;
	if (data->numSamples > (data->maxIndex-2*numSamples)) 
	{
		//primary buffer near limit (don't wait for overflow)
		//or video capture stopped
		//copy data to secondary buffer and restart primary buffer index
		//the buffer is only writen every 1sec or so, plenty of time for a read to complete,
		//anyway lock a mutex on the buffer just in case a read operation is still going on.
		// This is not a good idea as it may cause data loss
		//but since we sould never have to wait, it shouldn't be a problem.
		g_mutex_lock( data->mutex );
			data->snd_numSamples = data->numSamples;
			data->snd_numBytes = data->numSamples*sizeof(SAMPLE);
			memcpy(data->avi_sndBuff, data->recordedSamples ,data->snd_numBytes);
			/*flags that secondary buffer as data (can be saved to file)*/
			data->audio_flag=1;
		g_mutex_unlock( data->mutex );
		data->sampleIndex=0;
		data->numSamples = 0;
	}

	if(data->capAVI) return (paContinue); /*still capturing*/
	else 
	{
		/*recording stopped*/
		if(!(data->audio_flag)) 
		{
			/*need to copy remaining audio to secondary buffer*/
			g_mutex_lock( data->mutex);
				data->snd_numSamples = data->numSamples;
				data->snd_numBytes = data->numSamples*sizeof(SAMPLE);
				memcpy(data->avi_sndBuff, data->recordedSamples ,data->snd_numBytes);
				/*flags that secondary buffer as data (can be saved to file)*/
				data->audio_flag=1;
			g_mutex_unlock( data->mutex);
			data->sampleIndex=0;
			data->numSamples = 0;
		}
		data->streaming=0;
		return (paComplete);
	}
}

void
set_sound (struct GLOBAL *global, struct paRecordData* data) 
{
	if(global->Sound_SampRateInd==0)
		global->Sound_SampRate=global->Sound_IndexDev[global->Sound_UseDev].samprate;/*using default*/
	
	if(global->Sound_NumChanInd==0) 
	{
		/*using default if channels <3 or stereo(2) otherwise*/
		global->Sound_NumChan=(global->Sound_IndexDev[global->Sound_UseDev].chan<3)?global->Sound_IndexDev[global->Sound_UseDev].chan:2;
	}
	
	data->samprate = global->Sound_SampRate;
	data->channels = global->Sound_NumChan;
	data->numsec = global->Sound_NumSec;
	/*set audio device to use*/
	data->inputParameters.device = global->Sound_IndexDev[global->Sound_UseDev].id; /* input device */
}

/*no need of extra thread can be set in video thread*/
int
init_sound(struct paRecordData* data)
{
	PaError err;
	//int i;
	int totalFrames;
	int MP2Frames=0;
	int numSamples;

	/* setting maximum buffer size*/
	totalFrames = data->numsec * data->samprate;
	numSamples = totalFrames * data->channels;
	/*round to libtwolame Frames (1 Frame = 1152 samples)*/
	MP2Frames=numSamples/1152;
	numSamples=MP2Frames*1152;
	
	data->input_type = PA_SAMPLE_TYPE;
	data->mp2Buff = NULL;
	
	data->snd_numBytes = numSamples * sizeof(SAMPLE);
	
	data->recordedSamples = g_new0(SAMPLE, numSamples);
	data->maxIndex = numSamples;
	data->sampleIndex = 0;
	
	data->audio_flag = 0;
	data->flush = 0;
	data->streaming = 0;
	
	/*secondary shared buffer*/
	data->avi_sndBuff = g_new0(SAMPLE, numSamples);
	/*buffer for avi PCM*/
	data->avi_sndBuff1=NULL;
	/*delay buffers - for audio effects */
	data->delayBuff1 = NULL;
	data->delayBuff2 = NULL;
	data->delayIndex = 0;
	
	data->CombBuff10 = NULL;
	data->CombBuff11 = NULL;
	data->CombBuff20 = NULL;
	data->CombBuff21 = NULL;
	data->CombBuff30 = NULL;
	data->CombBuff31 = NULL;
	data->CombBuff40 = NULL;
	data->CombBuff41 = NULL;
	
	data->CombIndex1 = 0;
	data->CombIndex2 = 0;
	data->CombIndex3 = 0;
	data->CombIndex4 = 0;
	
	data->AllPassBuff1 = NULL;
	data->AllPassBuff2 = NULL;
	
	data->AllPassIndex = 0;
	data->wahData = NULL;
	
	err = Pa_Initialize();
	if( err != paNoError ) goto error;
	
	/* Record for a few seconds. */
	data->inputParameters.channelCount = data->channels;
	data->inputParameters.sampleFormat = PA_SAMPLE_TYPE;
	data->inputParameters.suggestedLatency = Pa_GetDeviceInfo( data->inputParameters.device )->defaultLowInputLatency;
	data->inputParameters.hostApiSpecificStreamInfo = NULL; 
	
	/*---------------------------- Record some audio. ----------------------------- */
	
	err = Pa_OpenStream(
		&data->stream,
		&data->inputParameters,
		NULL,                  /* &outputParameters, */
		data->samprate,
		paFramesPerBufferUnspecified,/* buffer Size - set by portaudio*/
		paNoFlag,      /* PaNoFlag - clip and dhiter*/
		recordCallback, /* sound callback */
		data ); /* callback userData */
	
	if( err != paNoError ) goto error;
	
	err = Pa_StartStream( data->stream );
	if( err != paNoError ) goto error; /*should close the stream if error ?*/
	
	/*sound start time - used to sync with video*/
	data->snd_begintime = ms_time();

	return (0);
error:
	g_printerr("An error occured while using the portaudio stream\n" );
	g_printerr("Error number: %d\n", err );
	g_printerr("Error message: %s\n", Pa_GetErrorText( err ) ); 
	data->streaming=0;
	data->flush=0;
	Pa_Terminate();
	g_free( data->recordedSamples );
	data->recordedSamples=NULL;
	g_free(data->avi_sndBuff);
	g_free(data->avi_sndBuff1);
	
	g_free(data->delayBuff1);
	g_free(data->delayBuff2);
	
	g_free(data->CombBuff10);
	g_free(data->CombBuff11);
	g_free(data->CombBuff20);
	g_free(data->CombBuff21);
	g_free(data->CombBuff30);
	g_free(data->CombBuff31);
	g_free(data->CombBuff40);
	g_free(data->CombBuff41);
	
	g_free(data->AllPassBuff1);
	g_free(data->AllPassBuff2);
	
	g_free(data->wahData);
	data->delayBuff1 = NULL;
	data->delayBuff2 = NULL;
	
	data->CombBuff10 = NULL;
	data->CombBuff11 = NULL;
	data->CombBuff20 = NULL;
	data->CombBuff21 = NULL;
	data->CombBuff30 = NULL;
	data->CombBuff31 = NULL;
	data->CombBuff40 = NULL;
	data->CombBuff41 = NULL;
	
	data->AllPassBuff1 = NULL;
	data->AllPassBuff2 = NULL;
	
	data->avi_sndBuff1=NULL;
	data->avi_sndBuff=NULL;
	data->wahData = NULL;
	
	return(-1);
} 

int
close_sound (struct paRecordData *data) 
{
	int err =0;
	/*stops and closes the audio stream*/
	err = Pa_StopStream( data->stream );
	if( err != paNoError ) goto error;
	
	err = Pa_CloseStream( data->stream );
	if( err != paNoError ) goto error; 
	
	/*make sure we stoped streaming */
	int stall = wait_ms( &data->streaming, FALSE, 10, 30 );
	if(!(stall)) 
	{
		g_printerr("WARNING:sound capture stall (still streaming(%d) \n",
			data->streaming);
		data->streaming = 0;
	}
	if(data->audio_flag) 
		g_printerr("Droped %i bytes of audio data\n", 
			data->snd_numBytes);
	data->audio_flag=0;
	data->flush = 0;
	/*---------------------------------------------------------------------*/
	/*make sure no operations are performed on the buffers*/
	g_mutex_lock( data->mutex);
		/*free primary buffer*/
		g_free( data->recordedSamples  );
		data->recordedSamples=NULL;
	
		Pa_Terminate();
	
		g_free(data->avi_sndBuff);
		data->avi_sndBuff = NULL;
		g_free(data->delayBuff1);
		g_free(data->delayBuff2);
		
		g_free(data->CombBuff10);
		g_free(data->CombBuff11);
		g_free(data->CombBuff20);
		g_free(data->CombBuff21);
		g_free(data->CombBuff30);
		g_free(data->CombBuff31);
		g_free(data->CombBuff40);
		g_free(data->CombBuff41);
		
		data->delayBuff1 = NULL;
		data->delayBuff2 = NULL;
		
		data->CombBuff10 = NULL;
		data->CombBuff11 = NULL;
		data->CombBuff20 = NULL;
		data->CombBuff21 = NULL;
		data->CombBuff30 = NULL;
		data->CombBuff31 = NULL;
		data->CombBuff40 = NULL;
		data->CombBuff41 = NULL;

		g_free(data->mp2Buff);
		data->mp2Buff = NULL;
		g_free(data->AllPassBuff1);
		data->AllPassBuff1 = NULL;
		g_free(data->AllPassBuff2);
		data->AllPassBuff2 = NULL;
	
		g_free(data->avi_sndBuff1);
		data->avi_sndBuff1 = NULL;
		g_free(data->wahData);
		data->wahData = NULL;
	g_mutex_unlock( data->mutex );
	
	return (0);
error:  
	g_printerr("An error occured while closing the portaudio stream\n" );
	g_printerr("Error number: %d\n", err );
	g_printerr("Error message: %s\n", Pa_GetErrorText( err ) );
	data->flush=0;
	data->audio_flag=0;
	data->streaming=0;
	g_mutex_lock( data->mutex);
		g_free( data->recordedSamples );
		data->recordedSamples=NULL;
		Pa_Terminate();
		g_free(data->avi_sndBuff);
		data->avi_sndBuff = NULL;
		g_free(data->delayBuff1);
		g_free(data->delayBuff2);
	
		g_free(data->CombBuff10);
		g_free(data->CombBuff11);
		g_free(data->CombBuff20);
		g_free(data->CombBuff21);
		g_free(data->CombBuff30);
		g_free(data->CombBuff31);
		g_free(data->CombBuff40);
		g_free(data->CombBuff41);
		
		g_free(data->AllPassBuff1);
		g_free(data->AllPassBuff2);
	
		data->delayBuff1 = NULL;
		data->delayBuff2 = NULL;
	
		data->CombBuff10 = NULL;
		data->CombBuff11 = NULL;
		data->CombBuff20 = NULL;
		data->CombBuff21 = NULL;
		data->CombBuff30 = NULL;
		data->CombBuff31 = NULL;
		data->CombBuff40 = NULL;
		data->CombBuff41 = NULL;
	
		data->AllPassBuff1 = NULL;
		data->AllPassBuff2 = NULL;
	
		g_free(data->mp2Buff);
		data->mp2Buff = NULL;
		g_free(data->avi_sndBuff1);
		data->avi_sndBuff1 = NULL;
		g_free(data->wahData);
		data->wahData=NULL;
	g_mutex_unlock( data->mutex );
	return(-1);
}

void Float2Int16 (struct paRecordData* data)
{
	if (data->avi_sndBuff1 == NULL) 
		data->avi_sndBuff1 = g_new0(short, data->maxIndex);
	
	int samp = 0;
	for(samp=0; samp<data->snd_numSamples; samp++)
	{
		data->avi_sndBuff1[samp] = (short) (data->avi_sndBuff[samp] * 32768 + 385);
		//data->avi_sndBuff1[samp] = (short) lrintf(data->avi_sndBuff[samp] * 0x7FFF);
	}
}

/*--------------------------- Effects ------------------------------------------*/
/* Echo effect */
/*delay_ms: echo delay in ms*/
/*decay: feedback gain (<1) */
void Echo(struct paRecordData* data, int delay_ms, float decay)
{
	int samp=0;
	SAMPLE out;
	
	int buff_size = (int) delay_ms * data->samprate/1000;
	
	if(data->delayBuff1 == NULL) 
		data->delayBuff1 = g_new0(SAMPLE, buff_size);
	/*if stereo add another buffer (max is 2 channels)*/
	if ((data->channels > 1) && (data->delayBuff2 == NULL))
		data->delayBuff2 = g_new0(SAMPLE, buff_size);
	
	for(samp = 0; samp < data->snd_numSamples; samp = samp + data->channels)
	{
		out = (0.7 * data->avi_sndBuff[samp]) + (0.3 * data->delayBuff1[data->delayIndex]);
		data->delayBuff1[data->delayIndex] = data->avi_sndBuff[samp] + (data->delayBuff1[data->delayIndex] * decay);
		data->avi_sndBuff[samp] = out;
		/*if stereo process second channel in separate*/
		if (data->channels > 1)
		{
			out = (0.7 * data->avi_sndBuff[samp+1]) + (0.3 * data->delayBuff2[data->delayIndex]);
			data->delayBuff2[data->delayIndex] = data->avi_sndBuff[samp] + (data->delayBuff2[data->delayIndex] * decay);
			data->avi_sndBuff[samp+1] = out;
		}
		
		if(++(data->delayIndex) >= buff_size) data->delayIndex=0;
	}
}

/* Non-linear amplifier with soft distortion curve. */
static SAMPLE CubicAmplifier( SAMPLE input )
{
	SAMPLE out;
	float temp;
	if( input < SAMPLE_SILENCE ) 
	{
#ifdef AUDIO_F32
		temp = input +1.0f;
		out = (temp * temp * temp) - 1.0f;
#else
		temp = (float) (input/MAX_SAMPLE) + 1.0f;
		out = (SAMPLE) ((temp * temp * temp)*MAX_SAMPLE - 1.0f);
#endif
	}
	else 
	{
#ifdef AUDIO_F32
		temp = input - 1.0f;
		out = (temp * temp *temp) + 1.0f;
#else
		temp = (float) (input/MAX_SAMPLE) - 1.0f;
		out = (SAMPLE) ((temp * temp *temp)*MAX_SAMPLE + 1.0f);
#endif
	}
	return out;
}

#define FUZZ(x) CubicAmplifier(CubicAmplifier(CubicAmplifier(x)))
/* Fuzz distortion */
void Fuzz (struct paRecordData* data)
{
	int samp=0;
	for(samp=0;samp<data->snd_numSamples;samp++)
		data->avi_sndBuff[samp] = FUZZ(data->avi_sndBuff[samp]);
}

/* four paralell Comb filters for reverb */
/*delay_ms: delay in ms   */
/*gain: feed gain (< 1)   */
static void CombFilter4 (struct paRecordData* data, 
	int delay1_ms,
	int delay2_ms,
	int delay3_ms,
	int delay4_ms,
	float gain1,
	float gain2,
	float gain3,
	float gain4,
	float in_gain)
{
	int samp=0;
	SAMPLE out1, out2, out3, out4;
	/*buff_size in samples*/
	int buff_size1 = (int) delay1_ms * (data->samprate/1000);
	int buff_size2 = (int) delay2_ms * (data->samprate/1000);
	int buff_size3 = (int) delay3_ms * (data->samprate/1000);
	int buff_size4 = (int) delay4_ms * (data->samprate/1000);
	
	if (data->CombBuff10 == NULL) 
		data->CombBuff10 = g_new0(SAMPLE, buff_size1);
	if((data->channels > 1) && data->CombBuff11 == NULL)
		data->CombBuff11 = g_new0(SAMPLE, buff_size1);
	
	if (data->CombBuff20 == NULL) 
		data->CombBuff20 = g_new0(SAMPLE, buff_size2);
	if((data->channels > 1) && data->CombBuff21 == NULL)
		data->CombBuff21 = g_new0(SAMPLE, buff_size2);
	
	if (data->CombBuff30 == NULL) 
		data->CombBuff30 = g_new0(SAMPLE, buff_size3);
	if((data->channels > 1) && data->CombBuff31 == NULL)
		data->CombBuff31 = g_new0(SAMPLE, buff_size3);
	
	if (data->CombBuff40 == NULL) 
		data->CombBuff40 = g_new0(SAMPLE, buff_size4);
	if((data->channels > 1) && data->CombBuff41 == NULL)
		data->CombBuff41 = g_new0(SAMPLE, buff_size4);
	
	for(samp = 0; samp < data->snd_numSamples; samp = samp + data->channels)
	{
		out1 = in_gain * data->avi_sndBuff[samp] + gain1 * data->CombBuff10[data->CombIndex1];
		out2 = in_gain * data->avi_sndBuff[samp] + gain2 * data->CombBuff20[data->CombIndex2];
		out3 = in_gain * data->avi_sndBuff[samp] + gain3 * data->CombBuff30[data->CombIndex3];
		out4 = in_gain * data->avi_sndBuff[samp] + gain4 * data->CombBuff40[data->CombIndex4];
		
		data->CombBuff10[data->CombIndex1] = data->avi_sndBuff[samp] + gain1 * data->CombBuff10[data->CombIndex1];
		data->CombBuff20[data->CombIndex2] = data->avi_sndBuff[samp] + gain2 * data->CombBuff20[data->CombIndex2];
		data->CombBuff30[data->CombIndex3] = data->avi_sndBuff[samp] + gain3 * data->CombBuff30[data->CombIndex3];
		data->CombBuff40[data->CombIndex4] = data->avi_sndBuff[samp] + gain4 * data->CombBuff40[data->CombIndex4];
				
		data->avi_sndBuff[samp] = out1 + out2 + out3+ out4;
		
		/*if stereo process second channel */
		if(data->channels > 1)
		{
			out1 = in_gain * data->avi_sndBuff[samp+1] + gain1 * data->CombBuff11[data->CombIndex1];
			out2 = in_gain * data->avi_sndBuff[samp+1] + gain2 * data->CombBuff21[data->CombIndex2];
			out3 = in_gain * data->avi_sndBuff[samp+1] + gain3 * data->CombBuff31[data->CombIndex3];
			out4 = in_gain * data->avi_sndBuff[samp+1] + gain4 * data->CombBuff41[data->CombIndex4];
		
			data->CombBuff11[data->CombIndex1] = data->avi_sndBuff[samp+1] + gain1 * data->CombBuff11[data->CombIndex1];
			data->CombBuff21[data->CombIndex2] = data->avi_sndBuff[samp+1] + gain2 * data->CombBuff21[data->CombIndex2];
			data->CombBuff31[data->CombIndex3] = data->avi_sndBuff[samp+1] + gain3 * data->CombBuff31[data->CombIndex3];
			data->CombBuff41[data->CombIndex4] = data->avi_sndBuff[samp+1] + gain4 * data->CombBuff41[data->CombIndex4];
			
			data->avi_sndBuff[samp+1] = out1 + out2 + out3+ out4;
		}
		
		if(++(data->CombIndex1) >= buff_size1) data->CombIndex1=0;
		if(++(data->CombIndex2) >= buff_size2) data->CombIndex2=0;
		if(++(data->CombIndex3) >= buff_size3) data->CombIndex3=0;
		if(++(data->CombIndex4) >= buff_size4) data->CombIndex4=0;
	}
}


/* all pass filter for reverb */
/*delay_ms: delay in ms                                 */
/* feedback_gain: <1                                    */
/*gain: input gain (gain+feedbackgain should be 1)      */
static void all_pass1 (struct paRecordData* data, int delay_ms, float gain)
{
	int samp=0;
	int buff_size = (int) delay_ms  * (data->samprate/1000);
	
	//SAMPLE temp;
	
	if (data->AllPassBuff1 == NULL) 
		data->AllPassBuff1 = g_new0(SAMPLE, buff_size);
	if ((data->channels > 1) && (data->AllPassBuff2== NULL)) 
		data->AllPassBuff2 = g_new0(SAMPLE, buff_size);
	
	for(samp = 0; samp < data->snd_numSamples; samp = samp + data->channels)
	{
	/*
		temp = data->AllPassBuff[data->AllPassIndex];
		data->AllPassBuff[data->AllPassIndex] = (temp * gain) + data->avi_sndBuff[samp];
		data->avi_sndBuff[samp] = temp - (gain * data->AllPassBuff[data->AllPassIndex]);
	*/
		data->AllPassBuff1[data->AllPassIndex] = data->avi_sndBuff[samp] + (gain * data->AllPassBuff1[data->AllPassIndex]);
		data->avi_sndBuff[samp] = ((data->AllPassBuff1[data->AllPassIndex] * (1 - gain*gain)) - data->avi_sndBuff[samp])/gain;
		if(data->channels > 1)
		{
			data->AllPassBuff2[data->AllPassIndex] = data->avi_sndBuff[samp+1] + (gain * data->AllPassBuff2[data->AllPassIndex]);
			data->avi_sndBuff[samp+1] = ((data->AllPassBuff2[data->AllPassIndex] * (1 - gain*gain)) - data->avi_sndBuff[samp+1])/gain;
		}
		
		if(++(data->AllPassIndex) >= buff_size) data->AllPassIndex=0;
	} 
	
}

void Reverb (struct paRecordData* data, int delay_ms)
{
	/*4 parallel comb filters*/
	CombFilter4 (data, delay_ms, delay_ms - 5, delay_ms -10, delay_ms -15, 
		0.55, 0.6, 0.5, 0.45, 0.7);
	/*all pass*/
	all_pass1 (data, delay_ms, 0.75);
}


/* Parameters:
	freq - LFO frequency (1.5)
	startphase - LFO startphase in RADIANS - usefull for stereo WahWah (0)
	depth - Wah depth (0.7)
	freqofs - Wah frequency offset (0.3)
	res - Resonance (2.5)

	!!!!!!!!!!!!! IMPORTANT!!!!!!!!! :
	depth and freqofs should be from 0(min) to 1(max) !
	res should be greater than 0 !  */

#define lfoskipsamples 30

void WahWah (struct paRecordData* data, float freq, float startphase, float depth, float freqofs, float res)
{
	float frequency, omega, sn, cs, alpha;
	float in, out;
	
	if(data->wahData == NULL) 
	{
		data->wahData = g_new0(WAHData, 1);
		data->wahData->lfoskip = freq * 2 * M_PI / data->samprate;
		data->wahData->phase = startphase;
		//if right channel set: phase += (float)M_PI;
	}

	int samp = 0;
	for(samp=0; samp<data->snd_numSamples; samp++)
	{
		in = data->avi_sndBuff[samp];
		
		if ((data->wahData->skipcount++) % lfoskipsamples == 0) 
		{
			frequency = (1 + cos(data->wahData->skipcount * data->wahData->lfoskip + data->wahData->phase)) / 2;
			frequency = frequency * depth * (1 - freqofs) + freqofs;
			frequency = exp((frequency - 1) * 6);
			omega = M_PI * frequency;
			sn = sin(omega);
			cs = cos(omega);
			alpha = sn / (2 * res);
			data->wahData->b0 = (1 - cs) / 2;
			data->wahData->b1 = 1 - cs;
			data->wahData->b2 = (1 - cs) / 2;
			data->wahData->a0 = 1 + alpha;
			data->wahData->a1 = -2 * cs;
			data->wahData->a2 = 1 - alpha;
		}
		out = (data->wahData->b0 * in + data->wahData->b1 * data->wahData->xn1 + 
			data->wahData->b2 * data->wahData->xn2 - data->wahData->a1 * data->wahData->yn1 - 
			data->wahData->a2 * data->wahData->yn2) / data->wahData->a0;
		data->wahData->xn2 = data->wahData->xn1;
		data->wahData->xn1 = in;
		data->wahData->yn2 = data->wahData->yn1;
		data->wahData->yn1 = out;

		// Prevents clipping
		if (out < -1.0)
			out = (float) -1.0;
		else if (out > 1.0)
			out = (float) 1.0;

		data->avi_sndBuff[samp] = (float) out;
	}
}

/*
static float waveshape_distort( float in ) 
{
	return 1.5f * in - 0.5f * in *in * in;
}
*/
