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
#include "file_io.h"

/*******************************************************************
 *                                                                 *
 *    Utilities for writing an AVI File                            *
 *    based for the most part in the avi encoder from libavformat  *
 *******************************************************************/

#ifndef O_BINARY
/* win32 wants a binary flag to open(); this sets it to null
   on platforms that don't have it. */
#define O_BINARY 0
#endif

#define INFO_LIST

//#define MAX_INFO_STRLEN 64
//static char id_str[MAX_INFO_STRLEN];

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

#define AVI_MAX_RIFF_SIZE       0x40000000LL    /*1Gb = 0x40000000LL*/
#define AVI_MASTER_INDEX_SIZE   256
#define AVI_MAX_STREAM_COUNT    10

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


int64_t avi_open_tag (avi_Context* AVI, const char *tag)
{
	io_write_4cc(AVI->writer, tag);
	io_write_wl32(AVI->writer, 0);
	return io_get_offset(AVI->writer);
}

static void avi_close_tag(avi_Context* AVI, int64_t start_pos)
{
	int64_t current_offset = io_get_offset(AVI->writer);
	int32_t size = (int32_t) (current_offset - start_pos);
	io_seek(AVI->writer, start_pos-4);
	io_write_wl32(AVI->writer, size);
	io_seek(AVI->writer, current_offset);

	fprintf(stderr, "AVI:(%" PRIu64 ") closing tag at %" PRIu64 " with size %i\n",current_offset, start_pos-4, size);

}

/* Calculate audio sample size from number of bits and number of channels.
   This may have to be adjusted for eg. 12 bits and stereo */

static int avi_audio_sample_size(io_Stream* stream)
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

static char* avi_stream2fourcc(char* tag, io_Stream* stream)
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

void avi_put_main_header(avi_Context* AVI, avi_RIFF* riff)
{
	AVI->fps = get_first_video_stream(AVI->stream_list)->fps;
	int width = get_first_video_stream(AVI->stream_list)->width;
	int height = get_first_video_stream(AVI->stream_list)->height;
	int time_base_num = AVI->time_base_num;
	int time_base_den = AVI->time_base_den;

	uint32_t data_rate = 0;
	if(time_base_den > 0 || time_base_num > 0) //these are not set yet so it's always false
		data_rate = (uint32_t) (INT64_C(1000000) * time_base_num/time_base_den);
	else
		fprintf(stderr, "AVI: bad time base (%i/%i): set it later", time_base_num, time_base_den);

	/*do not force index yet -only when closing*/
	/*this should prevent bad avi files even if it is not closed properly*/
	//if(hasIndex) flag |= AVIF_HASINDEX;
	//if(hasIndex && AVI->must_use_index) flag |= AVIF_MUSTUSEINDEX;
	AVI->avi_flags = AVIF_WASCAPTUREFILE;

	int64_t avih = avi_open_tag(AVI, "avih");      // main avi header
	riff->time_delay_off = io_get_offset(AVI->writer);
	io_write_wl32(AVI->writer, 1000000 / FRAME_RATE_SCALE); // time per frame (milisec)
	io_write_wl32(AVI->writer, data_rate);         // data rate
	io_write_wl32(AVI->writer, 0);                 // Padding multiple size (2048)
	io_write_wl32(AVI->writer, AVI->avi_flags);    // parameter Flags
	//riff->frames_hdr_all = io_get_offset(AVI->writer);
	io_write_wl32(AVI->writer, 0);			       // number of video frames
	io_write_wl32(AVI->writer, 0);			       // number of preview frames
	io_write_wl32(AVI->writer, AVI->stream_list_size); // number of data streams (audio + video)*/
	io_write_wl32(AVI->writer, 1024*1024);         // suggested playback buffer size (bytes)
	io_write_wl32(AVI->writer, width);		       // width
	io_write_wl32(AVI->writer, height);	    	   // height
	io_write_wl32(AVI->writer, 0);                 // time scale:  unit used to measure time (30)
	io_write_wl32(AVI->writer, 0);			       // data rate (frame rate * time scale)
	io_write_wl32(AVI->writer, 0);			       // start time (0)
	io_write_wl32(AVI->writer, 0);			       // size of AVI data chunk (in scale units)
	avi_close_tag(AVI, avih);     //write the chunk size
}

