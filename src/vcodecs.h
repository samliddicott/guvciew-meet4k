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

#ifndef VCODECS_H
#define VCODECS_H

#include <glib.h>


#define MAX_VCODECS 6 

#define CODEC_MJPEG 0
#define CODEC_YUV   1
#define CODEC_DIB   2
#define CODEC_MPEG  3
#define CODEC_FLV1  4
#define CODEC_WMV2  5

typedef struct _vcodecs_data
{
	gboolean avcodec;         //codec from avcodec
	const char *compressor;   //fourcc - upper case
	const char *description; //codec description
} vcodecs_data;


const char *get_vid4cc(int codec_ind);

const char *get_desc4cc(int codec_ind);

#endif
