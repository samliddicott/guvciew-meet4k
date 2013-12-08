/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#                                                                               #
# This is a heavily modified version of the matroska interface from x264        #
#           Copyright (C) 2005 Mike Matsnev                                     #
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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <unistd.h>
#include <math.h>

#include "ms_time.h"
#include "defs.h"
#include "video_format.h"
#include "matroska.h"
#include "acodecs.h"
#include "vcodecs.h"
#include "lavc_common.h"


/** 2 bytes * 3 for EBML IDs, 3 1-byte EBML lengths, 8 bytes for 64 bit
 * offset, 4 bytes for target EBML ID */
#define MAX_SEEKENTRY_SIZE 21

/** per-cuepoint-track - 3 1-byte EBML IDs, 3 1-byte EBML sizes, 2
 * 8-byte uint max */
#define MAX_CUETRACKPOS_SIZE 22

/** per-cuepoint - 2 1-byte EBML IDs, 2 1-byte EBML sizes, 8-byte uint max */
#define MAX_CUEPOINT_SIZE(num_tracks) 12 + MAX_CUETRACKPOS_SIZE*num_tracks


/** Some utilities for
 *  float and double conversion to/from int */
union mkv_union_intfloat32
{
	uint32_t i;
	float f;
};

union mkv_union_intfloat64
{
	uint64_t i;
	double f;
};

//static float mkv_int2float(uint32_t i)
//{
//	union mkv_union_intfloat32 v;
//	v.i = i;
//	return v.f;
//}

//static uint32_t mkv_float2int(float f)
//{
//	union mkv_union_intfloat32 v;
//	v.f = f;
//	return v.i;
//}

//static double mkv_int2double(uint64_t i)
//{
//	union mkv_union_intfloat64 v;
//	v.i = i;
//	return v.f;
//}

static uint64_t mkv_double2int(double f)
{
	union mkv_union_intfloat64 v;
	v.f = f;
	return v.i;
}


/** get id size */
static int ebml_id_size(unsigned int id)
{
	int bytes = 4, mask = 0x10;

	while (!(id & (mask << ((bytes - 1) * 8))) && bytes > 0)
	{
		mask <<= 1;
		bytes--;
	}

    return bytes;
}

/** write an id */
static void mkv_put_ebml_id(mkv_Context* MKV, unsigned int id)
{
    int i = ebml_id_size(id);
    while (i--)
        io_write_w8(MKV->writer, id >> (i*8));
}

/**
 * Write an EBML size meaning "unknown size".
 *
 * @param bytes The number of bytes the size should occupy (maximum: 8).
 */
static void mkv_put_ebml_size_unknown(mkv_Context* MKV, int bytes)
{
    if(bytes <= 8) //max is 64 bits
    {
		io_write_w8(MKV->writer, 0x1ff >> bytes);
		while (--bytes)
			io_write_w8(MKV->writer, 0xff);
	}
	else
		fprintf(stderr, "MKV: bad unknown size (%i > 8) bytes)\n", bytes);
}

/**
 * Calculate how many bytes are needed to represent a given number in EBML.
 */
static int ebml_num_size(uint64_t num)
{
    int bytes = 1;
    while ((num+1) >> bytes*7)
		bytes++;
    return bytes;
}

/**
 * Write a number in EBML variable length format.
 *
 * @param bytes The number of bytes that need to be used to write the number.
 *              If zero, any number of bytes can be used.
 */
static void mkv_put_ebml_num(mkv_Context* MKV, uint64_t num, int bytes)
{
    int i, needed_bytes = ebml_num_size(num);

    // sizes larger than this are currently undefined in EBML
    if(num >= (1ULL<<56)-1)
    {
		fprintf(stderr, "MKV: ebml number: %" PRIu64 "\n", num);
		return;
	}

    if (bytes == 0)
        // don't care how many bytes are used, so use the min
        bytes = needed_bytes;
    // the bytes needed to write the given size would exceed the bytes
    // that we need to use, so write unknown size. This shouldn't happen.
    if(bytes < needed_bytes)
    {
		fprintf(stderr, "MKV: bad requested size for ebml number: %" PRIu64 " (%i < %i)\n", num, bytes, needed_bytes);
		return;
	}

    num |= 1ULL << bytes*7;
    for (i = bytes - 1; i >= 0; i--)
        io_write_w8(MKV->writer, num >> i*8);
}

