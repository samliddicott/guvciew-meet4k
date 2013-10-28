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

#ifndef IO_STREAM_H
#define IO_STREAM_H


#include "../config.h"
#include "defs.h"

enum STREAM_TYPE
{
	STREAM_TYPE_VIDEO = 0,
	STREAM_TYPE_AUDIO = 1,
	STREAM_TYPE_SUB = 2 //not supported
};

typedef enum STREAM_TYPE STREAM_TYPE;

struct io_Stream
{
	STREAM_TYPE type;          //stream type

	int32_t id;

	uint32_t packet_count;

	/** AVI specific data */
	void* indexes; //pointer to avi_Index struct
	int32_t entry;
	int64_t rate_hdr_strm, frames_hdr_strm;
	char   compressor[8];        /* Type of compressor, 4 bytes + padding for 0 byte */

	int32_t codec_id;
	int32_t h264_process;		/* Set to 1 if codec private data used (NALU marker needs to be processed)*/

	//video
	int32_t   width;             /* Width  of a video frame */
	int32_t   height;            /* Height of a video frame */
	double fps;                  /* Frames per second */

	//audio
	int32_t   a_fmt;             /* Audio format, see #defines below */
	int32_t   a_chans;           /* Audio channels, 0 for no audio */
	int32_t   a_rate;            /* Rate in Hz */
	int32_t   a_bits;            /* bits per audio sample */
	int32_t   mpgrate;           /* mpg bitrate kbs*/
	int32_t   a_vbr;             /* 0 == no Variable BitRate */
	uint64_t audio_strm_length;  /* Total number of bytes of audio data */

	//stream private data (codec private data)
	BYTE*   extra_data;
	int32_t extra_data_size;

	struct io_Stream *previous, *next;
};

typedef struct io_Stream io_Stream;

/** adds a new stream to the stream list*/
io_Stream* add_new_stream(io_Stream** stream_list, int* list_size);
/** destroys all streams in stream list*/
void destroy_stream_list(io_Stream* stream_list, int* list_size);
/** get stream with index from stream list*/
io_Stream* get_stream(io_Stream* stream_list, int index);
/** get the first video stream from stream list */
io_Stream* get_first_video_stream(io_Stream* stream_list);
/** get the first audio stream from the stream list */
io_Stream* get_first_audio_stream(io_Stream* stream_list);
/** get the last stream from stream list */
io_Stream* get_last_stream(io_Stream* stream_list);

#endif
