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
/*******************************************************************************#
#   Some utilities for writing and reading AVI files.                           #
#   These are not intended to serve for a full blown                            #
#   AVI handling software (this would be much too complex)                      #
#   The only intention is to write out MJPEG encoded                            #
#   AVIs with sound and to be able to read them back again.                     #
#   These utilities should work with other types of codecs too, however.        #
#                                                                               #
#   Copyright (C) 1999 Rainer Johanni <Rainer@Johanni.de>                       #
********************************************************************************/
/*  Paulo Assis (6-4-2008): removed reading functions, cleaned build wranings  */

#include <stdio.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include "config.h"
#include "avilib.h"
#include "defs.h"

/* The following variable indicates the kind of error */

long AVI_errno = 0;

#define IO_BUFFER_SIZE 32768

/*******************************************************************
 *                                                                 *
 *    Utilities for writing an AVI File                            *
 *                                                                 *
 *******************************************************************/

#ifndef O_BINARY
/* win32 wants a binary flag to open(); this sets it to null
   on platforms that don't have it. */
#define O_BINARY 0
#endif

#define INFO_LIST

#define MAX_INFO_STRLEN 64
static char id_str[MAX_INFO_STRLEN];

#ifndef PACKAGE
#define PACKAGE "guvcview"
#endif
#ifndef VERSION
#define VERSION "1.0"
#endif

#define __MUTEX &AVI->mutex

#define AVI_INDEX_CLUSTER_SIZE 16384

#define AVIF_HASINDEX           0x00000010      /* Index at end of file */
#define AVIF_MUSTUSEINDEX       0x00000020
#define AVIF_ISINTERLEAVED      0x00000100
#define AVIF_TRUSTCKTYPE        0x00000800      /* Use CKType to find key frames */
#define AVIF_WASCAPTUREFILE     0x00010000
#define AVIF_COPYRIGHTED        0x00020000

#define AVI_MAX_RIFF_SIZE       0x40000000LL
#define AVI_MASTER_INDEX_SIZE   256
#define AVI_MAX_STREAM_COUNT    100

/* index flags */
#define AVIIF_INDEX             0x10

// bIndexType codes
//
#define AVI_INDEX_OF_INDEXES 0x00 	// when each entry in aIndex
									// array points to an index chunk

#define AVI_INDEX_OF_CHUNKS 0x01 	// when each entry in aIndex
									// array points to a chunk in the file

#define AVI_INDEX_IS_DATA 0x80 		// when each entry is aIndex is
									// really the data
									// bIndexSubtype codes for INDEX_OF_CHUNKS

#define AVI_INDEX_2FIELD 0x01 		// when fields within frames
									// are also indexed

struct avi_RIFF {
    int64_t riff_start, movi_list, odml_list;
    int64_t frames_hdr_all;
    int id;

    struct avi_RIFF *previous, *next;
};

typedef struct avi_RIFF avi_RIFF;

typedef struct avi_Writer
{
	FILE *fp;      /* file pointer     */

	BYTE *buffer;  /**< Start of the buffer. */
    int buffer_size;        /**< Maximum buffer size */
    BYTE *buf_ptr; /**< Current position in the buffer */
    BYTE *buf_end; /**< End of the buffer. */

	int64_t position; //file write offset increments on buffer flush
}avi_Writer;

struct avi_Context
{
	struct avi_Writer  *writer;
	__MUTEX_TYPE mutex;

	int flags; /* 0 - AVI is recordind;   1 - AVI is not recording*/

	avi_RIFF* riff_list; // avi_riff list (NULL terminated)
	int riff_list_size;

	avi_Stream* stream_list;
	int stream_list_size;

};

typedef struct avi_Context avi_Context;

int64_t avi_tell(avi_Context* AVI)
{
	//flush the file buffer
	fflush(AVI->writer->fp);
	//return the file pointer position
	return ((int64_t) ftello64(AVI->writer->fp));
}

int avi_seek(avi_Context* AVI, int64_t position)
{
	//flush the file buffer
	fflush(AVI->writer->fp);
	//try to move the file pointer to position
	int ret = fseeko(AVI->writer->fp, position, SEEK_SET);
	if(ret != 0)
		fprintf(stderr, "AVI: seek to file position 0x%x failed\n", position);

	return ret;
}

int64_t avi_get_offset(avi_Context* AVI)
{
	//buffer offset
	int64_t offset = AVI->writer->buf_ptr - AVI->writer->buffer;
	//add to file offset
	offset += AVI->writer->position;

	return offset;
}

int64_t flush_buffer(avi_Context* AVI)
{
	if (AVI->writer->buf_ptr > AVI->writer->buffer)
	{
		size_t nitems= AVI->writer->buf_ptr - AVI->writer->buffer;
		if(fwrite(AVI->writer->buffer, 1, nitems, AVI->writer->fp) < nitems)
		{
			perror("AVI: file write error");
			return -1;
		}
	}
	else if (AVI->writer->buf_ptr != AVI->writer->buffer)
	{
		fprintf(stderr, "AVI: Bad buffer pointer - dropping buffer\n");
	}

	AVI->writer->position += (int64_t) nitems;
	AVI->writer->buf_ptr = AVI->writer->buffer;

	//just for debug - should be commented out
	if(AVI->writer->position != avi_tell(AVI))
		fprintf(stderr, "AVI: file position discrepancy\n")

	return file_pos;
}

void write_skip(avi_Context* AVI, int amount)
{
	int i = 0;
	for (i = 0; i< amount; i++)
	{
		AVI->writer->buf_ptr++;
		if (AVI->writer->buf_ptr >= AVI->writer->buf_end)
			flush_buffer(AVI);
	}
}

void write_w8(avi_Context* AVI, BYTE b)
{
	*AVI->writer->buf_ptr++ = b;
    if (AVI->writer->buf_ptr >= AVI->writer->buf_end)
        flush_buffer(AVI);
}

void write_mem(avi_Context* AVI, BYTE *buf, int size)
{
	while (size > 0)
	{
		int len = AVI->writer->buf_end - AVI->writer->buf_ptr;
		if(len >= size)
			len = size;

        memcpy(AVI->writer->buf_ptr, buf, len);
        AVI->writer->buf_ptr += len;

       if (AVI->writer->buf_ptr >= AVI->writer->buf_end)
            flush_buffer(AVI);

        buf += len;
        size -= len;
    }
}

void write_wl16(avi_Context* AVI, uint16_t val)
{
    write_w8(AVI, (BYTE) val);
    write_w8(AVI, (BYTE) (val >> 8));
}

void write_wb16(avi_Context* AVI, uint16_t val)
{
    write_w8(AVI, (BYTE) (val >> 8));
    write_w8(AVI, (BYTE) val);
}

void write_wl24(avi_Context* AVI, uint32_t val)
{
    write_wl16(AVI, (uint16_t) (val & 0xffff));
    write_w8(AVI, (BYTE) (val >> 16));
}

void write_wb24(avi_Context* AVI, uint32_t val)
{
    write_wb16(AVI, (uint16_t) (val >> 8));
    write_w8(AVI, (BYTE) val);
}

void write_wl32(avi_Context* AVI, uint32_t val)
{
    write_w8(AVI, (BYTE) val);
    write_w8(AVI, (BYTE) (val >> 8));
    write_w8(AVI, (BYTE) (val >> 16));
    write_w8(AVI, (BYTE) (val >> 24));
}

void write_wb32(avi_Context* AVI, uint32_t val)
{
    write_w8(AVI, (BYTE) (val >> 24));
    write_w8(AVI, (BYTE) (val >> 16));
    write_w8(AVI, (BYTE) (val >> 8));
    write_w8(AVI, (BYTE) val);
}

void write_wl64(avi_Context* AVI, uint64_t val)
{
    write_wl32(AVI, (uint32_t)(val & 0xffffffff));
    write_wl32(AVI, (uint32_t)(val >> 32));
}

void write_wb64(avi_Context* AVI, uint64_t val)
{
    write_wb32(AVI, (uint32_t)(val >> 32));
    write_wb32(AVI, (uint32_t)(val & 0xffffffff));
}

#if BIG_ENDIAN
	#define write_w16 write_wb16
	#define write_w24 write_wb24
	#define write_w32 write_wb32
	#define write_w64 write_wb64
#else
	#define write_w16 write_wl16
	#define write_w24 write_wl24
	#define wrtie_w32 write_wl32
	#define write_w64 write_wl64
#endif

void write_4cc(avi_Context* AVI, const char *str)
{
    int len = 4;
    if(strlen(str) < len + 1) //null terminated string
	{
		len = strlen(str) -1;
	}

    write_mem(AVI, (BYTE *) str, len);

    len = 4 - len;
    //fill remaining chars with spaces
    while(len > 0)
    {
		write_w8(AVI, ' ');
		len--;
	}
}

int write_str(avi_Context* AVI, const char *str)
{
    int len = 1;
    if (str) {
        len += strlen(str);
        write_mem(AVI, (BYTE *) str, len);
    } else
        write_w8(AVI, 0);
    return len;
}

void avi_write_wl32_to(avi_Context* AVI, uint32_t val, int64_t position)
{
	int64_t current_offset = avi_get_offset(AVI);

	if(postion < AVI->writer->position) //write size to file
	{
		int res = 0;

		//move to postion
		res = avi_seek(AVI, position);
		if(res != 0)
			return;

		BYTE out[4];
		out[0] = (BYTE) val;
		out[1] = (BYTE) (val >> 8);
		out[2] = (BYTE) (val >> 16);
		out[3] = (BYTE) (val >> 24);
		//write the value to file
		if(fwrite(out, 1, 4, AVI->writer->fp) < 4)
			perror("AVI: size write error");

		//move back to writer position
		res = avi_seek(AVI, AVI->writer->position);
		if(res != 0)
			fprintf(stderr, "AVI: seek file to writer position 0x%x failed\n", AVI->writer->position);
	}
	else // can write to buffer
	{
		//store the current buffer pointer position
		BYTE* current_ptr = AVI->writer->buf_ptr;
		//move buffer pointer to position
		AVI->writer->buf_ptr = AVI->writer->buffer + (position - AVI->writer->position);
		//write the value
		write_wl32(AVI, val);
		//move the buffer pointer to initial position
		AVI->writer->buf_ptr = current_ptr;
	}
}