int64_t avi_put_bmp_header(avi_Context* AVI, io_Stream* stream)
{
	int frate = 15*FRAME_RATE_SCALE;
	if(stream->fps > 0.001)
		frate = (int) ((FRAME_RATE_SCALE*(stream->fps)) + 0.5);

	int64_t strh = avi_open_tag(AVI, "strh");// video stream header
	io_write_4cc(AVI->writer, "vids");              // stream type
	io_write_4cc(AVI->writer, stream->compressor);  // Handler (VIDEO CODEC)
	io_write_wl32(AVI->writer, 0);                  // Flags
	io_write_wl16(AVI->writer, 0);                  // stream priority
	io_write_wl16(AVI->writer, 0);                  // language tag
	io_write_wl32(AVI->writer, 0);                  // initial frames
	io_write_wl32(AVI->writer, FRAME_RATE_SCALE);   // Scale
	stream->rate_hdr_strm = io_get_offset(AVI->writer); //store this to set proper fps
	io_write_wl32(AVI->writer, frate);              // Rate: Rate/Scale == sample/second (fps) */
	io_write_wl32(AVI->writer, 0);                  // start time
	stream->frames_hdr_strm = io_get_offset(AVI->writer);
	io_write_wl32(AVI->writer, 0);                  // lenght of stream
	io_write_wl32(AVI->writer, 1024*1024);          // suggested playback buffer size
	io_write_wl32(AVI->writer, -1);                 // Quality
	io_write_wl32(AVI->writer, 0);                  // SampleSize
	io_write_wl16(AVI->writer, 0);                  // rFrame (left)
	io_write_wl16(AVI->writer, 0);                  // rFrame (top)
	io_write_wl16(AVI->writer, stream->width);      // rFrame (right)
	io_write_wl16(AVI->writer, stream->height);     // rFrame (bottom)
	avi_close_tag(AVI, strh); //write the chunk size

	return strh;
}

int64_t avi_put_wav_header(avi_Context* AVI, io_Stream* stream)
{
	int sampsize = avi_audio_sample_size(stream);

	int64_t strh = avi_open_tag(AVI, "strh");// audio stream header
	io_write_4cc(AVI->writer, "auds");
	io_write_wl32(AVI->writer, 1);                  // codec tag on strf
	io_write_wl32(AVI->writer, 0);                  // Flags
	io_write_wl16(AVI->writer, 0);                  // stream priority
	io_write_wl16(AVI->writer, 0);                  // language tag
	io_write_wl32(AVI->writer, 0);                  // initial frames
	stream->rate_hdr_strm = io_get_offset(AVI->writer);
	io_write_wl32(AVI->writer, sampsize/4);         // Scale
	io_write_wl32(AVI->writer, stream->mpgrate/8);  // Rate: Rate/Scale == sample/second (fps) */
	io_write_wl32(AVI->writer, 0);                  // start time
	stream->frames_hdr_strm = io_get_offset(AVI->writer);
	io_write_wl32(AVI->writer, 0);                  // lenght of stream
	io_write_wl32(AVI->writer, 12*1024);            // suggested playback buffer size
	io_write_wl32(AVI->writer, -1);                 // Quality
	io_write_wl32(AVI->writer, sampsize/4);         // SampleSize
	io_write_wl16(AVI->writer, 0);                  // rFrame (left)
	io_write_wl16(AVI->writer, 0);                  // rFrame (top)
	io_write_wl16(AVI->writer, 0);                  // rFrame (right)
	io_write_wl16(AVI->writer, 0);                  // rFrame (bottom)
	avi_close_tag(AVI, strh); //write the chunk size

	return strh;
}