static void mkv_put_ebml_uint(mkv_Context* MKV, unsigned int elementid, uint64_t val)
{
    int i, bytes = 1;
    uint64_t tmp = val;
    while (tmp>>=8) bytes++;

    mkv_put_ebml_id(MKV, elementid);
    mkv_put_ebml_num(MKV, bytes, 0);
    for (i = bytes - 1; i >= 0; i--)
        io_write_w8(MKV->writer, val >> i*8);
}

static void mkv_put_ebml_float(mkv_Context* MKV, unsigned int elementid, double val)
{
    mkv_put_ebml_id(MKV, elementid);
    mkv_put_ebml_num(MKV, 8, 0);
    io_write_wb64(MKV->writer, mkv_double2int(val));
}

static void mkv_put_ebml_binary(mkv_Context* MKV, unsigned int elementid,
                            void *buf, int size)
{
    mkv_put_ebml_id(MKV, elementid);
    mkv_put_ebml_num(MKV, size, 0);
    io_write_buf(MKV->writer, buf, size);
}

static void mkv_put_ebml_string(mkv_Context* MKV, unsigned int elementid, char *str)
{
    mkv_put_ebml_binary(MKV, elementid, str, strlen(str));
}

/**
 * Write a void element of a given size. Useful for reserving space in
 * the file to be written to later.
 *
 * @param size The number of bytes to reserve, which must be at least 2.
 */
static void mkv_put_ebml_void(mkv_Context* MKV, uint64_t size)
{
    int64_t currentpos = io_get_offset(MKV->writer);

    if(size < 2)
    {
		fprintf(stderr, "MKV: wrong void size %" PRIu64 " < 2", size);
	}

    mkv_put_ebml_id(MKV, EBML_ID_VOID);
    // we need to subtract the length needed to store the size from the
    // size we need to reserve so 2 cases, we use 8 bytes to store the
    // size if possible, 1 byte otherwise
    if (size < 10)
        mkv_put_ebml_num(MKV, size-1, 0);
    else
        mkv_put_ebml_num(MKV, size-9, 8);
    while(io_get_offset(MKV->writer) < currentpos + size)
        io_write_w8(MKV->writer, 0);
}

static ebml_master mkv_start_ebml_master(mkv_Context* MKV, unsigned int elementid, uint64_t expectedsize)
{
	//if 0 reserve max (8 bytes)
    int bytes = expectedsize ? ebml_num_size(expectedsize) : 8;
    mkv_put_ebml_id(MKV, elementid);
    mkv_put_ebml_size_unknown(MKV, bytes);
    return (ebml_master){ io_get_offset(MKV->writer), bytes };
}

static void mkv_end_ebml_master(mkv_Context* MKV, ebml_master master)
{
    int64_t pos = io_get_offset(MKV->writer);

    if (io_seek(MKV->writer, master.pos - master.sizebytes) < 0)
        return;
    mkv_put_ebml_num(MKV, pos - master.pos, master.sizebytes);
	io_seek(MKV->writer, pos);
}

//static void mkv_put_xiph_size(mkv_Context* MKV, int size)
//{
//    int i;
//    for (i = 0; i < size / 255; i++)
//		io_write_w8(MKV->writer, 255);
//    io_write_w8(MKV->writer, size % 255);
//}

/**
 * Initialize a mkv_seekhead element to be ready to index level 1 Matroska
 * elements. If a maximum number of elements is specified, enough space
 * will be reserved at the current file location to write a seek head of
 * that size.
 *
 * @param segment_offset The absolute offset to the position in the file
 *                       where the segment begins.
 * @param numelements The maximum number of elements that will be indexed
 *                    by this seek head, 0 if unlimited.
 */
static mkv_seekhead * mkv_start_seekhead(mkv_Context* MKV, int64_t segment_offset, int numelements)
{
    mkv_seekhead *new_seekhead = g_new0(mkv_seekhead, 1);
    if (new_seekhead == NULL)
        return NULL;

    new_seekhead->segment_offset = segment_offset;

    if (numelements > 0)
    {
        new_seekhead->filepos = io_get_offset(MKV->writer);
        // 21 bytes max for a seek entry, 10 bytes max for the SeekHead ID
        // and size, and 3 bytes to guarantee that an EBML void element
        // will fit afterwards
        new_seekhead->reserved_size = numelements * MAX_SEEKENTRY_SIZE + 13;
        new_seekhead->max_entries = numelements;
        mkv_put_ebml_void(MKV, new_seekhead->reserved_size);
    }
    return new_seekhead;
}