void avi_write_4cc_to(avi_Context* AVI, const char *val, int64_t position)
{
	int64_t current_offset = avi_get_offset(AVI);

	if(postion < AVI->writer->position) //write size to file
	{
		int res = 0;

		//move to postion
		res = avi_seek(AVI, position);
		if(res != 0)
			return;

		//write the value to file
		if(fwrite(val, 1, 4, AVI->writer->fp) < 4)
			perror("AVI: 4cc write error");

		//move back to writer position
		res = avi_seek(AVI, AVI->writer->position);
		if(res != 0)
			fprintf(stderr, "AVI: seek file to writer position 0x%x failed\n", AVI->writer->position);
	}
	else // can write to buffer
	{
		//store the current buffer pointer position
		BYTE* current_ptr = AVI->writer->buf_ptr;
		//move buffer pointer to position
		AVI->writer->buf_ptr = AVI->writer->buffer + (position - AVI->writer->position);
		//write the value
		write_4cc(AVI, val);
		//move the buffer pointer to initial position
		AVI->writer->buf_ptr = current_ptr;
	}
}

int64_t avi_open_tag (avi_Context* AVI, const char *tag)
{
	write_4cc(AVI, tag);
	write_wl32(AVI, 0);
	return avi_get_offset(AVI);
}

void avi_close_tag(avi_Context* AVI, int64_t start_pos)
{
	int64_t current_offset = avi_get_offset(AVI);
	uint32_t size = uint32_t (current_offset - start_pos);

	avi_write_wl32_to(AVI, size, start_pos - 4);
}

/* create a new writer for filename*/
avi_Writer* avi_create_writer(const char *filename)
{
	avi_Writer* writer = g_new0(avi_Writer, 1);

	if(writer == NULL)
		return NULL;

	writer->buffer_size = IO_BUFFER_SIZE;
	writer->buffer = g_new0(BYTE, writer->buffer_size);
	writer->buf_ptr = writer->buffer;
	writer->buf_end = writer->buf_ptr + writer->buffer_size;

	writer->fp = fopen(filename, "wb");

	if (writer->fp == NULL)
	{
		perror("Could not open file for writing");
		g_free(writer);
		return NULL;
	}

	return writer;
}

void avi_destroy_writer(avi_Context* AVI)
{
	//clean up
	//flush the buffer to file
	flush_buffer(AVI);

	//flush the file buffer
	fflush(AVI->writer->fp);
	//close the file pointer
	fclose(AVI->writer->fp);

	free(writer->buffer);
}


/* Calculate audio sample size from number of bits and number of channels.
   This may have to be adjusted for eg. 12 bits and stereo */

static int avi_audio_sample_size(avi_Stream* stream)
{
	if(stream->type != STREAM_TYPE_AUDIO)
		return -1;

	int s;
	if (stream->a_fmt != WAVE_FORMAT_PCM)
	{
		s = 4;
	}
	else
	{
		s = ((stream->a_bits+7)/8)*stream->a_chans;
		if(s<4) s=4; /* avoid possible zero divisions */
	}
	return s;
}

static char* avi_stream2fourcc(char* tag, avi_Stream* stream)
{
    tag[0] = '0' + (stream->id)/10;
    tag[1] = '0' + (stream->id)%10;
    switch(stream->type)
    {
		case STREAM_TYPE_VIDEO:
			tag[2] = 'd';
			tag[3] = 'c';
			break;
		case STREAM_TYPE_SUB:
			// note: this is not an official code
			tag[2] = 's';
			tag[3] = 'b';
			break;
		default: //audio
			tag[2] = 'w';
			tag[3] = 'b';
			break;
	}
    tag[4] = '\0';
    return tag;
}

avi_Stream* avi_get_last_stream(avi_Context* AVI)
{
	avi_Stream* stream = AVI->stream_list;

	if(!stream)
		return NULL;

	while(stream->next != NULL)
		stream = stream->next;

	return stream;
}

avi_Stream* avi_get_stream(avi_Context* AVI, int index)
{
	avi_Stream* stream = AVI->stream_list;

	if(!stream)
		return NULL;

	int j = 0;

	while(stream->next != NULL && (j < index))
	{
		stream = stream->next;
		j++;
	}

	if(j != index)
		return NULL;

	return stream;
}

avi_Stream* avi_add_new_stream(avi_Context* AVI)
{
	avi_Stream* stream = g_new0(avi_Stream, 1);
	stream->next = NULL;
	stream->id = AVI->stream_list_size;

	if(stream->id == 0)
	{
		stream->previous = NULL;
		AVI->stream_list = stream;
	}
	else
	{

		avi_Stream* last_stream = avi_get_last_stream(AVI);
		stream->previous = last_stream;
		last_stream->next = stream;
	}

	AVI->stream_list_size++;

	return(stream);
}

avi_Stream* get_first_video_stream(avi_Context* AVI)
{
	avi_Stream* stream = AVI->stream_list;

	if(!stream)
		return NULL;

	if(stream->type == STREAM_TYPE_VIDEO)
		return stream;

	while(stream->next != NULL)
	{
		stream = stream->next;

		if(stream->type == STREAM_TYPE_VIDEO)
			return stream;
	}

	return NULL;
}

int avi_get_total_extra_size(avi_Context* AVI)
{
	avi_Stream* stream = AVI->stream_list;

	if(!stream)
		return -1;

	int size = 0;
	if(stream->extra_data_size && stream->extra_data)
	{
		size += (stream->extra_data_size+1) & ~1;

		if(stream->type == STREAM_TYPE_VIDEO)
			size += 4*2; // 2 dWord (tag + size)
	}

	while(stream->next != NULL)
	{
		stream = stream->next;
		if(stream->extra_data_size && stream->extra_data)
		{
			size += (stream->extra_data_size+1) & ~1;
			if(stream->type == STREAM_TYPE_VIDEO)
				size += 4*2; // 2 dWord (tag + size)
		}
	}

	return size;

}

void avi_put_main_header(avi_Context* AVI)
{
	double fps = get_first_video_stream(AVI)->fps;
	int width = get_first_video_stream(AVI)->width;
	int height = get_first_video_stream(AVI)->height;

	int ms_per_frame = 0;
	if(fps > 0.001)
		ms_per_frame=(int) (1000000/fps + 0.5);

	/*do not force index yet -only when closing*/
	/*this should prevent bad avi files even if it is not closed properly*/
	//if(hasIndex) flag |= AVIF_HASINDEX;
	//if(hasIndex && AVI->must_use_index) flag |= AVIF_MUSTUSEINDEX;
	uint32_t flags = AVIF_WASCAPTUREFILE;

	int64_t avih = avi_open_tag(AVI, "avih"); // main avi header
	write_wl32(AVI, ms_per_frame);		// time delay between frames (milisec)
	write_wl32(AVI, 0);                 // data rate
	write_wl32(AVI, 0);                 // Padding multiple size (2048)
	write_wl32(AVI, flags);			    // parameter Flags
	write_wl32(AVI, 0);			        // number of video frames
	write_wl32(AVI, 0);			        // number of preview frames
	write_wl32(AVI, AVI->stream_list_size); // number of data streams (audio + video)*/
	write_wl32(AVI, 0);			        // suggested playback buffer size (bytes)
	write_wl32(AVI, width);		        // width
	write_wl32(AVI, height);	    	// height
	write_wl32(AVI, 0);			        // time scale:  unit used to measure time (30)
	write_wl32(AVI, 0);			        // data rate (frame rate * time scale)
	write_wl32(AVI, 0);			        // start time (0)
	write_wl32(AVI, 0);			        // size of AVI data chunk (in scale units)
	avi_close_tag(AVI, avih); //write the chunk size
}

void avi_put_bmp_header(avi_Context* AVI, avi_Stream* stream)
{
	int frate = 0;
	if(stream->fps > 0.001)
		frate = (int) (FRAME_RATE_SCALE*stream->fps + 0.5);

	int64_t strh = avi_open_tag(AVI, "strh");// video stream header
	write_4cc(AVI, "vids");              // stream type
	write_4cc(AVI, stream->compressor);  // Handler (VIDEO CODEC)
	write_wl32(AVI, 0);                  // Flags
	write_wl32(AVI, 0);                  // stream priority
	write_wl32(AVI, 0);                  // language tag
	write_wl32(AVI, 0);                  // initial frames
	write_wl32(AVI, FRAME_RATE_SCALE);   // Scale
	write_wl32(AVI, frate);              // Rate: Rate/Scale == sample/second (fps) */
	write_wl32(AVI, 0);                  // start time
	write_wl32(AVI, 0);                  // lenght of stream
	write_wl32(AVI, 0);                  // suggested playback buffer size
	write_wl32(AVI, -1);                 // Quality
	write_wl32(AVI, 0);                  // SampleSize
	write_wl16(AVI, 0);                  // rFrame (left)
	write_wl16(AVI, 0);                  // rFrame (top)
	write_wl16(AVI, 0);                  // rFrame (right)
	write_wl16(AVI, 0);                  // rFrame (bottom)
	avi_close_tag(AVI, strh); //write the chunk size
}

void avi_put_wav_header(avi_Context* AVI, avi_Stream* stream)
{
	int sampsize = avi_audio_sample_size(stream);

	int64_t strh = avi_open_tag(AVI, "strh");// audio stream header
	write_4cc(AVI, "auds");
	write_4cc(AVI, stream->compressor);  // Handler (AUDIO CODEC)
	write_wl32(AVI, 0);                  // Flags
	write_wl32(AVI, 0);                  // stream priority
	write_wl32(AVI, 0);                  // language tag
	write_wl32(AVI, 0);                  // initial frames
	write_wl32(AVI, sampsize/4);         // Scale
	write_wl32(AVI, stream->mpgrate/8);   // Rate: Rate/Scale == sample/second (fps) */
	write_wl32(AVI, 0);                  // start time
	write_wl32(AVI, 4*stream->audio_bytes/sampsize); // lenght of stream
	write_wl32(AVI, 0);                  // suggested playback buffer size
	write_wl32(AVI, -1);                 // Quality
	write_wl32(AVI, sampsize/4);         // SampleSize
	write_wl16(AVI, 0);                  // rFrame (left)
	write_wl16(AVI, 0);                  // rFrame (top)
	write_wl16(AVI, 0);                  // rFrame (right)
	write_wl16(AVI, 0);                  // rFrame (bottom)
	avi_close_tag(AVI, strh); //write the chunk size
}

void avi_put_vstream_format_header(avi_Context* AVI, avi_Stream* stream)
{
	int64_t strf = avi_open_tag(AVI, "strf");// stream format header
	write_wl32(AVI, 40);             // sruct Size
	write_wl32(AVI, stream->width);     // Width
	write_wl32(AVI, stream->height);    // Height
	write_wl16(AVI, 1);              // Planes
	write_wl16(AVI, 24);             // Count - bitsperpixel - 1,4,8 or 24  32
	if(strncmp(stream->compressor,"DIB",3)==0)
		write_wl32(AVI, 0);          // Compression
	else
		write_4cc(AVI, stream->compressor);
	write_wl32(AVI, stream->width*stream->height);// image size (in bytes?)
	write_wl32(AVI, 0);              // XPelsPerMeter
	write_wl32(AVI, 0);              // YPelsPerMeter
	write_wl32(AVI, 0);              // ClrUsed: Number of colors used
	write_wl32(AVI, 0);              // ClrImportant: Number of colors important
	avi_close_tag(AVI, strf); //write the chunk size
}

