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

#include <glib/gprintf.h>
#include <math.h>
#include <string.h>

#include "audio_effects.h"

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

//clip float samples [-1.0 ; 1.0]
static float clip_float (float in)
{
	in = (in < -1.0) ? -1.0 : (in > 1.0) ? 1.0 : in;
	
	return in;
}

/*------------------------------------- Effects ------------------------------------------*/
/* Echo effect */
/*delay_ms: echo delay in ms*/
/*decay: feedback gain (<1) */
void Echo(struct paRecordData* data, AudBuff *proc_buff, struct audio_effects *aud_eff, int delay_ms, float decay)
{
	int samp=0;
	SAMPLE out;
	
	if(aud_eff->ECHO == NULL)
	{
		aud_eff->ECHO = g_new0(delay_data, 1);
		aud_eff->ECHO->buff_size = (int) delay_ms * data->samprate * 0.001;
		aud_eff->ECHO->delayBuff1 = g_new0(SAMPLE, aud_eff->ECHO->buff_size);
		aud_eff->ECHO->delayBuff2 = NULL;
		if(data->channels > 1)
			aud_eff->ECHO->delayBuff2 = g_new0(SAMPLE, aud_eff->ECHO->buff_size);
	}
	
	for(samp = 0; samp < data->aud_numSamples; samp = samp + data->channels)
	{
		out = (0.7 * proc_buff->frame[samp]) + 
			(0.3 * aud_eff->ECHO->delayBuff1[aud_eff->ECHO->delayIndex]);
		aud_eff->ECHO->delayBuff1[aud_eff->ECHO->delayIndex] = proc_buff->frame[samp] + 
			(aud_eff->ECHO->delayBuff1[aud_eff->ECHO->delayIndex] * decay);
		proc_buff->frame[samp] = clip_float(out);
		/*if stereo process second channel in separate*/
		if (data->channels > 1)
		{
			out = (0.7 * proc_buff->frame[samp+1]) + 
				(0.3 * aud_eff->ECHO->delayBuff2[aud_eff->ECHO->delayIndex]);
			aud_eff->ECHO->delayBuff2[aud_eff->ECHO->delayIndex] = proc_buff->frame[samp] + 
				(aud_eff->ECHO->delayBuff2[aud_eff->ECHO->delayIndex] * decay);
			proc_buff->frame[samp+1] = clip_float(out);
		}
		
		if(++(aud_eff->ECHO->delayIndex) >= aud_eff->ECHO->buff_size) aud_eff->ECHO->delayIndex=0;
	}
}


/*Butterworth Filter for HP or LP
out(n) = a1 * in + a2 * in(n-1) + a3 * in(n-2) - b1*out(n-1) - b2*out(n-2)
*/
static void Butt(Filt_data *FILT, SAMPLE *Buff, int NumSamples, int channels)
{
	int index = 0;
	SAMPLE out;
	
	for (index = 0; index < NumSamples; index = index + channels)
	{
		out = FILT->a1 * Buff[index] + FILT->a2 * FILT->buff_in1[0] +
			FILT->a3 * FILT->buff_in1[1] - FILT->b1 * FILT->buff_out1[0] -
			FILT->b2 * FILT->buff_out1[1];
		FILT->buff_in1[1] = FILT->buff_in1[0]; //in(n-2) = in(n-1)
		FILT->buff_in1[0] = Buff[index]; // in(n-1) = in
		FILT->buff_out1[1] = FILT->buff_out1[0]; //out(n-2) = out(n-1)
		FILT->buff_out1[0] = out; //out(n-1) = out
	
		Buff[index] = clip_float(out);
		/*process second channel*/
		if(channels > 1)
		{
			out = FILT->a1 * Buff[index+1] + FILT->a2 * FILT->buff_in2[0] +
				FILT->a3 * FILT->buff_in2[1] - FILT->b1 * FILT->buff_out2[0] -
				FILT->b2 * FILT->buff_out2[1];
			FILT->buff_in2[1] = FILT->buff_in2[0]; //in(n-2) = in(n-1)
			FILT->buff_in2[0] = Buff[index+1]; // in(n-1) = in
			FILT->buff_out2[1] = FILT->buff_out2[0]; //out(n-2) = out(n-1)
			FILT->buff_out2[0] = out; //out(n-1) = out
		
			Buff[index+1] = clip_float(out);
		}
	}
}

