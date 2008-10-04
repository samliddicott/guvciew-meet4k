/*******************************************************************************#
#	    guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
#										#
# This program is free software; you can redistribute it and/or modify         	#
# it under the terms of the GNU General Public License as published by   	#
# the Free Software Foundation; either version 2 of the License, or           	#
# (at your option) any later version.                                          	#
#                                                                              	#
# This program is distributed in the hope that it will be useful,              	#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             	#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 	#
#                                                                              	#
# You should have received a copy of the GNU General Public License           	#
# along with this program; if not, write to the Free Software                  	#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	#
#                                                                              	#
********************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <twolame.h>
#include "sound.h"
#include "defs.h"

/*mp2 encoder options*/
twolame_options *encodeOptions;

/*mp2 buffer*/


/*compress pcm data to MP2 (twolame)*/
int
init_MP2_encoder(struct paRecordData* pdata)
{
	//int mp2fill_size=0;
	
	/* grab a set of default encode options */
        encodeOptions = twolame_init();
	
	// mono/stereo and sampling-frequency
        twolame_set_num_channels(encodeOptions, pdata->channels);
        if (data->channels == 1) {
                twolame_set_mode(encodeOptions, TWOLAME_MONO);
        } else {
                twolame_set_mode(encodeOptions, TWOLAME_JOINT_STEREO);
        }
  

        /* Set the input and output sample rate to the same */
        twolame_set_in_samplerate(encodeOptions, pdata->samprate);
        twolame_set_out_samplerate(encodeOptions, pdata->samprate);
  
        /* Set the bitrate to 160 kbps */
        twolame_set_bitrate(encodeOptions, 160);
	
	/* initialise twolame with this set of options */
	if (twolame_init_params( encodeOptions ) != 0) {
		fprintf(stderr, "Error: configuring libtwolame encoder failed.\n");
		return(-1);
	}
	
	return (0);
}

int
MP2_encode(struct paRecordData* pdata, BYTE *mp2_buff, int buff_size) {
	int mp2fill_size=0;
	if (pdata->streaming) {
		int num_samples = pdata->snd_numBytes / (pdata->channels*sizeof(SAMPLE)); /*samples per channel*/
		// Encode the audio!
		mp2fill_size = twolame_encode_buffer_interleaved(encodeOptions, 
				pdata->avi_sndBuff, num_samples, mp2buffer, buff_size);
	} else {
		// flush 
		mp2fill_size = twolame_encode_flush(encodeOptions, mp2buffer, global->mp2buffer_size);
	}
	return (mp2fill_size);
}

int close_MP2_encoder() {
/*clean twolame encoder*/
	twolame_close( &encodeOptions );
}
