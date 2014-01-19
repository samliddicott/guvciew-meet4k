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

#ifndef SOUND_H
#define SOUND_H

#include <glib.h>
#include <pthread.h>
#include "globals.h"
#include "../config.h"
#include "defs.h"

/*------------- portaudio defs ----------------*/
/*---- can be override in rc file or GUI ------*/

#define DEFAULT_LATENCY_DURATION 100.0
#define DEFAULT_LATENCY_CORRECTION -130.0

#define SAMPLE_RATE  (0) /* 0 device default*/
//#define FRAMES_PER_BUFFER (4096)

/* sound can go for more 1 seconds than video          */

#define NUM_CHANNELS    (0) /* 0-device default 1-mono 2-stereo */

#define PA_SAMPLE_TYPE     paFloat32
#define PA_FOURCC          WAVE_FORMAT_PCM //use PCM 16 bits converted from float
#define PULSE_SAMPLE_TYPE  PA_SAMPLE_FLOAT32LE //for PCM -> PA_SAMPLE_S16LE


#define SAMPLE_SILENCE  (0.0f)
#define MAX_SAMPLE (1.0f)
#define PRINTF_S_FORMAT "%.8f"

/*buffer flags*/
#define AUD_IN_USE           0 /* in use by interrupt handler*/
#define AUD_PROCESS          1 /* ready to process */
#define AUD_PROCESSING       2 /* processing */
#define AUD_PROCESSED        3 /* ready to write to disk */
#define AUD_READY            4 /* ready to re-use by interrupt handler*/

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

	unsigned long framesPerBuffer;   //frames per buffer passed in audio callback
	char device_name[512];           //device name - for pulse
	int device_id;                   //device id - for portaudio

	int w_ind;                       // producer index
	int r_ind;                       // consumer index
	int bw_ind;                      // audio_buffer in_use index
	int br_ind;                      // audio_buffer processing
	int blast_ind;                   // last in_use index (for vu meter)
	int last_ind;                    // last producer index (for vu meter)
	int channels;                    // channels
	gboolean streaming;              // audio streaming flag
	int flush;                       // flush mp2 buffer flag
	int samprate;                    // samp rate
	int numsec;                      // aprox. number of seconds for out buffer size
	int aud_numBytes;                // bytes copied to out buffer*/
	int aud_numSamples;              // samples copied to out buffer*/
	int64_t snd_begintime;           // audio recording start time*/
	//int64_t api_ts_ref;
	int capVid;                      // video capture flag
	SAMPLE *recordedSamples;         // callback buffer
	int sampleIndex;
	AudBuff *audio_buff[AUDBUFF_NUM];// ring buffers for audio data captured from device
	int audio_buff_flag[AUDBUFF_NUM];// process_buffer flags
	int64_t a_ts;                    // audio frame time stamp
	int64_t ts_ref;                  // timestamp video reference
	int64_t a_last_ts;               // last audio frame timestamp
	int64_t ts_drift;                // time drift between audio device clock and system clock
	gint16 *pcm_sndBuff;             // buffer for pcm coding with int16
	float  *float_sndBuff;           // buffer for pcm coding with float
	BYTE *mp2Buff;                   // mp2 encode buffer
	int mp2BuffSize;                 // mp2 buffer size
	int snd_Flags;                   // effects flag
	int skip_n;                      // video frames to skip
	UINT64 delay;                    // in nanosec - h264 has a two frame delay that must be compensated

	int outbuf_size;	             //size of output buffer
	struct lavcAData* lavc_data;     //libavcodec data
	__MUTEX_TYPE mutex;

	//PORTAUDIO SUPPORT
	void *stream;

	//PULSE SUPPORT
#ifdef PULSEAUDIO
	 __THREAD_TYPE pulse_read_th;
	/*The main loop context*/
	//GMainContext *maincontext;
#endif
};

int
record_sound ( const void *inputBuffer, unsigned long numSamples, int64_t timestamp, void *userData );

void
record_silence ( unsigned long numSamples, void *userData );

void
set_sound (struct GLOBAL *global, struct paRecordData* data);

int
init_sound(struct paRecordData* data);

int
close_sound (struct paRecordData *data);

void
SampleConverter (struct paRecordData* data);

#endif