/*
 HP Filter: out(n) = a1 * in + a2 * in(n-1) + a3 * in(n-2) - b1*out(n-1) - b2*out(n-2)
  f - cuttof freq., from ~0 Hz to SampleRate/2 - though many synths seem to filter only  up to SampleRate/4
  r  = rez amount, from sqrt(2) to ~ 0.1
 
      c = tan(pi * f / sample_rate);
      a1 = 1.0 / ( 1.0 + r * c + c * c);
      a2 = -2*a1;
      a3 = a1;
      b1 = 2.0 * ( c*c - 1.0) * a1;
      b2 = ( 1.0 - r * c + c * c) * a1;
 
 */
static void 
HPF(struct paRecordData* data, AudBuff *proc_buff, struct audio_effects *aud_eff, int cutoff_freq, float res)
{
	if(aud_eff->HPF == NULL)
	{
		float inv_samprate = 1.0 / data->samprate;
		aud_eff->HPF = g_new0(Filt_data, 1);
		aud_eff->HPF->c = tan(M_PI * cutoff_freq * inv_samprate);
		aud_eff->HPF->a1 = 1.0 / (1.0 + (res * aud_eff->HPF->c) + (aud_eff->HPF->c * aud_eff->HPF->c));
		aud_eff->HPF->a2 = -2.0 * aud_eff->HPF->a1;
		aud_eff->HPF->a3 = aud_eff->HPF->a1;
		aud_eff->HPF->b1 = 2.0 * ((aud_eff->HPF->c * aud_eff->HPF->c) - 1.0) * aud_eff->HPF->a1;
		aud_eff->HPF->b2 = (1.0 - (res * aud_eff->HPF->c) + (aud_eff->HPF->c * aud_eff->HPF->c)) * aud_eff->HPF->a1;
	}

	Butt(aud_eff->HPF, proc_buff->frame, data->aud_numSamples, data->channels);
}
/*
 LP Filter: out(n) = a1 * in + a2 * in(n-1) + a3 * in(n-2) - b1*out(n-1) - b2*out(n-2)
  f - cuttof freq., from ~0 Hz to SampleRate/2 - though many synths seem to filter only  up to SampleRate/4
  r  = rez amount, from sqrt(2) to ~ 0.1
 
      c = 1.0 / tan(pi * f / sample_rate);

      a1 = 1.0 / ( 1.0 + r * c + c * c);
      a2 = 2* a1;
      a3 = a1;
      b1 = 2.0 * ( 1.0 - c*c) * a1;
      b2 = ( 1.0 - r * c + c * c) * a1;

 */
static void 
LPF(struct paRecordData* data, AudBuff *proc_buff, struct audio_effects *aud_eff, float cutoff_freq, float res)
{
	if(aud_eff->LPF1 == NULL)
	{
		aud_eff->LPF1 = g_new0(Filt_data, 1);
		aud_eff->LPF1->c = 1.0 / tan(M_PI * cutoff_freq / data->samprate);
		aud_eff->LPF1->a1 = 1.0 / (1.0 + (res * aud_eff->LPF1->c) + (aud_eff->LPF1->c * aud_eff->LPF1->c));
		aud_eff->LPF1->a2 = 2.0 * aud_eff->LPF1->a1;
		aud_eff->LPF1->a3 = aud_eff->LPF1->a1;
		aud_eff->LPF1->b1 = 2.0 * (1.0 - (aud_eff->LPF1->c * aud_eff->LPF1->c)) * aud_eff->LPF1->a1;
		aud_eff->LPF1->b2 = (1.0 - (res * aud_eff->LPF1->c) + (aud_eff->LPF1->c * aud_eff->LPF1->c)) * aud_eff->LPF1->a1;
	}

	Butt(aud_eff->LPF1, proc_buff->frame, data->aud_numSamples, data->channels);
}