void avi_put_astream_format_header(avi_Context* AVI, avi_Stream* stream)
{
	int axd_size        = stream->extra_data_size;
	int axd_size_align  = (stream->extra_data_size+1) & ~1;

	int strf_size = 4 * 4 + 2;
	if (axd_size > 0 && stream->extra_data)
		strf_size += axd_size_align;

	int sampsize = avi_audio_sample_size(stream);

	int64_t strf = avi_open_tag(AVI, "strf");// audio stream format
	write_wl32(AVI, strf_size);        // header size
	write_wl16(AVI, stream->a_fmt);    // Format tag
	write_wl16(AVI, stream->a_chans);  // Number of channels
	write_wl32(AVI, stream->a_rate);   // SamplesPerSec
	write_wl32(AVI, stream->mpgrate/8);// Average Bytes per sec
	write_wl16(AVI, sampsize/4);      // BlockAlign
	write_wl16(AVI, stream->a_bits);   //BitsPerSample
	write_wl16(AVI, axd_size);        //size of extra data
	// write extradata (codec private)
	if (axd_size > 0 && stream->extra_data)
	{
		write_mem(AVI, stream->extra_data, axd_size);
		if (axd_size != axd_size_align)
		{
			write_w8(AVI, 0);  //align
		}
	}
	avi_close_tag(AVI, strf); //write the chunk size
}

void avi_put_vproperties_header(avi_Context* AVI, avi_Stream* stream)
{
	int vprp= avi_open_tag(AVI, "vprp");

    write_wl32(AVI, 0); //video format  = unknown
    write_wl32(AVI, 0); //video standard= unknown
    write_wl32(AVI, 2 * lrintf(stream->fps)); // dwVerticalRefreshRate
    write_wl32(AVI, stream->width ); //horizontal pixels
    write_wl32(AVI, stream->height); //vertical lines
    write_wl16(AVI, stream->width);  //Active Frame Aspect Ratio (4:3 - 16:9)
    write_wl16(AVI, stream->height); //Active Frame Aspect Ratio
    write_wl32(AVI, stream->width ); //Active Frame Height in Pixels
    write_wl32(AVI, stream->height);//Active Frame Height in Lines
    write_wl32(AVI, 1); //progressive FIXME
	//Field Framing Information
    write_wl32(AVI, stream->height);
    write_wl32(AVI, stream->width );
    write_wl32(AVI, stream->height);
    write_wl32(AVI, stream->width );
    write_wl32(AVI, 0);
    write_wl32(AVI, 0);
    write_wl32(AVI, 0);
    write_wl32(AVI, 0);

    avi_close_tag(AVI, vprp);
}

int64_t avi_create_riff_tags(avi_Context* AVI, avi_RIFF* riff)
{
	int64_t off = 0;
	riff->riff_start = avi_open_tag(AVI, "RIFF");

	if(riff->id == 0)
	{
		write_4cc(AVI, "AVI ");
		off = avi_open_tag(AVI, "LIST");
		write_4cc(AVI, "hdrl");
	}
	else
	{
		write_4cc(AVI, "AVIX");
		off = avi_open_tag(AVI, "LIST");
		write_4cc(AVI, "movi");

		riff->movi_list = off;
	}

	return off;
}

void avi_create_riff_header(avi_Context* AVI, avi_RIFF* riff)
{
	int64_t list1 = avi_create_riff_tags(AVI, riff);

	avi_put_main_header(AVI);

	int i, j = 0;

	for(j=0; j< AVI->stream_list_size; j++)
	{
		avi_Stream* stream = avi_get_stream(AVI, j);

		int64_t list2 = avi_open_tag(AVI, "LIST");
		write_4cc(AVI,"strl");              //stream list

		if(stream->type == STREAM_TYPE_VIDEO)
		{
			if(stream->extra_data_size > 0 && stream->extra_data)
						list_size += 2 + ((stream->extra_data_size+1) & ~1);

			avi_put_bmp_header(AVI, stream);
			avi_put_vstream_format_header(AVI, stream);

			// write extradata (codec private)
			if (stream->extra_data_size > 0 && stream->extra_data)
			{
				int size_align = (stream->extra_data_size+1) & ~1;
				write_4cc(AVI,"strd");
				write_wl32(AVI, size_align);
				write_mem(AVI, stream->extra_data, stream->extra_data_size);
				if (stream->extra_data_size != size_align)
				{
					write_w8(AVI, 0);  //align
				}
			}
		}
		else
		{
			avi_put_wav_header(AVI, stream);
			avi_put_astream_format_header(AVI, stream);
		}
		/* Starting to lay out AVI OpenDML master index.
		 * We want to make it JUNK entry for now, since we'd
		 * like to get away without making AVI an OpenDML one
		 * for compatibility reasons.
		 */
		char tag[5];
		stream->indexes.entry = stream->indexes.ents_allocated = 0;
		stream->indexes.indx_start = avi_get_offset(AVI);
		int64_t ix = avi_open_tag(AVI, "JUNK");           // ’ix##’
		write_wl16(AVI, 4);               // wLongsPerEntry must be 4 (size of each entry in aIndex array)
		write_w8(AVI, 0);                 // bIndexSubType must be 0 (frame index) or AVI_INDEX_2FIELD
		write_w8(AVI, 0);                 // bIndexType (0 == AVI_INDEX_OF_INDEXES)
		write_wl32(AVI, 0);               // nEntriesInUse (will fill out later on)
		write_4cc(AVI, avi_stream2fourcc(tag, stream)); // dwChunkId
		write_wl32(AVI, 0);               // dwReserved[3] must be 0
		write_wl32(AVI, 0);
		write_wl32(AVI, 0);
		for (i=0; i < AVI_MASTER_INDEX_SIZE; i++)
		{
			write_wl64(AVI, 0);           // absolute file offset, offset 0 is unused entry
			write_wl32(AVI, 0);           // dwSize - size of index chunk at this offset
			write_wl32(AVI, 0);           // dwDuration - time span in stream ticks
		}
		avi_close_tag(AVI, ix); //write the chunk size

		if(stream->type == STREAM_TYPE_VIDEO)
			avi_put_vproperties_header(AVI, stream);

		avi_close_tag(AVI, list2); //write the chunk size
	}

	riff->odml_list = avi_open_tag(AVI, "JUNK");
    write_4cc(AVI, "odml");
    write_4cc(AVI, "dmlh");
    write_wl32(AVI, 248);
    for (i = 0; i < 248; i+= 4)
        write_wl32(AVI, 0);
    avi_close_tag(AVI, avi->odml_list);

	avi_close_tag(AVI, list1); //write the chunk size

	/* some padding for easier tag editing */
    list2 = avi_open_tag(AVI, "JUNK");
    for (i = 0; i < 1016; i += 4)
        write_wl32(AVI, 0);
    avi_close_tag(AVI, list2); //write the chunk size

    riff->movi_list = avi_open_tag(AVI, "LIST");
    write_4cc(AVI, "movi");
}

avi_RIFF* avi_get_last_riff(avi_Context* AVI)
{
	avi_RIFF* last_riff = AVI->riff_list;
	while(last_riff->next != NULL)
		last_riff = last_riff->next;

	return last_riff;
}

avi_RIFF* avi_get_riff(avi_Context* AVI, int index)
{
	avi_RIFF* riff = AVI->riff_list;

	if(!riff)
		return NULL;

	int j = 0;

	while(riff->next != NULL && (j < index))
	{
		riff = riff->next;
		j++;
	}

	if(j != index)
		return NULL;

	return riff;
}

//call this after adding all the streams
avi_RIFF* avi_add_new_riff(avi_Context* AVI)
{
	avi_RIFF* riff = g_new0(avi_RIFF, 1);

	if(riff == NULL)
		return NULL;

	riff->next = NULL;
	riff->id = AVI->riff_list_size;

	if(riff->id == 0)
	{
		riff->previous = NULL;
		AVI->riff_list = riff;
		avi_create_riff_header(AVI, riff);
	}
	else
	{
		avi_RIFF* last_riff = avi_get_last_riff(AVI);
		riff->previous = last_riff;
		last_riff->next = riff;
		avi_create_riff_tags(AVI, riff);
	}

	AVI->riff_list_size++;

	return riff;
}

//second function to get called (add video stream to avi_Context)
avi_Stream*
avi_add_video_stream(avi_Context *AVI,
					int width,
					int height,
					double fps,
					char* compressor)
{
	avi_Stream* stream = avi_add_new_stream(AVI);
	stream->type = STREAM_TYPE_VIDEO;
	stream->fps = fps;
	stream->width = width;
	stream->height = height;
	if(compressor)
		strncpy(stream->compressor, compressor, 8);

	return stream;
}

//third function to get called (add audio stream to avi_Context)
avi_Stream*
avi_add_audio_stream(avi_Context *AVI,
					long   format,
					long   channels,
					long   rate,
					long   bits,
					long   mpgrate,
					long   vbr,
					off_t  audio_bytes,
					char* compressor)
{
	avi_Stream* stream = avi_add_new_stream(AVI);
	stream->type = STREAM_TYPE_AUDIO;

	stream->a_fmt = format;
	stream->a_rate = rate;
	stream->a_bits = bits;
	stream->mpgrate = mpgrate;
	stream->a_vbr = vbr;
	stream->audio_bytes = audio_bytes;

	if(compressor)
		strncpy(stream->compressor, compressor, 8);


	return stream;
}

/*
   first function to get called

   avi_create_context: Open an AVI File and write a bunch
                         of zero bytes as space for the header.
                         Creates a mutex.

   returns a pointer to avi_Context on success, a NULL pointer on error
*/
avi_Context* avi_create_context(const char * filename)
{
	avi_Context* AVI = g_new0(avi_Context, 1);

	if(AVI == NULL)
		return NULL;

	AVI->writer = avi_create_witer(filename);

	if (AVI->writer == NULL)
	{
		perror("Could not open file for writing");
		g_free(AVI);
		return NULL;
	}

	__INIT_MUTEX(__MUTEX);
	AVI->flags = 0; /*recordind*/

	AVI->riff_list = NULL;
	AVI->riff_list_size = 0;

	AVI->stream_list = NULL;
	AVI->stream_list_size = 0;

	return AVI;
}

