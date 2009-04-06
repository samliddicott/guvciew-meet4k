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

#ifndef VIDEO_FORMAT_H
#define VIDEO_FORMAT_H

#include "../config.h"
#include "defs.h"
#include "avilib.h"

#ifdef HAS_AVFORMAT_H
  #include <avformat.h>
#else
  #ifdef HAS_LIBAVFORMAT_AVFORMAT_H
    #include <libavformat/avformat.h>
  #else
    #ifdef HAS_FFMPEG_AVFORMAT_H
      #include <ffmpeg/avformat.h>
    #else
      #include <libavformat/avformat.h>
    #endif
  #endif
#endif

struct VideoFormatData
{
	AVFormatContext *format_context;
	struct avi_t *AviOut;
};

#endif