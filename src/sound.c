/*******************************************************************************#
#	    guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#										#
# This program is free software; you can redistribute it and/or modify         	#
# it under the terms of the GNU General Public License as published by   	#
# the Free Software Foundation; either version 2 of the License, or           	#
# (at your option) any later version.                                          	#
#                                                                              	#
# This program is distributed in the hope that it will be useful,              	#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             	#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 	#
#                                                                              	#
# You should have received a copy of the GNU General Public License           	#
# along with this program; if not, write to the Free Software                  	#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	#
#                                                                              	#
********************************************************************************/

#include "sound.h"
#include "ms_time.h"
#include "globals.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <portaudio.h>

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
    
    /*will be reset to 0 after all audio has been writen to AVI*/
    data->recording=1;
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
	pthread_mutex_lock( &data->mutex);
	    data->snd_numBytes = data->numSamples*sizeof(SAMPLE);
	    memcpy(data->avi_sndBuff, data->recordedSamples ,data->snd_numBytes);
	pthread_mutex_unlock( &data->mutex );
	data->sampleIndex=0;
	data->numSamples = 0;
	//flags that secondary buffer as data (can be saved to file)
	data->audio_flag=1;
	
    }

    
    if(data->capAVI) return (paContinue); /*still capturing*/
    else {
        /*recording stopped*/
	if(!(data->audio_flag)) {
		/*need to copy remaining audio to secondary buffer*/
		data->snd_numBytes = data->numSamples*sizeof(SAMPLE);
		memcpy(data->avi_sndBuff, data->recordedSamples , data->snd_numBytes);
		data->sampleIndex=0;
		data->numSamples = 0;
		//flags that secondary buffer as data (can be saved to file)
		data->audio_flag=1;
	}
	data->streaming=0;
	return (paComplete);
    }
}

void
set_sound (struct GLOBAL *global, struct paRecordData* data) {
	if(global->Sound_SampRateInd==0)
	   global->Sound_SampRate=global->Sound_IndexDev[global->Sound_UseDev].samprate;/*using default*/
	
	if(global->Sound_NumChanInd==0) {
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
	int i;
	int totalFrames;
	int MP2Frames=0;
	int numSamples;

	/* setting maximum buffer size*/
	totalFrames = data->numsec * data->samprate;
	numSamples = totalFrames * data->channels;
	/*round to libtwolame Frames (1 Frame = 1152 samples)*/
	MP2Frames=numSamples/1152;
	numSamples=MP2Frames*1152;
	
	data->snd_numBytes = numSamples * sizeof(SAMPLE);
    
	data->recordedSamples = (SAMPLE *) malloc( data->snd_numBytes ); /*capture buffer*/
    	data->maxIndex = numSamples;
    	data->sampleIndex = 0;
	
	data->audio_flag = 0;
	data->recording = 0;
    	data->streaming = 0;
	
	data->avi_sndBuff = (SAMPLE *) malloc( data->snd_numBytes );/*secondary shared buffer*/
    
	if( data->recordedSamples == NULL )
	{
		printf("Could not allocate record array.\n");
		return (-2);
	}
	for( i=0; i<numSamples; i++ ) data->recordedSamples[i] = 0;
	
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
			  recordCallback, /* sound callback - using blocking API*/
			  data ); /* callback userData -no callback no data */
	if( err != paNoError ) goto error;
	
	err = Pa_StartStream( data->stream );
	if( err != paNoError ) goto error; /*should close the stream if error ?*/
	
	/*sound start time - used to sync with video*/
	data->snd_begintime = ms_time();
    
    	return (0);
error:
	fprintf( stderr, "An error occured while using the portaudio stream\n" );
	fprintf( stderr, "Error number: %d\n", err );
	fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) ); 
	data->streaming=0;
	data->recording=0;
	Pa_Terminate();
	if(data->recordedSamples) free( data->recordedSamples );
	data->recordedSamples=NULL;
	if(data->avi_sndBuff) free(data->avi_sndBuff);
	data->avi_sndBuff=NULL;
	
	return(-1);

}	   

int
close_sound (struct paRecordData *data) 
{
    	int stall=30;
        int err =0;
        /*stops and closes the audio stream*/
	err = Pa_StopStream( data->stream );
	if( err != paNoError ) goto error;
	
	err = Pa_CloseStream( data->stream );
	if( err != paNoError ) goto error; 
	
    	/*wait for last audio chunks to be saved on video file */
    	while (( data->streaming || data->audio_flag || data->recording ) &&
	       (stall>0)) 
    	{
		Pa_Sleep(100);
		stall--; /*prevents stalls (waits at max 30*100 ms)*/
	}
    	if(!(stall>0)) 
        {
		printf("WARNING:sound capture stall (streaming=%d flag=%d recording=%d)\n",
	                   data->streaming, data->audio_flag, data->recording);

		data->streaming = 0;
		data->audio_flag = 0;
		data->recording  = 0;
	    	Pa_Sleep(500); /* wait 500ms so any pending read may finish    */
	}
        /*---------------------------------------------------------------------*/
	/*make sure no operations are performed on the buffers*/
	pthread_mutex_lock( &data->mutex);
	    /*free primary buffer*/
	    if(data->recordedSamples) free( data->recordedSamples  );
	    data->recordedSamples=NULL;
	
	    Pa_Terminate();
	
	    if (data->avi_sndBuff) free(data->avi_sndBuff);
	    data->avi_sndBuff = NULL;
	    if (data->mp2Buff) free(data->mp2Buff);
	    data->mp2Buff = NULL;
	pthread_mutex_unlock( &data->mutex );
	
	return (0);
error:  
	fprintf( stderr, "An error occured while closing the portaudio stream\n" );
	fprintf( stderr, "Error number: %d\n", err );
	fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
	data->recording=0;
	data->audio_flag=0;
	data->streaming=0;
	pthread_mutex_lock( &data->mutex);
	    if(data->recordedSamples) free( data->recordedSamples );
	    data->recordedSamples=NULL;
	    Pa_Terminate();
	    if (data->avi_sndBuff) free(data->avi_sndBuff);
    	    data->avi_sndBuff = NULL;
	    if (data->mp2Buff) free(data->mp2Buff);
	    data->mp2Buff = NULL;
	pthread_mutex_unlock( &data->mutex );
	return(-1);
}