void avi_destroy_context(avi_Context* AVI)
{
	//clean up
	avi_destroy_writer(AVI);

	avi_RIFF* riff = avi_get_last_riff(AVI);
	while(riff->previous != NULL) //from end to start
	{
		avi_RIFF* prev_riff = riff->previous;
		free(riff);
		riff = prev_riff;
		AVI->riff_list_size--;
	}
	free(riff); //free the last one;

	avi_Stream* stream = avi_get_last_stream(AVI);
	while(stream->previous != NULL) //from end to start
	{
		avi_Stream* prev_stream = stream->previous;
		free(stream);
		stream = prev_stream;
		AVI->stream_list_size--;
	}
	free(stream); //free the last one;

	//free avi_Context
	free(AVI);
}

avi_Ientry* avi_get_ientry(avi_Index* idx, int ent_id)
{
	int cl = ent_id / AVI_INDEX_CLUSTER_SIZE;
    int id = ent_id % AVI_INDEX_CLUSTER_SIZE;
    return &idx->cluster[cl][id];
}

/*

static int avi_write_counters(avi_Context* AVI, int riff_id)
{
    AVIOContext *pb = s->pb;
    AVIContext *avi = s->priv_data;
    int n, au_byterate, au_ssize, au_scale, nb_frames = 0;
    int64_t file_size;
    AVCodecContext* stream;

    file_size = avio_tell(pb);
    for(n = 0; n < s->nb_streams; n++) {
        AVIStream *avist= s->streams[n]->priv_data;

        assert(avist->frames_hdr_strm);
        stream = s->streams[n]->codec;
        avio_seek(pb, avist->frames_hdr_strm, SEEK_SET);
        ff_parse_specific_params(stream, &au_byterate, &au_ssize, &au_scale);
        if(au_ssize == 0) {
            avio_wl32(pb, avist->packet_count);
        } else {
            avio_wl32(pb, avist->audio_strm_length / au_ssize);
        }
        if(stream->codec_type == AVMEDIA_TYPE_VIDEO)
            nb_frames = FFMAX(nb_frames, avist->packet_count);
    }
    if(riff_id == 1) {
        assert(avi->frames_hdr_all);
        avio_seek(pb, avi->frames_hdr_all, SEEK_SET);
        avio_wl32(pb, nb_frames);
    }
    avio_seek(pb, file_size, SEEK_SET);

    return 0;
}

*/
static int avi_write_ix(avi_Context* AVI)
{
    char tag[5];
    char ix_tag[] = "ix00";
    int i, j;

	avi_RIFF *riff = avi_get_last_riff(AVI);

    if (riff->id > AVI_MASTER_INDEX_SIZE)
        return -1;

    for (i=0;i<AVI->stream_list_size;i++)
    {
        avi_Stream *avist= avi_get_stream(AVI, i);
        int64_t ix, pos;

        avi_stream2fourcc(tag, avist);

        ix_tag[3] = '0' + i;

        /* Writing AVI OpenDML leaf index chunk */
        ix = avi_tell(AVI);
        write_4cc(AVI, ix_tag);     /* ix?? */
        write_wl32(AVI, avist->indexes.entry * 8 + 24);
                                      /* chunk size */
        write_wl16(AVI, 2);           /* wLongsPerEntry */
        write_w8(AVI, 0);             /* bIndexSubType (0 == frame index) */
        write_w8(AVI, 1);             /* bIndexType (1 == AVI_INDEX_OF_CHUNKS) */
        write_wl32(AVI, avist->indexes.entry);
                                      /* nEntriesInUse */
        write_4cc(AVI, tag);        /* dwChunkId */
        write_wl64(AVI, riff->movi_list);/* qwBaseOffset */
        write_wl32(AVI, 0);             /* dwReserved_3 (must be 0) */

        for (j=0; j<avist->indexes.entry; j++)
        {
             avi_Ientry* ie = avi_get_ientry(&avist->indexes, j);
             write_wl32(AVI, ie->pos + 8);
             write_wl32(AVI, ((uint32_t)ie->len & ~0x80000000) |
                          (ie->flags & 0x10 ? 0 : 0x80000000));
         }
         flush_buffer(AVI);
         pos = avi_tell(AVI); //current position

         /* Updating one entry in the AVI OpenDML master index */
         avi_seek(AVI, avist->indexes.indx_start - 8);
         write_4cc(AVI, "indx");            /* enabling this entry */
         write_skip(AVI, 8);
         write_wl32(AVI, riff->id);         /* nEntriesInUse */
         write_skip(AVI, 16*riff->id);
         write_wl64(AVI, ix);                   /* qwOffset */
         write_wl32(AVI, pos - ix);             /* dwSize */
         write_wl32(AVI, avist->indexes.entry); /* dwDuration */

		//return to position
         avi_seek(AVI, pos);
    }
    return 0;
}

static int avi_write_idx1(avi_Context* AVI)
{

    int64_t idx_chunk;
    int i;
    char tag[5];


    avi_Stream *avist;
    avi_Ientry* ie = 0, *tie;
    int empty, stream_id = -1;

    idx_chunk = avi_open_tag(AVI, "idx1");
    for (i=0;i<AVI->stream_list_size;i++)
    {
            avist = avi_get_stream(AVI, i);
            avist->entry=0;
    }

    do
    {
        empty = 1;
        for (i=0;i<AVI->stream_list_size;i++)
        {
			avist= avi_get_stream(AVI, i);
            if (avist->indexes.entry <= avist->entry)
                continue;

            tie = avi_get_ientry(&avist->indexes, avist->entry);
            if (empty || tie->pos < ie->pos)
            {
                ie = tie;
                stream_id = i; //avist->id
            }
            empty = 0;
        }

        if (!empty)
        {
            avist = avi_get_stream(AVI, stream_id);
            avi_stream2fourcc(tag, avist);
            write_4cc(AVI, tag);
            write_wl32(AVI, ie->flags);
            write_wl32(AVI, ie->pos);
            write_wl32(AVI, ie->len);
            avist->entry++;
        }
    }
    while (!empty);

    avi_close_tag(AVI, idx_chunk);

	avi_RIFF *riff = avi_get_last_riff(AVI);
    avi_write_counters(AVI, riff->id);

    return 0;
}

int avi_write_packet(avi_Context* AVI, AVPacket *pkt, int block_align)
{
    unsigned char tag[5];
    unsigned int flags=0;
	int stream_index =  pkt->stream_index;

    avi_Stream *avist= avi_get_stream(AVI, stream_index);

    int size= pkt->size;

	avi_RIFF* riff = avi_get_last_riff(AVI);
	//align
	/*
    while(block_align==0 && pkt->dts != AV_NOPTS_VALUE && pkt->dts > avist->packet_count)
    {
        AVPacket empty_packet;

        av_init_packet(&empty_packet);
        empty_packet.size= 0;
        empty_packet.data= NULL;
        empty_packet.stream_index= stream_index;
        avi_write_packet(AVI, &empty_packet);
    }
    */

    avist->packet_count++;

    // Make sure to put an OpenDML chunk when the file size exceeds the limits
    if (avi_tell(AVI) - riff->riff_start > AVI_MAX_RIFF_SIZE)
    {
        avi_write_ix(AVI);
        avi_close_tag(AVI, riff->movi_list);

        if (riff->id == 1)
            avi_write_idx1(AVI);

        avi_close_tag(AVI, riff->riff_start);

        avi_add_new_riff(AVI);
    }

    avi_stream2fourcc(tag, avist);

    if(pkt->flags & AV_PKT_FLAG_KEY)
        flags = 0x10;

    if (avist->type == STREAM_TYPE_AUDIO)
       avist->audio_strm_length += size;


    avi_Index* idx = &avist->indexes;
    int cl = idx->entry / AVI_INDEX_CLUSTER_SIZE;
    int id = idx->entry % AVI_INDEX_CLUSTER_SIZE;
    if (idx->ents_allocated <= idx->entry)
    {
        idx->cluster = av_realloc(idx->cluster, (cl+1)*sizeof(void*));
        if (!idx->cluster)
            return -1;
        idx->cluster[cl] = av_malloc(AVI_INDEX_CLUSTER_SIZE*sizeof(AVIIentry));
        if (!idx->cluster[cl])
            return -1;
        idx->ents_allocated += AVI_INDEX_CLUSTER_SIZE;
    }

    idx->cluster[cl][id].flags = flags;
    idx->cluster[cl][id].pos = avio_tell(pb) - avi->movi_list;
    idx->cluster[cl][id].len = size;
    idx->entry++;


    write_4cc(AVI, tag);
    write_wl32(AVI, size);
    write_mem(AVI, pkt->data, size);
    if (size & 1)
        write_w8(pb, 0);

    flush_buffer(AVI);

    return 0;
}

































/* AVI_MAX_LEN: The maximum length of an AVI file, we stay a bit below
    the 2GB limit (Remember: 2*10^9 is smaller than 2 GB - using 1900*1024*1024) */

ULONG AVI_MAX_LEN = AVI_MAX_SIZE;

ULONG AVI_set_MAX_LEN(ULONG len)
{
	/*clip to max size*/
	if(len > AVI_MAX_SIZE) {len = AVI_MAX_SIZE; }
	else { AVI_MAX_LEN = len; }

	return (AVI_MAX_LEN);
}

/* HEADERBYTES: The number of bytes to reserve for the header */

#define HEADERBYTES 2048

#define PAD_EVEN(x) ( ((x)+1) & ~1 )


/* Copy n into dst as a 4 byte, little endian number.
   Should also work on big endian machines */

static void long2str(unsigned char *dst, int n)
{
	dst[0] = (n    )&0xff;
	dst[1] = (n>> 8)&0xff;
	dst[2] = (n>>16)&0xff;
	dst[3] = (n>>24)&0xff;
}

/* Convert a string of 4 or 2 bytes to a number,
   also working on big endian machines */

static unsigned long str2ulong(BYTE *str)
{
	return ( str[0] | (str[1]<<8) | (str[2]<<16) | (str[3]<<24) );
}
//~ static unsigned long str2ushort(BYTE *str)
//~ {
   //~ return ( str[0] | (str[1]<<8) );
//~ }

/* Calculate audio sample size from number of bits and number of channels.
   This may have to be adjusted for eg. 12 bits and stereo */

static int avi_sampsize(struct avi_t *AVI, int j)
{
	int s;
	if (AVI->track[j].a_fmt != WAVE_FORMAT_PCM)
	{
		s = 4;
	}
	else
	{
		s = ((AVI->track[j].a_bits+7)/8)*AVI->track[j].a_chans;
		if(s<4) s=4; /* avoid possible zero divisions */
	}
	return s;
}