/*
Wave distort
parameters: in - input from [-1..1]
             a - distort parameter from 1 to infinity
*/
/*
SAMPLE 
waveshape_distort( SAMPLE in, float a ) 
{
	return (in*(abs(in) + a)/(in*in + (a-1)*abs(in) + 1));
}
*/
/*
fold back distortion
 parameters: in input from [-1..1]
             treshold - fold treshold value [-1..1]
*/
/*
SAMPLE 
foldback(SAMPLE in, float threshold)
{
	if (in > threshold || in < -threshold)
	{
		in = fabs(fabs(fmod(in - threshold, threshold * 4)) - threshold * 2) - threshold;
	}
	return in;
}
*/

/* Non-linear amplifier with soft distortion curve. */
static SAMPLE CubicAmplifier( SAMPLE input )
{
	SAMPLE out;
	float temp;
	if( input < SAMPLE_SILENCE ) 
	{

		temp = input + 1.0f;
		out = (temp * temp * temp) - 1.0f;
	}
	else 
	{
		temp = input - 1.0f;
		out = (temp * temp * temp) + 1.0f;
	}
	return clip_float(out);
}


#define FUZZ(x) CubicAmplifier(CubicAmplifier(CubicAmplifier(CubicAmplifier(x))))
/* Fuzz distortion */
void Fuzz (struct paRecordData* data, AudBuff *proc_buff, struct audio_effects *aud_eff)
{
	int samp=0;
	for(samp = 0; samp < data->aud_numSamples; samp++)
		proc_buff->frame[samp] = FUZZ(proc_buff->frame[samp]);
	HPF(data, proc_buff, aud_eff, 1000, 0.9);
}

