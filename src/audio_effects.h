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

#ifndef AUDIO_EFFECTS_H
#define AUDIO_EFFECTS_H

#include "sound.h"

//----------- structs for effects (audio_effects.c)------------
//data for Butterworth filter (LP or HP)
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

//data for Comb4 filter
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

// data for delay
typedef struct _delay_data
{
	int buff_size;
	SAMPLE *delayBuff1; // delay buffer 1 - first channel
	SAMPLE *delayBuff2; // delay buffer 2 - second channel (stereo)
	int delayIndex; // delay buffer index
} delay_data;

// data for WahWah effect
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

struct audio_effects
{
	delay_data *ECHO;
	delay_data *AP1;
	Comb4_data *COMB4;
	Filt_data *HPF;
	Filt_data *LPF1;
	RATE_data *RT1;
	WAHData* wahData;
};


void
Echo(struct paRecordData* data, AudBuff *proc_buff, struct audio_effects *aud_eff, int delay_ms, float decay);

void 
Fuzz (struct paRecordData* data, AudBuff *proc_buff, struct audio_effects *aud_eff);

void 
Reverb (struct paRecordData* data, AudBuff *proc_buff, struct audio_effects *aud_eff, int delay_ms);

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
WahWah (struct paRecordData* data, AudBuff *proc_buff, struct audio_effects *aud_eff, 
	float freq, float startphase, float depth, float freqofs, float res);

void 
change_pitch (struct paRecordData* data, AudBuff *proc_buff, struct audio_effects *aud_eff, int rate);

struct audio_effects* init_audio_effects (void);

void
close_DELAY(delay_data *DELAY);

void
close_FILT(Filt_data *FILT);

void
close_WAHWAH(WAHData *wahData);

void
close_REVERB(struct audio_effects *aud_eff);

void 
close_pitch (struct audio_effects *aud_eff);

void 
close_audio_effects(struct audio_effects *aud_eff);

#endif
