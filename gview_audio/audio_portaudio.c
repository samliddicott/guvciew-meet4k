/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
#           Flemming Frandsen <dren.dk@gmail.com>                               #
#                             Add VU meter OSD                                  #
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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <portaudio.h>
/* support for internationalization - i18n */
#include <locale.h>
#include <libintl.h>

#include "gview.h"
#include "gviewaudio.h"

extern int verbosity;

/*
 * Portaudio record callback
 * args:
 *    inputBuffer - pointer to captured input data (for recording)
 *    outputBuffer - pointer ouput data (for playing - NOT USED)
 *    framesPerBuffer - buffer size
 *    timeInfo - pointer to time data (for timestamping)
 *    statusFlags - stream status
 *    userData - pointer to user data (audio context)
 *
 * asserts:
 *    audio_ctx (userData) is not null
 *
 * returns: error code (0 ok)
 */
static int
recordCallback (const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData )
{
	audio_context_t *audio_ctx = (audio_context_t *) userData;

	/*asserts*/
	assert(audio_ctx != NULL);

	int res = 0;

	unsigned long numSamples = framesPerBuffer * audio_ctx->channels;
	uint64_t frame_length = NSEC_PER_SEC / audio_ctx->samprate; /*in nanosec*/

	PaTime ts_sec = timeInfo->inputBufferAdcTime; /*in seconds (double)*/
	int64_t ts = ts_sec * NSEC_PER_SEC; /*in nanosec (monotonic time)*/

	if(statusFlags & paInputOverflow)
	{
		printf( "AUDIO: portaudio buffer overflow\n" );
		/*determine the number of samples dropped*/
		if(audio_ctx->a_last_ts <= 0)
			audio_ctx->a_last_ts = audio_ctx->snd_begintime;

		int64_t d_ts = ts - audio_ctx->a_last_ts;
		int n_samples = (d_ts / frame_length) * audio_ctx->channels;
		//record_silence (n_samples, userData);
	}
	if(statusFlags & paInputUnderflow)
		print( "AUDIO: portaudio buffer underflow\n" );

	//res = record_sound ( inputBuffer, numSamples, ts, userData );

	audio_ctx->a_last_ts = ts + (framesPerBuffer * frame_length);

	if(res < 0 )
		return (paComplete); /*capture stopped*/
	else
		return (paContinue); /*still capturing*/
}


/*
 * list audio devices for portaudio api
 * args:
 *    audio_ctx - pointer to audio context data
 * asserts:
 *    audio_ctx is not null
 *
 * returns: error code (0 ok)
 */
