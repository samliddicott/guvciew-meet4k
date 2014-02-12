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

#ifndef GVIEWAUDIO_H
#define GVIEWAUDIO_H

#include <inttypes.h>
#include <sys/types.h>

#define AUDIO_NONE          (0)
#define AUDIO_PORTAUDIO     (1)
#define AUDIO_PULSE         (2)

/*Audio Effects*/
#define AUD_FX_NOEF   (0)
#define AUD_FX_ECHO   (1<<0)
#define AUD_FX_FUZZ   (1<<1)
#define AUD_FX_REVERB (1<<2)
#define AUD_FX_WAHWAH (1<<3)
#define AUD_FX_DUCKY  (1<<4)

typedef float sample_t;

typedef struct _audio_device_t
{
	int id;                 /*audo device id*/
	int channels;           /*max channels*/
	int samprate;           /*default samplerate*/
	char name[512];         /*device name*/
	char description[256];  /*device description*/
} audio_device_t;

typedef struct _audio_context_t
{
	int num_input_dev;            /*number of audio input devices in list*/
	audio_device_t *list_devices; /*audio input devices list*/
	int default_dev;              /*default device list index*/
	int channels;                 /*channels*/
	int samprate;                 /*sample rate*/
	                              /*circular buffer*/
	                              /*buffer index*/
	int64_t a_last_ts;            /*last timestamp (in nanosec)*/
	int64_t snd_begintime;        /*monotonic sound capture start ref time*/

} audio_context_t;

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
void set_audio_verbosity(int value);

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
int audio_init(int api);


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
void audio_close();

#endif
