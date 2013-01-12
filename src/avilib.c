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
#include <math.h>
#include "config.h"
#include "avilib.h"
#include "defs.h"
#include "lavc_common.h"

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

static int64_t avi_tell(avi_Context* AVI)
{
	//flush the file buffer
	fflush(AVI->writer->fp);

	//return the file pointer position
	return ((int64_t) ftello64(AVI->writer->fp));
}

static int64_t flush_buffer(avi_Context* AVI)
{
	size_t nitems = 0;
	if (AVI->writer->buf_ptr > AVI->writer->buffer)
	{
		nitems= AVI->writer->buf_ptr - AVI->writer->buffer;
		if(fwrite(AVI->writer->buffer, 1, nitems, AVI->writer->fp) < nitems)
		{
			perror("AVI: file write error");
			return -1;
		}
	}
	else if (AVI->writer->buf_ptr < AVI->writer->buffer)
	{
		fprintf(stderr, "AVI: Bad buffer pointer - dropping buffer\n");
		AVI->writer->buf_ptr = AVI->writer->buffer;
		return -1;
	}

	int64_t size_inc = (AVI->writer->size - AVI->writer->position) + nitems;
	if(size_inc > 0)
		AVI->writer->size += size_inc;

	AVI->writer->position = avi_tell(AVI); //update current file pointer position

	AVI->writer->buf_ptr = AVI->writer->buffer;

	//just for debug - should be removed
	if(AVI->writer->position > AVI->writer->size)
		fprintf(stderr, "AVI: file pointer above expected file size\n");

	return AVI->writer->position;
}

static int avi_seek(avi_Context* AVI, int64_t position)
{
	int ret = 0;

	if(position <= AVI->writer->size) //position is on the file
	{
		//flush the memory buffer (we need an empty buffer)
		flush_buffer(AVI);
		//try to move the file pointer to position
		int ret = fseeko(AVI->writer->fp, position, SEEK_SET);
		if(ret != 0)
			fprintf(stderr, "AVI: seek to file position 0x%llx failed\n", position);
		else
			AVI->writer->position = position; //update writer position
		//we are now on position with an empty memory buffer
	}
	else // position is on the buffer
	{
		//move file pointer to EOF
		if(AVI->writer->position != AVI->writer->size)
		{
			fseeko(AVI->writer->fp, AVI->writer->size, SEEK_SET);
			AVI->writer->position = AVI->writer->size;
		}
		//move buffer pointer to position
		AVI->writer->buf_ptr = AVI->writer->buffer + (position - AVI->writer->size);
	}
	return ret;
}

static int avi_skip(avi_Context* AVI, int amount)
{
	//flush the memory buffer (clean buffer)
	flush_buffer(AVI);
	//try to move the file pointer to position
	int ret = fseeko(AVI->writer->fp, amount, SEEK_CUR);
	if(ret != 0)
		fprintf(stderr, "AVI: skip file pointer by 0x%x failed\n", amount);

	AVI->writer->position = avi_tell(AVI); //update current file pointer position

	//we are on position with an empty memory buffer
	return ret;
}

static int64_t avi_get_offset(avi_Context* AVI)
{
	//buffer offset
	int64_t offset = AVI->writer->buf_ptr - AVI->writer->buffer;
	if(offset < 0)
	{
		fprintf(stderr, "AVI: bad buf pointer\n");
		AVI->writer->buf_ptr = AVI->writer->buffer;
		offset = 0;
	}
	//add to file offset
	offset += AVI->writer->position;

	return offset;
}

static void write_w8(avi_Context* AVI, BYTE b)
{
	*AVI->writer->buf_ptr++ = b;
    if (AVI->writer->buf_ptr >= AVI->writer->buf_end)
        flush_buffer(AVI);
}

