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
	int input_type;
	PaStreamParameters inputParameters;
	PaStream *stream;
	int sampleIndex;
	int maxIndex;
	int channels;
	int numSamples;
	int streaming;
	int flush;
	int audio_flag;
	int samprate;
	int numsec;
	int snd_numBytes;
	int snd_begintime;
	int capAVI;
	SAMPLE *recordedSamples;
	SAMPLE *avi_sndBuff;
	SAMPLE *delayBuff1;
	//SAMPLE *delayBuff2;
	BYTE *mp2Buff;
	int mp2BuffSize;
	int snd_Flags;
	GMutex *mutex;
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
Echo(struct paRecordData *data, int decay);

#endif

