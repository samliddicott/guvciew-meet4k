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
	SAMPLE *delayBuff; // delay buffer - echo
	int delayIndex; // delay buffer index
	SAMPLE *CombBuff; // comb filter buffer 
	int CombIndex; //comb filter buffer index
	SAMPLE *AllPassBuff; // all pass filter buffer
	int AllPassIndex; // all pass filter buffer index
	BYTE *mp2Buff; //mp2 encode buffer
	int mp2BuffSize; // mp2 buffer size
	int snd_Flags; // effects flag
	GMutex *mutex; // audio mutex
	//pthread_cond_t cond;
	
} __attribute__ ((packed));

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
Echo(struct paRecordData *data, int delay_ms, int decay);

void 
Fuzz (struct paRecordData* data);

#endif