static int mkv_add_seekhead_entry(mkv_seekhead *seekhead, unsigned int elementid, uint64_t filepos)
{
    mkv_seekhead_entry *entries = seekhead->entries;
	fprintf(stderr,"MKV: add seekhead entry %i (max %i)\n", seekhead->num_entries, seekhead->max_entries);
    // don't store more elements than we reserved space for
    if (seekhead->max_entries > 0 && seekhead->max_entries <= seekhead->num_entries)
        return -1;

	entries = g_renew(mkv_seekhead_entry, entries, seekhead->num_entries + 1);
    //entries = av_realloc(entries, (seekhead->num_entries + 1) * sizeof(mkv_seekhead_entry));
    if (entries == NULL)
    {
		fprintf(stderr, "MKV: couldn't allocate memory for seekhead entries\n");
        return -1;
	}
    entries[seekhead->num_entries].elementid = elementid;
    entries[seekhead->num_entries].segmentpos = filepos - seekhead->segment_offset;

	seekhead->num_entries++;

    seekhead->entries = entries;
    return 0;
}

/**
 * Write the seek head to the file and free it. If a maximum number of
 * elements was specified to mkv_start_seekhead(), the seek head will
 * be written at the location reserved for it. Otherwise, it is written
 * at the current location in the file.
 *
 * @return The file offset where the seekhead was written,
 * -1 if an error occurred.
 */
static int64_t mkv_write_seekhead(mkv_Context* MKV, mkv_seekhead *seekhead)
{
    ebml_master metaseek, seekentry;
    int64_t currentpos;
    int i;

    currentpos = io_get_offset(MKV->writer);

    if (seekhead->reserved_size > 0)
    {
        if (io_seek(MKV->writer, seekhead->filepos) < 0)
        {
			fprintf(stderr, "MKV: failed to write seekhead at pos %" PRIu64 "\n", seekhead->filepos);
            currentpos = -1;
            goto fail;
        }
    }

    metaseek = mkv_start_ebml_master(MKV, MATROSKA_ID_SEEKHEAD, seekhead->reserved_size);
    for (i = 0; i < seekhead->num_entries; i++)
    {
        mkv_seekhead_entry *entry = &seekhead->entries[i];

        seekentry = mkv_start_ebml_master(MKV, MATROSKA_ID_SEEKENTRY, MAX_SEEKENTRY_SIZE);

        mkv_put_ebml_id(MKV, MATROSKA_ID_SEEKID);
        mkv_put_ebml_num(MKV, ebml_id_size(entry->elementid), 0);
        mkv_put_ebml_id(MKV, entry->elementid);

        mkv_put_ebml_uint(MKV, MATROSKA_ID_SEEKPOSITION, entry->segmentpos);
        mkv_end_ebml_master(MKV, seekentry);
    }
    mkv_end_ebml_master(MKV, metaseek);

    if (seekhead->reserved_size > 0) {
        uint64_t remaining = seekhead->filepos + seekhead->reserved_size - io_get_offset(MKV->writer);
        mkv_put_ebml_void(MKV, remaining);
        io_seek(MKV->writer, currentpos);

        currentpos = seekhead->filepos;
    }
fail:
    g_free(seekhead->entries);
    g_free(seekhead);

    return currentpos;
}

static mkv_cues * mkv_start_cues(int64_t segment_offset)
{
    mkv_cues *cues = g_new0(mkv_cues, 1);
    if (cues == NULL)
        return NULL;

    cues->segment_offset = segment_offset;
    return cues;
}

static int mkv_add_cuepoint(mkv_cues *cues, int stream, int64_t ts, int64_t cluster_pos)
{
    mkv_cuepoint *entries = cues->entries;

    if (ts < 0)
        return 0;

	entries = g_renew(mkv_cuepoint, entries, cues->num_entries + 1);
    //entries = av_realloc(entries, (cues->num_entries + 1) * sizeof(mkv_cuepoint));
    if (entries == NULL)
        return AVERROR(ENOMEM);

    entries[cues->num_entries].pts = ts;
    entries[cues->num_entries].tracknum = stream + 1;
    entries[cues->num_entries].cluster_pos = cluster_pos - cues->segment_offset;

	cues->num_entries++;

    cues->entries = entries;
    return 0;
}

