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

#ifndef LAVC_COMMON_H
#define LAVC_COMMON_H

#include "../config.h"
#include "defs.h"
#include "sound.h"

#ifdef HAS_AVCODEC_H
  #include <avcodec.h>
#else
  #ifdef HAS_LIBAVCODEC_AVCODEC_H
    #include <libavcodec/avcodec.h>
  #else
    #ifdef HAS_FFMPEG_AVCODEC_H
      #include <ffmpeg/avcodec.h>
    #else
      #include <libavcodec/avcodec.h>
    #endif
  #endif
#endif

/*video*/
struct lavcData
{
	/*video*/
	AVCodec *codec;
	AVCodecContext *codec_context;
	AVFrame *picture;

	BYTE* tmpbuf;
	int outbuf_size;
	BYTE* outbuf;
};

/*Audio*/
struct lavcAData
{
	/*video*/
	AVCodec *codec;
	AVCodecContext *codec_context;

	int outbuf_size;
	BYTE* outbuf;
};

int encode_lavc_frame (BYTE *picture_buf, struct lavcData* data, int format);

int encode_lavc_audio_frame (short *audio_buf, struct lavcAData* data);

/* arg = pointer to lavcData struct =>
 * *arg = struct lavcData**
 */
int clean_lavc (void *arg);

int clean_lavc_audio (void* arg);

struct lavcData* init_lavc(int width, int height, int fps_num, int fps_den, int codec_ind);

struct lavcAData* init_lavc_audio(struct paRecordData *pdata, int codec_ind);

#endif