static ssize_t avi_write (int fd, BYTE *buf, size_t len)
{
	ssize_t n = 0;
	ssize_t r = 0;

	while (r < len)
	{
		n = write (fd, buf + r, len - r);
		if (n < 0)
			return n;

		r += n;
	}
	return r;
}

/* Add a chunk (=tag and data) to the AVI file,
   returns -1 on write error, 0 on success */

static int avi_add_chunk(struct avi_t *AVI, BYTE *tag, BYTE *data, int length)
{
	BYTE c[8];
	BYTE p=0;
	/* Copy tag and length int c, so that we need only 1 write system call
	for these two values */

	memcpy(c,tag,4);
	long2str(c+4,length);

	/* Output tag, length and data, restore previous position
	if the write fails */

	if( avi_write(AVI->fdes,c,8) != 8 ||
		avi_write(AVI->fdes,data,length) != length ||
		avi_write(AVI->fdes,&p,length&1) != (length&1)) // if len is uneven, write a pad byte
	{
		lseek(AVI->fdes,AVI->pos,SEEK_SET);
		AVI_errno = AVI_ERR_WRITE;
		return -1;
	}

	/* Update file position */
	AVI->pos += 8 + PAD_EVEN(length);

	//fprintf(stderr, "pos=%lu %s\n", AVI->pos, tag);

	return 0;
}

static int avi_add_index_entry(struct avi_t *AVI, BYTE *tag, long flags, long pos, long len)
{
	void *ptr;

	if(AVI->n_idx>=AVI->max_idx)
	{
	ptr = realloc((void *)AVI->idx,(AVI->max_idx+4096)*16);
	if(ptr == 0)
	{
		AVI_errno = AVI_ERR_NO_MEM;
		return -1;
	}
		AVI->max_idx += 4096;
		AVI->idx = (BYTE((*)[16]) ) ptr;
	}

	/* Add index entry */

	memcpy(AVI->idx[AVI->n_idx],tag,4);
	long2str(AVI->idx[AVI->n_idx]+ 4,flags);
	long2str(AVI->idx[AVI->n_idx]+ 8,pos);
	long2str(AVI->idx[AVI->n_idx]+12,len);

	/* Update counter */

	AVI->n_idx++;

	if(len>AVI->max_len) AVI->max_len=len;

	return 0;
}

//SLM
#ifndef S_IRUSR
#define S_IRWXU       00700       /* read, write, execute: owner */
#define S_IRUSR       00400       /* read permission: owner */
#define S_IWUSR       00200       /* write permission: owner */
#define S_IXUSR       00100       /* execute permission: owner */
#define S_IRWXG       00070       /* read, write, execute: group */
#define S_IRGRP       00040       /* read permission: group */
#define S_IWGRP       00020       /* write permission: group */
#define S_IXGRP       00010       /* execute permission: group */
#define S_IRWXO       00007       /* read, write, execute: other */
#define S_IROTH       00004       /* read permission: other */
#define S_IWOTH       00002       /* write permission: other */
#define S_IXOTH       00001       /* execute permission: other */
#endif


#define OUT4CC(s) \
   if(nhb<=HEADERBYTES-4) memcpy(AVI_header+nhb,s,4); nhb += 4

#define OUTLONG(n) \
   if(nhb<=HEADERBYTES-4) long2str(AVI_header+nhb,n); nhb += 4

#define OUTSHRT(n) \
   if(nhb<=HEADERBYTES-2) { \
      AVI_header[nhb  ] = (n   )&0xff; \
      AVI_header[nhb+1] = (n>>8)&0xff; \
   } \
   nhb += 2

#define OUTCHR(n) \
   if(nhb<=HEADERBYTES-1) { \
      AVI_header[nhb  ] = (n   )&0xff; \
   } \
   nhb += 1

#define OUTMEM(d, s) \
   { \
     unsigned int s_ = (s); \
     if(nhb <= HEADERBYTES-s_) \
        memcpy(AVI_header+nhb, (d), s_); \
     nhb += s_; \
   }

//ThOe write preliminary AVI file header: 0 frames, max vid/aud size
static int avi_update_header(struct avi_t *AVI)
{
	int njunk, sampsize, ms_per_frame, frate, flag;
	int movi_len, hdrl_start, strl_start, j;
	BYTE AVI_header[HEADERBYTES];
	long nhb;
	ULONG xd_size, xd_size_align2;

	//assume index will be written
	//int hasIndex=1;

	//assume max size
	movi_len = AVI_MAX_LEN - HEADERBYTES + 4;



	if(AVI->fps < 0.001)
	{
		frate=0;
		ms_per_frame=0;
	}
	else
	{
		frate = (int) (FRAME_RATE_SCALE*AVI->fps + 0.5);
		ms_per_frame=(int) (1000000/AVI->fps + 0.5);
	}

	/* Prepare the file header */

	nhb = 0;

	/* The RIFF header */

	OUT4CC ("RIFF");
	OUTLONG(movi_len);    // assume max size
	OUT4CC ("AVI ");

	/* Start the header list */

	OUT4CC ("LIST");
	OUTLONG(0);        /* Length of list in bytes, don't know yet */
	hdrl_start = nhb;  /* Store start position */
	OUT4CC ("hdrl");

	/* The main AVI header */

	/* The Flags in AVI File header */

#define AVIF_HASINDEX           0x00000010      /* Index at end of file */
#define AVIF_MUSTUSEINDEX       0x00000020
#define AVIF_ISINTERLEAVED      0x00000100
#define AVIF_TRUSTCKTYPE        0x00000800      /* Use CKType to find key frames */
#define AVIF_WASCAPTUREFILE     0x00010000
#define AVIF_COPYRIGHTED        0x00020000

	OUT4CC ("avih");
	OUTLONG(56);			/* # of bytes to follow */
	OUTLONG(ms_per_frame);		/* Microseconds per frame */
	//ThOe ->0
	//OUTLONG(10000000);		/* MaxBytesPerSec, I hope this will never be used */
	OUTLONG(0);
	OUTLONG(0);			/* PaddingGranularity (whatever that might be) */
					/* Other sources call it 'reserved' */
	//flag = AVIF_ISINTERLEAVED;
	flag = AVIF_WASCAPTUREFILE;
	/*do not force index yet -only when closing*/
	/*this should prevent bad avi files even if it is not closed properly*/
	//if(hasIndex) flag |= AVIF_HASINDEX;
	//if(hasIndex && AVI->must_use_index) flag |= AVIF_MUSTUSEINDEX;
	OUTLONG(flag);			/* Flags */
	OUTLONG(0);			// no frames yet
	OUTLONG(0);			/* InitialFrames */

	OUTLONG(AVI->anum+1);		/*streams audio + video*/

	OUTLONG(0);			/* SuggestedBufferSize */
	OUTLONG(AVI->width);		/* Width */
	OUTLONG(AVI->height);		/* Height */
					/* MS calls the following 'reserved': */
	OUTLONG(0);			/* TimeScale:  Unit used to measure time */
	OUTLONG(0);			/* DataRate:   Data rate of playback     */
	OUTLONG(0);			/* StartTime:  Starting time of AVI data */
	OUTLONG(0);			/* DataLength: Size of AVI data chunk    */


	/* Start the video stream list ---------------------------------- */

	OUT4CC ("LIST");
	OUTLONG(0);        /* Length of list in bytes, don't know yet */
	strl_start = nhb;  /* Store start position */
	OUT4CC ("strl");

	/* The video stream header */

	OUT4CC ("strh");
	OUTLONG(64);                 /* # of bytes to follow */
	OUT4CC ("vids");             /* Type */
	OUT4CC (AVI->compressor);    /* Handler */
	OUTLONG(0);                  /* Flags */
	OUTLONG(0);                  /* Reserved, MS says: wPriority, wLanguage */
	OUTLONG(0);                  /* InitialFrames */
	OUTLONG(FRAME_RATE_SCALE);   /* Scale */
	OUTLONG(frate);              /* Rate: Rate/Scale == samples/second */
	OUTLONG(0);                  /* Start */
	OUTLONG(0);                  // no frames yet
	OUTLONG(0);                  /* SuggestedBufferSize */
	OUTLONG(-1);                 /* Quality */
	OUTLONG(0);                  /* SampleSize */
	OUTLONG(0);                  /* Frame */
	OUTLONG(0);                  /* Frame */
	OUTLONG(0);                  /* Frame */
	OUTLONG(0);                  /* Frame */

	/* The video stream format */

	xd_size        = AVI->extradata_size;
	xd_size_align2 = (AVI->extradata_size+1) & ~1;

	OUT4CC ("strf");
	OUTLONG(40 + xd_size_align2);/* # of bytes to follow */
	OUTLONG(40 + xd_size);       /* Size */
	OUTLONG(AVI->width);         /* Width */
	OUTLONG(AVI->height);        /* Height */
	OUTSHRT(1);                  /* Planes */
	OUTSHRT(24);                 /*Count - bitsperpixel - 1,4,8 or 24  32*/
	if(strncmp(AVI->compressor,"DIB",3)==0) {OUTLONG (0); }    /* Compression */
	else { OUT4CC (AVI->compressor); }
	//OUT4CC (AVI->compressor);

	// ThOe (*3)
	OUTLONG(AVI->width*AVI->height);  /* SizeImage (in bytes?) */
	//OUTLONG(AVI->width*AVI->height*3);  /* SizeImage */
	OUTLONG(0);                  /* XPelsPerMeter */
	OUTLONG(0);                  /* YPelsPerMeter */
	OUTLONG(0);                  /* ClrUsed: Number of colors used */
	OUTLONG(0);                  /* ClrImportant: Number of colors important */

	// write extradata
	if (xd_size > 0 && AVI->extradata)
	{
		OUTMEM(AVI->extradata, xd_size);
		if (xd_size != xd_size_align2)
		{
			OUTCHR(0);
		}
	}

	/* Finish stream list, i.e. put number of bytes in the list to proper pos */

	long2str(AVI_header+strl_start-4,nhb-strl_start);


	/* Start the audio stream list ---------------------------------- */
	/* only 1 audio stream => AVI->anum=1*/
	for(j=0; j<AVI->anum; ++j)
	{

		sampsize = avi_sampsize(AVI, j);
		//sampsize = avi_sampsize(AVI);

		OUT4CC ("LIST");
		OUTLONG(0);        /* Length of list in bytes, don't know yet */
		strl_start = nhb;  /* Store start position */
		OUT4CC ("strl");

		/* The audio stream header */

		OUT4CC ("strh");
		OUTLONG(64);            /* # of bytes to follow */
		OUT4CC ("auds");

		// -----------
		// ThOe
		OUT4CC ("\0\0\0\0");             /* Format (Optionally) */
		// -----------

		OUTLONG(0);             /* Flags */
		OUTLONG(0);             /* Reserved, MS says: wPriority, wLanguage */
		OUTLONG(0);             /* InitialFrames */

		// ThOe /4
		OUTLONG(sampsize/4);      /* Scale */
		OUTLONG(AVI->track[j].mpgrate/8);
		OUTLONG(0);             /* Start */
		OUTLONG(4*AVI->track[j].audio_bytes/sampsize);   /* Length */
		OUTLONG(0);             /* SuggestedBufferSize */
		OUTLONG(-1);            /* Quality */

		// ThOe /4
		OUTLONG(sampsize/4);    /* SampleSize */

		OUTLONG(0);             /* Frame */
		OUTLONG(0);             /* Frame */
		OUTLONG(0);             /* Frame */
		OUTLONG(0);             /* Frame */

		/* The audio stream format */

		OUT4CC ("strf");
		OUTLONG(16);                   /* # of bytes to follow */
		OUTSHRT(AVI->track[j].a_fmt);           /* Format */
		OUTSHRT(AVI->track[j].a_chans);         /* Number of channels */
		OUTLONG(AVI->track[j].a_rate);          /* SamplesPerSec */
		// ThOe
		OUTLONG(AVI->track[j].mpgrate/8);
		//ThOe (/4)

		OUTSHRT(sampsize/4);           /* BlockAlign */


		OUTSHRT(AVI->track[j].a_bits);          /* BitsPerSample */

		/* Finish stream list, i.e. put number of bytes in the list to proper pos */

		long2str(AVI_header+strl_start-4,nhb-strl_start);
	}

	/* Finish header list */

	long2str(AVI_header+hdrl_start-4,nhb-hdrl_start);


	/* Calculate the needed amount of junk bytes, output junk */

	njunk = HEADERBYTES - nhb - 8 - 12;

	/* Safety first: if njunk <= 0, somebody has played with
	HEADERBYTES without knowing what (s)he did.
	This is a fatal error */

	if(njunk<=0)
	{
		fprintf(stderr,"AVI_close_output_file: # of header bytes too small\n");
		exit(1);
	}

	OUT4CC ("JUNK");
	OUTLONG(njunk);
	memset(AVI_header+nhb,0,njunk);

	nhb += njunk;

	/* Start the movi list */

	OUT4CC ("LIST");
	OUTLONG(movi_len); /* Length of list in bytes */
	OUT4CC ("movi");

	/* Output the header, truncate the file to the number of bytes
	actually written, report an error if someting goes wrong */


	if ( lseek(AVI->fdes,0,SEEK_SET)<0 ||
		avi_write(AVI->fdes,AVI_header,HEADERBYTES)!=HEADERBYTES ||
		lseek(AVI->fdes,AVI->pos,SEEK_SET)<0)
	{
		AVI_errno = AVI_ERR_CLOSE;
		return -1;
	}

	return 0;
}