static int64_t mkv_write_cues(mkv_Context* MKV, mkv_cues *cues, int num_tracks)
{
    ebml_master cues_element;
    int64_t currentpos;
    int i, j;

    currentpos = io_get_offset(MKV->writer);
    cues_element = mkv_start_ebml_master(MKV, MATROSKA_ID_CUES, 0);

    for (i = 0; i < cues->num_entries; i++)
    {
        ebml_master cuepoint, track_positions;
        mkv_cuepoint *entry = &cues->entries[i];
        uint64_t pts = entry->pts;

        cuepoint = mkv_start_ebml_master(MKV, MATROSKA_ID_POINTENTRY, MAX_CUEPOINT_SIZE(num_tracks));
        mkv_put_ebml_uint(MKV, MATROSKA_ID_CUETIME, pts);

        // put all the entries from different tracks that have the exact same
        // timestamp into the same CuePoint
        for (j = 0; j < cues->num_entries - i && entry[j].pts == pts; j++)
        {
            track_positions = mkv_start_ebml_master(MKV, MATROSKA_ID_CUETRACKPOSITION, MAX_CUETRACKPOS_SIZE);
            mkv_put_ebml_uint(MKV, MATROSKA_ID_CUETRACK          , entry[j].tracknum   );
            mkv_put_ebml_uint(MKV, MATROSKA_ID_CUECLUSTERPOSITION, entry[j].cluster_pos);
            mkv_end_ebml_master(MKV, track_positions);
        }
        i += j - 1;
        mkv_end_ebml_master(MKV, cuepoint);
    }
    mkv_end_ebml_master(MKV, cues_element);

    return currentpos;
}

static void mkv_write_codecprivate(mkv_Context* MKV, io_Stream* stream)
{
	if (stream->extra_data_size && stream->extra_data != NULL)
		mkv_put_ebml_binary(MKV, MATROSKA_ID_CODECPRIVATE, stream->extra_data, stream->extra_data_size);
}

static void  mkv_write_trackdefaultduration(mkv_Context* MKV, io_Stream* stream)
{
	if(stream->type == STREAM_TYPE_VIDEO)
	{
		mkv_put_ebml_uint(MKV, MATROSKA_ID_TRACKDEFAULTDURATION, floor(1E9/stream->fps));
	}
}

static int mkv_write_tracks(mkv_Context* MKV)
{
    ebml_master tracks;
    int i, ret;

    ret = mkv_add_seekhead_entry(MKV->main_seekhead, MATROSKA_ID_TRACKS, io_get_offset(MKV->writer));
    if (ret < 0) return ret;

    tracks = mkv_start_ebml_master(MKV, MATROSKA_ID_TRACKS, 0);

    for (i = 0; i < MKV->stream_list_size; i++)
    {
        io_Stream* stream = get_stream(MKV->stream_list, i);
        ebml_master subinfo, track;

        track = mkv_start_ebml_master(MKV, MATROSKA_ID_TRACKENTRY, 0);
        mkv_put_ebml_uint (MKV, MATROSKA_ID_TRACKNUMBER     , i + 1);
        mkv_put_ebml_uint (MKV, MATROSKA_ID_TRACKUID        , i + 1);
        mkv_put_ebml_uint (MKV, MATROSKA_ID_TRACKFLAGLACING , 0);    // no lacing (yet)
		mkv_put_ebml_uint(MKV, MATROSKA_ID_TRACKFLAGDEFAULT, 1);

        char* mkv_codec_name;
        if(stream->type == STREAM_TYPE_VIDEO)
        {
			int codec_index = get_vcodec_index(stream->codec_id);
			if(codec_index < 0)
			{
				fprintf(stderr, "MKV: bad video codec index for id:0x%x\n",stream->codec_id);
				return -1;
			}
			mkv_codec_name = (char *) get_mkvCodec(codec_index);
		}
		else
		{
			int codec_index = get_acodec_index(stream->codec_id);
			if(codec_index < 0)
			{
				fprintf(stderr, "MKV: bad audio codec index for id:0x%x\n",stream->codec_id);
				return -1;
			}
			mkv_codec_name = (char *) get_mkvACodec(codec_index);
		}

        mkv_put_ebml_string(MKV, MATROSKA_ID_CODECID, mkv_codec_name);

        if (MKV->mode == WEBM_FORMAT && !(stream->codec_id == AV_CODEC_ID_VP8 ||
                                        stream->codec_id == AV_CODEC_ID_VORBIS))
		{
            fprintf(stderr, "MKV: Only VP8 video and Vorbis audio are supported for WebM.\n");
            return -2;
        }

        switch (stream->type)
        {
            case STREAM_TYPE_VIDEO:
                mkv_put_ebml_uint(MKV, MATROSKA_ID_TRACKTYPE, MATROSKA_TRACK_TYPE_VIDEO);
                subinfo = mkv_start_ebml_master(MKV, MATROSKA_ID_TRACKVIDEO, 0);
                // XXX: interlace flag?
                mkv_put_ebml_uint (MKV, MATROSKA_ID_VIDEOPIXELWIDTH , stream->width);
				mkv_put_ebml_uint (MKV, MATROSKA_ID_VIDEOPIXELHEIGHT, stream->height);
                mkv_put_ebml_uint(MKV, MATROSKA_ID_VIDEODISPLAYWIDTH , stream->width);
                mkv_put_ebml_uint(MKV, MATROSKA_ID_VIDEODISPLAYHEIGHT, stream->height);
                mkv_put_ebml_uint(MKV, MATROSKA_ID_VIDEODISPLAYUNIT, 3);

                mkv_end_ebml_master(MKV, subinfo);
                break;

            case STREAM_TYPE_AUDIO:
                mkv_put_ebml_uint(MKV, MATROSKA_ID_TRACKTYPE, MATROSKA_TRACK_TYPE_AUDIO);


                //no mkv-specific ID, use ACM mode
                //put_ebml_string(pb, MATROSKA_ID_CODECID, "A_MS/ACM");

                subinfo = mkv_start_ebml_master(MKV, MATROSKA_ID_TRACKAUDIO, 0);
                mkv_put_ebml_uint(MKV, MATROSKA_ID_AUDIOCHANNELS, stream->a_chans);
                mkv_put_ebml_float(MKV, MATROSKA_ID_AUDIOSAMPLINGFREQ, stream->a_rate);
                mkv_put_ebml_uint(MKV, MATROSKA_ID_AUDIOBITDEPTH, stream->a_bits);
                mkv_end_ebml_master(MKV, subinfo);
                break;

            default:
               fprintf(stderr, "MKV: Only audio and video are supported by the Matroska muxer.\n");
               break;
        }

        mkv_write_codecprivate(MKV, stream);
        mkv_write_trackdefaultduration(MKV, stream);

        mkv_end_ebml_master(MKV, track);
    }
    mkv_put_ebml_void(MKV, 200); // add some extra space
    mkv_end_ebml_master(MKV, tracks);
    return 0;
}