void avi_put_vstream_format_header(avi_Context* AVI, io_Stream* stream)
{
	int vxd_size        = stream->extra_data_size;
	int vxd_size_align  = (stream->extra_data_size+1) & ~1;

	int64_t strf = avi_open_tag(AVI, "strf");   // stream format header
	io_write_wl32(AVI->writer, 40 + vxd_size);  // sruct Size
	io_write_wl32(AVI->writer, stream->width);  // Width
	io_write_wl32(AVI->writer, stream->height); // Height
	io_write_wl16(AVI->writer, 1);              // Planes
	io_write_wl16(AVI->writer, 24);             // Count - bitsperpixel - 1,4,8 or 24  32
	if(strncmp(stream->compressor,"DIB",3)==0)
		io_write_wl32(AVI->writer, 0);          // Compression
	else
		io_write_4cc(AVI->writer, stream->compressor);
	io_write_wl32(AVI->writer, stream->width*stream->height*3);// image size (in bytes?)
	io_write_wl32(AVI->writer, 0);              // XPelsPerMeter
	io_write_wl32(AVI->writer, 0);              // YPelsPerMeter
	io_write_wl32(AVI->writer, 0);              // ClrUsed: Number of colors used
	io_write_wl32(AVI->writer, 0);              // ClrImportant: Number of colors important
	// write extradata (codec private)
	if (vxd_size > 0 && stream->extra_data)
	{
		io_write_buf(AVI->writer, stream->extra_data, vxd_size);
		if (vxd_size != vxd_size_align)
		{
			io_write_w8(AVI->writer, 0);  //align
		}
	}
	avi_close_tag(AVI, strf); //write the chunk size
}

void avi_put_astream_format_header(avi_Context* AVI, io_Stream* stream)
{
	int axd_size        = stream->extra_data_size;
	int axd_size_align  = (stream->extra_data_size+1) & ~1;

	int sampsize = avi_audio_sample_size(stream);

	int64_t strf = avi_open_tag(AVI, "strf");// audio stream format
	io_write_wl16(AVI->writer, stream->a_fmt);    // Format (codec) tag
	io_write_wl16(AVI->writer, stream->a_chans);  // Number of channels
	io_write_wl32(AVI->writer, stream->a_rate);   // SamplesPerSec
	io_write_wl32(AVI->writer, stream->mpgrate/8);// Average Bytes per sec
	io_write_wl16(AVI->writer, sampsize/4);       // BlockAlign
	io_write_wl16(AVI->writer, stream->a_bits);   //BitsPerSample
	io_write_wl16(AVI->writer, axd_size);         //size of extra data
	// write extradata (codec private)
	if (axd_size > 0 && stream->extra_data)
	{
		io_write_buf(AVI->writer, stream->extra_data, axd_size);
		if (axd_size != axd_size_align)
		{
			io_write_w8(AVI->writer, 0);  //align
		}
	}
	avi_close_tag(AVI, strf); //write the chunk size
}

void avi_put_vproperties_header(avi_Context* AVI, io_Stream* stream)
{
	uint32_t refresh_rate =  (uint32_t) lrintf(2.0 * AVI->fps);
	if(AVI->time_base_den  > 0 || AVI->time_base_num > 0) //these are not set yet so it's always false
	{
		double time_base = AVI->time_base_num / (double) AVI->time_base_den;
		refresh_rate = lrintf(1.0/time_base);
	}
	int vprp= avi_open_tag(AVI, "vprp");
    io_write_wl32(AVI->writer, 0);              //video format  = unknown
    io_write_wl32(AVI->writer, 0);              //video standard= unknown
    io_write_wl32(AVI->writer, refresh_rate);   // dwVerticalRefreshRate
    io_write_wl32(AVI->writer, stream->width ); //horizontal pixels
    io_write_wl32(AVI->writer, stream->height); //vertical lines
    io_write_wl16(AVI->writer, stream->height); //Active Frame Aspect Ratio (4:3 - 16:9)
    io_write_wl16(AVI->writer, stream->width);  //Active Frame Aspect Ratio
    io_write_wl32(AVI->writer, stream->width ); //Active Frame Height in Pixels
    io_write_wl32(AVI->writer, stream->height); //Active Frame Height in Lines
    io_write_wl32(AVI->writer, 1);              //progressive FIXME
	//Field Framing Information
    io_write_wl32(AVI->writer, stream->height);
    io_write_wl32(AVI->writer, stream->width );
    io_write_wl32(AVI->writer, stream->height);
    io_write_wl32(AVI->writer, stream->width );
    io_write_wl32(AVI->writer, 0);
    io_write_wl32(AVI->writer, 0);
    io_write_wl32(AVI->writer, 0);
    io_write_wl32(AVI->writer, 0);

    avi_close_tag(AVI, vprp);
}

