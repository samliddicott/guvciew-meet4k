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

#ifndef SOUND_H
#define SOUND_H

#include <portaudio.h>
#include <glib.h>
#include "globals.h"
#include "../config.h"

#ifdef PULSEAUDIO
  #include <pulse/simple.h>
  #include <pulse/error.h>
  #include <pulse/gccmacro.h>
#endif

/*------------- portaudio defs ----------------*/
/*---- can be override in rc file or GUI ------*/

#define DEFAULT_LATENCY_DURATION 100.0
#define DEFAULT_LATENCY_CORRECTION -130.0

#define SAMPLE_RATE  (0) /* 0 device default*/
//#define FRAMES_PER_BUFFER (4096)

/* sound can go for more 1 seconds than video          */

#define NUM_CHANNELS    (0) /* 0-device default 1-mono 2-stereo */

#define PA_SAMPLE_TYPE  paFloat32
#define PA_FOURCC       WAVE_FORMAT_PCM //use PCM 16 bits converted from float

#define SAMPLE_SILENCE  (0.0f)
#define MAX_SAMPLE (1.0f)
#define PRINTF_S_FORMAT "%.8f"

//API index
#define PORT  0
#define PULSE 1

typedef struct _AudBuff
{
	gboolean used;
	QWORD time_stamp;
	SAMPLE *frame;
} AudBuff;

// main audio interface struct
struct paRecordData
{
	int api; //0-Portaudio 1-pulse audio
	int input_type; // audio SAMPLE type
	PaStreamParameters inputParameters;
	PaStream *stream;
	unsigned long framesPerBuffer; //frames per buffer passed in audio callback

	int w_ind;                    // producer index
	int r_ind;                    // consumer index
	int channels;                 // channels
	gboolean streaming;           // audio streaming flag
	int flush;                    // flush mp2 buffer flag
	int samprate;                 // samp rate
	int numsec;                   // aprox. number of seconds for out buffer size
	int aud_numBytes;             // bytes copied to out buffer*/
	int aud_numSamples;           // samples copied to out buffer*/
	int64_t snd_begintime;        // audio recording start time*/
	int capVid;                   // video capture flag
	SAMPLE *recordedSamples;      // callback buffer
	int sampleIndex;
	AudBuff *audio_buff;          // ring buffer for audio data
	int64_t a_ts;                 // audio frame time stamp
	int64_t ts_ref;               // timestamp video reference
	int64_t ts_drift;             // time drift between audio device clock and system clock
	gint16 *pcm_sndBuff;          // buffer for pcm coding with int16
	BYTE *mp2Buff;                // mp2 encode buffer
	int mp2BuffSize;              // mp2 buffer size
	int snd_Flags;                // effects flag
	int skip_n;                   // video frames to skip
	UINT64 delay;                 // in nanosec - h264 has a two frame delay that must be compensated
	GMutex *mutex;                // audio mutex
	
	//PULSE SUPPORT
#ifdef PULSEAUDIO
	pa_simple *pulse_simple;
	GThread *pulse_read_th;
#endif
};

int 
recordCallback (const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData );

void
set_sound (struct GLOBAL *global, struct paRecordData* data, void* lav_aud_data);

int
init_sound(struct paRecordData* data);

int
close_sound (struct paRecordData *data);

void 
Float2Int16 (struct paRecordData* data, AudBuff *proc_buff);

#ifdef PULSEAUDIO
void
pulse_set_audio (struct GLOBAL *global, struct paRecordData* data);

int
pulse_init_audio(struct paRecordData* data);

int
pulse_close_sound (struct paRecordData *data);
#endif

#endif