int mkv_write_header(mkv_Context* MKV)
{
    ebml_master ebml_header, segment_info;
    int ret;

    ebml_header = mkv_start_ebml_master(MKV, EBML_ID_HEADER, 0);
    mkv_put_ebml_uint   (MKV, EBML_ID_EBMLVERSION        ,           1);
    mkv_put_ebml_uint   (MKV, EBML_ID_EBMLREADVERSION    ,           1);
    mkv_put_ebml_uint   (MKV, EBML_ID_EBMLMAXIDLENGTH    ,           4);
    mkv_put_ebml_uint   (MKV, EBML_ID_EBMLMAXSIZELENGTH  ,           8);
    if (MKV->mode == WEBM_FORMAT)
		mkv_put_ebml_string (MKV, EBML_ID_DOCTYPE        , "webm");
	else
		mkv_put_ebml_string (MKV, EBML_ID_DOCTYPE        , "matroska");
    mkv_put_ebml_uint   (MKV, EBML_ID_DOCTYPEVERSION     ,           2);
    mkv_put_ebml_uint   (MKV, EBML_ID_DOCTYPEREADVERSION ,           2);
    mkv_end_ebml_master(MKV, ebml_header);

    MKV->segment = mkv_start_ebml_master(MKV, MATROSKA_ID_SEGMENT, 0);
    MKV->segment_offset = io_get_offset(MKV->writer);

    // we write 2 seek heads - one at the end of the file to point to each
    // cluster, and one at the beginning to point to all other level one
    // elements (including the seek head at the end of the file), which
    // isn't more than 10 elements if we only write one of each other
    // currently defined level 1 element
    MKV->main_seekhead    = mkv_start_seekhead(MKV, MKV->segment_offset, 10);
    //fprintf(stderr, "MKV: allocated main seekhead at 0x%x\n",  MKV->main_seekhead);
    if (!MKV->main_seekhead)
    {
		fprintf(stderr,"MKV: couldn't allocate seekhead\n");
        return -1;
    }

    ret = mkv_add_seekhead_entry(MKV->main_seekhead, MATROSKA_ID_INFO, io_get_offset(MKV->writer));
    if (ret < 0) return ret;

    segment_info = mkv_start_ebml_master(MKV, MATROSKA_ID_INFO, 0);
    mkv_put_ebml_uint(MKV, MATROSKA_ID_TIMECODESCALE, MKV->timescale);
    mkv_put_ebml_string(MKV, MATROSKA_ID_MUXINGAPP , "Guvcview Muxer-2013.01");
    mkv_put_ebml_string(MKV, MATROSKA_ID_WRITINGAPP, "Guvcview");

	/*generate seg uid - 16 byte random int*/
	GRand* rand_uid= g_rand_new_with_seed(2);
	int seg_uid[4] = {0,0,0,0};
	seg_uid[0] = g_rand_int_range(rand_uid, G_MININT32, G_MAXINT32);
	seg_uid[1] = g_rand_int_range(rand_uid, G_MININT32, G_MAXINT32);
	seg_uid[2] = g_rand_int_range(rand_uid, G_MININT32, G_MAXINT32);
	seg_uid[3] = g_rand_int_range(rand_uid, G_MININT32, G_MAXINT32);
    mkv_put_ebml_binary(MKV, MATROSKA_ID_SEGMENTUID, seg_uid, 16);

    // reserve space for the duration
    MKV->duration = 0;
    MKV->duration_offset = io_get_offset(MKV->writer);
    mkv_put_ebml_void(MKV, 11);                  // assumes double-precision float to be written
    mkv_end_ebml_master(MKV, segment_info);

    ret = mkv_write_tracks(MKV);
    if (ret < 0) return ret;


    MKV->cues = mkv_start_cues(MKV->segment_offset);
    if (MKV->cues == NULL)
    {
		fprintf(stderr,"MKV: couldn't allocate cues\n");
        return -1;
    }

    io_flush_buffer(MKV->writer);
    return 0;
}