static void write_buf(avi_Context* AVI, BYTE *buf, int size)
{
	while (size > 0)
	{
		int len = AVI->writer->buf_end - AVI->writer->buf_ptr;
		if(len < 0)
			fprintf(stderr,"AVI: (write_buf) buff pointer outside buffer\n");
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

static void write_wl16(avi_Context* AVI, uint16_t val)
{
    write_w8(AVI, (BYTE) val);
    write_w8(AVI, (BYTE) (val >> 8));
}

void write_wb16(avi_Context* AVI, uint16_t val)
{
    write_w8(AVI, (BYTE) (val >> 8));
    write_w8(AVI, (BYTE) val);
}

static void write_wl24(avi_Context* AVI, uint32_t val)
{
    write_wl16(AVI, (uint16_t) (val & 0xffff));
    write_w8(AVI, (BYTE) (val >> 16));
}

static void write_wb24(avi_Context* AVI, uint32_t val)
{
    write_wb16(AVI, (uint16_t) (val >> 8));
    write_w8(AVI, (BYTE) val);
}

static void write_wl32(avi_Context* AVI, uint32_t val)
{
    write_w8(AVI, (BYTE) val);
    write_w8(AVI, (BYTE) (val >> 8));
    write_w8(AVI, (BYTE) (val >> 16));
    write_w8(AVI, (BYTE) (val >> 24));
}

static void write_wb32(avi_Context* AVI, uint32_t val)
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

static void write_wb64(avi_Context* AVI, uint64_t val)
{
    write_wb32(AVI, (uint32_t)(val >> 32));
    write_wb32(AVI, (uint32_t)(val & 0xffffffff));
}

#if BIGENDIAN
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

static void write_4cc(avi_Context* AVI, const char *str)
{
    int len = 4;
    if(strlen(str) < len )
	{
		len = strlen(str);
	}

    write_buf(AVI, (BYTE *) str, len);

    len = 4 - len;
    //fill remaining chars with spaces
    while(len > 0)
    {
		write_w8(AVI, ' ');
		len--;
	}
}

static int write_str(avi_Context* AVI, const char *str)
{
    int len = 1;
    if (str) {
        len += strlen(str);
        write_buf(AVI, (BYTE *) str, len);
    } else
        write_w8(AVI, 0);
    return len;
}

int64_t avi_open_tag (avi_Context* AVI, const char *tag)
{
	write_4cc(AVI, tag);
	write_wl32(AVI, 0);
	return avi_get_offset(AVI);
}

static void avi_close_tag(avi_Context* AVI, int64_t start_pos)
{
	int64_t current_offset = avi_get_offset(AVI);
	int32_t size = (int32_t) (current_offset - start_pos);
	avi_seek(AVI, start_pos-4);
	write_wl32(AVI, size);
	avi_seek(AVI, current_offset);

	fprintf(stderr, "AVI:(0x%llx) closing tag at 0x%llx with size 0x%llx\n",current_offset, start_pos-4, size);

}

/* create a new writer for filename*/
static avi_Writer* avi_create_writer(const char *filename)
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

static void avi_destroy_writer(avi_Context* AVI)
{
	//clean up
	//flush the buffer to file
	flush_buffer(AVI);

	//flush the file buffer
	fflush(AVI->writer->fp);
	//close the file pointer
	fclose(AVI->writer->fp);

	free(AVI->writer->buffer);
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

avi_Stream* get_first_audio_stream(avi_Context* AVI)
{
	avi_Stream* stream = AVI->stream_list;

	if(!stream)
		return NULL;

	if(stream->type == STREAM_TYPE_AUDIO)
		return stream;

	while(stream->next != NULL)
	{
		stream = stream->next;

		if(stream->type == STREAM_TYPE_AUDIO)
			return stream;
	}

	return NULL;
}

void avi_put_main_header(avi_Context* AVI, avi_RIFF* riff)
{
	AVI->fps = get_first_video_stream(AVI)->fps;
	int width = get_first_video_stream(AVI)->width;
	int height = get_first_video_stream(AVI)->height;
	int time_base_num = AVI->time_base_num;
	int time_base_den = AVI->time_base_den;
	
	uint32_t data_rate = 0; 
	if(time_base_den > 0 || time_base_num > 0) //these are not set yet so it's always false
		data_rate = (uint32_t) (INT64_C(1000000) * time_base_num/time_base_den);
	else
		fprintf(stderr, "AVI: bad time base (%i/%i)", time_base_num, time_base_den);
	
	/*do not force index yet -only when closing*/
	/*this should prevent bad avi files even if it is not closed properly*/
	//if(hasIndex) flag |= AVIF_HASINDEX;
	//if(hasIndex && AVI->must_use_index) flag |= AVIF_MUSTUSEINDEX;
	uint32_t flags = AVIF_WASCAPTUREFILE;

	int64_t avih = avi_open_tag(AVI, "avih"); // main avi header
	//AVI->time_delay_off = avi_get_offset(AVI);
	write_wl32(AVI, 1000000 / FRAME_RATE_SCALE); // time delay between frames (milisec)
	write_wl32(AVI, data_rate);         // data rate
	write_wl32(AVI, 0);                 // Padding multiple size (2048)
	write_wl32(AVI, flags);			    // parameter Flags
	riff->frames_hdr_all = avi_get_offset(AVI);
	write_wl32(AVI, 0);			        // number of video frames
	write_wl32(AVI, 0);			        // number of preview frames
	write_wl32(AVI, AVI->stream_list_size); // number of data streams (audio + video)*/
	write_wl32(AVI, 1024*1024);         // suggested playback buffer size (bytes)
	write_wl32(AVI, width);		        // width
	write_wl32(AVI, height);	    	// height
	write_wl32(AVI, 0);                 // time scale:  unit used to measure time (30)
	write_wl32(AVI, 0);			        // data rate (frame rate * time scale)
	write_wl32(AVI, 0);			        // start time (0)
	write_wl32(AVI, 0);			        // size of AVI data chunk (in scale units)
	avi_close_tag(AVI, avih); //write the chunk size
}

int64_t avi_put_bmp_header(avi_Context* AVI, avi_Stream* stream)
{
	int frate = 15*FRAME_RATE_SCALE;
	if(stream->fps > 0.001)
		frate = (int) (FRAME_RATE_SCALE*(stream->fps + 0.5));

	int64_t strh = avi_open_tag(AVI, "strh");// video stream header
	write_4cc(AVI, "vids");              // stream type
	write_4cc(AVI, stream->compressor);  // Handler (VIDEO CODEC)
	write_wl32(AVI, 0);                  // Flags
	write_wl16(AVI, 0);                  // stream priority
	write_wl16(AVI, 0);                  // language tag
	write_wl32(AVI, 0);                  // initial frames
	write_wl32(AVI, FRAME_RATE_SCALE);   // Scale
	stream->rate_hdr_strm = avi_get_offset(AVI); //store this to set proper fps
	write_wl32(AVI, frate);              // Rate: Rate/Scale == sample/second (fps) */
	write_wl32(AVI, 0);                  // start time
	stream->frames_hdr_strm = avi_get_offset(AVI);
	write_wl32(AVI, 0);                  // lenght of stream
	write_wl32(AVI, 1024*1024);          // suggested playback buffer size
	write_wl32(AVI, -1);                 // Quality
	write_wl32(AVI, 0);                  // SampleSize
	write_wl16(AVI, 0);                  // rFrame (left)
	write_wl16(AVI, 0);                  // rFrame (top)
	write_wl16(AVI, stream->width);                  // rFrame (right)
	write_wl16(AVI, stream->height);                  // rFrame (bottom)
	avi_close_tag(AVI, strh); //write the chunk size

	return strh;
}

int64_t avi_put_wav_header(avi_Context* AVI, avi_Stream* stream)
{
	int sampsize = avi_audio_sample_size(stream);

	int64_t strh = avi_open_tag(AVI, "strh");// audio stream header
	write_4cc(AVI, "auds");
	write_wl32(AVI, 1);                  // codec tag on strf
	write_wl32(AVI, 0);                  // Flags
	write_wl16(AVI, 0);                  // stream priority
	write_wl16(AVI, 0);                  // language tag
	write_wl32(AVI, 0);                  // initial frames
	stream->rate_hdr_strm = avi_get_offset(AVI);
	write_wl32(AVI, sampsize/4);         // Scale
	write_wl32(AVI, stream->mpgrate/8);   // Rate: Rate/Scale == sample/second (fps) */
	write_wl32(AVI, 0);                  // start time
	stream->frames_hdr_strm = avi_get_offset(AVI);
	write_wl32(AVI, 0);                  // lenght of stream
	write_wl32(AVI, 12*1024);            // suggested playback buffer size
	write_wl32(AVI, -1);                 // Quality
	write_wl32(AVI, sampsize/4);         // SampleSize
	write_wl16(AVI, 0);                  // rFrame (left)
	write_wl16(AVI, 0);                  // rFrame (top)
	write_wl16(AVI, 0);                  // rFrame (right)
	write_wl16(AVI, 0);                  // rFrame (bottom)
	avi_close_tag(AVI, strh); //write the chunk size

	return strh;
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
	write_wl32(AVI, stream->width*stream->height*3);// image size (in bytes?)
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

	int sampsize = avi_audio_sample_size(stream);

	int64_t strf = avi_open_tag(AVI, "strf");// audio stream format
	write_wl16(AVI, stream->a_fmt);    // Format (codec) tag
	write_wl16(AVI, stream->a_chans);  // Number of channels
	write_wl32(AVI, stream->a_rate);   // SamplesPerSec
	write_wl32(AVI, stream->mpgrate/8);// Average Bytes per sec
	write_wl16(AVI, sampsize/4);       // BlockAlign
	write_wl16(AVI, stream->a_bits);   //BitsPerSample
	write_wl16(AVI, axd_size);         //size of extra data
	// write extradata (codec private)
	if (axd_size > 0 && stream->extra_data)
	{
		write_buf(AVI, stream->extra_data, axd_size);
		if (axd_size != axd_size_align)
		{
			write_w8(AVI, 0);  //align
		}
	}
	avi_close_tag(AVI, strf); //write the chunk size
}

void avi_put_vproperties_header(avi_Context* AVI, avi_Stream* stream)
{
	uint32_t refresh_rate =  (uint32_t) lrintf(2.0 * AVI->fps);
	if(AVI->time_base_den  > 0 || AVI->time_base_num > 0) //these are not set yet so it's always false
	{
		double time_base = AVI->time_base_num / (double) AVI->time_base_den;
		refresh_rate = lrintf(1.0/time_base);
	}
	int vprp= avi_open_tag(AVI, "vprp");
    write_wl32(AVI, 0); //video format  = unknown
    write_wl32(AVI, 0); //video standard= unknown
    write_wl32(AVI, refresh_rate); // dwVerticalRefreshRate
    write_wl32(AVI, stream->width ); //horizontal pixels
    write_wl32(AVI, stream->height); //vertical lines
    write_wl16(AVI, stream->height);  //Active Frame Aspect Ratio (4:3 - 16:9)
    write_wl16(AVI, stream->width); //Active Frame Aspect Ratio
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

		riff->movi_list = off; //update movi list pos for this riff
	}

	return off;
}

//only for riff id = 0
void avi_create_riff_header(avi_Context* AVI, avi_RIFF* riff)
{
	int64_t list1 = avi_create_riff_tags(AVI, riff);

	avi_put_main_header(AVI, riff);

	int i, j = 0;

	for(j=0; j< AVI->stream_list_size; j++)
	{
		avi_Stream* stream = avi_get_stream(AVI, j);

		int64_t list2 = avi_open_tag(AVI, "LIST");
		write_4cc(AVI,"strl");              //stream list

		if(stream->type == STREAM_TYPE_VIDEO)
		{
			avi_put_bmp_header(AVI, stream);
			avi_put_vstream_format_header(AVI, stream);

			// write extradata (codec private)
			if (stream->extra_data_size > 0 && stream->extra_data)
			{
				int size_align = (stream->extra_data_size+1) & ~1;
				write_4cc(AVI,"strd");
				write_wl32(AVI, size_align);
				write_buf(AVI, stream->extra_data, stream->extra_data_size);
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

	AVI->odml_list = avi_open_tag(AVI, "JUNK");
    write_4cc(AVI, "odml");
    write_4cc(AVI, "dmlh");
    write_wl32(AVI, 248);
    for (i = 0; i < 248; i+= 4)
        write_wl32(AVI, 0);
    avi_close_tag(AVI, AVI->odml_list);

	avi_close_tag(AVI, list1); //write the chunk size

	/* some padding for easier tag editing */
    int64_t list3 = avi_open_tag(AVI, "JUNK");
    for (i = 0; i < 1016; i += 4)
        write_wl32(AVI, 0);
    avi_close_tag(AVI, list3); //write the chunk size

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
					int32_t width,
					int32_t height,
					double fps,
					int32_t codec_id,
					char* compressor)
{
	avi_Stream* stream = avi_add_new_stream(AVI);
	stream->type = STREAM_TYPE_VIDEO;
	stream->fps = fps;
	stream->width = width;
	stream->height = height;
	stream->codec_id = codec_id;

	if(compressor)
		strncpy(stream->compressor, compressor, 8);

	return stream;
}

//third function to get called (add audio stream to avi_Context)
avi_Stream*
avi_add_audio_stream(avi_Context *AVI,
					int32_t   channels,
					int32_t   rate,
					int32_t   bits,
					int32_t   mpgrate,
					int32_t   codec_id,
					int32_t   format)
{
	avi_Stream* stream = avi_add_new_stream(AVI);
	stream->type = STREAM_TYPE_AUDIO;

	stream->a_rate = rate;
	stream->a_bits = bits;
	stream->mpgrate = mpgrate;
	stream->a_vbr = 0;
	stream->codec_id = codec_id;
	stream->a_fmt = format;

	return stream;
}

/*
   first function to get called

   avi_create_context:  Open an AVI File and write a bunch
                        of zero bytes as space for the header.
                        Creates a mutex.

   returns a pointer to avi_Context on success, a NULL pointer on error
*/
avi_Context* avi_create_context(const char * filename)
{
	avi_Context* AVI = g_new0(avi_Context, 1);

	if(AVI == NULL)
		return NULL;

	AVI->writer = avi_create_writer(filename);

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

static int avi_write_counters(avi_Context* AVI, avi_RIFF* riff)
{
    int n, nb_frames = 0;
    flush_buffer(AVI);
    
	int time_base_num = AVI->time_base_num;
	int time_base_den = AVI->time_base_den;
	
    int64_t file_size = avi_get_offset(AVI);//avi_tell(AVI);
    fprintf(stderr, "AVI: file size = %llu\n", file_size);

    for(n = 0; n < AVI->stream_list_size; n++)
    {
        avi_Stream *stream = avi_get_stream(AVI, n);
		
		if(stream->rate_hdr_strm <= 0)
        {
			fprintf(stderr, "AVI: stream rate header pos not valid\n");
		}
		else
		{
			avi_seek(AVI, stream->rate_hdr_strm);
		
			if(stream->type == STREAM_TYPE_VIDEO && AVI->fps > 0.001)
			{
				uint32_t rate =(uint32_t) FRAME_RATE_SCALE * lrintf(AVI->fps + 0.5);
				fprintf(stderr,"AVI: storing rate(%i)\n",rate);
				write_wl32(AVI, rate);
			}
		}
		
        if(stream->frames_hdr_strm <= 0)
        {
			fprintf(stderr, "AVI: stream frames header pos not valid\n");
		}
		else
		{
			avi_seek(AVI, stream->frames_hdr_strm);

			if(stream->type == STREAM_TYPE_VIDEO)
			{
				write_wl32(AVI, stream->packet_count);
				nb_frames = MAX(nb_frames, stream->packet_count);
			}
			else
			{
				int sampsize = avi_audio_sample_size(stream);
				write_wl32(AVI, 4*stream->audio_strm_length/sampsize);
			}		
		}
    }
    if(riff->id == 0)
    {
        if(riff->frames_hdr_all <= 0)
        {
			fprintf(stderr, "AVI: riff frames header pos not valid\n");
        }
        else
        {
			avi_seek(AVI, riff->frames_hdr_all);
			write_wl32(AVI, nb_frames);
		}
    }
/*
    //update frame time delay
    if(AVI->time_delay_off <= 0)
    {
		fprintf(stderr, "AVI: avi frame time pos not valid\n");
    }
    else
    {
		avi_seek(AVI, AVI->time_delay_off);
		uint32_t us_per_frame = 0;
		if(AVI->fps > 0.001)
			us_per_frame=(uint32_t) lrintf(1000000.0 / AVI->fps + 0.5);
		write_wl32(AVI, us_per_frame);
		fprintf(stderr, "AVI: frame time (%i us)\n", us_per_frame);
		
		uint32_t data_rate = 0; 
		if(time_base_den > 0 || time_base_num > 0) //these are not set yet so it's always false
			data_rate = (uint32_t) (INT64_C(1000000) * time_base_num/time_base_den);
		else
			fprintf(stderr, "AVI: bad time base (%i/%i)\n", time_base_num, time_base_den);
		write_wl32(AVI, data_rate);
	}
*/
	//return to position (EOF)
    avi_seek(AVI, file_size);

    return 0;
}

static int avi_write_ix(avi_Context* AVI)
{
    char tag[5];
    char ix_tag[] = "ix00";
    int i, j;

	avi_RIFF *riff = avi_get_last_riff(AVI);

    if (riff->id >= AVI_MASTER_INDEX_SIZE)
        return -1;

    for (i=0;i<AVI->stream_list_size;i++)
    {
        avi_Stream *avist = avi_get_stream(AVI, i);
        int64_t ix, pos;

        avi_stream2fourcc(tag, avist);

        ix_tag[3] = '0' + i;

        /* Writing AVI OpenDML leaf index chunk */
        ix = avi_get_offset(AVI);
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
         pos = avi_get_offset(AVI); //current position

         /* Updating one entry in the AVI OpenDML master index */
         avi_seek(AVI, avist->indexes.indx_start - 8);
         write_4cc(AVI, "indx");            /* enabling this entry */
         avi_skip(AVI, 8);
         write_wl32(AVI, riff->id +1);         /* nEntriesInUse */
         avi_skip(AVI, 16*(riff->id +1));
         write_wl64(AVI, ix);                   /* qwOffset */
         write_wl32(AVI, pos - ix);             /* dwSize */
         write_wl32(AVI, avist->indexes.entry); /* dwDuration */

		//return to position
         avi_seek(AVI, pos);
    }
    return 0;
}

static int avi_write_idx1(avi_Context* AVI, avi_RIFF *riff)
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
	fprintf(stderr, "AVI: wrote idx\n");
    avi_write_counters(AVI, riff);

    return 0;
}

int avi_write_packet(avi_Context* AVI, int stream_index, BYTE *data, uint32_t size, int64_t dts, int block_align, int32_t flags)
{
    char tag[5];
    unsigned int i_flags=0;

    avi_Stream *avist= avi_get_stream(AVI, stream_index);

	avi_RIFF* riff = avi_get_last_riff(AVI);
	//align
    while(block_align==0 && dts != AV_NOPTS_VALUE && dts > avist->packet_count)
        avi_write_packet(AVI, stream_index, NULL, 0, AV_NOPTS_VALUE, 0, 0);

    avist->packet_count++;

    // Make sure to put an OpenDML chunk when the file size exceeds the limits
    if (avi_get_offset(AVI) - riff->riff_start > AVI_MAX_RIFF_SIZE)
    {
        avi_write_ix(AVI);
        avi_close_tag(AVI, riff->movi_list);

        if (riff->id == 0)
            avi_write_idx1(AVI, riff);

        avi_close_tag(AVI, riff->riff_start);

        avi_add_new_riff(AVI);
    }

    avi_stream2fourcc(tag, avist);

    if(flags & AV_PKT_FLAG_KEY) //key frame
        i_flags = 0x10;

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
        idx->cluster[cl] = av_malloc(AVI_INDEX_CLUSTER_SIZE*sizeof(avi_Ientry));
        if (!idx->cluster[cl])
            return -1;
        idx->ents_allocated += AVI_INDEX_CLUSTER_SIZE;
    }

    idx->cluster[cl][id].flags = i_flags;
    idx->cluster[cl][id].pos = avi_get_offset(AVI) - riff->movi_list;
    idx->cluster[cl][id].len = size;
    idx->entry++;


    write_4cc(AVI, tag);
    write_wl32(AVI, size);
    write_buf(AVI, data, size);
    if (size & 1)
        write_w8(AVI, 0);

    flush_buffer(AVI);

    return 0;
}

int avi_close(avi_Context* AVI)
{
    int res = 0;
    int i, j, n, nb_frames;
    int64_t file_size;

    avi_RIFF* riff = avi_get_last_riff(AVI);

    if (riff->id == 0)
    {
        avi_close_tag(AVI, riff->movi_list);
        fprintf(stderr, "AVI: (0x%llx) close movi tag\n",avi_get_offset(AVI));
        res = avi_write_idx1(AVI, riff);
        avi_close_tag(AVI, riff->riff_start);
    }
    else
    {
        avi_write_ix(AVI);
        avi_close_tag(AVI, riff->movi_list);
        avi_close_tag(AVI, riff->riff_start);

        file_size = avi_get_offset(AVI);
        avi_seek(AVI, AVI->odml_list - 8);
        write_4cc(AVI, "LIST"); /* Making this AVI OpenDML one */
        avi_skip(AVI, 16);

        for (n=nb_frames=0;n<AVI->stream_list_size;n++)
        {
            avi_Stream *stream = avi_get_stream(AVI, n);

            if (stream->type == STREAM_TYPE_VIDEO)
            {
                if (nb_frames < stream->packet_count)
                        nb_frames = stream->packet_count;
            }
            else
            {
                if (stream->codec_id == CODEC_ID_MP2 || stream->codec_id == CODEC_ID_MP3)
                        nb_frames += stream->packet_count;
            }
        }
        write_wl32(AVI, nb_frames);
        avi_seek(AVI, file_size);

        avi_write_counters(AVI, riff);
    }


    for (i=0; i<AVI->stream_list_size; i++)
    {
         avi_Stream *stream = avi_get_stream(AVI, i);

         for (j=0; j<stream->indexes.ents_allocated/AVI_INDEX_CLUSTER_SIZE; j++)
              av_free(stream->indexes.cluster[j]);
         av_freep(&stream->indexes.cluster);
         stream->indexes.ents_allocated = stream->indexes.entry = 0;
    }

    return res;
}