int64_t avi_create_riff_tags(avi_Context* AVI, avi_RIFF* riff)
{
	int64_t off = 0;
	riff->riff_start = avi_open_tag(AVI, "RIFF");

	if(riff->id == 1)
	{
		io_write_4cc(AVI->writer, "AVI ");
		off = avi_open_tag(AVI, "LIST");
		io_write_4cc(AVI->writer, "hdrl");
	}
	else
	{
		io_write_4cc(AVI->writer, "AVIX");
		off = avi_open_tag(AVI, "LIST");
		io_write_4cc(AVI->writer, "movi");

		riff->movi_list = off; //update movi list pos for this riff
	}

	return off;
}

//only for riff id = 1
void avi_create_riff_header(avi_Context* AVI, avi_RIFF* riff)
{
	int64_t list1 = avi_create_riff_tags(AVI, riff);

	avi_put_main_header(AVI, riff);

	int i, j = 0;

	for(j=0; j< AVI->stream_list_size; j++)
	{
		io_Stream* stream = get_stream(AVI->stream_list, j);

		int64_t list2 = avi_open_tag(AVI, "LIST");
		io_write_4cc(AVI->writer,"strl");              //stream list

		if(stream->type == STREAM_TYPE_VIDEO)
		{
			avi_put_bmp_header(AVI, stream);
			avi_put_vstream_format_header(AVI, stream);
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
		avi_Index* indexes = (avi_Index*) stream->indexes;
		indexes->entry = indexes->ents_allocated = 0;
		indexes->indx_start = io_get_offset(AVI->writer);
		int64_t ix = avi_open_tag(AVI, "JUNK");           // ’ix##’
		io_write_wl16(AVI->writer, 4);               // wLongsPerEntry must be 4 (size of each entry in aIndex array)
		io_write_w8(AVI->writer, 0);                 // bIndexSubType must be 0 (frame index) or AVI_INDEX_2FIELD
		io_write_w8(AVI->writer, AVI_INDEX_OF_INDEXES);  // bIndexType (0 == AVI_INDEX_OF_INDEXES)
		io_write_wl32(AVI->writer, 0);               // nEntriesInUse (will fill out later on)
		io_write_4cc(AVI->writer, avi_stream2fourcc(tag, stream)); // dwChunkId
		io_write_wl32(AVI->writer, 0);               // dwReserved[3] must be 0
		io_write_wl32(AVI->writer, 0);
		io_write_wl32(AVI->writer, 0);
		for (i=0; i < AVI_MASTER_INDEX_SIZE; i++)
		{
			io_write_wl64(AVI->writer, 0);           // absolute file offset, offset 0 is unused entry
			io_write_wl32(AVI->writer, 0);           // dwSize - size of index chunk at this offset
			io_write_wl32(AVI->writer, 0);           // dwDuration - time span in stream ticks
		}
		avi_close_tag(AVI, ix); //write the chunk size

		if(stream->type == STREAM_TYPE_VIDEO)
			avi_put_vproperties_header(AVI, stream);

		avi_close_tag(AVI, list2); //write the chunk size
	}

	AVI->odml_list = avi_open_tag(AVI, "JUNK");
    io_write_4cc(AVI->writer, "odml");
    io_write_4cc(AVI->writer, "dmlh");
    io_write_wl32(AVI->writer, 248);
    for (i = 0; i < 248; i+= 4)
        io_write_wl32(AVI->writer, 0);
    avi_close_tag(AVI, AVI->odml_list);

	avi_close_tag(AVI, list1); //write the chunk size

	/* some padding for easier tag editing */
    int64_t list3 = avi_open_tag(AVI, "JUNK");
    for (i = 0; i < 1016; i += 4)
        io_write_wl32(AVI->writer, 0);
    avi_close_tag(AVI, list3); //write the chunk size

    riff->movi_list = avi_open_tag(AVI, "LIST");
    io_write_4cc(AVI->writer, "movi");
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

	int j = 1;

	while(riff->next != NULL && (j < index))
	{
		riff = riff->next;
		j++;
	}

	if(j != index)
		return NULL;

	return riff;
}

static void clean_indexes(avi_Context* AVI)
{
	int i=0, j=0;
	
	for (i=0; i<AVI->stream_list_size; i++)
    {
        io_Stream *stream = get_stream(AVI->stream_list, i);

		avi_Index* indexes = (avi_Index*) stream->indexes;
		for (j=0; j<indexes->ents_allocated/AVI_INDEX_CLUSTER_SIZE; j++)
             av_free(indexes->cluster[j]);
        av_freep(&indexes->cluster);
        indexes->ents_allocated = indexes->entry = 0;
    }
}

//call this after adding all the streams
avi_RIFF* avi_add_new_riff(avi_Context* AVI)
{
	avi_RIFF* riff = g_new0(avi_RIFF, 1);

	if(riff == NULL)
		return NULL;

	riff->next = NULL;
	riff->id = AVI->riff_list_size + 1;

	if(riff->id == 1)
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

	clean_indexes(AVI);
	
	fprintf(stderr, "AVI: adding new RIFF (%i)\n",riff->id);
	return riff;
}

//second function to get called (add video stream to avi_Context)
io_Stream*
avi_add_video_stream(avi_Context *AVI,
					int32_t width,
					int32_t height,
					double fps,
					int32_t codec_id,
					const char* compressor)
{
	io_Stream* stream = add_new_stream(&AVI->stream_list, &AVI->stream_list_size);
	stream->type = STREAM_TYPE_VIDEO;
	stream->fps = fps;
	stream->width = width;
	stream->height = height;
	stream->codec_id = codec_id;

	stream->indexes = (void *) g_new0(avi_Index, 1);

	if(compressor)
		strncpy(stream->compressor, compressor, 8);

	return stream;
}

//third function to get called (add audio stream to avi_Context)
io_Stream*
avi_add_audio_stream(avi_Context *AVI,
					int32_t   channels,
					int32_t   rate,
					int32_t   bits,
					int32_t   mpgrate,
					int32_t   codec_id,
					int32_t   format)
{
	io_Stream* stream = add_new_stream(&AVI->stream_list, &AVI->stream_list_size);
	stream->type = STREAM_TYPE_AUDIO;

	stream->a_rate = rate;
	stream->a_bits = bits;
	stream->mpgrate = mpgrate;
	stream->a_vbr = 0;
	stream->codec_id = codec_id;
	stream->a_fmt = format;

	stream->indexes = (void *) g_new0(avi_Index, 1);

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

	AVI->writer = io_create_writer(filename, 0);

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
	io_destroy_writer(AVI->writer);

	avi_RIFF* riff = avi_get_last_riff(AVI);
	while(riff->previous != NULL) //from end to start
	{
		avi_RIFF* prev_riff = riff->previous;
		free(riff);
		riff = prev_riff;
		AVI->riff_list_size--;
	}
	free(riff); //free the last one;

	destroy_stream_list(AVI->stream_list, &AVI->stream_list_size);

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
    io_flush_buffer(AVI->writer);

	//int time_base_num = AVI->time_base_num;
	//int time_base_den = AVI->time_base_den;

    int64_t file_size = io_get_offset(AVI->writer);//avi_tell(AVI);
    fprintf(stderr, "AVI: file size = %" PRIu64 "\n", file_size);

    for(n = 0; n < AVI->stream_list_size; n++)
    {
        io_Stream *stream = get_stream(AVI->stream_list, n);

		if(stream->rate_hdr_strm <= 0)
        {
			fprintf(stderr, "AVI: stream rate header pos not valid\n");
		}
		else
		{
			io_seek(AVI->writer, stream->rate_hdr_strm);

			if(stream->type == STREAM_TYPE_VIDEO && AVI->fps > 0.001)
			{
				uint32_t rate =(uint32_t) FRAME_RATE_SCALE * lrintf(AVI->fps);
				fprintf(stderr,"AVI: storing rate(%i)\n",rate);
				io_write_wl32(AVI->writer, rate);
			}
		}

        if(stream->frames_hdr_strm <= 0)
        {
			fprintf(stderr, "AVI: stream frames header pos not valid\n");
		}
		else
		{
			io_seek(AVI->writer, stream->frames_hdr_strm);

			if(stream->type == STREAM_TYPE_VIDEO)
			{
				io_write_wl32(AVI->writer, stream->packet_count);
				nb_frames = MAX(nb_frames, stream->packet_count);
			}
			else
			{
				int sampsize = avi_audio_sample_size(stream);
				io_write_wl32(AVI->writer, 4*stream->audio_strm_length/sampsize);
			}
		}
    }
    
    avi_RIFF* riff_1 = avi_get_riff(AVI, 1);
    if(riff_1->id == 1) /*should always be true*/
    {
        if(riff_1->time_delay_off <= 0)
        {
			fprintf(stderr, "AVI: riff main header pos not valid\n");
        }
        else
        {
			uint32_t us_per_frame = 1000; //us
			if(AVI->fps > 0.001)
				us_per_frame=(uint32_t) lrintf(1000000.0 / AVI->fps); 
		
			AVI->avi_flags |= AVIF_HASINDEX;
			
			io_seek(AVI->writer, riff_1->time_delay_off);
			io_write_wl32(AVI->writer, us_per_frame);      // time_per_frame
			io_write_wl32(AVI->writer, 0);                 // data rate
			io_write_wl32(AVI->writer, 0);                 // Padding multiple size (2048)
			io_write_wl32(AVI->writer, AVI->avi_flags);    // parameter Flags
			//io_seek(AVI->writer, riff_1->frames_hdr_all);
			io_write_wl32(AVI->writer, nb_frames);
		}
    }
    
	//return to position (EOF)
    io_seek(AVI->writer, file_size);

    return 0;
}

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
        io_Stream *stream = get_stream(AVI->stream_list, i);
        int64_t ix, pos;

        avi_stream2fourcc(tag, stream);

        ix_tag[3] = '0' + i; /*only 10 streams supported*/

        /* Writing AVI OpenDML leaf index chunk */
        ix = io_get_offset(AVI->writer);
        io_write_4cc(AVI->writer, ix_tag);     /* ix?? */
        avi_Index* indexes = (avi_Index *) stream->indexes;
        io_write_wl32(AVI->writer, indexes->entry * 8 + 24);
                                      /* chunk size */
        io_write_wl16(AVI->writer, 2);           /* wLongsPerEntry */
        io_write_w8(AVI->writer, 0);             /* bIndexSubType (0 == frame index) */
        io_write_w8(AVI->writer, AVI_INDEX_OF_CHUNKS); /* bIndexType (1 == AVI_INDEX_OF_CHUNKS) */
        io_write_wl32(AVI->writer, indexes->entry);
                                      /* nEntriesInUse */
        io_write_4cc(AVI->writer, tag);        /* dwChunkId */
        io_write_wl64(AVI->writer, riff->movi_list);/* qwBaseOffset */
        io_write_wl32(AVI->writer, 0);             /* dwReserved_3 (must be 0) */

        for (j=0; j< indexes->entry; j++)
        {
             avi_Ientry* ie = avi_get_ientry(indexes, j);
             io_write_wl32(AVI->writer, ie->pos + 8);
             io_write_wl32(AVI->writer, ((uint32_t)ie->len & ~0x80000000) |
                          (ie->flags & 0x10 ? 0 : 0x80000000));
         }
         io_flush_buffer(AVI->writer);
         pos = io_get_offset(AVI->writer); //current position
         fprintf(stderr,"AVI: wrote ix %s with %i entries\n",tag, indexes->entry);

         /* Updating one entry in the AVI OpenDML master index */
         io_seek(AVI->writer, indexes->indx_start);
         io_write_4cc(AVI->writer, "indx");            /* enabling this entry */
         io_skip(AVI->writer, 8);
         io_write_wl32(AVI->writer, riff->id);         /* nEntriesInUse */
         io_skip(AVI->writer, 16*(riff->id));
         io_write_wl64(AVI->writer, ix);               /* qwOffset */
         io_write_wl32(AVI->writer, pos - ix);         /* dwSize */
         io_write_wl32(AVI->writer, indexes->entry);   /* dwDuration */
		
		//return to position
         io_seek(AVI->writer, pos);
    }
    return 0;
}