static int mkv_processh264_nalu(BYTE *data, int size)
{
	//replace 00 00 00 01 (nalu marker) with nalu size
	int last_nalu = 0; //marks last nalu in buffer
	int tot_nal = 0;
	uint8_t *nal_start = data;
	uint8_t *sp = data;
	uint8_t *ep = NULL;
	uint32_t nal_size = 0;
	
	while (!last_nalu)
	{
		nal_size = 0;
		
		//search for NALU marker
		for(sp = nal_start; sp < data + size - 4; ++sp)
		{
			if(sp[0] == 0x00 &&
			   sp[1] == 0x00 &&
			   sp[2] == 0x00 &&
			   sp[3] == 0x01)
			{
				nal_start = sp + 4;
				break;
			}
		}

		//search for end of NALU
		for(ep = nal_start; ep < data + size - 4; ++ep)
		{
			if(ep[0] == 0x00 &&
			   ep[1] == 0x00 &&
			   ep[2] == 0x00 &&
			   ep[3] == 0x01)
			{
				nal_size = ep - nal_start;
				nal_start = ep;//reset for next NALU
				break;
			}
		}
		
		if(!nal_size)
		{
			last_nalu = 1;
			nal_size = data + size - nal_start;
		}
		
		sp[0] = (nal_size >> 24) & 0x000000FF;
		sp[1] = (nal_size >> 16) & 0x000000FF;
		sp[2] = (nal_size >> 8) & 0x000000FF;
		sp[3] = (nal_size) & 0x000000FF;
		
		tot_nal++;

	}
	
	return tot_nal;
}

static int mkv_blockgroup_size(int pkt_size)
{
    int size = pkt_size + 4;
    size += ebml_num_size(size);
    size += 2;              // EBML ID for block and block duration
    size += 8;              // max size of block duration
    size += ebml_num_size(size);
    size += 1;              // blockgroup EBML ID
    return size;
}

static void mkv_write_block(mkv_Context* MKV,
                            unsigned int blockid,
                            int stream_index,
                            BYTE* data,
                            int size,
                            uint64_t pts,
                            int flags)
{
	io_Stream* stream = get_stream(MKV->stream_list, stream_index);
	if(stream->codec_id == AV_CODEC_ID_H264 && stream->h264_process)
		mkv_processh264_nalu(data, size);
		
	uint8_t block_flags = 0x00;
	
	if(!!(flags & AV_PKT_FLAG_KEY)) //for simple block
		block_flags |= 0x80;
		
    mkv_put_ebml_id(MKV, blockid);
    mkv_put_ebml_num(MKV, size+4, 0);
    io_write_w8(MKV->writer, 0x80 | (stream_index + 1));// this assumes stream_index is less than 126
    io_write_wb16(MKV->writer, pts - MKV->cluster_pts); //pts and cluster_pts are scaled
    io_write_w8(MKV->writer, block_flags);
    io_write_buf(MKV->writer, data, size);
}