/* four paralell Comb filters for reverb */
static void 
CombFilter4 (struct paRecordData* data,
	AudBuff *proc_buff,
	struct audio_effects *aud_eff,
	int delay1_ms, // delay for filter 1
	int delay2_ms, //delay for filter 2
	int delay3_ms, //delay for filter 3
	int delay4_ms, //delay for filter 4
	float gain1, // feed gain for filter 1
	float gain2, //feed gain for filter 2
	float gain3, //feed gain for filter 3
	float gain4, //feed gain for filter 4
	float in_gain) //input line gain
{
	int samp=0;
	SAMPLE out1, out2, out3, out4;
	/*buff_size in samples*/
	
	if (aud_eff->COMB4 == NULL) 
	{
		aud_eff->COMB4 = g_new0(Comb4_data, 1);
		/*buff_size in samples*/
		aud_eff->COMB4->buff_size1 = (int) delay1_ms * (data->samprate * 0.001);
		aud_eff->COMB4->buff_size2 = (int) delay2_ms * (data->samprate * 0.001);
		aud_eff->COMB4->buff_size3 = (int) delay3_ms * (data->samprate * 0.001);
		aud_eff->COMB4->buff_size4 = (int) delay4_ms * (data->samprate * 0.001);
		
		aud_eff->COMB4->CombBuff10 = g_new0(SAMPLE, aud_eff->COMB4->buff_size1);
		aud_eff->COMB4->CombBuff20 = g_new0(SAMPLE, aud_eff->COMB4->buff_size2);
		aud_eff->COMB4->CombBuff30 = g_new0(SAMPLE, aud_eff->COMB4->buff_size3);
		aud_eff->COMB4->CombBuff40 = g_new0(SAMPLE, aud_eff->COMB4->buff_size4);
		aud_eff->COMB4->CombBuff11 = NULL;
		aud_eff->COMB4->CombBuff21 = NULL;
		aud_eff->COMB4->CombBuff31 = NULL;
		aud_eff->COMB4->CombBuff41 = NULL;
		if(data->channels > 1)
		{
			aud_eff->COMB4->CombBuff11 = g_new0(SAMPLE, aud_eff->COMB4->buff_size1);
			aud_eff->COMB4->CombBuff21 = g_new0(SAMPLE, aud_eff->COMB4->buff_size2);
			aud_eff->COMB4->CombBuff31 = g_new0(SAMPLE, aud_eff->COMB4->buff_size3);
			aud_eff->COMB4->CombBuff41 = g_new0(SAMPLE, aud_eff->COMB4->buff_size4);
		}
	}
	
	for(samp = 0; samp < data->aud_numSamples; samp = samp + data->channels)
	{
		out1 = in_gain * proc_buff->frame[samp] + 
			gain1 * aud_eff->COMB4->CombBuff10[aud_eff->COMB4->CombIndex1];
		out2 = in_gain * proc_buff->frame[samp] + 
			gain2 * aud_eff->COMB4->CombBuff20[aud_eff->COMB4->CombIndex2];
		out3 = in_gain * proc_buff->frame[samp] + 
			gain3 * aud_eff->COMB4->CombBuff30[aud_eff->COMB4->CombIndex3];
		out4 = in_gain * proc_buff->frame[samp] + 
			gain4 * aud_eff->COMB4->CombBuff40[aud_eff->COMB4->CombIndex4];
		
		aud_eff->COMB4->CombBuff10[aud_eff->COMB4->CombIndex1] = proc_buff->frame[samp] + 
			gain1 * aud_eff->COMB4->CombBuff10[aud_eff->COMB4->CombIndex1];
		aud_eff->COMB4->CombBuff20[aud_eff->COMB4->CombIndex2] = proc_buff->frame[samp] + 
			gain2 * aud_eff->COMB4->CombBuff20[aud_eff->COMB4->CombIndex2];
		aud_eff->COMB4->CombBuff30[aud_eff->COMB4->CombIndex3] = proc_buff->frame[samp] + 
			gain3 * aud_eff->COMB4->CombBuff30[aud_eff->COMB4->CombIndex3];
		aud_eff->COMB4->CombBuff40[aud_eff->COMB4->CombIndex4] = proc_buff->frame[samp] + 
			gain4 * aud_eff->COMB4->CombBuff40[aud_eff->COMB4->CombIndex4];
				
		proc_buff->frame[samp] = clip_float(out1 + out2 + out3 + out4);
		
		/*if stereo process second channel */
		if(data->channels > 1)
		{
			out1 = in_gain * proc_buff->frame[samp+1] + 
				gain1 * aud_eff->COMB4->CombBuff11[aud_eff->COMB4->CombIndex1];
			out2 = in_gain * proc_buff->frame[samp+1] + 
				gain2 * aud_eff->COMB4->CombBuff21[aud_eff->COMB4->CombIndex2];
			out3 = in_gain * proc_buff->frame[samp+1] + 
				gain3 * aud_eff->COMB4->CombBuff31[aud_eff->COMB4->CombIndex3];
			out4 = in_gain * proc_buff->frame[samp+1] + 
				gain4 * aud_eff->COMB4->CombBuff41[aud_eff->COMB4->CombIndex4];
		
			aud_eff->COMB4->CombBuff11[aud_eff->COMB4->CombIndex1] = proc_buff->frame[samp+1] + 
				gain1 * aud_eff->COMB4->CombBuff11[aud_eff->COMB4->CombIndex1];
			aud_eff->COMB4->CombBuff21[aud_eff->COMB4->CombIndex2] = proc_buff->frame[samp+1] + 
				gain2 * aud_eff->COMB4->CombBuff21[aud_eff->COMB4->CombIndex2];
			aud_eff->COMB4->CombBuff31[aud_eff->COMB4->CombIndex3] = proc_buff->frame[samp+1] + 
				gain3 * aud_eff->COMB4->CombBuff31[aud_eff->COMB4->CombIndex3];
			aud_eff->COMB4->CombBuff41[aud_eff->COMB4->CombIndex4] = proc_buff->frame[samp+1] + 
				gain4 * aud_eff->COMB4->CombBuff41[aud_eff->COMB4->CombIndex4];
			
			proc_buff->frame[samp+1] = clip_float(out1 + out2 + out3 + out4);
		}
		
		if(++(aud_eff->COMB4->CombIndex1) >= aud_eff->COMB4->buff_size1) aud_eff->COMB4->CombIndex1=0;
		if(++(aud_eff->COMB4->CombIndex2) >= aud_eff->COMB4->buff_size2) aud_eff->COMB4->CombIndex2=0;
		if(++(aud_eff->COMB4->CombIndex3) >= aud_eff->COMB4->buff_size3) aud_eff->COMB4->CombIndex3=0;
		if(++(aud_eff->COMB4->CombIndex4) >= aud_eff->COMB4->buff_size4) aud_eff->COMB4->CombIndex4=0;
	}
}


