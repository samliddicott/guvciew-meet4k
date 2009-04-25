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

/*------------- portaudio defs ----------------*/
/*---- can be override in rc file or GUI ------*/

#define SAMPLE_RATE  (0) /* 0 device default*/
//#define FRAMES_PER_BUFFER (4096)

#define NUM_SECONDS     (1) /* captures 1 second bloks */
/* sound can go for more 1 seconds than video          */

#define NUM_CHANNELS    (0) /* 0-device default 1-mono 2-stereo */

#define PA_SAMPLE_TYPE  paFloat32
#define PA_FOURCC       WAVE_FORMAT_PCM //use PCM 16 bits converted from float
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define MAX_SAMPLE (1.0f)
#define PRINTF_S_FORMAT "%.8f"

// main audio interface struct
struct paRecordData
{
	int input_type; // audio SAMPLE type
	PaStreamParameters inputParameters;
	PaStream *stream;
	unsigned long framesPerBuffer; //frames per buffer passed in audio callback
	int MPEG_Frame_size; //number of samples per mpeg frame (1152 for MP2)
	int sampleIndex; // callback buffer index
	int maxIndex; // maximum callback buffer index
	int channels; // channels
	int numSamples; //captured samples in callback
	int streaming; // audio streaming flag
	int flush; // flush mp2 buffer flag
	int audio_flag; // ou buffer data ready flag
	int samprate; // samp rate
	int tresh;    //samples treshold for output buffer in audio callback
	int numsec; // aprox. number of seconds for out buffer size
	int snd_numBytes; //bytes copied to out buffer*/
	int snd_numSamples; //samples copied to out buffer*/
	int64_t snd_begintime; //audio recording start time*/
	int capVid; // video capture flag
	SAMPLE *recordedSamples; // callback buffer
	SAMPLE *vid_sndBuff; // out buffer
	INT64 a_ts; //audio frame time stamp
	gint16 *vid_sndBuff1; //buffer for pcm coding with int16
	BYTE *mp2Buff; //mp2 encode buffer
	int mp2BuffSize; // mp2 buffer size
	int snd_Flags; // effects flag
	GMutex *mutex; // audio mutex
	//pthread_cond_t cond;
	
};

int 
recordCallback (const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData );

void
set_sound (struct GLOBAL *global, struct paRecordData* data);

int
init_sound(struct paRecordData* data);

int
close_sound (struct paRecordData *data);

void 
Float2Int16 (struct paRecordData* data);


#endif