static int mkv_write_packet_internal(mkv_Context* MKV,
							int stream_index,
							BYTE* data,
                            int size,
                            int duration,
                            uint64_t pts,
                            int flags)
{
    int keyframe = !!(flags & AV_PKT_FLAG_KEY);
	
	int use_simpleblock = 1;
    
    int ret;
    uint64_t ts = pts / MKV->timescale; //scale the time stamp

	io_Stream* stream = get_stream(MKV->stream_list, stream_index);
	stream->packet_count++;

    if (!MKV->cluster_pos)
    {
        MKV->cluster_pos = io_get_offset(MKV->writer);
        MKV->cluster = mkv_start_ebml_master(MKV, MATROSKA_ID_CLUSTER, 0);
        mkv_put_ebml_uint(MKV, MATROSKA_ID_CLUSTERTIMECODE, MAX(0, ts));
		MKV->cluster_pts = MAX(0, ts);
    }

	if(use_simpleblock)
		mkv_write_block(MKV, MATROSKA_ID_SIMPLEBLOCK, stream_index, data, size, ts, flags);
	else
	{
		ebml_master blockgroup = mkv_start_ebml_master(MKV, MATROSKA_ID_BLOCKGROUP, mkv_blockgroup_size(size));
		mkv_write_block(MKV, MATROSKA_ID_BLOCK, stream_index, data, size, ts, flags);
		if(duration)
			mkv_put_ebml_uint(MKV, MATROSKA_ID_BLOCKDURATION, duration);
		mkv_end_ebml_master(MKV, blockgroup);
	}

    if (get_stream(MKV->stream_list, stream_index)->type == STREAM_TYPE_VIDEO && keyframe)
    {
		//fprintf(stderr,"MKV: add a cue point\n");
        ret = mkv_add_cuepoint(MKV->cues, stream_index, ts, MKV->cluster_pos);
        if (ret < 0) return ret;
    }

    MKV->duration = MAX(MKV->duration, ts + duration);
    return 0;
}

static int mkv_copy_packet(mkv_Context* MKV,
							int stream_index,
							BYTE* data,
                            int size,
                            int duration,
                            uint64_t pts,
                            int flags)
{
	if(size > MKV->pkt_buffer_size)
	{
		MKV->pkt_buffer_size = size;
		MKV->pkt_buffer = g_renew(BYTE, MKV->pkt_buffer, MKV->pkt_buffer_size);
	}

    if (!MKV->pkt_buffer)
    {
		fprintf(stderr,"MKV: couldn't allocate mem for packet\n");
        return -1;
	}
	memcpy(MKV->pkt_buffer, data, size);
	MKV->pkt_size = size;
    MKV->pkt_duration = duration;
    MKV->pkt_pts = pts;
    MKV->pkt_flags = flags;
    MKV->pkt_stream_index = stream_index;

    return 0;
}

/** public interface */
int mkv_write_packet(mkv_Context* MKV,
					int stream_index,
					BYTE* data,
                    int size,
                    int duration,
                    uint64_t pts,
                    int flags)
{
    int ret, keyframe = !!(flags & AV_PKT_FLAG_KEY);
    uint64_t ts = pts;

	ts -= MKV->first_pts;

    int cluster_size = io_get_offset(MKV->writer) - MKV->cluster_pos;

	io_Stream* stream = get_stream(MKV->stream_list, stream_index);
    // start a new cluster every 5 MB or 5 sec, or 32k / 1 sec for streaming or
    // after 4k and on a keyframe
    if (MKV->cluster_pos &&
        (/*(cluster_size > 32*1024 && ts > MKV->cluster_pts + 1000) ||*/
         (cluster_size > 5*1024*1024 && ts > MKV->cluster_pts + 5000) ||
         (stream->type == STREAM_TYPE_VIDEO && keyframe && cluster_size > 4*1024)))
    {
		//fprintf(stderr, "MKV: Starting new cluster at offset %" PRIu64 " bytes, pts %" PRIu64 "\n", io_get_offset(MKV->writer), ts);
        mkv_end_ebml_master(MKV, MKV->cluster);
        MKV->cluster_pos = 0;
    }

    // check if we have an audio packet cached
    if (MKV->pkt_size > 0)
    {
        ret = mkv_write_packet_internal(MKV,
							MKV->pkt_stream_index,
							MKV->pkt_buffer,
							MKV->pkt_size,
							MKV->pkt_duration,
							MKV->pkt_pts,
							MKV->pkt_flags);

        MKV->pkt_size = 0;
        if (ret < 0)
        {
            fprintf(stderr, "MKV: Could not write cached audio packet\n");
            return ret;
        }
    }

    // buffer an audio packet to ensure the packet containing the video
    // keyframe's timecode is contained in the same cluster for WebM
    if (stream->type == STREAM_TYPE_AUDIO)
        ret = mkv_copy_packet(MKV, stream_index, data, size, duration, ts, flags);
    else
		ret = mkv_write_packet_internal(MKV, stream_index, data, size, duration, ts, flags);

    return ret;
}