/* all pass filter                                      */
/*delay_ms: delay in ms                                 */
/*gain: filter gain                                     */
static void 
all_pass (delay_data *AP, SAMPLE *Buff, int NumSamples, int channels, float gain)
{
	int samp = 0;
	float inv_gain = 1.0 / gain;

	for(samp = 0; samp < NumSamples; samp += channels)
	{
		AP->delayBuff1[AP->delayIndex] = Buff[samp] + 
			(gain * AP->delayBuff1[AP->delayIndex]);
		Buff[samp] = ((AP->delayBuff1[AP->delayIndex] * (1 - gain*gain)) - 
			Buff[samp]) * inv_gain;
		if(channels > 1)
		{
			AP->delayBuff2[AP->delayIndex] = Buff[samp+1] + 
				(gain * AP->delayBuff2[AP->delayIndex]);
			Buff[samp+1] = ((AP->delayBuff2[AP->delayIndex] * (1 - gain*gain)) - 
				Buff[samp+1]) * inv_gain;
		}
		
		if(++(AP->delayIndex) >= AP->buff_size) AP->delayIndex=0;
	} 
}

/*all pass for reverb*/
static void 
all_pass1 (struct paRecordData* data, AudBuff *proc_buff, struct audio_effects *aud_eff, int delay_ms, float gain)
{
	if(aud_eff->AP1 == NULL)
	{
		aud_eff->AP1 = g_new0(delay_data, 1);
		aud_eff->AP1->buff_size = (int) delay_ms  * (data->samprate * 0.001);
		aud_eff->AP1->delayBuff1 = g_new0(SAMPLE, aud_eff->AP1->buff_size);
		aud_eff->AP1->delayBuff2 = NULL;
		if(data->channels > 1)
			aud_eff->AP1->delayBuff2 = g_new0(SAMPLE, aud_eff->AP1->buff_size);
	}
	
	all_pass (aud_eff->AP1, proc_buff->frame, data->aud_numSamples, data->channels, gain);
}

void Reverb (struct paRecordData* data, AudBuff *proc_buff, struct audio_effects *aud_eff, int delay_ms)
{
	/*4 parallel comb filters*/
	CombFilter4 (data, proc_buff, aud_eff, delay_ms, delay_ms - 5, delay_ms -10, delay_ms -15, 
		0.55, 0.6, 0.5, 0.45, 0.7);
	/*all pass*/
	all_pass1 (data, proc_buff, aud_eff, delay_ms, 0.75);
}


/* Parameters:
	freq - LFO frequency (1.5)
	startphase - LFO startphase in RADIANS - usefull for stereo WahWah (0)
	depth - Wah depth (0.7)
	freqofs - Wah frequency offset (0.3)
	res - Resonance (2.5)

	!!!!!!!!!!!!! IMPORTANT!!!!!!!!! :
	depth and freqofs should be from 0(min) to 1(max) !
	res should be greater than 0 !  */

#define lfoskipsamples 30

