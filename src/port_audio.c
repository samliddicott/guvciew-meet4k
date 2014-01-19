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

#include <string.h>
#include "port_audio.h"
#include "ms_time.h"

int
portaudio_list_snd_devices(struct GLOBAL *global)
{
	int   it, numDevices, defaultDisplayed;
	const PaDeviceInfo *deviceInfo;

	//reset device count
	global->Sound_numInputDev = 0;

	numDevices = Pa_GetDeviceCount();
	if( numDevices < 0 )
	{
		g_print( "SOUND DISABLE: Pa_CountDevices returned 0x%x\n", numDevices );
		//err = numDevices;
		global->Sound_enable=0;
	}
	else
	{
		global->Sound_DefDev = 0;

		for( it=0; it<numDevices; it++ )
		{
			deviceInfo = Pa_GetDeviceInfo( it );
			if (global->debug) g_print( "--------------------------------------- device #%d\n", it );
			// Mark global and API specific default devices
			defaultDisplayed = 0;

			// with pulse, ALSA is now listed first and doesn't set a API default- 11-2009
			if( it == Pa_GetDefaultInputDevice() )
			{
				if (global->debug) g_print( "[ Default Input" );
				defaultDisplayed = 1;
				global->Sound_DefDev=global->Sound_numInputDev;/*default index in array of input devs*/
			}
			else if( it == Pa_GetHostApiInfo( deviceInfo->hostApi )->defaultInputDevice )
			{
				const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
				if (global->debug) g_print( "[ Default %s Input", hostInfo->name );
				defaultDisplayed = 2;
				//global->Sound_DefDev=global->Sound_numInputDev;/*index in array of input devs*/
			}
			// OUTPUT device doesn't matter for capture
			if( it == Pa_GetDefaultOutputDevice() )
			{
			 	if (global->debug)
				{
					g_print( (defaultDisplayed ? "," : "[") );
					g_print( " Default Output" );
				}
				defaultDisplayed = 3;
			}
			else if( it == Pa_GetHostApiInfo( deviceInfo->hostApi )->defaultOutputDevice )
			{
				const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
				if (global->debug)
				{
					g_print( (defaultDisplayed ? "," : "[") );
					g_print( " Default %s Output", hostInfo->name );/* OSS ALSA etc*/
				}
				defaultDisplayed = 4;
			}

			if( defaultDisplayed!=0 )
				if (global->debug) g_print( " ]\n" );

			/* print device info fields */
			if (global->debug)
			{
				g_print( "Name                     = %s\n", deviceInfo->name );
				g_print( "Host API                 = %s\n",  Pa_GetHostApiInfo( deviceInfo->hostApi )->name );
				g_print( "Max inputs = %d", deviceInfo->maxInputChannels  );
			}
			// INPUT devices (if it has input channels it's a capture device)
			if (deviceInfo->maxInputChannels >0)
			{
				global->Sound_numInputDev++;
				//allocate new Sound Device Info
				global->Sound_IndexDev = g_renew(sndDev, global->Sound_IndexDev, global->Sound_numInputDev);
				//fill structure with sound data
				global->Sound_IndexDev[global->Sound_numInputDev-1].id=it; /*saves dev id*/
				strncpy(global->Sound_IndexDev[global->Sound_numInputDev-1].name, deviceInfo->name, 511);
				strncpy(global->Sound_IndexDev[global->Sound_numInputDev-1].description, deviceInfo->name, 255);
				global->Sound_IndexDev[global->Sound_numInputDev-1].chan=deviceInfo->maxInputChannels;
				global->Sound_IndexDev[global->Sound_numInputDev-1].samprate=deviceInfo->defaultSampleRate;
				//Sound_IndexDev[Sound_numInputDev].Hlatency=deviceInfo->defaultHighInputLatency;
				//Sound_IndexDev[Sound_numInputDev].Llatency=deviceInfo->defaultLowInputLatency;
			}
			if (global->debug)
			{
				g_print( ", Max outputs = %d\n", deviceInfo->maxOutputChannels  );
				g_print( "Def. low input latency   = %8.3f\n", deviceInfo->defaultLowInputLatency  );
				g_print( "Def. low output latency  = %8.3f\n", deviceInfo->defaultLowOutputLatency  );
				g_print( "Def. high input latency  = %8.3f\n", deviceInfo->defaultHighInputLatency  );
				g_print( "Def. high output latency = %8.3f\n", deviceInfo->defaultHighOutputLatency  );
				g_print( "Def. sample rate         = %8.2f\n", deviceInfo->defaultSampleRate );
			}

		}

		if (global->debug) g_print("----------------------------------------------\n");
	}

	return 0;
}