int mkv_close(mkv_Context* MKV)
{
    int64_t currentpos, cuespos;
    int ret;
	printf("closing MKV\n");
    // check if we have an audio packet cached
    if (MKV->pkt_size > 0)
    {
        ret = mkv_write_packet_internal(MKV,
							MKV->pkt_stream_index,
							MKV->pkt_buffer,
							MKV->pkt_size,
							MKV->pkt_duration,
							MKV->pkt_pts,
							MKV->pkt_flags);

        MKV->pkt_size = 0;
        if (ret < 0)
        {
            fprintf(stderr, "MKV: Could not write cached audio packet\n");
            return ret;
        }
    }

	printf("closing cluster\n");
	if(MKV->cluster_pos)
		mkv_end_ebml_master(MKV, MKV->cluster);

	if (MKV->cues->num_entries)
	{
		printf("writing cues\n");
		cuespos = mkv_write_cues(MKV, MKV->cues, MKV->stream_list_size);
		printf("add seekhead\n");
		ret = mkv_add_seekhead_entry(MKV->main_seekhead, MATROSKA_ID_CUES, cuespos);
        if (ret < 0) return ret;
	}
	printf("write seekhead\n");
    mkv_write_seekhead(MKV, MKV->main_seekhead);

    // update the duration
    fprintf(stderr,"MKV: end duration = %" PRIu64 " (%f) \n", MKV->duration, (float) MKV->duration);
    currentpos = io_get_offset(MKV->writer);
    io_seek(MKV->writer, MKV->duration_offset);

    mkv_put_ebml_float(MKV, MATROSKA_ID_DURATION, (float) MKV->duration);
	io_seek(MKV->writer, currentpos);

    mkv_end_ebml_master(MKV, MKV->segment);
    av_freep(&MKV->cues->entries);
    av_freep(&MKV->cues);

    return 0;
}

mkv_Context* mkv_create_context(char* filename, int mode)
{
	mkv_Context* MKV = g_new0(mkv_Context, 1);

	MKV->writer = io_create_writer(filename, 0);
	MKV->mode = mode;
	MKV->main_seekhead = NULL;
	MKV->cues = NULL;
	MKV->pkt_buffer = NULL;
	MKV->stream_list = NULL;
	MKV->timescale = 1000000;

	return MKV;
}

void mkv_destroy_context(mkv_Context* MKV)
{
	io_destroy_writer(MKV->writer);

	destroy_stream_list(MKV->stream_list, &MKV->stream_list_size);

	if(MKV->pkt_buffer != NULL)
		g_free(MKV->pkt_buffer);
	g_free(MKV);
}

io_Stream*
mkv_add_video_stream(mkv_Context *MKV,
					int32_t width,
					int32_t height,
					int32_t fps,
					int32_t fps_num,
					int32_t codec_id)
{
	io_Stream* stream = add_new_stream(&MKV->stream_list, &MKV->stream_list_size);
	stream->type = STREAM_TYPE_VIDEO;
	stream->width = width;
	stream->height = height;
	stream->codec_id = codec_id;

	stream->fps = (double) fps/fps_num;
	stream->indexes = NULL;

	return stream;
}

io_Stream*
mkv_add_audio_stream(mkv_Context *MKV,
					int32_t   channels,
					int32_t   rate,
					int32_t   bits,
					int32_t   mpgrate,
					int32_t   codec_id,
					int32_t   format)
{
	io_Stream* stream = add_new_stream(&MKV->stream_list, &MKV->stream_list_size);
	stream->type = STREAM_TYPE_AUDIO;

	stream->a_rate = rate;
	stream->a_bits = bits;
	stream->mpgrate = mpgrate;
	stream->a_vbr = 0;
	stream->codec_id = codec_id;
	stream->a_fmt = format;

	stream->indexes = NULL;

	return stream;
}