void WahWah (struct paRecordData* data, AudBuff *proc_buff, struct audio_effects *aud_eff, 
	float freq, float startphase, float depth, float freqofs, float res)
{
	float frequency, omega, sn, cs, alpha;
	float in, out;
	
	if(aud_eff->wahData == NULL) 
	{
		aud_eff->wahData = g_new0(WAHData, 1);
		aud_eff->wahData->lfoskip = freq * 2 * M_PI / data->samprate;
		aud_eff->wahData->phase = startphase;
		//if right channel set: phase += (float)M_PI;
	}

	int samp = 0;
	for(samp = 0; samp < data->aud_numSamples; samp++)
	{
		in = proc_buff->frame[samp];
		
		if ((aud_eff->wahData->skipcount++) % lfoskipsamples == 0) 
		{
			frequency = (1 + cos(aud_eff->wahData->skipcount * aud_eff->wahData->lfoskip + aud_eff->wahData->phase)) * 0.5;
			frequency = frequency * depth * (1 - freqofs) + freqofs;
			frequency = exp((frequency - 1) * 6);
			omega = M_PI * frequency;
			sn = sin(omega);
			cs = cos(omega);
			alpha = sn / (2 * res);
			aud_eff->wahData->b0 = (1 - cs) * 0.5;
			aud_eff->wahData->b1 = 1 - cs;
			aud_eff->wahData->b2 = (1 - cs) * 0.5;
			aud_eff->wahData->a0 = 1 + alpha;
			aud_eff->wahData->a1 = -2 * cs;
			aud_eff->wahData->a2 = 1 - alpha;
		}
		out = (aud_eff->wahData->b0 * in + aud_eff->wahData->b1 * aud_eff->wahData->xn1 + 
			aud_eff->wahData->b2 * aud_eff->wahData->xn2 - aud_eff->wahData->a1 * aud_eff->wahData->yn1 - 
			aud_eff->wahData->a2 * aud_eff->wahData->yn2) / aud_eff->wahData->a0;
		aud_eff->wahData->xn2 = aud_eff->wahData->xn1;
		aud_eff->wahData->xn1 = in;
		aud_eff->wahData->yn2 = aud_eff->wahData->yn1;
		aud_eff->wahData->yn1 = out;

		proc_buff->frame[samp] = clip_float(out);
	}
}

/*reduce number of samples with linear interpolation*/
//rate - rate of samples to remove [1,...[ 
// rate = 1-> XXX (splits channels) 2 -> X0X0X  3 -> X00X00X 4 -> X000X000X
static void
change_rate_less(RATE_data *RT, SAMPLE *Buff, int rate, int NumSamples, int channels)
{
	int samp = 0;
	int n = 0, i = 0;
	//float mid1 = 0.0;
	//float mid2 = 0.0;
	//float inv_rate = 1.0 / rate;
	
	for (samp = 0; samp < NumSamples; samp += channels)
	{
		
		
		if (n==0)
		{
			RT->rBuff1[i] = Buff[samp];
			if(channels > 1)
				RT->rBuff2[i] = Buff[samp + 1];
			
			i++;
		}
		if(++n >= rate) n=0;
	}
	RT->numsamples = i;
}

/*increase audio tempo by adding audio windows of wtime_ms in given rate                           */
/*rate: 2 -> [w1..w2][w1..w2][w2..w3][w2..w3]  3-> [w1..w2][w1..w2][w1..w2][w2..w3][w2..w3][w2..w3]*/ 
static void
change_tempo_more(struct paRecordData* data, AudBuff *proc_buff, struct audio_effects *aud_eff, int rate, int wtime_ms)
{
	int samp = 0;
	int i = 0;
	int r = 0;
	int index = 0;
	
	if(aud_eff->RT1->wBuff1 == NULL) 
	{
		aud_eff->RT1->wSize  = wtime_ms * data->samprate * 0.001;
		aud_eff->RT1->wBuff1 = g_new0(SAMPLE, aud_eff->RT1->wSize);
		if (data->channels >1)
			aud_eff->RT1->wBuff2 = g_new0(SAMPLE, aud_eff->RT1->wSize);
	}
	
	//printf("samples  = %i\n", data->RT1->numsamples);
	for(samp = 0; samp < aud_eff->RT1->numsamples; samp++)
	{
		aud_eff->RT1->wBuff1[i] = aud_eff->RT1->rBuff1[samp];
		if(data->channels > 1)
			aud_eff->RT1->wBuff2[i] = aud_eff->RT1->rBuff2[samp];
		
		if((++i) > aud_eff->RT1->wSize)
		{
			for (r = 0; r < rate; r++)
			{
				for(i = 0; i < aud_eff->RT1->wSize; i++)
				{
					proc_buff->frame[index] = aud_eff->RT1->wBuff1[i];
					if (data->channels > 1)
						proc_buff->frame[index +1] = aud_eff->RT1->wBuff2[i];
					index += data->channels;
				}
			}
			i = 0;
		}
	}
}

