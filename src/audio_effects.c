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
void Echo(struct paRecordData* data, int delay_ms, float decay)
{
	int samp=0;
	SAMPLE out;
	
	if(data->ECHO == NULL)
	{
		data->ECHO = g_new0(delay_data, 1);
		data->ECHO->buff_size = (int) delay_ms * data->samprate * 0.001;
		data->ECHO->delayBuff1 = g_new0(SAMPLE, data->ECHO->buff_size);
		data->ECHO->delayBuff2 = NULL;
		if(data->channels > 1)
			data->ECHO->delayBuff2 = g_new0(SAMPLE, data->ECHO->buff_size);
	}
	
	for(samp = 0; samp < data->snd_numSamples; samp = samp + data->channels)
	{
		out = (0.7 * data->avi_sndBuff[samp]) + 
			(0.3 * data->ECHO->delayBuff1[data->ECHO->delayIndex]);
		data->ECHO->delayBuff1[data->ECHO->delayIndex] = data->avi_sndBuff[samp] + 
			(data->ECHO->delayBuff1[data->ECHO->delayIndex] * decay);
		data->avi_sndBuff[samp] = clip_float(out);
		/*if stereo process second channel in separate*/
		if (data->channels > 1)
		{
			out = (0.7 * data->avi_sndBuff[samp+1]) + 
				(0.3 * data->ECHO->delayBuff2[data->ECHO->delayIndex]);
			data->ECHO->delayBuff2[data->ECHO->delayIndex] = data->avi_sndBuff[samp] + 
				(data->ECHO->delayBuff2[data->ECHO->delayIndex] * decay);
			data->avi_sndBuff[samp+1] = clip_float(out);
		}
		
		if(++(data->ECHO->delayIndex) >= data->ECHO->buff_size) data->ECHO->delayIndex=0;
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
HPF(struct paRecordData* data, int cutoff_freq, float res)
{
	if(data->HPF == NULL)
	{
		float inv_samprate = 1.0 / data->samprate;
		data->HPF = g_new0(Filt_data, 1);
		data->HPF->c = tan(M_PI * cutoff_freq * inv_samprate);
		data->HPF->a1 = 1.0 / (1.0 + (res * data->HPF->c) + (data->HPF->c * data->HPF->c));
		data->HPF->a2 = -2.0 * data->HPF->a1;
		data->HPF->a3 = data->HPF->a1;
		data->HPF->b1 = 2.0 * ((data->HPF->c * data->HPF->c) - 1.0) * data->HPF->a1;
		data->HPF->b2 = (1.0 - (res * data->HPF->c) + (data->HPF->c * data->HPF->c)) * data->HPF->a1;
	}

	Butt(data->HPF, data->avi_sndBuff, data->snd_numSamples, data->channels);
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
LPF(struct paRecordData* data, float cutoff_freq, float res)
{
	if(data->LPF1 == NULL)
	{
		data->LPF1 = g_new0(Filt_data, 1);
		data->LPF1->c = 1.0 / tan(M_PI * cutoff_freq / data->samprate);
		data->LPF1->a1 = 1.0 / (1.0 + (res * data->LPF1->c) + (data->LPF1->c * data->LPF1->c));
		data->LPF1->a2 = 2.0 * data->LPF1->a1;
		data->LPF1->a3 = data->LPF1->a1;
		data->LPF1->b1 = 2.0 * (1.0 - (data->LPF1->c * data->LPF1->c)) * data->LPF1->a1;
		data->LPF1->b2 = (1.0 - (res * data->LPF1->c) + (data->LPF1->c * data->LPF1->c)) * data->LPF1->a1;
	}

	Butt(data->LPF1, data->avi_sndBuff, data->snd_numSamples, data->channels);
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
void Fuzz (struct paRecordData* data)
{
	int samp=0;
	for(samp = 0; samp < data->snd_numSamples; samp++)
		data->avi_sndBuff[samp] = FUZZ(data->avi_sndBuff[samp]);
	HPF(data, 1000, 0.9);
}

/* four paralell Comb filters for reverb */
static void 
CombFilter4 (struct paRecordData* data, 
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
	
	if (data->COMB4 == NULL) 
	{
		data->COMB4 = g_new0(Comb4_data, 1);
		/*buff_size in samples*/
		data->COMB4->buff_size1 = (int) delay1_ms * (data->samprate * 0.001);
		data->COMB4->buff_size2 = (int) delay2_ms * (data->samprate * 0.001);
		data->COMB4->buff_size3 = (int) delay3_ms * (data->samprate * 0.001);
		data->COMB4->buff_size4 = (int) delay4_ms * (data->samprate * 0.001);
		
		data->COMB4->CombBuff10 = g_new0(SAMPLE, data->COMB4->buff_size1);
		data->COMB4->CombBuff20 = g_new0(SAMPLE, data->COMB4->buff_size2);
		data->COMB4->CombBuff30 = g_new0(SAMPLE, data->COMB4->buff_size3);
		data->COMB4->CombBuff40 = g_new0(SAMPLE, data->COMB4->buff_size4);
		data->COMB4->CombBuff11 = NULL;
		data->COMB4->CombBuff21 = NULL;
		data->COMB4->CombBuff31 = NULL;
		data->COMB4->CombBuff41 = NULL;
		if(data->channels > 1)
		{
			data->COMB4->CombBuff11 = g_new0(SAMPLE, data->COMB4->buff_size1);
			data->COMB4->CombBuff21 = g_new0(SAMPLE, data->COMB4->buff_size2);
			data->COMB4->CombBuff31 = g_new0(SAMPLE, data->COMB4->buff_size3);
			data->COMB4->CombBuff41 = g_new0(SAMPLE, data->COMB4->buff_size4);
		}
	}
	
	for(samp = 0; samp < data->snd_numSamples; samp = samp + data->channels)
	{
		out1 = in_gain * data->avi_sndBuff[samp] + 
			gain1 * data->COMB4->CombBuff10[data->COMB4->CombIndex1];
		out2 = in_gain * data->avi_sndBuff[samp] + 
			gain2 * data->COMB4->CombBuff20[data->COMB4->CombIndex2];
		out3 = in_gain * data->avi_sndBuff[samp] + 
			gain3 * data->COMB4->CombBuff30[data->COMB4->CombIndex3];
		out4 = in_gain * data->avi_sndBuff[samp] + 
			gain4 * data->COMB4->CombBuff40[data->COMB4->CombIndex4];
		
		data->COMB4->CombBuff10[data->COMB4->CombIndex1] = data->avi_sndBuff[samp] + 
			gain1 * data->COMB4->CombBuff10[data->COMB4->CombIndex1];
		data->COMB4->CombBuff20[data->COMB4->CombIndex2] = data->avi_sndBuff[samp] + 
			gain2 * data->COMB4->CombBuff20[data->COMB4->CombIndex2];
		data->COMB4->CombBuff30[data->COMB4->CombIndex3] = data->avi_sndBuff[samp] + 
			gain3 * data->COMB4->CombBuff30[data->COMB4->CombIndex3];
		data->COMB4->CombBuff40[data->COMB4->CombIndex4] = data->avi_sndBuff[samp] + 
			gain4 * data->COMB4->CombBuff40[data->COMB4->CombIndex4];
				
		data->avi_sndBuff[samp] = clip_float(out1 + out2 + out3 + out4);
		
		/*if stereo process second channel */
		if(data->channels > 1)
		{
			out1 = in_gain * data->avi_sndBuff[samp+1] + 
				gain1 * data->COMB4->CombBuff11[data->COMB4->CombIndex1];
			out2 = in_gain * data->avi_sndBuff[samp+1] + 
				gain2 * data->COMB4->CombBuff21[data->COMB4->CombIndex2];
			out3 = in_gain * data->avi_sndBuff[samp+1] + 
				gain3 * data->COMB4->CombBuff31[data->COMB4->CombIndex3];
			out4 = in_gain * data->avi_sndBuff[samp+1] + 
				gain4 * data->COMB4->CombBuff41[data->COMB4->CombIndex4];
		
			data->COMB4->CombBuff11[data->COMB4->CombIndex1] = data->avi_sndBuff[samp+1] + 
				gain1 * data->COMB4->CombBuff11[data->COMB4->CombIndex1];
			data->COMB4->CombBuff21[data->COMB4->CombIndex2] = data->avi_sndBuff[samp+1] + 
				gain2 * data->COMB4->CombBuff21[data->COMB4->CombIndex2];
			data->COMB4->CombBuff31[data->COMB4->CombIndex3] = data->avi_sndBuff[samp+1] + 
				gain3 * data->COMB4->CombBuff31[data->COMB4->CombIndex3];
			data->COMB4->CombBuff41[data->COMB4->CombIndex4] = data->avi_sndBuff[samp+1] + 
				gain4 * data->COMB4->CombBuff41[data->COMB4->CombIndex4];
			
			data->avi_sndBuff[samp+1] = clip_float(out1 + out2 + out3 + out4);
		}
		
		if(++(data->COMB4->CombIndex1) >= data->COMB4->buff_size1) data->COMB4->CombIndex1=0;
		if(++(data->COMB4->CombIndex2) >= data->COMB4->buff_size2) data->COMB4->CombIndex2=0;
		if(++(data->COMB4->CombIndex3) >= data->COMB4->buff_size3) data->COMB4->CombIndex3=0;
		if(++(data->COMB4->CombIndex4) >= data->COMB4->buff_size4) data->COMB4->CombIndex4=0;
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
all_pass1 (struct paRecordData* data, int delay_ms, float gain)
{
	if(data->AP1 == NULL)
	{
		data->AP1 = g_new0(delay_data, 1);
		data->AP1->buff_size = (int) delay_ms  * (data->samprate * 0.001);
		data->AP1->delayBuff1 = g_new0(SAMPLE, data->AP1->buff_size);
		data->AP1->delayBuff2 = NULL;
		if(data->channels > 1)
			data->AP1->delayBuff2 = g_new0(SAMPLE, data->AP1->buff_size);
	}
	
	all_pass (data->AP1, data->avi_sndBuff, data->snd_numSamples, data->channels, gain);
}

void Reverb (struct paRecordData* data, int delay_ms)
{
	/*4 parallel comb filters*/
	CombFilter4 (data, delay_ms, delay_ms - 5, delay_ms -10, delay_ms -15, 
		0.55, 0.6, 0.5, 0.45, 0.7);
	/*all pass*/
	all_pass1 (data, delay_ms, 0.75);
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

void WahWah (struct paRecordData* data, float freq, float startphase, float depth, float freqofs, float res)
{
	float frequency, omega, sn, cs, alpha;
	float in, out;
	
	if(data->wahData == NULL) 
	{
		data->wahData = g_new0(WAHData, 1);
		data->wahData->lfoskip = freq * 2 * M_PI / data->samprate;
		data->wahData->phase = startphase;
		//if right channel set: phase += (float)M_PI;
	}

	int samp = 0;
	for(samp = 0; samp < data->snd_numSamples; samp++)
	{
		in = data->avi_sndBuff[samp];
		
		if ((data->wahData->skipcount++) % lfoskipsamples == 0) 
		{
			frequency = (1 + cos(data->wahData->skipcount * data->wahData->lfoskip + data->wahData->phase)) * 0.5;
			frequency = frequency * depth * (1 - freqofs) + freqofs;
			frequency = exp((frequency - 1) * 6);
			omega = M_PI * frequency;
			sn = sin(omega);
			cs = cos(omega);
			alpha = sn / (2 * res);
			data->wahData->b0 = (1 - cs) * 0.5;
			data->wahData->b1 = 1 - cs;
			data->wahData->b2 = (1 - cs) * 0.5;
			data->wahData->a0 = 1 + alpha;
			data->wahData->a1 = -2 * cs;
			data->wahData->a2 = 1 - alpha;
		}
		out = (data->wahData->b0 * in + data->wahData->b1 * data->wahData->xn1 + 
			data->wahData->b2 * data->wahData->xn2 - data->wahData->a1 * data->wahData->yn1 - 
			data->wahData->a2 * data->wahData->yn2) / data->wahData->a0;
		data->wahData->xn2 = data->wahData->xn1;
		data->wahData->xn1 = in;
		data->wahData->yn2 = data->wahData->yn1;
		data->wahData->yn1 = out;

		data->avi_sndBuff[samp] = clip_float(out);
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
change_tempo_more(struct paRecordData* data, int rate, int wtime_ms)
{
	int samp = 0;
	int i = 0;
	int r = 0;
	int index = 0;
	
	if(data->RT1->wBuff1 == NULL) 
	{
		data->RT1->wSize  = wtime_ms * data->samprate * 0.001;
		data->RT1->wBuff1 = g_new0(SAMPLE, data->RT1->wSize);
		if (data->channels >1)
			data->RT1->wBuff2 = g_new0(SAMPLE, data->RT1->wSize);
	}
	
	//printf("samples  = %i\n", data->RT1->numsamples);
	for(samp = 0; samp < data->RT1->numsamples; samp++)
	{
		data->RT1->wBuff1[i] = data->RT1->rBuff1[samp];
		if(data->channels > 1)
			data->RT1->wBuff2[i] = data->RT1->rBuff2[samp];
		
		if((++i) > data->RT1->wSize)
		{
			for (r = 0; r < rate; r++)
			{
				for(i = 0; i < data->RT1->wSize; i++)
				{
					data->avi_sndBuff[index] = data->RT1->wBuff1[i];
					if (data->channels > 1)
						data->avi_sndBuff[index +1] = data->RT1->wBuff2[i];
					index += data->channels;
				}
			}
			i = 0;
		}
	}
}

void 
close_pitch (struct paRecordData* data)
{
	if(data->RT1 != NULL)
	{
		g_free(data->RT1->rBuff1);
		g_free(data->RT1->rBuff2);
		g_free(data->RT1->wBuff1);
		g_free(data->RT1->wBuff2);
		g_free(data->RT1);
		data->RT1 = NULL;
		close_FILT(data->LPF1);
		data->LPF1 = NULL;
	}
}

void 
change_pitch (struct paRecordData* data, int rate)
{
	if(data->RT1 == NULL)
	{
		data->RT1 = g_new0(RATE_data, 1);
		
		data->RT1->wBuff1 = NULL;
		data->RT1->wBuff2 = NULL;
		data->RT1->rBuff1 = g_new0(SAMPLE, (data->snd_numSamples/data->channels));
		data->RT1->rBuff2 = NULL;
		if(data->channels > 1)
			data->RT1->rBuff2 = g_new0(SAMPLE, (data->snd_numSamples/data->channels));
		
	}
	
	//printf("all samples: %i\n",data->snd_numSamples);
	change_rate_less(data->RT1, data->avi_sndBuff, rate, data->snd_numSamples, data->channels);
	change_tempo_more(data, rate, 20);
	LPF(data, (data->samprate * 0.25), 0.9);
}