/*
   first function to get called

   AVI_open_output_file: Open an AVI File and write a bunch
                         of zero bytes as space for the header.
                         Creates a mutex.

   returns a pointer to avi_t on success, a zero pointer on error
*/

int AVI_open_output_file(struct avi_t *AVI, const char * filename)
{
	int i;
	BYTE AVI_header[HEADERBYTES];
	//int mask = 0;

	/*resets AVI struct*/
	memset((void *)AVI,0,sizeof(struct avi_t));

	__INIT_MUTEX(__MUTEX);

	AVI->closed = 0; /*opened - recordind*/

	/* Since Linux needs a long time when deleting big files,
	we do not truncate the file when we open it.
	Instead it is truncated when the AVI file is closed */
	AVI->fdes = g_open(filename, O_RDWR|O_CREAT,
		S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);

	//AVI->fdes = open(filename,O_RDWR|O_CREAT|O_BINARY,0644 &~mask);
	if (AVI->fdes < 0)
	{
		AVI_errno = AVI_ERR_OPEN;

		__CLOSE_MUTEX(__MUTEX);

		return -1;
	}

	/* Write out HEADERBYTES bytes, the header will go here
	when we are finished with writing */

	for (i=0;i<HEADERBYTES;i++) AVI_header[i] = 0;
	i = avi_write(AVI->fdes,AVI_header,HEADERBYTES);
	if (i != HEADERBYTES)
	{

		if(fsync(AVI->fdes) || close(AVI->fdes))
			perror("AVI ERROR: couldn't write to avi file\n");
		AVI_errno = AVI_ERR_WRITE;

		__CLOSE_MUTEX(__MUTEX);

		return -2;
	}

	AVI->pos  = HEADERBYTES;
	AVI->mode = AVI_MODE_WRITE; /* open for writing */

	//init
	AVI->anum = 0;
	AVI->aptr = 0;

	return 0;
}

void AVI_set_video(struct avi_t *AVI, int width, int height, double fps, const char *compressor)
{
	/* may only be called if file is open for writing */

	if(AVI->mode==AVI_MODE_READ) return;

	__LOCK_MUTEX(__MUTEX);
		AVI->width  = width;
		AVI->height = height;
		AVI->fps    = fps; /* just in case avi doesn't close properly */
			   /* it will be set at close                 */
		if(strncmp(compressor, "RGB", 3)==0)
		{
			memset(AVI->compressor, 0, 4);
		}
		else
		{
			memcpy(AVI->compressor,compressor,4);
		}

		AVI->compressor[4] = 0;
		avi_update_header(AVI);
	__UNLOCK_MUTEX(__MUTEX);

}

void AVI_set_audio(struct avi_t *AVI, int channels, long rate, int mpgrate, int bits, int format)
{
	/* may only be called if file is open for writing */

	if(AVI->mode==AVI_MODE_READ) return;

	__LOCK_MUTEX(__MUTEX);
		//inc audio tracks
		AVI->aptr=AVI->anum;
		++AVI->anum;

		if(AVI->anum > AVI_MAX_TRACKS)
		{
			fprintf(stderr, "error - only %d audio tracks supported\n", AVI_MAX_TRACKS);
			exit(1);
		}

		AVI->track[AVI->aptr].audio_bytes = 0;
		AVI->track[AVI->aptr].a_chans = channels;
		AVI->track[AVI->aptr].a_rate  = rate; /*sample rate*/
		AVI->track[AVI->aptr].a_bits  = bits;
		AVI->track[AVI->aptr].a_fmt   = format;
		AVI->track[AVI->aptr].mpgrate = mpgrate; /* bit rate in b/s */

		avi_update_header(AVI);
	__UNLOCK_MUTEX(__MUTEX);
}

/*
  Write the header of an AVI file and close it.
  returns 0 on success, -1 on write error.
*/

