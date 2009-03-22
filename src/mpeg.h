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

#ifndef MPG4_H
#define MPG4_H

#include "defs.h"
#include <avcodec.h>
//#include <avformat.h>

struct mpegData
{
	AVCodec *codec;
	AVCodecContext *codec_context;
	AVFrame *picture;

	BYTE* tmpbuf;
	int outbuf_size;
	BYTE* outbuf;

};

struct mpegData* init_mpeg (int width, int height, int fps);

int encode_mpeg_frame (BYTE *picture_buf, struct mpegData* data, int isUYVY);

void clean_mpeg (struct mpegData* data);



#endif
