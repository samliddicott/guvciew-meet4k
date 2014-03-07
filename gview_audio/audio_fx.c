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

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
/* support for internationalization - i18n */
#include <locale.h>
#include <libintl.h>

#include "../config.h"
#include "gviewaudio.h"
#include "gview.h"

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif

/*----------- structs for audio effects ------------*/

/*data for Butterworth filter (LP or HP)*/
typedef struct _fx_filt_data_t
{
	sample_t buff_in1[2];
	sample_t buff_in2[2];
	sample_t buff_out1[2];
	sample_t buff_out2[2];
	float c;
	float a1;
	float a2;
	float a3;
	float b1;
	float b2;
} fx_filt_data_t;

/*data for Comb4 filter*/
typedef struct _fx_comb4_data_t
{
	int buff_size1;
	int buff_size2;
	int buff_size3;
	int buff_size4;

	sample_t *CombBuff10; // four parallel  comb filters - first channel
	sample_t *CombBuff11; // four parallel comb filters - second channel
	sample_t *CombBuff20; // four parallel  comb filters - first channel
	sample_t *CombBuff21; // four parallel comb filters - second channel
	sample_t *CombBuff30; // four parallel  comb filters - first channel
	sample_t *CombBuff31; // four parallel comb filters - second channel
	sample_t *CombBuff40; // four parallel  comb filters - first channel
	sample_t *CombBuff41; // four parallel comb filters - second channel

	int CombIndex1; //comb filter 1 index
	int CombIndex2; //comb filter 2 index
	int CombIndex3; //comb filter 3 index
	int CombIndex4; //comb filter 4 index
} fx_comb4_data_t;

/* data for delay*/
typedef struct _fx_delay_data_t
{
	int buff_size;
	sample_t *delayBuff1; // delay buffer 1 - first channel
	sample_t *delayBuff2; // delay buffer 2 - second channel (stereo)
	int delayIndex; // delay buffer index
} fx_delay_data_t;

/* data for WahWah effect*/
typedef struct _fx_wah_data_t
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
} fx_wah_data_t;

typedef struct _fx_tsd_data_t
{
	float tempo;
} fx_tsd_data_t;

typedef struct _fx_rate_data_t
{
	sample_t *rBuff1;
	sample_t *rBuff2;
	sample_t *wBuff1;
	sample_t *wBuff2;
	int wSize;
	int numsamples;
} fx_rate_data_t;

typedef struct _audio_fx_t
{
	fx_delay_data_t *ECHO;
	fx_delay_data_t *AP1;
	fx_comb4_data_t *COMB4;
	fx_filt_data_t  *HPF;
	fx_filt_data_t  *LPF1;
	fx_rate_data_t  *RT1;
	fx_wah_data_t   *wahData;
} audio_fx_t;

/*audio fx data*/
static audio_fx_t *aud_fx = NULL;

/*
 * initialize audio fx data
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void audio_fx_init ()
{
	aud_fx = calloc(1, sizeof(audio_fx_t));
	/*Echo effect data */
	aud_fx->ECHO = NULL;
	/* 4 parallel comb filters data*/
	aud_fx->COMB4 = NULL;
	/*all pass 1 filter data*/
	aud_fx->AP1 = NULL;
	/*WahWah effect data*/
	aud_fx->wahData = NULL;
	/*high pass filter data*/
	aud_fx->HPF = NULL;
	/*rate transposer*/
	aud_fx->RT1 = NULL;
	/*low pass filter*/
	aud_fx->LPF1 = NULL;
}

/*
 * clip float samples [-1.0 ; 1.0]
 * args:
 *   in - float sample
 *
 * asserts:
 *   none
 *
 * returns: float sample
 */
static float clip_float (float in)
{
	in = (in < -1.0) ? -1.0 : (in > 1.0) ? 1.0 : in;

	return in;
}

/*
 * Butterworth Filter for HP or LP
 * out(n) = a1 * in + a2 * in(n-1) + a3 * in(n-2) - b1*out(n-1) - b2*out(n-2)
 * args:
 *   FILT - pointer to fx_filt_data_t
 *   Buff - sampe buffer
 *   NumSamples - samples in buffer
 *   channels - number of audio channels
 */
