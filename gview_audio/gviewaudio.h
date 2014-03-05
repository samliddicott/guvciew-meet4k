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

/*Audio API*/
#define AUDIO_NONE          (0)
#define AUDIO_PORTAUDIO     (1)
#define AUDIO_PULSE         (2)

/*Audio Buffer flags*/
#define AUDIO_BUFF_FREE     (0)
#define AUDIO_BUFF_USED     (1)

/*Audio stream flag*/
#define AUDIO_STRM_ON       (1)
#define AUDIO_STRM_OFF      (0)

/*Audio Effects*/
#define AUD_FX_NOEF   (0)
#define AUD_FX_ECHO   (1<<0)
#define AUD_FX_FUZZ   (1<<1)
#define AUD_FX_REVERB (1<<2)
#define AUD_FX_WAHWAH (1<<3)
#define AUD_FX_DUCKY  (1<<4)

/*sample type int16_t or float for return buffer data*/
#define SAMPLE_TYPE_INT16  (0) //interleaved
#define SAMPLE_TYPE_FLOAT  (1) //interleaved
#define SAMPLE_TYPE_INT16P (2) //planar
#define SAMPLE_TYPE_FLOATP (3) //planar

/*internally is always float*/
typedef float sample_t;

typedef struct _audio_buff_t
{
	sample_t *data;
	int64_t timestamp;
	int flag;
} audio_buff_t;

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
	int api;                      /*audio api for this context*/
	int num_input_dev;            /*number of audio input devices in list*/
	audio_device_t *list_devices; /*audio input devices list*/
	int device;                   /*current device list index*/
	int channels;                 /*channels*/
	int samprate;                 /*sample rate*/

	/*all ts are monotonic based: both real and generated*/
	int64_t current_ts;           /*current buffer generated timestamp*/
	int64_t last_ts;              /*last real timestamp (in nanosec)*/
	int64_t snd_begintime;        /*sound capture start ref time*/
	int64_t ts_drift;             /*drift between real and generated ts*/

	sample_t *capture_buff;       /*pointer to capture data*/
	int capture_buff_size;

	void *stream;                 /*pointer to audio stream (portaudio)*/

	int stream_flag;             /*stream flag*/

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
void audio_set_verbosity(int value);

/*
 * audio initialization
 * args:
 *   api - audio API to use
 *           (AUDIO_NONE, AUDIO_PORTAUDIO, AUDIO_PULSE, ...)
 *
 * asserts:
 *   none
 *
 * returns: pointer to audio context (NULL if AUDIO_NONE)
 */
audio_context_t *audio_init(int api);

/*
 * start audio stream capture
 * args:
 *   audio_ctx - pointer to audio context data
 *
 * asserts:
 *   audio_ctx is not null
 *
 * returns: error code
 */
int audio_start(audio_context_t *audio_ctx);

/*
 * get the next used buffer from the ring buffer
 * args:
 *   audio_ctx - pointer to audio context
 *   buff - pointer to an allocated audio buffer
 *   type - type of data (SAMPLE_TYPE_[INT16|FLOAT])
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int audio_get_next_buffer(audio_context_t *audio_ctx, audio_buff_t *buff, int type);

/*
 * stop audio stream capture
 * args:
 *   audio_ctx - pointer to audio context data
 *
 * asserts:
 *   audio_ctx is not null
 *
 * returns: error code
 */
int audio_stop(audio_context_t *audio_ctx);

/*
 * close and clean audio context
 * args:
 *   audio_ctx - pointer to audio context data
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void audio_close(audio_context_t *audio_ctx);

#endif
