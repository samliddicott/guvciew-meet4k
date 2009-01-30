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

/*data for Butterworth filter (LP or HP)*/
typedef struct _Filt_data
{
	SAMPLE buff_in1[2];
	SAMPLE buff_in2[2];
	SAMPLE buff_out1[2];
	SAMPLE buff_out2[2];
	float c;
	float a1;
	float a2;
	float a3;
	float b1;
	float b2;
} Filt_data;

/*data for Comb4 filter*/
typedef struct _Comb4_data
{
	int buff_size1;
	int buff_size2;
	int buff_size3;
	int buff_size4;

	SAMPLE *CombBuff10; // four parallel  comb filters - first channel
	SAMPLE *CombBuff11; // four parallel comb filters - second channel
	SAMPLE *CombBuff20; // four parallel  comb filters - first channel
	SAMPLE *CombBuff21; // four parallel comb filters - second channel
	SAMPLE *CombBuff30; // four parallel  comb filters - first channel
	SAMPLE *CombBuff31; // four parallel comb filters - second channel
	SAMPLE *CombBuff40; // four parallel  comb filters - first channel
	SAMPLE *CombBuff41; // four parallel comb filters - second channel

	int CombIndex1; //comb filter 1 index
	int CombIndex2; //comb filter 2 index
	int CombIndex3; //comb filter 3 index
	int CombIndex4; //comb filter 4 index
} Comb4_data;

typedef struct _delay_data
{
	int buff_size;
	SAMPLE *delayBuff1; // delay buffer 1 - first channel
	SAMPLE *delayBuff2; // delay buffer 2 - second channel (stereo)
	int delayIndex; // delay buffer index
} delay_data;

/*data for WahWah effect*/
typedef struct _WAHData
{
	float lfoskip;
	unsigned long skipcount;
	float xn1;
	float xn2;
	float yn1;
	float yn2;
	float b0;
	float b1;
	float b2;
	float a0;
	float a1;
	float a2;
	float phase;
} WAHData;

typedef struct _TSD_data
{
	float tempo;
} TSD_data;

typedef struct _RATE_data
{
	SAMPLE *rBuff1;
	SAMPLE *rBuff2;
	SAMPLE *wBuff1;
	SAMPLE *wBuff2;
	int wSize;
	int numsamples;
} RATE_data;

struct paRecordData
{
	int input_type; // audio SAMPLE type
	PaStreamParameters inputParameters;
	PaStream *stream;
	int sampleIndex; // callback buffer index
	int maxIndex; // maximum callback buffer index
	int channels; // channels
	int numSamples; //captured samples in callback
	int streaming; // audio streaming flag
	int flush; // flush mp2 buffer flag
	int audio_flag; // ou buffer data ready flag
	int samprate; // samp rate
	int numsec; // aprox. number of seconds for out buffer size
	int snd_numBytes; //bytes copied to out buffer*/
	int snd_numSamples; //samples copied to out buffer*/
	int snd_begintime; //audio recording start time*/
	int capAVI; // avi capture flag
	SAMPLE *recordedSamples; // callback buffer
	SAMPLE *avi_sndBuff; // out buffer

	delay_data *ECHO;
	delay_data *AP1;
	Comb4_data *COMB4;
	Filt_data *HPF;
	Filt_data *LPF1;
	RATE_data *RT1;

	gint16 *avi_sndBuff1; //buffer for pcm coding with int16
	BYTE *mp2Buff; //mp2 encode buffer
	int mp2BuffSize; // mp2 buffer size
	WAHData* wahData;
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
close_DELAY(delay_data *DELAY);

void
close_FILT(Filt_data *FILT);

void
close_WAHWAH(WAHData *wahData);

void
close_REVERB(struct paRecordData *data);

void 
close_pitch (struct paRecordData* data);

void 
Float2Int16 (struct paRecordData* data);

void
Echo(struct paRecordData *data, int delay_ms, float decay);

void 
Fuzz (struct paRecordData* data);

void 
Reverb (struct paRecordData* data, int delay_ms);

/* Parameters:
	freq - LFO frequency (1.5)
	startphase - LFO startphase in RADIANS - usefull for stereo WahWah (0)
	depth - Wah depth (0.7)
	freqofs - Wah frequency offset (0.3)
	res - Resonance (2.5)

	!!!!!!!!!!!!! IMPORTANT!!!!!!!!! :
	depth and freqofs should be from 0(min) to 1(max) !
	res should be greater than 0 !  */
void 
WahWah (struct paRecordData* data, float freq, float startphase, float depth, float freqofs, float res);

void 
change_pitch (struct paRecordData* data, int rate);

#endif