static int avi_close_output_file(struct avi_t *AVI)
{

	int ret, njunk, sampsize, hasIndex, ms_per_frame, frate, idxerror, flag;
	ULONG movi_len;
	int hdrl_start, strl_start, j;
	BYTE AVI_header[HEADERBYTES];
	long nhb;
	ULONG xd_size, xd_size_align2;
	/* Calculate length of movi list */

	movi_len = AVI->pos - HEADERBYTES + 4;

#ifdef INFO_LIST
	long info_len;
	time_t rawtime;
#endif

	/* Try to ouput the index entries. This may fail e.g. if no space
	is left on device. We will report this as an error, but we still
	try to write the header correctly (so that the file still may be
	readable in the most cases */

	idxerror = 0;
	hasIndex = 1;

	ret = avi_add_chunk(AVI,(BYTE *)"idx1",(void *)AVI->idx,AVI->n_idx*16);
	hasIndex = (ret==0);

	if(ret)
	{
		idxerror = 1;
		AVI_errno = AVI_ERR_WRITE_INDEX;
	}

	/* Calculate Microseconds per frame */

	if(AVI->fps < 0.001)
	{
		frate=0;
		ms_per_frame = 0;
	}
	else
	{
		frate = (int) (FRAME_RATE_SCALE*AVI->fps + 0.5);
		ms_per_frame=(int) (1000000/AVI->fps + 0.5);
	}

	/* Prepare the file header */

	nhb = 0;

	/* The RIFF header */

	OUT4CC ("RIFF");
	OUTLONG(AVI->pos - 8);    /* # of bytes to follow */
	OUT4CC ("AVI ");

	/* Start the header list */

	OUT4CC ("LIST");
	OUTLONG(0);        /* Length of list in bytes, don't know yet */
	hdrl_start = nhb;  /* Store start position */
	OUT4CC ("hdrl");

	/* The main AVI header */

	/* The Flags in AVI File header */

#define AVIF_HASINDEX           0x00000010      /* Index at end of file */
#define AVIF_MUSTUSEINDEX       0x00000020
#define AVIF_ISINTERLEAVED      0x00000100
#define AVIF_TRUSTCKTYPE        0x00000800      /* Use CKType to find key frames */
#define AVIF_WASCAPTUREFILE     0x00010000
#define AVIF_COPYRIGHTED        0x00020000

	OUT4CC ("avih");
	OUTLONG(56);			/* # of bytes to follow */
	OUTLONG(ms_per_frame);		/* Microseconds per frame */
	//ThOe ->0
	//OUTLONG(10000000);		/* MaxBytesPerSec, I hope this will never be used */
	OUTLONG(0);
	OUTLONG(0);			/* PaddingGranularity (whatever that might be) */
					/* Other sources call it 'reserved' */
	flag = AVIF_WASCAPTUREFILE;
	if(hasIndex) flag |= AVIF_HASINDEX;
	if(hasIndex && AVI->must_use_index) flag |= AVIF_MUSTUSEINDEX;
	OUTLONG(flag);			/* Flags */
	OUTLONG(AVI->video_frames);	/* TotalFrames */
	OUTLONG(0);			/* InitialFrames */
	OUTLONG(AVI->anum+1);		/* streams audio + video */

	OUTLONG(0);			/* SuggestedBufferSize */
	OUTLONG(AVI->width);		/* Width */
	OUTLONG(AVI->height);		/* Height */
					/* MS calls the following 'reserved': */
	OUTLONG(0);			/* TimeScale:  Unit used to measure time */
	OUTLONG(0);			/* DataRate:   Data rate of playback     */
	OUTLONG(0);			/* StartTime:  Starting time of AVI data */
	OUTLONG(0);			/* DataLength: Size of AVI data chunk    */


	/* Start the video stream list ---------------------------------- */

	OUT4CC ("LIST");
	OUTLONG(0);        /* Length of list in bytes, don't know yet */
	strl_start = nhb;  /* Store start position */
	OUT4CC ("strl");

	/* The video stream header */

	OUT4CC ("strh");
	OUTLONG(64);                 /* # of bytes to follow */
	OUT4CC ("vids");             /* Type */
	OUT4CC (AVI->compressor);    /* Handler */
	OUTLONG(0);                  /* Flags */
	OUTLONG(0);                  /* Reserved, MS says: wPriority, wLanguage */
	OUTLONG(0);                  /* InitialFrames */
	OUTLONG(FRAME_RATE_SCALE);   /* Scale */
	OUTLONG(frate);              /* Rate: Rate/Scale == samples/second */
	OUTLONG(0);                  /* Start */
	OUTLONG(AVI->video_frames);  /* Length */
	OUTLONG(AVI->max_len);       /* SuggestedBufferSize */
	OUTLONG(-1);                 /* Quality */
	OUTLONG(0);                  /* SampleSize */
	OUTLONG(0);                  /* Frame */
	OUTLONG(0);                  /* Frame */
	OUTLONG(0);                  /* Frame */
	OUTLONG(0);                  /* Frame */

	/* The video stream format i- this is a BITMAPINFO structure*/

	xd_size        = AVI->extradata_size;
	//printf("extra size=%d\n",xd_size);
	xd_size_align2 = (AVI->extradata_size+1) & ~1;

	OUT4CC ("strf");
	OUTLONG(40 + xd_size_align2);      /* # of bytes to follow (biSize) ?????*/
	OUTLONG(40 + xd_size);             /* biSize */
	OUTLONG(AVI->width);               /* biWidth */
	OUTLONG(AVI->height);              /* biHeight */
	OUTSHRT(1);                        /* Planes - allways 1 */
	OUTSHRT(24);                       /*Count - bitsperpixel - 1,4,8 or 24  32*/
	if(strncmp(AVI->compressor,"DIB",3)==0) {OUTLONG (0); }    /* Compression */
	else { OUT4CC (AVI->compressor); }
	//OUT4CC (AVI->compressor);
	// ThOe (*3)
	OUTLONG(AVI->width*AVI->height);  /* SizeImage (in bytes?) should be biSizeImage = ((((biWidth * biBitCount) + 31) & ~31) >> 3) * biHeight*/
	OUTLONG(0);                       /* XPelsPerMeter */
	OUTLONG(0);                       /* YPelsPerMeter */
	OUTLONG(0);                       /* ClrUsed: Number of colors used */
	OUTLONG(0);                       /* ClrImportant: Number of colors important */

	// write extradata if present
	if (xd_size > 0 && AVI->extradata)
	{
		OUTMEM(AVI->extradata, xd_size);
		if (xd_size != xd_size_align2)
		{
			OUTCHR(0);
		}
	}

	/* Finish stream list, i.e. put number of bytes in the list to proper pos */

	long2str(AVI_header+strl_start-4,nhb-strl_start);

	/* Start the audio stream list ---------------------------------- */

	for(j=0; j<AVI->anum; ++j)
	{

		ULONG nBlockAlign = 0;
		ULONG avgbsec = 0;
		ULONG scalerate = 0;

		sampsize = avi_sampsize(AVI, j);
		sampsize = AVI->track[j].a_fmt==(WAVE_FORMAT_PCM || WAVE_FORMAT_IEEE_FLOAT)?sampsize*4:sampsize;

		nBlockAlign = (AVI->track[j].a_rate<32000)?576:1152;
		/*
		printf("XXX sampsize (%d) block (%ld) rate (%ld) audio_bytes (%ld) mp3rate(%ld,%ld)\n",
		 sampsize, nBlockAlign, AVI->track[j].a_rate,
		 (long int)AVI->track[j].audio_bytes,
		 1000*AVI->track[j].mp3rate/8, AVI->track[j].mp3rate);
		 */

		if ((AVI->track[j].a_fmt==WAVE_FORMAT_PCM) || (AVI->track[j].a_fmt==WAVE_FORMAT_IEEE_FLOAT))
		{
			sampsize = (AVI->track[j].a_chans<2)?sampsize/2:sampsize;
			avgbsec = AVI->track[j].a_rate*sampsize/4;
			scalerate = AVI->track[j].a_rate*sampsize/4;
		}
		else
		{
			avgbsec = AVI->track[j].mpgrate/8;
			scalerate = AVI->track[j].mpgrate/8;
		}

		OUT4CC ("LIST");
		OUTLONG(0);        /* Length of list in bytes, don't know yet */
		strl_start = nhb;  /* Store start position */
		OUT4CC ("strl");

		/* The audio stream header */

		OUT4CC ("strh");
		OUTLONG(64);            /* # of bytes to follow */
		OUT4CC ("auds");

		// -----------
		// ThOe
		OUT4CC ("\0\0\0\0");             /* Format (Optionally) */
		// -----------

		OUTLONG(0);             /* Flags */
		OUTLONG(0);             /* Reserved, MS says: wPriority, wLanguage */
		OUTLONG(0);             /* InitialFrames */

		// VBR
		if ((AVI->track[j].a_fmt != WAVE_FORMAT_PCM) && AVI->track[j].a_vbr)
		{
			OUTLONG(nBlockAlign);                   /* Scale */
			OUTLONG(AVI->track[j].a_rate);          /* Rate */
			OUTLONG(0);                             /* Start */
			OUTLONG(AVI->track[j].audio_chunks);    /* Length */
			OUTLONG(0);                             /* SuggestedBufferSize */
			OUTLONG(0);                             /* Quality */
			OUTLONG(0);                             /* SampleSize */
			OUTLONG(0);                             /* Frame */
			OUTLONG(0);                             /* Frame */
			OUTLONG(0);                             /* Frame */
			OUTLONG(0);                             /* Frame */
		}
		else
		{
			OUTLONG(sampsize/4);                    /* Scale */
			OUTLONG(scalerate);                     /* Rate */
			OUTLONG(0);                             /* Start */
			OUTLONG(4*AVI->track[j].audio_bytes/sampsize);  /* Length */
			OUTLONG(0);                             /* SuggestedBufferSize */
			OUTLONG(0xffffffff);                    /* Quality */
			OUTLONG(sampsize/4);                    /* SampleSize */
			OUTLONG(0);                             /* Frame */
			OUTLONG(0);                             /* Frame */
			OUTLONG(0);                             /* Frame */
			OUTLONG(0);                             /* Frame */
		}

		/* The audio stream format */

		OUT4CC ("strf");

		if ((AVI->track[j].a_fmt != WAVE_FORMAT_PCM) && AVI->track[j].a_vbr)
		{

			OUTLONG(30);                            /* # of bytes to follow */ // mplayer writes 28
			OUTSHRT(AVI->track[j].a_fmt);           /* Format */                  // 2
			OUTSHRT(AVI->track[j].a_chans);         /* Number of channels */      // 2
			OUTLONG(AVI->track[j].a_rate);          /* SamplesPerSec */           // 4
			//ThOe/tibit
			OUTLONG(AVI->track[j].mpgrate/8);  /* maybe we should write an avg. */ // 4
			OUTSHRT(nBlockAlign);                   /* BlockAlign */              // 2
			OUTSHRT(AVI->track[j].a_bits);          /* BitsPerSample */           // 2

			OUTSHRT(12);                            /* cbSize */                   // 2
			OUTSHRT(1);                             /* wID */                      // 2
			OUTLONG(2);                             /* fdwFlags */                 // 4
			OUTSHRT(nBlockAlign);                   /* nBlockSize */               // 2
			OUTSHRT(1);                             /* nFramesPerBlock */          // 2
			OUTSHRT(0);                             /* nCodecDelay */              // 2
		}
		else if ((AVI->track[j].a_fmt != WAVE_FORMAT_PCM) && !AVI->track[j].a_vbr)
		{
			OUTLONG(30);                            /* # of bytes to follow (30)*/
			OUTSHRT(AVI->track[j].a_fmt);           /* Format */
			OUTSHRT(AVI->track[j].a_chans);         /* Number of channels */
			OUTLONG(AVI->track[j].a_rate);          /* SamplesPerSec */
			//ThOe/tibit
			OUTLONG(AVI->track[j].mpgrate/8);
			OUTSHRT(sampsize/4);                    /* BlockAlign */
			OUTSHRT(AVI->track[j].a_bits);          /* BitsPerSample */

			OUTSHRT(12);                            /* cbSize */
			OUTSHRT(1);                             /* wID */
			OUTLONG(2);                             /* fdwFlags */
			OUTSHRT(nBlockAlign);                   /* nBlockSize */
			OUTSHRT(1);                             /* nFramesPerBlock */
			OUTSHRT(0);                             /* nCodecDelay */
		}
		else
		{
			OUTLONG(18);                            /* # of bytes to follow (18)*/
			OUTSHRT(AVI->track[j].a_fmt);           /* Format */
			OUTSHRT(AVI->track[j].a_chans);         /* Number of channels */
			OUTLONG(AVI->track[j].a_rate);          /* SamplesPerSec */
			//ThOe/tibit
			OUTLONG(avgbsec);                       /* Avg bytes/sec */
			OUTSHRT(sampsize/4);                    /* BlockAlign */
			OUTSHRT(AVI->track[j].a_bits);          /* BitsPerSample */
			OUTSHRT(0);                             /* cbSize */
		}

		/* Finish stream list, i.e. put number of bytes in the list to proper pos */

		long2str(AVI_header+strl_start-4,nhb-strl_start);

	}

	/* Finish header list */

	long2str(AVI_header+hdrl_start-4,nhb-hdrl_start);

	// add INFO list --- (0.6.0pre4)

#ifdef INFO_LIST
	OUT4CC ("LIST");

	//FIXME
	info_len = MAX_INFO_STRLEN + 12;
	OUTLONG(info_len);
	OUT4CC ("INFO");

	OUT4CC ("INAM");
	OUTLONG(MAX_INFO_STRLEN);

	sprintf(id_str, "\t");
	memset(AVI_header+nhb, 0, MAX_INFO_STRLEN);
	memcpy(AVI_header+nhb, id_str, strlen(id_str));
	nhb += MAX_INFO_STRLEN;

	OUT4CC ("ISFT");
	OUTLONG(MAX_INFO_STRLEN);

	sprintf(id_str, "%s-%s", PACKAGE, VERSION);
	memset(AVI_header+nhb, 0, MAX_INFO_STRLEN);
	memcpy(AVI_header+nhb, id_str, strlen(id_str));
	nhb += MAX_INFO_STRLEN;

	OUT4CC ("ICMT");
	OUTLONG(MAX_INFO_STRLEN);

	time(&rawtime);
	sprintf(id_str, "\t%s %s", ctime(&rawtime), "");
	memset(AVI_header+nhb, 0, MAX_INFO_STRLEN);
	memcpy(AVI_header+nhb, id_str, 25);
	nhb += MAX_INFO_STRLEN;
#endif


	/* Calculate the needed amount of junk bytes, output junk */

	njunk = HEADERBYTES - nhb - 8 - 12;

	/* Safety first: if njunk <= 0, somebody has played with
	HEADERBYTES without knowing what (s)he did.
	This is a fatal error */

	if(njunk<=0)
	{
		fprintf(stderr,"AVI_close_output_file: # of header bytes too small\n");
		exit(1);
	}

	OUT4CC ("JUNK");
	OUTLONG(njunk);
	memset(AVI_header+nhb,0,njunk);
	nhb += njunk;

	/* Start the movi list */

	OUT4CC ("LIST");
	OUTLONG(movi_len); /* Length of list in bytes */
	OUT4CC ("movi");

	/* Output the header, truncate the file to the number of bytes
	actually written, report an error if someting goes wrong */

	if ( lseek(AVI->fdes,0,SEEK_SET)<0 ||
		avi_write(AVI->fdes,AVI_header,HEADERBYTES)!=HEADERBYTES ||
		ftruncate(AVI->fdes,AVI->pos)<0 )
	{
		AVI_errno = AVI_ERR_CLOSE;
		return -1;
	}

	if(idxerror) return -1;

	return 0;
}
/*
   avi_write_data:
   Add video or audio data to the file;

   Return values:
    0    No error;
   -1    Error, AVI_errno is set appropriatly;

*/

