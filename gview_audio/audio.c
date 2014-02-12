/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
#           Flemming Frandsen <dren.dk@gmail.com>                               #
#                             Add VU meter OSD                                  #
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

/*******************************************************************************#
#                                                                               #
#  Audio library                                                                #
#                                                                               #
********************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
/* support for internationalization - i18n */
#include <locale.h>
#include <libintl.h>

#include "gviewaudio.h"
#include "audio_portaudio.h"


#define AUDBUFF_NUM  80    //number of audio buffers

int verbosity = 0;
static int audio_api = AUDIO_PORTAUDIO;

/*
 * set verbosity
 * args:
 *   value - verbosity value
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void set_audio_verbosity(int value)
{
	verbosity = value;
}

/*
 * audio initialization
 * args:
 *   api - audio API to use
 *           (AUDIO_NONE, AUDIO_PORTAUDIO, AUDIO_PULSE, ...)
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int audio_init(int api)
{

	int ret = 0;

	audio_api = api;

	switch(audio_api)
	{
		case AUDIO_NONE:
			break;

		case AUDIO_PULSE:
			break;

		case AUDIO_PORTAUDIO:
		default:
			ret = audio_init_portaudio();
			break;
	}

	return ret;
}

/*
 * close and clean audio context
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void audio_close()
{
	switch(audio_api)
	{
		case AUDIO_NONE:
			break;

		case AUDIO_PULSE:
			break;

		case AUDIO_PORTAUDIO:
		default:
			audio_close_portaudio();
			break;
	}
}