/*--------------------------- audio record callback -----------------------*/
static int
recordCallback (const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData )
{
	struct paRecordData *pdata = (struct paRecordData*)userData;

	int channels = pdata->channels;

	unsigned long numSamples = framesPerBuffer * channels;
	UINT64 frame_length = G_NSEC_PER_SEC / pdata->samprate; /*in nanosec*/

	PaTime ts_sec = timeInfo->inputBufferAdcTime; /*in seconds (double)*/
	int64_t ts = ts_sec * G_NSEC_PER_SEC; /*in nanosec (monotonic time)*/

	if(statusFlags & paInputOverflow)
	{
		g_print( "AUDIO: portaudio buffer overflow\n" );
		/*determine the number of samples dropped*/
		if(pdata->a_last_ts <= 0)
			pdata->a_last_ts = pdata->snd_begintime;
			
		int64_t d_ts = ts - pdata->a_last_ts;
		int n_samples = (d_ts / frame_length) * channels;
		record_silence (n_samples, userData);
	}
	if(statusFlags & paInputUnderflow)
		g_print( "AUDIO: portaudio buffer underflow\n" );

	int res = record_sound ( inputBuffer, numSamples, ts, userData );
	
	pdata->a_last_ts = ts + (framesPerBuffer * frame_length);

	if(res < 0 )
		return (paComplete); /*capture stopped*/
	else
		return (paContinue); /*still capturing*/
}

/*--------------------------- API initialization -------------------------*/
int
port_init_audio(struct paRecordData* pdata)
{
	PaError err = paNoError;
	PaStream *stream = NULL;

	PaStreamParameters inputParameters;

	if(stream)
	{
		if( !(Pa_IsStreamStopped( stream )))
		{
			Pa_AbortStream( pdata->stream );
			Pa_CloseStream( pdata->stream );
			pdata->stream = NULL;
		}
	}

	inputParameters.device = pdata->device_id;
	inputParameters.channelCount = pdata->channels;
	inputParameters.sampleFormat = PA_SAMPLE_TYPE;

	if (Pa_GetDeviceInfo( inputParameters.device ))
		inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
		//inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultHighInputLatency;
	else
		inputParameters.suggestedLatency = DEFAULT_LATENCY_DURATION/1000.0;
	inputParameters.hostApiSpecificStreamInfo = NULL;

	/*---------------------------- start recording Audio. ----------------------------- */

	err = Pa_OpenStream(
		&stream,
		&inputParameters,
		NULL,                  /* &outputParameters, */
		pdata->samprate,        /* sample rate        */
		paFramesPerBufferUnspecified,/* buffer in frames (use API optimal)*/
		paNoFlag,              /* PaNoFlag - clip and dhiter*/
		recordCallback,        /* sound callback     */
		pdata );                /* callback userData  */

	if( err != paNoError ) goto error;

	err = Pa_StartStream( stream );
	pdata->stream = (void *) stream; //store stream pointer

	if( err != paNoError ) goto error; /*should close the stream if error ?*/

	const PaStreamInfo* stream_info = Pa_GetStreamInfo (stream);
	g_print("AUDIO: latency of %8.3f msec\n", 1000 * stream_info->inputLatency);

	return 0;

error:
	g_printerr("AUDIO: An error occured while starting the portaudio API\n" );
	g_printerr("       Error number: %d\n", err );
	g_printerr("       Error message: %s\n", Pa_GetErrorText( err ) );
	pdata->streaming=FALSE;

	if(stream) Pa_AbortStream( stream );

	/*lavc is allways checked and cleaned when finishing worker thread*/
	return(-1);
}

int
port_close_audio (struct paRecordData *pdata)
{
	PaError err = paNoError;
	PaStream *stream = (PaStream *) pdata->stream;
	int ret = 0;

	/*stops and closes the audio stream*/
	if(stream)
	{
		if(Pa_IsStreamActive( stream ) > 0)
		{
			g_print("Aborting audio stream\n");
			err = Pa_AbortStream( stream );
		}
		else
		{
			g_print("Stoping audio stream\n");
			err = Pa_StopStream( stream );
		}

		if( err != paNoError )
		{
			g_printerr("An error occured while stoping the audio stream\n" );
			g_printerr("Error number: %d\n", err );
			g_printerr("Error message: %s\n", Pa_GetErrorText( err ) );
			ret = -1;
		}

		g_print("Closing audio stream...\n");
		err = Pa_CloseStream( stream );

		if( err != paNoError )
		{
			g_printerr("An error occured while closing the audio stream\n" );
			g_printerr("Error number: %d\n", err );
			g_printerr("Error message: %s\n", Pa_GetErrorText( err ) );
			ret = -1;
		}
	}
	else
		g_print("Invalid stream pointer.\n");


	pdata->stream = NULL;

	return (ret);
}