static int avi_write_idx1(avi_Context* AVI, avi_RIFF *riff)
{

    int64_t idx_chunk;
    int i;
    char tag[5];


    io_Stream *stream;
    avi_Ientry* ie = 0, *tie;
    int empty, stream_id = -1;

    idx_chunk = avi_open_tag(AVI, "idx1");
    for (i=0;i<AVI->stream_list_size;i++)
    {
            stream = get_stream(AVI->stream_list, i);
            stream->entry=0;
    }

    do
    {
        empty = 1;
        for (i=0;i<AVI->stream_list_size;i++)
        {
			stream = get_stream(AVI->stream_list, i);
			avi_Index* indexes = (avi_Index*) stream->indexes;
            if (indexes->entry <= stream->entry)
                continue;

            tie = avi_get_ientry(indexes, stream->entry);
            if (empty || tie->pos < ie->pos)
            {
                ie = tie;
                stream_id = i;
            }
            empty = 0;
        }

        if (!empty)
        {
            stream = get_stream(AVI->stream_list, stream_id);
            avi_stream2fourcc(tag, stream);
            io_write_4cc(AVI->writer, tag);
            io_write_wl32(AVI->writer, ie->flags);
            io_write_wl32(AVI->writer, ie->pos);
            io_write_wl32(AVI->writer, ie->len);
            stream->entry++;
        }
    }
    while (!empty);

    avi_close_tag(AVI, idx_chunk);
	fprintf(stderr, "AVI: wrote idx1\n");
    avi_write_counters(AVI, riff);

    return 0;
}