static int avi_write_data(struct avi_t *AVI, BYTE *data, long length, int audio, int keyframe)
{
	int ret=0;
	int n=0;
	char astr[5];
	/* Check for maximum file length */

	if ( (AVI->pos + 8 + length + 8 + (AVI->n_idx+1)*16) > AVI_MAX_LEN )
	{
		AVI_errno = AVI_ERR_SIZELIM;
		ret=1;

		/*if it is bigger than max size (1900Mb) + extra size (20 Mb) return imediatly*/
		/*else still write the data but will return an error                          */
		if ((AVI->pos + 8 + length + 8 + (AVI->n_idx+1)*16) > (AVI_MAX_LEN + AVI_EXTRA_SIZE)) return (-1);
	}

	//set tag for current audio track
	snprintf((char *)astr, sizeof(astr), "0%1dwb", (int)(AVI->aptr+1));

	/* Add index entry */
	if(audio)
		n = avi_add_index_entry(AVI,(BYTE *) astr,0x00,AVI->pos,length);
	else
		n = avi_add_index_entry(AVI,(BYTE *) "00dc",((keyframe)?0x10:0x0),AVI->pos,length);

	if(n) return(-1);

	/* Output tag and data */
	if(audio)
		n = avi_add_chunk(AVI,(BYTE *) astr,data,length);
	else
		n = avi_add_chunk(AVI,(BYTE *)"00dc",data,length);

	if (n) return(-1);

	return ret;
}

int AVI_write_frame(struct avi_t *AVI, BYTE *data, long bytes, int keyframe)
{
	int ret=0;
	off_t pos;

	if(AVI->mode==AVI_MODE_READ) { AVI_errno = AVI_ERR_NOT_PERM; return -1; }

	__LOCK_MUTEX(__MUTEX);
		pos = AVI->pos;
		ret = avi_write_data(AVI,data,bytes,0, keyframe);

		if(!(ret < 0))
		{
			AVI->last_pos = pos;
			AVI->last_len = bytes;
			AVI->video_frames++;
		}
	__UNLOCK_MUTEX(__MUTEX);
	return ret;
}

int AVI_dup_frame(struct avi_t *AVI)
{
	if(AVI->mode==AVI_MODE_READ) { AVI_errno = AVI_ERR_NOT_PERM; return -1; }

	if(AVI->last_pos==0) return 0; /* No previous real frame */

	__LOCK_MUTEX(__MUTEX);
		if(avi_add_index_entry(AVI,(BYTE *) "00dc",0x10,AVI->last_pos,AVI->last_len)) return -1;
		AVI->video_frames++;
		AVI->must_use_index = 1;
	__UNLOCK_MUTEX(__MUTEX);
	return 0;
}

int AVI_write_audio(struct avi_t *AVI, BYTE *data, long bytes)
{
	int ret=0;
	if(AVI->mode==AVI_MODE_READ) { AVI_errno = AVI_ERR_NOT_PERM; return -1; }

	__LOCK_MUTEX(__MUTEX);
		ret = avi_write_data(AVI,data,bytes,1,0);

		if(!(ret<0))
		{
			AVI->track[AVI->aptr].audio_bytes += bytes;
			AVI->track[AVI->aptr].audio_chunks++;
		}
	__UNLOCK_MUTEX(__MUTEX);
	return ret;
}


/*doesn't check for size limit - called when closing avi*/
int AVI_append_audio(struct avi_t *AVI, BYTE *data, long bytes)
{
	// won't work for >2gb
	long i, length, pos;
	BYTE c[4];

	if(AVI->mode==AVI_MODE_READ) { AVI_errno = AVI_ERR_NOT_PERM; return -1; }

	// update last index entry:
	__LOCK_MUTEX(__MUTEX);
		--AVI->n_idx;
		length = str2ulong(AVI->idx[AVI->n_idx]+12);
		pos    = str2ulong(AVI->idx[AVI->n_idx]+8);

		//update;
		long2str(AVI->idx[AVI->n_idx]+12,length+bytes);

		++AVI->n_idx;

		AVI->track[AVI->aptr].audio_bytes += bytes;

		//update chunk header
		//xio_lseek(AVI->fdes, pos+4, SEEK_SET);
		lseek(AVI->fdes, pos+4, SEEK_SET);
		long2str(c, length+bytes);
		avi_write(AVI->fdes, c, 4);

		//xio_lseek(AVI->fdes, pos+8+length, SEEK_SET);
		lseek(AVI->fdes, pos+8+length, SEEK_SET);
		i=PAD_EVEN(length + bytes);

		bytes = i - length;
		avi_write(AVI->fdes, data, bytes);
		AVI->pos = pos + 8 + i;
	__UNLOCK_MUTEX(__MUTEX);

	return 0;
}


ULONG AVI_bytes_remain(struct avi_t *AVI)
{
	if(AVI->mode==AVI_MODE_READ) return 0;

	return ( AVI_MAX_LEN - (AVI->pos + 8 + 16*AVI->n_idx));
}

ULONG AVI_bytes_written(struct avi_t *AVI)
{
	if(AVI->mode==AVI_MODE_READ) return 0;

	return (AVI->pos + 8 + 16*AVI->n_idx);
}

int AVI_set_audio_track(struct avi_t *AVI, int track)
{
	if(track < 0 || track + 1 > AVI->anum) return(-1);

	//this info is not written to file anyway
	AVI->aptr=track;
	return 0;
}

void AVI_set_audio_vbr(struct avi_t *AVI, long is_vbr)
{
	AVI->track[AVI->aptr].a_vbr = is_vbr;
}


/*******************************************************************
 *                                                                 *
 *    Utilities for reading video and audio from an AVI File       *
 *                                                                 *
 *******************************************************************/

int AVI_close(struct avi_t *AVI)
{
	int ret,j;

	/* If the file was open for writing, the header and index still have
	to be written */

	__LOCK_MUTEX(__MUTEX);
		AVI->closed = 1; /*closed - not recording*/
		if(AVI->mode == AVI_MODE_WRITE)
			ret = avi_close_output_file(AVI);
		else
			ret = 0;

		/* Even if a error occurred, we first clean up */
		if(fsync(AVI->fdes) || close(AVI->fdes))
			perror("AVI ERROR: couldn't write avi data\n");
		if(AVI->idx) free(AVI->idx);
		if(AVI->video_index) free(AVI->video_index);
		for (j=0; j<AVI->anum; j++)
		{
			AVI->track[j].audio_bytes=0;
			if(AVI->track[j].audio_index) free(AVI->track[j].audio_index);
		}
	__UNLOCK_MUTEX(__MUTEX);
	/*free the mutex*/
	__CLOSE_MUTEX(__MUTEX);

	return ret;
}


#define ERR_EXIT(x) \
{ \
   AVI_close(AVI); \
   AVI_errno = x; \
   return 0; \
}

int AVI_getErrno()
{
	return AVI_errno;
}

/* AVI_print_error: Print most recent error (similar to perror) */

char *(avi_errors[]) =
{
  /*  0 */ "avilib - No Error",
  /*  1 */ "avilib - AVI file size limit reached",
  /*  2 */ "avilib - Error opening AVI file",
  /*  3 */ "avilib - Error reading from AVI file",
  /*  4 */ "avilib - Error writing to AVI file",
  /*  5 */ "avilib - Error writing index (file may still be usable)",
  /*  6 */ "avilib - Error closing AVI file",
  /*  7 */ "avilib - Operation (read/write) not permitted",
  /*  8 */ "avilib - Out of memory (malloc failed)",
  /*  9 */ "avilib - Not an AVI file",
  /* 10 */ "avilib - AVI file has no header list (corrupted?)",
  /* 11 */ "avilib - AVI file has no MOVI list (corrupted?)",
  /* 12 */ "avilib - AVI file has no video data",
  /* 13 */ "avilib - operation needs an index",
  /* 14 */ "avilib - Unkown Error"
};
static int num_avi_errors = sizeof(avi_errors)/sizeof(char*);

static char error_string[4096];

void AVI_print_error(char *str)
{
	int aerrno;

	aerrno = (AVI_errno>=0 && AVI_errno<num_avi_errors) ? AVI_errno : num_avi_errors-1;

	fprintf(stderr,"%s: %s\n",str,avi_errors[aerrno]);

	/* for the following errors, perror should report a more detailed reason: */

	if(AVI_errno == AVI_ERR_OPEN ||
		AVI_errno == AVI_ERR_READ ||
		AVI_errno == AVI_ERR_WRITE ||
		AVI_errno == AVI_ERR_WRITE_INDEX ||
		AVI_errno == AVI_ERR_CLOSE )
	{
		perror("REASON");
	}
}

char *AVI_strerror()
{
	int aerrno;

	aerrno = (AVI_errno>=0 && AVI_errno<num_avi_errors) ? AVI_errno : num_avi_errors-1;

	if(AVI_errno == AVI_ERR_OPEN ||
		AVI_errno == AVI_ERR_READ ||
		AVI_errno == AVI_ERR_WRITE ||
		AVI_errno == AVI_ERR_WRITE_INDEX ||
		AVI_errno == AVI_ERR_CLOSE )
	{
		sprintf(error_string,"%s - %s",avi_errors[aerrno],strerror(errno));
		return error_string;
	}
	else
	{
		return avi_errors[aerrno];
	}
}