void 
change_pitch (struct paRecordData* data, AudBuff *proc_buff, struct audio_effects *aud_eff, int rate)
{
	if(aud_eff->RT1 == NULL)
	{
		aud_eff->RT1 = g_new0(RATE_data, 1);
		
		aud_eff->RT1->wBuff1 = NULL;
		aud_eff->RT1->wBuff2 = NULL;
		aud_eff->RT1->rBuff1 = g_new0(SAMPLE, (data->aud_numSamples/data->channels));
		aud_eff->RT1->rBuff2 = NULL;
		if(data->channels > 1)
			aud_eff->RT1->rBuff2 = g_new0(SAMPLE, (data->aud_numSamples/data->channels));
		
	}
	
	//printf("all samples: %i\n",data->aud_numSamples);
	change_rate_less(aud_eff->RT1, proc_buff->frame, rate, data->aud_numSamples, data->channels);
	change_tempo_more(data, proc_buff, aud_eff, rate, 20);
	LPF(data, proc_buff, aud_eff, (data->samprate * 0.25), 0.9);
}

struct audio_effects* init_audio_effects (void)
{
	struct audio_effects* aud_eff;
	aud_eff = g_new0(struct audio_effects, 1);
	/*Echo effect data */
	aud_eff->ECHO = NULL;
	/* 4 parallel comb filters data*/
	aud_eff->COMB4 = NULL;
	/*all pass 1 filter data*/
	aud_eff->AP1 = NULL;
	/*WahWah effect data*/ 
	aud_eff->wahData = NULL;
	/*high pass filter data*/
	aud_eff->HPF = NULL;
	/*rate transposer*/
	aud_eff->RT1 = NULL;
	/*low pass filter*/
	aud_eff->LPF1 = NULL;
	
	return (aud_eff);
}

void
close_DELAY(delay_data *DELAY)
{
	if(DELAY != NULL)
	{
		g_free(DELAY->delayBuff1);
		g_free(DELAY->delayBuff2);
		g_free(DELAY);
	}
}

static void 
close_COMB4(Comb4_data *COMB4)
{
	if(COMB4 != NULL)
	{
		g_free(COMB4->CombBuff10);
		g_free(COMB4->CombBuff20);
		g_free(COMB4->CombBuff30);
		g_free(COMB4->CombBuff40);
		
		g_free(COMB4->CombBuff11);
		g_free(COMB4->CombBuff21);
		g_free(COMB4->CombBuff31);
		g_free(COMB4->CombBuff41);
		
		g_free(COMB4);
	}
}

void
close_FILT(Filt_data *FILT)
{
	if(FILT != NULL)
	{
		g_free(FILT);
	}
}

void
close_WAHWAH(WAHData *wahData)
{
	if(wahData != NULL)
	{
		g_free(wahData);
	}
}

void
close_REVERB(struct audio_effects *aud_eff)
{
	close_DELAY(aud_eff->AP1);
	aud_eff->AP1 = NULL;
	close_COMB4(aud_eff->COMB4);
	aud_eff->COMB4 = NULL;
}

void 
close_pitch (struct audio_effects *aud_eff)
{
	if(aud_eff->RT1 != NULL)
	{
		g_free(aud_eff->RT1->rBuff1);
		g_free(aud_eff->RT1->rBuff2);
		g_free(aud_eff->RT1->wBuff1);
		g_free(aud_eff->RT1->wBuff2);
		g_free(aud_eff->RT1);
		aud_eff->RT1 = NULL;
		close_FILT(aud_eff->LPF1);
		aud_eff->LPF1 = NULL;
	}
}

void close_audio_effects (struct audio_effects *aud_eff)
{
	close_DELAY(aud_eff->ECHO);
	aud_eff->ECHO = NULL;
	close_REVERB(aud_eff);
	close_WAHWAH(aud_eff->wahData);
	aud_eff->wahData = NULL;
	close_FILT(aud_eff->HPF);
	aud_eff->HPF = NULL;
	close_pitch(aud_eff);
	g_free(aud_eff);
	aud_eff = NULL;
}