int avi_write_packet(avi_Context* AVI, int stream_index, BYTE *data, uint32_t size, int64_t dts, int block_align, int32_t flags)
{
    char tag[5];
    unsigned int i_flags=0;

    io_Stream *stream= get_stream(AVI->stream_list, stream_index);

	avi_RIFF* riff = avi_get_last_riff(AVI);
	//align
    while(block_align==0 && dts != AV_NOPTS_VALUE && dts > stream->packet_count)
        avi_write_packet(AVI, stream_index, NULL, 0, AV_NOPTS_VALUE, 0, 0);

    stream->packet_count++;

    // Make sure to put an OpenDML chunk when the file size exceeds the limits
    if (io_get_offset(AVI->writer) - riff->riff_start > AVI_MAX_RIFF_SIZE)
    {
        avi_write_ix(AVI);
        avi_close_tag(AVI, riff->movi_list);

        if (riff->id == 1)
            avi_write_idx1(AVI, riff);

        avi_close_tag(AVI, riff->riff_start);

        avi_add_new_riff(AVI);
        
        riff = avi_get_last_riff(AVI); //update riff
    }

    avi_stream2fourcc(tag, stream);

    if(flags & AV_PKT_FLAG_KEY) //key frame
        i_flags = 0x10;

    if (stream->type == STREAM_TYPE_AUDIO)
       stream->audio_strm_length += size;


    avi_Index* idx = (avi_Index*) stream->indexes;
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
    idx->cluster[cl][id].pos = io_get_offset(AVI->writer) - riff->movi_list;
    idx->cluster[cl][id].len = size;
    idx->entry++;


    io_write_4cc(AVI->writer, tag);
    io_write_wl32(AVI->writer, size);
    io_write_buf(AVI->writer, data, size);
    if (size & 1)
        io_write_w8(AVI->writer, 0);

    io_flush_buffer(AVI->writer);

    return 0;
}