static int audio_portaudio_list_devices(audio_context_t *audio_ctx)
{
	int   it, numDevices, defaultDisplayed;
	const PaDeviceInfo *deviceInfo;

	//reset device count
	audio_ctx->num_input_dev = 0;

	numDevices = Pa_GetDeviceCount();
	if( numDevices < 0 )
	{
		printf( "AUDIO: Audio disabled: Pa_CountDevices returned %i\n", numDevices );
	}
	else
	{
		audio_ctx->default_dev = 0;

		for( it=0; it < numDevices; it++ )
		{
			deviceInfo = Pa_GetDeviceInfo( it );
			if (verbosity > 0)
				printf( "--------------------------------------- device #%d\n", it );
			/* Mark audio_ctx and API specific default devices*/
			defaultDisplayed = 0;

			/* with pulse, ALSA is now listed first and doesn't set a API default- 11-2009*/
			if( it == Pa_GetDefaultInputDevice() )
			{
				if (verbosity > 0)
					printf( "[ Default Input" );
				defaultDisplayed = 1;
				audio_ctx->default_dev = audio_ctx->num_input_dev;/*default index in array of input devs*/
			}
			else if( it == Pa_GetHostApiInfo( deviceInfo->hostApi )->defaultInputDevice )
			{
				const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
				if (verbosity > 0)
					printf( "[ Default %s Input", hostInfo->name );
				defaultDisplayed = 2;
			}
			/* OUTPUT device doesn't matter for capture*/
			if( it == Pa_GetDefaultOutputDevice() )
			{
			 	if (verbosity > 0)
				{
					printf( (defaultDisplayed ? "," : "[") );
					printf( " Default Output" );
				}
				defaultDisplayed = 3;
			}
			else if( it == Pa_GetHostApiInfo( deviceInfo->hostApi )->defaultOutputDevice )
			{
				const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
				if (verbosity > 0)
				{
					printf( (defaultDisplayed ? "," : "[") );
					printf( " Default %s Output", hostInfo->name );/* OSS ALSA etc*/
				}
				defaultDisplayed = 4;
			}

			if( defaultDisplayed!=0 )
				if (verbosity > 0)
					printf( " ]\n" );

			/* print device info fields */
			if (verbosity > 0)
			{
				printf( "Name                     = %s\n", deviceInfo->name );
				printf( "Host API                 = %s\n",  Pa_GetHostApiInfo( deviceInfo->hostApi )->name );
				printf( "Max inputs = %d", deviceInfo->maxInputChannels  );
			}
			/* INPUT devices (if it has input channels it's a capture device)*/
			if (deviceInfo->maxInputChannels > 0)
			{
				audio_ctx->num_input_dev++;
				/*add device to list*/
				audio_ctx->list_devices = realloc(audio_ctx->list_devices, audio_ctx->num_input_dev * sizeof(audio_device_t));
				/*fill device data*/
				audio_ctx->list_devices[audio_ctx->num_input_dev-1].id = it;
				strncpy(audio_ctx->list_devices[audio_ctx->num_input_dev-1].name, deviceInfo->name, 511);
				strncpy(audio_ctx->list_devices[audio_ctx->num_input_dev-1].description, deviceInfo->name, 255);
				audio_ctx->list_devices[audio_ctx->num_input_dev-1].channels = deviceInfo->maxInputChannels;
				audio_ctx->list_devices[audio_ctx->num_input_dev-1].samprate = deviceInfo->defaultSampleRate;
				//audio_ctx->list_devices[audio_ctx->num_input_dev-1].Hlatency = deviceInfo->defaultHighInputLatency;
				//audio_ctx->list_devices[audio_ctx->num_input_dev-1].Llatency = deviceInfo->defaultLowInputLatency;
			}
			if (verbosity > 0)
			{
				printf( ", Max outputs = %d\n", deviceInfo->maxOutputChannels  );
				printf( "Def. low input latency   = %8.3f\n", deviceInfo->defaultLowInputLatency  );
				printf( "Def. low output latency  = %8.3f\n", deviceInfo->defaultLowOutputLatency  );
				printf( "Def. high input latency  = %8.3f\n", deviceInfo->defaultHighInputLatency  );
				printf( "Def. high output latency = %8.3f\n", deviceInfo->defaultHighOutputLatency  );
				printf( "Def. sample rate         = %8.2f\n", deviceInfo->defaultSampleRate );
			}

		}

		if (verbosity > 0)
			printf("----------------------------------------------\n");
	}

	return 0;
}

/*
 * init portaudio api
 * args:
 *
 * asserts:
 *    none
 * returns: pointer to audio context data
 *     or NULL if error
 */
audio_context_t *audio_init_portaudio()
{
	audio_context_t *audio_ctx = calloc(1, sizeof(audio_context_t));

	audio_portaudio_list_devices(audio_ctx);

	return audio_ctx;
}

/*
 * close and clean audio context for portaudio api
 * args:
 *   audio_ctx - pointer to audio context data
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void audio_close_portaudio(audio_context_t *audio_ctx)
{
	if(audio_ctx == NULL)
		return;

	if(audio_ctx->list_devices != NULL);
		free(audio_ctx->list_devices);
	audio_ctx->list_devices = NULL;

	free(audio_ctx);
}