static void Butt(fx_filt_data_t *FILT,
	sample_t *Buff,
	int NumSamples,
	int channels)
{
	int index = 0;
	sample_t out;

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
 * HP Filter: out(n) = a1 * in + a2 * in(n-1) + a3 * in(n-2) - b1*out(n-1) - b2*out(n-2)
 * f - cuttof freq., from ~0 Hz to SampleRate/2 - though many synths seem to filter only  up to SampleRate/4
 * r  = rez amount, from sqrt(2) to ~ 0.1
 *
 *  c = tan(pi * f / sample_rate);
 *  a1 = 1.0 / ( 1.0 + r * c + c * c);
 *  a2 = -2*a1;
 *  a3 = a1;
 *  b1 = 2.0 * ( c*c - 1.0) * a1;
 *  b2 = ( 1.0 - r * c + c * c) * a1;
 * args:
 *   audio_ctx - pointer to audio context
 *   proc_buff -pointer to audio buffer to be processed
 *   cutoff_freq - filter cut off frequency
 *   res - rez amount
 *
 * asserts:
 *    none
 *
 * returns: none
 */
static void HPF(audio_context_t *audio_ctx,
	audio_buff_t *proc_buff,
	int cutoff_freq,
	float res)
{
	if(aud_fx->HPF == NULL)
	{
		float inv_samprate = 1.0 / audio_ctx->samprate;
		aud_fx->HPF = calloc(1, sizeof(fx_filt_data_t));
		aud_fx->HPF->c = tan(M_PI * cutoff_freq * inv_samprate);
		aud_fx->HPF->a1 = 1.0 / (1.0 + (res * aud_fx->HPF->c) + (aud_fx->HPF->c * aud_fx->HPF->c));
		aud_fx->HPF->a2 = -2.0 * aud_fx->HPF->a1;
		aud_fx->HPF->a3 = aud_fx->HPF->a1;
		aud_fx->HPF->b1 = 2.0 * ((aud_fx->HPF->c * aud_fx->HPF->c) - 1.0) * aud_fx->HPF->a1;
		aud_fx->HPF->b2 = (1.0 - (res * aud_fx->HPF->c) + (aud_fx->HPF->c * aud_fx->HPF->c)) * aud_fx->HPF->a1;
	}

	Butt(aud_fx->HPF, proc_buff->data, audio_ctx->capture_buff_size, audio_ctx->channels);
}

/*
 * LP Filter: out(n) = a1 * in + a2 * in(n-1) + a3 * in(n-2) - b1*out(n-1) - b2*out(n-2)
 * f - cuttof freq., from ~0 Hz to SampleRate/2 -
 *     though many synths seem to filter only  up to SampleRate/4
 * r  = rez amount, from sqrt(2) to ~ 0.1
 *
 * c = 1.0 / tan(pi * f / sample_rate);
 * a1 = 1.0 / ( 1.0 + r * c + c * c);
 * a2 = 2* a1;
 * a3 = a1;
 * b1 = 2.0 * ( 1.0 - c*c) * a1;
 * b2 = ( 1.0 - r * c + c * c) * a1;
 *
 * args:
 *   audio_ctx - pointer to audio context
 *   proc_buff -pointer to audio buffer to be processed
 *   cutoff_freq - filter cut off frequency
 *   res - rez amount
 *
 * asserts:
 *    none
 *
 * returns: none
 */
static void LPF(audio_context_t *audio_ctx,
	audio_buff_t *proc_buff,
	float cutoff_freq,
	float res)
{
	if(aud_fx->LPF1 == NULL)
	{
		aud_fx->LPF1 = calloc(1, sizeof(fx_filt_data_t));
		aud_fx->LPF1->c = 1.0 / tan(M_PI * cutoff_freq / audio_ctx->samprate);
		aud_fx->LPF1->a1 = 1.0 / (1.0 + (res * aud_fx->LPF1->c) + (aud_fx->LPF1->c * aud_fx->LPF1->c));
		aud_fx->LPF1->a2 = 2.0 * aud_fx->LPF1->a1;
		aud_fx->LPF1->a3 = aud_fx->LPF1->a1;
		aud_fx->LPF1->b1 = 2.0 * (1.0 - (aud_fx->LPF1->c * aud_fx->LPF1->c)) * aud_fx->LPF1->a1;
		aud_fx->LPF1->b2 = (1.0 - (res * aud_fx->LPF1->c) + (aud_fx->LPF1->c * aud_fx->LPF1->c)) * aud_fx->LPF1->a1;
	}

	Butt(aud_fx->LPF1, proc_buff->data, audio_ctx->capture_buff_size, audio_ctx->channels);
}

/* Non-linear amplifier with soft distortion curve.
 * args:
 *   input - sample input
 *
 * asserts:
 *   none
 *
 * returns: processed sample
 */
static sample_t CubicAmplifier( sample_t input )
{
	sample_t out;
	float temp;
	if( input < 0 ) /*silence*/
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

/*
 * Echo effect
 * args:
 *   audio_ctx - audio context
 *   proc_buff - audio buffer to be processed
 *   delay_ms - echo delay in ms
 *   decay - feedback gain (<1)
 *
 * asserts:
 *   audio_ctx is not null
 *
 * returns: none
 */
void audio_fx_echo(audio_context_t *audio_ctx,
	audio_buff_t *proc_buff,
	int delay_ms,
	float decay)
{
	int samp=0;
	sample_t out;

	if(aud_fx->ECHO == NULL)
	{
		aud_fx->ECHO = calloc(1, sizeof(fx_delay_data_t));
		aud_fx->ECHO->buff_size = (int) delay_ms * audio_ctx->samprate * 0.001;
		aud_fx->ECHO->delayBuff1 = calloc(aud_fx->ECHO->buff_size, sizeof(sample_t));
		aud_fx->ECHO->delayBuff2 = NULL;
		if(audio_ctx->channels > 1)
			aud_fx->ECHO->delayBuff2 = calloc(aud_fx->ECHO->buff_size, sizeof(sample_t));
	}

	for(samp = 0; samp < audio_ctx->capture_buff_size; samp = samp + audio_ctx->channels)
	{
		out = (0.7 * proc_buff->data[samp]) +
			(0.3 * aud_fx->ECHO->delayBuff1[aud_fx->ECHO->delayIndex]);
		aud_fx->ECHO->delayBuff1[aud_fx->ECHO->delayIndex] = proc_buff->data[samp] +
			(aud_fx->ECHO->delayBuff1[aud_fx->ECHO->delayIndex] * decay);
		proc_buff->data[samp] = clip_float(out);
		/*if stereo process second channel in separate*/
		if (audio_ctx->channels > 1)
		{
			out = (0.7 * proc_buff->data[samp+1]) +
				(0.3 * aud_fx->ECHO->delayBuff2[aud_fx->ECHO->delayIndex]);
			aud_fx->ECHO->delayBuff2[aud_fx->ECHO->delayIndex] = proc_buff->data[samp] +
				(aud_fx->ECHO->delayBuff2[aud_fx->ECHO->delayIndex] * decay);
			proc_buff->data[samp+1] = clip_float(out);
		}

		if(++(aud_fx->ECHO->delayIndex) >= aud_fx->ECHO->buff_size) aud_fx->ECHO->delayIndex=0;
	}
}



/*
 * clean fx_delay_data_t
 * args:
 *   DELAY - pointer to fx_delay_data_t
 *
 * asserts:
 *   none
 *
 * returns: none
 */
static void close_DELAY(fx_delay_data_t *DELAY)
{
	if(DELAY != NULL)
	{
		free(DELAY->delayBuff1);
		free(DELAY->delayBuff2);
		free(DELAY);
	}
}

/*
 * clean fx_comb4_data_t
 * args:
 *   COMB4 - pointer to fx_comb4_data_t
 *
 * asserts:
 *   none
 *
 * returns: none
 */
static void close_COMB4(fx_comb4_data_t *COMB4)
{
	if(COMB4 != NULL)
	{
		free(COMB4->CombBuff10);
		free(COMB4->CombBuff20);
		free(COMB4->CombBuff30);
		free(COMB4->CombBuff40);

		free(COMB4->CombBuff11);
		free(COMB4->CombBuff21);
		free(COMB4->CombBuff31);
		free(COMB4->CombBuff41);

		free(COMB4);
	}
}

/*
 * clean fx_filt_data_t
 * args:
 *   FILT - pointer to fx_filt_data_t
 *
 * asserts:
 *   none
 *
 * returns: none
 */
static void close_FILT(fx_filt_data_t *FILT)
{
	if(FILT != NULL)
	{
		free(FILT);
	}
}

/*
 * clean fx_wah_data_t
 * args:
 *   WAH - pointer to fx_wah_data_t
 *
 * asserts:
 *   none
 *
 * returns: none
 */
static void close_WAHWAH(fx_wah_data_t *WAH)
{
	if(WAH != NULL)
	{
		free(WAH);
	}
}

/*
 * clean reverb data
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
static void close_reverb()
{
	close_DELAY(aud_fx->AP1);
	aud_fx->AP1 = NULL;
	close_COMB4(aud_fx->COMB4);
	aud_fx->COMB4 = NULL;
}

/*
 * clean pitch data
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
static void close_pitch ()
{
	if(aud_fx->RT1 != NULL)
	{
		free(aud_fx->RT1->rBuff1);
		free(aud_fx->RT1->rBuff2);
		free(aud_fx->RT1->wBuff1);
		free(aud_fx->RT1->wBuff2);
		free(aud_fx->RT1);
		aud_fx->RT1 = NULL;
		close_FILT(aud_fx->LPF1);
		aud_fx->LPF1 = NULL;
	}
}

/*
 * clean audio fx data
 * args:
 *   none
 *
 * asserts:
 *   aud_fx is not null
 *
 * returns: none
 */
void audio_fx_close()
{
	/*assertions*/
	assert(aud_fx != NULL);

	close_DELAY(aud_fx->ECHO);
	aud_fx->ECHO = NULL;
	close_reverb();
	close_WAHWAH(aud_fx->wahData);
	aud_fx->wahData = NULL;
	close_FILT(aud_fx->HPF);
	aud_fx->HPF = NULL;
	close_pitch();

	free(aud_fx);
	aud_fx = NULL;
}