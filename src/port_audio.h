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

#ifndef PORT_AUDIO_H
#define PORT_AUDIO_H

#include <portaudio.h>
#include "guvcview.h"
#include "sound.h"

/*------------- portaudio defs ----------------*/
/*---- can be override in rc file or GUI ------*/

#define DEFAULT_LATENCY_DURATION 100.0
#define DEFAULT_LATENCY_CORRECTION -130.0

#define SAMPLE_RATE  (0) /* 0 device default*/
//#define FRAMES_PER_BUFFER (4096)

/* sound can go for more 1 seconds than video          */

#define NUM_CHANNELS    (0) /* 0-device default 1-mono 2-stereo */

#define PA_SAMPLE_TYPE     paFloat32
#define PA_FOURCC          WAVE_FORMAT_PCM //use PCM 16 bits converted from float

int
portaudio_list_snd_devices(struct GLOBAL *global);

int
port_init_audio(struct paRecordData* pdata);

int
port_close_audio(struct paRecordData *pdata);

#endif