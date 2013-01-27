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

#ifndef VIDEO_FORMAT_H
#define VIDEO_FORMAT_H

#include "../config.h"
#include "defs.h"
#include "avilib.h"
#include "matroska.h"

#define AVI_FORMAT   0
#define MKV_FORMAT   1
#define WEBM_FORMAT   2

#define MAX_VFORMATS 3

typedef struct _vformats_data
{
	gboolean avformat;        //is a avformat format
	const char *name;         //format name
	const char *description;  //format description
	const char *extension;    //format extension
	const char *format_str;
	const char *pattern;      //file filter pattern
	int flags;                //lavf flags
} vformats_data;


struct VideoFormatData
{
	avi_Context *avi;
	mkv_Context *mkv;

	int b_writing_frame;      //set when writing frame
	int b_header_written;     //set after mkv header written
	INT64 old_vpts;           //previous video pts
	INT64 vpts;               //video stream presentation time stamp
	INT64 old_apts;           //previous audio time stamp
	INT64 apts;               //audio stream presentation time stamp

	int vcodec;
	int64_t vdts;
	int vblock_align;
	int vduration;
	int32_t vflags;
	int keyframe;

	int acodec;
	int64_t adts;
	int ablock_align;
	int aduration;
	int32_t aflags;
};

const char *get_vformat_extension(int codec_ind);

const char *get_vformat_desc(int codec_ind);

const char *get_vformat_pattern(int codec_ind);
/**
int init_FormatContext(void *data, int format);

int clean_FormatContext (void* arg);

int write_video_packet (BYTE *picture_buf, int size, int fps, struct VideoFormatData* data);

int write_audio_packet (BYTE *audio_buf, int size, struct VideoFormatData* data);
**/
#endif
