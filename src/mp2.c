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

#include <stdlib.h>
#include <glib.h>
#include <twolame.h>
#include "mp2.h"
#include "globals.h"
#include "defs.h"

/*mp2 encoder options*/
twolame_options *encodeOptions;

/*mp2 buffer*/


/*compress pcm data to MP2 (twolame)*/
int
init_MP2_encoder(struct paRecordData* pdata, int bitRate)
{
	/* grab a set of default encode options */
	encodeOptions = twolame_init();
	
	// mono/stereo and sampling-frequency
	twolame_set_num_channels(encodeOptions, pdata->channels);
	if (pdata->channels == 1) 
	{
		twolame_set_mode(encodeOptions, TWOLAME_MONO);
	}
	else
	{
		twolame_set_mode(encodeOptions, TWOLAME_JOINT_STEREO);
	}

	pdata->mp2BuffSize = pdata->snd_numBytes*2;
	pdata->mp2Buff = g_new0(BYTE, pdata->mp2BuffSize); /*mp2 buffer*/
	/* Set the input and output sample rate to the same */
	twolame_set_in_samplerate(encodeOptions, pdata->samprate);
	twolame_set_out_samplerate(encodeOptions, pdata->samprate);

	/* Set the bitrate (160 Kbps by default) */
	twolame_set_bitrate(encodeOptions, bitRate);
	
	/* initialise twolame with this set of options */
	if (twolame_init_params( encodeOptions ) != 0) 
	{
		g_printerr("Error: configuring libtwolame encoder failed.\n");
		return(-1);
	}
	
	return (0);
}

int
MP2_encode(struct paRecordData* pdata, int ms_delay) 
{
	int mp2fill_size=0;
	if (ms_delay) 
	{
		/*add delay (silence)*/
		UINT32 shiftFrames=abs(ms_delay*pdata->samprate/1000);
		UINT32 shiftSamples=shiftFrames*pdata->channels;
	
		SAMPLE *EmptySamp;
		EmptySamp = g_new0(SAMPLE, shiftSamples);
		// Encode silent samples

		mp2fill_size = twolame_encode_buffer_float32_interleaved(encodeOptions, 
			(float *) EmptySamp, shiftSamples/(pdata->channels), 
			pdata->mp2Buff, pdata->mp2BuffSize);
		g_free(EmptySamp);
	} 
	else
	{
		/*encode buffer*/
		if (!(pdata->flush)) 
		{
			int num_samples = pdata->snd_numBytes / (pdata->channels*sizeof(SAMPLE)); /*samples per channel*/
			// Encode the audio
			mp2fill_size = twolame_encode_buffer_float32_interleaved(encodeOptions, 
				(float *) pdata->avi_sndBuff, num_samples, pdata->mp2Buff, pdata->mp2BuffSize);
		}
		else 
		{
			// flush 
			mp2fill_size = twolame_encode_flush(encodeOptions, pdata->mp2Buff, pdata->mp2BuffSize);
		}
	}
	return (mp2fill_size);
}

void close_MP2_encoder() 
{
	/*clean twolame encoder*/
	twolame_close( &encodeOptions );
}