int avi_close(avi_Context* AVI)
{
    int res = 0;
    int n, nb_frames;
    int64_t file_size;

    avi_RIFF* riff = avi_get_last_riff(AVI);

    if (riff->id == 1)
    {
        avi_close_tag(AVI, riff->movi_list);
        fprintf(stderr, "AVI: (%" PRIu64 ") close movi tag\n",io_get_offset(AVI->writer));
        res = avi_write_idx1(AVI, riff);
        avi_close_tag(AVI, riff->riff_start);
    }
    else
    {
        avi_write_ix(AVI);
        avi_close_tag(AVI, riff->movi_list);
        avi_close_tag(AVI, riff->riff_start);

        file_size = io_get_offset(AVI->writer);
        io_seek(AVI->writer, AVI->odml_list - 8);
        io_write_4cc(AVI->writer, "LIST"); /* Making this AVI OpenDML one */
        io_skip(AVI->writer, 16);

        for (n=nb_frames=0;n<AVI->stream_list_size;n++)
        {
            io_Stream *stream = get_stream(AVI->stream_list, n);

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
        io_write_wl32(AVI->writer, nb_frames);
        io_seek(AVI->writer, file_size);

        avi_write_counters(AVI, riff);
    }

	clean_indexes(AVI);

    return res;
}
