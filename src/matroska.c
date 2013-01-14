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

static float mkv_int2float(uint32_t i)
{
	union mkv_union_intfloat32 v;
	v.i = i;
	return v.f;
}

static uint32_t mkv_float2int(float f)
{
	union mkv_union_intfloat32 v;
	v.f = f;
	return v.i;
}

static double mkv_int2double(uint64_t i)
{
	union mkv_union_intfloat64 v;
	v.i = i;
	return v.f;
}

static uint64_t mkv_double2int(double f)
{
	union av_intfloat64 v;
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
		fprintf(stderr, "MKV: ebml number: %llu\n", num);
		return;
	}

    if (bytes == 0)
        // don't care how many bytes are used, so use the min
        bytes = needed_bytes;
    // the bytes needed to write the given size would exceed the bytes
    // that we need to use, so write unknown size. This shouldn't happen.
    if(bytes < needed_bytes)
    {
		fprintf(stderr, "MKV: bad requested size for ebml number: %llu (%i < %i)\n", num, bytes, needed_bytes);
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
                            const void *buf, int size)
{
    mkv_put_ebml_id(MKV, elementid);
    mkv_put_ebml_num(MKV, size, 0);
    io_write_buf(MKV->writer, buf, size);
}

static void mkv_put_ebml_string(mkv_Context* MKV, unsigned int elementid, const char *str)
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
		fprintf(stderr, "MKV: wrong void size %llu < 2", size);
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

static void mkv_put_xiph_size(mkv_Context* MKV, int size)
{
    int i;
    for (i = 0; i < size / 255; i++)
		io_write_w8(MKV->writer, 255);
    io_write_w8(MKV->writer, size % 255);
}

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
    entries[seekhead->num_entries  ].elementid = elementid;
    entries[seekhead->num_entries++].segmentpos = filepos - seekhead->segment_offset;

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

	g_renew(mkv_cuepoint, entries, cues->num_entries + 1);
    //entries = av_realloc(entries, (cues->num_entries + 1) * sizeof(mkv_cuepoint));
    if (entries == NULL)
        return AVERROR(ENOMEM);

    entries[cues->num_entries  ].pts = ts;
    entries[cues->num_entries  ].tracknum = stream + 1;
    entries[cues->num_entries++].cluster_pos = cluster_pos - cues->segment_offset;

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


static int mkv_write_tracks(mkv_Context* MKV)
{
    //MatroskaMuxContext *mkv = s->priv_data;
    //AVIOContext *pb = s->pb;

    ebml_master tracks;
    int i, j, ret;

    ret = mkv_add_seekhead_entry(MKV->main_seekhead, MATROSKA_ID_TRACKS, io_get_offset(MKV->writer));
    if (ret < 0) return ret;

    tracks = mkv_start_ebml_master(MKV->writer, MATROSKA_ID_TRACKS, 0);

    for (i = 0; i < MKV->stream_list_size; i++)
    {
        io_Stream* stream = get_stream(MKV->stream_list, i);
        ebml_master subinfo, track;

        track = mkv_ start_ebml_master(MKV, MATROSKA_ID_TRACKENTRY, 0);
        mkv_put_ebml_uint (MKV, MATROSKA_ID_TRACKNUMBER     , i + 1);
        mkv_put_ebml_uint (MKV, MATROSKA_ID_TRACKUID        , i + 1);
        mkv_put_ebml_uint (MKV, MATROSKA_ID_TRACKFLAGLACING , 0);    // no lacing (yet)

        //if ((tag = av_dict_get(st->metadata, "title", NULL, 0)))
        //    put_ebml_string(pb, MATROSKA_ID_TRACKNAME, tag->value);
        //tag = av_dict_get(st->metadata, "language", NULL, 0);
        //put_ebml_string(pb, MATROSKA_ID_TRACKLANGUAGE, tag ? tag->value:"und");

        //if (stream->disposition)
        //mkv_put_ebml_uint(MKV, MATROSKA_ID_TRACKFLAGDEFAULT, !!(stream->disposition & AV_DISPOSITION_DEFAULT));
		mkv_put_ebml_uint(MKV, MATROSKA_ID_TRACKFLAGDEFAULT, 1);

        // look for a codec ID string specific to mkv to use,
        // if none are found, use AVI codes
        //for (j = 0; ff_mkv_codec_tags[j].id != AV_CODEC_ID_NONE; j++) {
        //    if (ff_mkv_codec_tags[j].id == codec->codec_id) {
        //        put_ebml_string(pb, MATROSKA_ID_CODECID, ff_mkv_codec_tags[j].str);
        //        native_id = 1;
        //        break;
        //    }
        //}
        char* mkv_codec_name;
        if(stream->type == STREAM_TYPE_VIDEO)
        {
			int codec_index = get_vcodec_index(stream->codec_id);
			if(codec_index < 0)
			{
				fprintf(stderr, "MKV: bad video codec index for id:0x%x\n",stream->codec_id);
				return -1;
			}
			mkv_codec_name = get_mkvCodec(codec_index);
		}
		else
		{
			int codec_index = get_acodec_index(stream->codec_id);
			if(codec_index < 0)
			{
				fprintf(stderr, "MKV: bad audio codec index for id:0x%x\n",stream->codec_id);
				return -1;
			}
			mkv_codec_name = get_mkvCodec(codec_index);
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
                mkv_put_ebml_uint(MKV, MATROSKA_ID_TRACKDEFAULTDURATION, 1000*1E9);


                subinfo = mkv_start_ebml_master(MKV, MATROSKA_ID_TRACKVIDEO, 0);
                // XXX: interlace flag?
                mkv_put_ebml_uint (MKV, MATROSKA_ID_VIDEOPIXELWIDTH , stream->width);
                mkv_put_ebml_uint (MKV, MATROSKA_ID_VIDEOPIXELHEIGHT, stream->height);

               /***
                if ((tag = av_dict_get(s->metadata, "stereo_mode", NULL, 0)))
                {
                    uint8_t stereo_fmt = atoi(tag->value);
                    int valid_fmt = 0;

                    switch (mkv->mode) {
                    case MODE_WEBM:
                        if (stereo_fmt <= MATROSKA_VIDEO_STEREOMODE_TYPE_TOP_BOTTOM
                            || stereo_fmt == MATROSKA_VIDEO_STEREOMODE_TYPE_RIGHT_LEFT)
                            valid_fmt = 1;
                        break;
                    case MODE_MATROSKAv2:
                        if (stereo_fmt <= MATROSKA_VIDEO_STEREOMODE_TYPE_BOTH_EYES_BLOCK_RL)
                            valid_fmt = 1;
                        break;
                    }

                    if (valid_fmt)
                        put_ebml_uint (pb, MATROSKA_ID_VIDEOSTEREOMODE, stereo_fmt);
                }
                **/
                mkv_put_ebml_uint(MKV, MATROSKA_ID_VIDEODISPLAYWIDTH , stream->width);
                mkv_put_ebml_uint(MKV, MATROSKA_ID_VIDEODISPLAYHEIGHT, stream->height);
                mkv_put_ebml_uint(MKV, MATROSKA_ID_VIDEODISPLAYUNIT, 3);

                mkv_end_ebml_master(MKV, subinfo);
                break;

            case AVMEDIA_TYPE_AUDIO:
                mkv_put_ebml_uint(MKV, MATROSKA_ID_TRACKTYPE, MATROSKA_TRACK_TYPE_AUDIO);

                //if (!native_id)
                //    // no mkv-specific ID, use ACM mode
                //    put_ebml_string(pb, MATROSKA_ID_CODECID, "A_MS/ACM");

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
        ret = mkv_write_codecprivate(MKV, stream);
        if (ret < 0) return ret;

        mkv_end_ebml_master(MKV, track);

        // ms precision is the de-facto standard timescale for mkv files
        //avpriv_set_pts_info(st, 64, 1, 1000);
    }
    mkv_end_ebml_master(pb, tracks);
    return 0;
}

static int mkv_write_header(mkv_Context* MKV)
{
    ebml_master ebml_header, segment_info;
    int ret, i;
	
	if (MKV->mode == WEBM_FORMAT)
	
	else
	

    mkv->tracks = av_mallocz(s->nb_streams * sizeof(*mkv->tracks));
    if (!mkv->tracks)
    {
		fprintf(stderr,"MKV: couldn't allocate tracks\n");
        return -1;
    }

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
    if (!MKV->main_seekhead)
    {
		fprintf(stderr,"MKV: couldn't allocate seekhead\n");
        return -1;
    }

    ret = mkv_add_seekhead_entry(MKV->main_seekhead, MATROSKA_ID_INFO, io_get_offset(MKV->writer));
    if (ret < 0) return ret;

    segment_info = mkv_start_ebml_master(MKV, MATROSKA_ID_INFO, 0);
    mkv_put_ebml_uint(MKV, MATROSKA_ID_TIMECODESCALE, 1000000);
    mkv_put_ebml_string(MKV, MATROSKA_ID_MUXINGAPP , "Guvcview Muxer-2013.01");
    mkv_put_ebml_string(pb, MATROSKA_ID_WRITINGAPP, "Guvcview");
    
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
	
	//should it go here ???
    mkv_write_seekhead(MKV, MKV->main_seekhead);

    MKV->cues = mkv_start_cues(MKV->segment_offset);
    if (MKV->cues == NULL)
    {
		fprintf(stderr,"MKV: couldn't allocate cues\n");
        return -1;
    }

    io_flush_buffer(AVI->writer);
    return 0;
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
                            uint64_t dts,
                            uint64_t pts, 
                            int flags)
{
    int64_t ts = MKV->tracks[stream_index].write_dts ? dts : pts;

    mkv_put_ebml_id(MKV, blockid);
    mkv_put_ebml_num(MKV, size+4, 0);
    io_write_w8(MKV->writer, 0x80 | (stream_index + 1));     // this assumes stream_index is less than 126
    io_write_wb16(MKV->writer, ts - MKV->cluster_pts);
    io_write_w8(MKV->writer, flags);
    io_write_buf(MKV->writer, data, size);
}

static int mkv_write_packet_internal(mkv_Context* MKV,
							int stream_index,
							BYTE* data, 
                            int size,
                            int duration,
                            uint64_t dts,
                            uint64_t pts, 
                            int flags)
{
    int keyframe = !!(flags & AV_PKT_FLAG_KEY);
    int ret;
    int64_t ts = MKV->tracks[stream_index].write_dts ? dts : pts;

    if (!MKV->cluster_pos) 
    {
        MKV->cluster_pos = io_get_offset(MKV->writer);
        MKV->cluster = mkv_start_ebml_master(MKV, MATROSKA_ID_CLUSTER, 0);
        mkv_put_ebml_uint(MKV, MATROSKA_ID_CLUSTERTIMECODE, MAX(0, ts));
		MKV->cluster_pts = MAX(0, ts);
    }

	ebml_master blockgroup = mkv_start_ebml_master(MKV, MATROSKA_ID_BLOCKGROUP, mkv_blockgroup_size(size));
	mkv_write_block(MKV, MATROSKA_ID_BLOCK, stream_index, data, size, dts, pts, flags);
	mkv_put_ebml_uint(MKV, MATROSKA_ID_BLOCKDURATION, duration);
	mkv_end_ebml_master(pb, blockgroup);
    

    if (get_stream(MKV->stream_list, stream_index)->type == STREAM_TYPE_VIDEO && keyframe) 
    {
        ret = mkv_add_cuepoint(MKV->cues, stream_index, ts, MKV->cluster_pos);
        if (ret < 0) return ret;
    }

    MKV->duration = MAX(MKV->duration, ts + duration);
    return 0;
}

static int mkv_write_packet((mkv_Context* MKV,
							int stream_index,
							BYTE* data, 
                            int size,
                            int duration,
                            uint64_t dts,
                            uint64_t pts, 
                            int flags)
{
    int ret, keyframe = !!(flags & AV_PKT_FLAG_KEY);
    int64_t ts = MKV->tracks[stream_index].write_dts ? dts : pts;
    int cluster_size = io_get_offset(MKV->writer) - MKV->cluster_pos;

    // start a new cluster every 5 MB or 5 sec, or 32k / 1 sec for streaming or
    // after 4k and on a keyframe
    if (MKV->cluster_pos &&
        ((cluster_size > 32*1024 || ts > MKV->cluster_pts + 1000) ||
          cluster_size > 5*1024*1024 || ts > MKV->cluster_pts + 5000 ||
         (get_stream(MKV->stream_list, stream_index)->type == STREAM_TYPE_VIDEO && keyframe && cluster_size > 4*1024))) 
    {
		fprintf(stderr, "MKV: Starting new cluster at offset %" PRIu64 " bytes, pts %" PRIu64 "\n", io_get_offset(MKV->writer), ts);
        mkv_end_ebml_master(MKV, MKV->cluster);
        MKV->cluster_pos = 0;
    }

	/**
    // check if we have an audio packet cached
    if (MKV->cur_audio_pkt_size > 0) {
        ret = mkv_write_packet_internal(MKV, );
        mkv->cur_audio_pkt.size = 0;
        if (ret < 0) {
            av_log(s, AV_LOG_ERROR, "Could not write cached audio packet ret:%d\n", ret);
            return ret;
        }
    }

    // buffer an audio packet to ensure the packet containing the video
    // keyframe's timecode is contained in the same cluster for WebM
    if (codec->codec_type == AVMEDIA_TYPE_AUDIO)
        ret = mkv_copy_packet(mkv, pkt);
    else
    
    **/
        ret = mkv_write_packet_internal(MKV, stream_index, data, size, duration, dts, pts, flags);
        
    return ret;
}

static int mkv_close(mkv_Context* MKV)
{
    int64_t currentpos, cuespos;
    int ret;

	/**
    // check if we have an audio packet cached
    if (mkv->cur_audio_pkt.size > 0) {
        ret = mkv_write_packet_internal(s, &mkv->cur_audio_pkt);
        mkv->cur_audio_pkt.size = 0;
        if (ret < 0) {
            av_log(s, AV_LOG_ERROR, "Could not write cached audio packet ret:%d\n", ret);
            return ret;
        }
    }
	*/
	
	/**
    if (mkv->dyn_bc) {
        end_ebml_master(mkv->dyn_bc, mkv->cluster);
        mkv_flush_dynbuf(s);
    } else if (mkv->cluster_pos) {
        end_ebml_master(pb, mkv->cluster);
    }
    */
	if(MKV->cluster_pos)
		mkv_end_ebml_master(MKV, MKV->cluster);

	if (MKV->cues->num_entries) 
	{
		cuespos = mkv_write_cues(MKV, MKV->cues, MKV->stream_list_size);

		ret = mkv_add_seekhead_entry(MKV->main_seekhead, MATROSKA_ID_CUES, cuespos);
        if (ret < 0) return ret;
	}

    mkv_write_seekhead(MKV, MKV->main_seekhead);

    // update the duration
    fprintf(stderr,"MKV: end duration = %" PRIu64 "\n", MKV->duration);
    currentpos = io_get_offset(MKV->writer);
    io_seek(MKV->writer, MKV->duration_offset);
    mkv_put_ebml_float(MKV, MATROSKA_ID_DURATION, MKV->duration);
	io_seek(MKV, currentpos);

    mkv_end_ebml_master(MKV, MKV->segment);
    av_free(MKV->tracks);
    av_freep(&MKV->cues->entries);
    av_freep(&MKV->cues);

    return 0;
}

mkv_Context* mkv_create_context(int mode
								char* filename)
{
	mkv_Context* MKV = g_new0(mkv_Context, 1);

	MKV->writer = io_create_writer(filename);
	MKV->mode = mode;
	MKV->main_seekhead = NULL;
	MKV->cues = NULL;
	MKV->tracks = NULL;
	MKV->audio_buffer = NULL;
	MKV->stream_list = NULL;
	
	return MKV;
}

void mkv_destroy_context(mkv_Context* MKV)
{
	io_destroy_writer(MKV->writer);
	
	destroy_stream_list(MKV->stream_list, &MKV->stream_list_size);
	
	if(MKV->audio_buffer != NULL)
		g_free(MKV->audio_buffer);
	g_free(MKV);
}

io_Stream* 
mkv_add_video_stream(mkv_Context *MKV,
					int32_t width,
					int32_t height,
					int32_t codec_id)
{
	io_Stream* stream = io_add_new_stream(MKV->stream_list, &MKV->stream_list_size);
	stream->type = STREAM_TYPE_VIDEO;
	stream->width = width;
	stream->height = height;
	stream->codec_id = codec_id;

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
	io_Stream* stream = io_add_new_stream(MKV->stream_list, &MKV->stream_list_size);
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






#define CLSIZE    55242880 /*5 Mb  (1Mb = 1048576)*/
#define CHECK(x)  do { if ((x) < 0) return -1; } while (0)

struct mk_Context
{
	struct mk_Context *next, **prev, *parent;
	struct mk_Writer  *owner;
	unsigned id;
	void *data;
	int64_t d_cur, d_max;
};

typedef struct mk_Context mk_Context;

struct mk_Writer
{
	FILE *fp;
	/*-------header------*/
	int format; // 1 matroska , 2 webm
	int video_only;            //not muxing audio
	int64_t duration_ptr;      //file location pointer for duration
	int64_t segment_size_ptr;  //file location pointer for segment size
	int64_t cues_pos;
	int64_t seekhead_pos;
	mk_Context *root, *cluster, *frame;
	mk_Context *freelist;
	mk_Context *actlist;
	gboolean wrote_header;
	/*-------video-------*/
	UINT64 def_duration;
	int64_t def_duration_ptr;
	int64_t timescale;
	int64_t cluster_tc_scaled;
	int64_t frame_tc, prev_frame_tc_scaled, max_frame_tc;
	gboolean in_frame, keyframe;
	/*-------audio-------*/
	mk_Context *audio_frame;
	int64_t audio_frame_tc, audio_prev_frame_tc_scaled;
	int64_t audio_block, block_n;
	gboolean audio_in_frame;
	/*--------cues-------*/
	mk_Context *cues;
	int64_t cue_time;
	int64_t cue_video_track_pos;
	int64_t cue_audio_track_pos;
	/*-----seek head-----*/
	unsigned cluster_index;
	int64_t *cluster_pos;
};

static mk_Context *mk_createContext(mk_Writer *w, mk_Context *parent, unsigned id)
{
	mk_Context *c;

	if (w->freelist)
	{
		c = w->freelist;
		w->freelist = w->freelist->next;
	}
	else
	{
		c = g_new0(mk_Context, 1);
	}

	if (c == NULL)
		return NULL;

	c->parent = parent;
	c->owner = w;
	c->id = id;

	if (c->owner->actlist)
		c->owner->actlist->prev = &c->next;
	c->next = c->owner->actlist;
	c->prev = &c->owner->actlist;
	c->owner->actlist = c;

	return c;
}

static int mk_appendContextData(mk_Context *c, const void *data, int64_t size)
{
	if (!size) return 0;

	int64_t ns = c->d_cur + size;
	if (ns > c->d_max)
	{
		void *dp;
		int64_t dn = c->d_max ? c->d_max << 1 : 16;
		while (ns > dn)
			dn <<= 1;

		dp = g_renew(BYTE, c->data, dn);
		if (dp == NULL)
			return -1;

		c->data = dp;
		c->d_max = dn;
	}

	memcpy((char*)c->data + c->d_cur, data, size);
	c->d_cur = ns;

	return 0;
}

static int mk_writeID(mk_Context *c, unsigned id)
{
	unsigned char c_id[4] = { id >> 24, id >> 16, id >> 8, id };
	if (c_id[0])
		return mk_appendContextData(c, c_id, 4);
	if (c_id[1])
		return mk_appendContextData(c, c_id+1, 3);
	if (c_id[2])
		return mk_appendContextData(c, c_id+2, 2);
	return mk_appendContextData(c, c_id+3, 1);
}

static int mk_writeSize(mk_Context *c, int64_t size)
{
	unsigned char c_size[8] = { size >> 56, size >> 48, size >> 40, size >> 32, size >> 24, size >> 16, size >> 8, size };
	unsigned char head = 0x80;
	int64_t max = 0x80;
	int i;

	for (i = 7; i >= 0; i--)
	{
		if (size < max - 1)
		{
			c_size[i] |= head;
			return mk_appendContextData(c, c_size + i, 8 - i);
		}
		head >>= 1;
		max <<= 7;
	}

	/* size overflow (> 72 PB), write max permitted size instead */
	return mk_writeSize(c, 0xFFFFFFFFFFFFFELL);
}

static int mk_flushContextData(mk_Context *c)
{
	if (c->d_cur == 0)
		return 0;

	if (c->parent)
		CHECK(mk_appendContextData(c->parent, c->data, c->d_cur));
	else if (fwrite(c->data, c->d_cur, 1, c->owner->fp) != 1)
		return -1;
	c->d_cur = 0;

	return 0;
}

static int mk_closeContext(mk_Context *c, int64_t *off)
{
	if (c->id)
	{
		CHECK(mk_writeID(c->parent, c->id));
		CHECK(mk_writeSize(c->parent, c->d_cur));
	}

	if (c->parent && off != NULL)
		*off += c->parent->d_cur;

	CHECK(mk_flushContextData(c));

	if (c->next)
		c->next->prev = c->prev;
	*(c->prev) = c->next;
	c->next = c->owner->freelist;
	c->owner->freelist = c;

	return 0;
}

static void mk_destroyContexts(mk_Writer *w)
{
	mk_Context  *cur, *next;

	for (cur = w->freelist; cur; cur = next)
	{
		next = cur->next;
		g_free(cur->data);
		g_free(cur);
	}

	for (cur = w->actlist; cur; cur = next)
	{
		next = cur->next;
		g_free(cur->data);
		g_free(cur);
	}

	w->freelist = w->actlist = w->root = NULL;
}

static int mk_writeStr(mk_Context *c, unsigned id, const char *str)
{
	size_t  len = strlen(str);

	CHECK(mk_writeID(c, id));
	CHECK(mk_writeSize(c, len));
	CHECK(mk_appendContextData(c, str, len));
	return 0;
}

static int mk_writeBin(mk_Context *c, unsigned id, const void *data, int64_t size)
{
	CHECK(mk_writeID(c, id));
	CHECK(mk_writeSize(c, size));
	CHECK(mk_appendContextData(c, data, size));

	return 0;
}

static int mk_writeVoid(mk_Context *c, int64_t size)
{
	BYTE EbmlVoid = 0x00;
	int i=0;
	CHECK(mk_writeID(c, EBML_ID_VOID));
	CHECK(mk_writeSize(c, size));
	for (i=0; i<size; i++)
		CHECK(mk_appendContextData(c, &EbmlVoid, 1));
	return 0;
}

static int mk_writeUIntRaw(mk_Context *c, unsigned id, int64_t ui)
{
	unsigned char c_ui[8] = { ui >> 56, ui >> 48, ui >> 40, ui >> 32, ui >> 24, ui >> 16, ui >> 8, ui };

	CHECK(mk_writeID(c, id));
	CHECK(mk_writeSize(c, 8));
	CHECK(mk_appendContextData(c, c_ui, 8));
	return 0;
}

static int mk_writeUInt(mk_Context *c, unsigned id, int64_t ui)
{
	unsigned char c_ui[8] = { ui >> 56, ui >> 48, ui >> 40, ui >> 32, ui >> 24, ui >> 16, ui >> 8, ui };
	unsigned i = 0;

	CHECK(mk_writeID(c, id));
	while (i < 7 && c_ui[i] == 0)
		++i;
	CHECK(mk_writeSize(c, 8 - i));
	CHECK(mk_appendContextData(c, c_ui+i, 8 - i));
	return 0;
}

static int mk_writeSegPos(mk_Context *c, int64_t ui)
{
	unsigned char c_ui[8] = { ui >> 56, ui >> 48, ui >> 40, ui >> 32, ui >> 24, ui >> 16, ui >> 8, ui };

	CHECK(mk_appendContextData(c, c_ui, 8 ));
	return 0;
}

//static int mk_writeSInt(mk_Context *c, unsigned id, int64_t si)
//{
//	unsigned char c_si[8] = { si >> 56, si >> 48, si >> 40, si >> 32, si >> 24, si >> 16, si >> 8, si };
//	unsigned i = 0;
//
//	CHECK(mk_writeID(c, id));
//	if (si < 0)
//		while (i < 7 && c_si[i] == 0xff && c_si[i+1] & 0x80)
//			++i;
//	else
//		while (i < 7 && c_si[i] == 0 && !(c_si[i+1] & 0x80))
//			++i;
//	CHECK(mk_writeSize(c, 8 - i));
//	CHECK(mk_appendContextData(c, c_si+i, 8 - i));
//	return 0;
//}

static int mk_writeFloatRaw(mk_Context *c, float f)
{
	union
	{
		float f;
		unsigned u;
	} u;
	unsigned char c_f[4];

	u.f = f;
	c_f[0] = u.u >> 24;
	c_f[1] = u.u >> 16;
	c_f[2] = u.u >> 8;
	c_f[3] = u.u;

	return mk_appendContextData(c, c_f, 4);
}

static int mk_writeFloat(mk_Context *c, unsigned id, float f)
{
	CHECK(mk_writeID(c, id));
	CHECK(mk_writeSize(c, 4));
	CHECK(mk_writeFloatRaw(c, f));
	return 0;
}

static unsigned mk_ebmlSizeSize(unsigned s)
{
	if (s < 0x7f)
		return 1;
	if (s < 0x3fff)
		return 2;
	if (s < 0x1fffff)
		return 3;
	if (s < 0x0fffffff)
		return 4;
	return 5;
}

//static unsigned mk_ebmlSIntSize(int64_t si)
//{
//	unsigned char c_si[8] = { si >> 56, si >> 48, si >> 40, si >> 32, si >> 24, si >> 16, si >> 8, si };
//	unsigned i = 0;
//
//	if (si < 0)
//		while (i < 7 && c_si[i] == 0xff && c_si[i+1] & 0x80)
//			++i;
//	else
//		while (i < 7 && c_si[i] == 0 && !(c_si[i+1] & 0x80))
//			++i;
//
//	return 8 - i;
//}

mk_Writer *mk_createWriter(const char *filename, int format)
{
	mk_Writer *w = g_new0(mk_Writer, 1);

	if (w == NULL)
		return NULL;

	w->format = format;

	w->root = mk_createContext(w, NULL, 0);
	if (w->root == NULL)
	{
		g_free(w);
		return NULL;
	}

	w->fp = fopen(filename, "wb");
	if (w->fp == NULL)
	{
		perror("Could not open file for writing");
		mk_destroyContexts(w);
		g_free(w);
		return NULL;
	}

	return w;
}

/*
 - matroska header -
 codecID: (audio)
         MP2 - "A_MPEG/L2"
         PCM - "A_PCM/INT/LIT"
*/
int mk_writeHeader(mk_Writer *w, const char *writingApp,
		 const char *codecID,
		 const char *AcodecID,
		 const void *codecPrivate, int64_t codecPrivateSize,
		 UINT64 default_frame_duration,  /*video */
		 const void *AcodecPrivate, int64_t AcodecPrivateSize,
		 UINT64 default_aframe_duration, /*audio */
		 int64_t timescale,
		 unsigned width, unsigned height,
		 unsigned d_width, unsigned d_height,
		 float SampRate, int channels, int bitsSample)
{
	mk_Context *c, *ti, *v, *ti2, *ti3, *a;
	BYTE empty[8] = {0x00,0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
	int extra=6; /* use six extra bytes for segment size */
	int64_t pos = 0; /* current position in header */
	if (w->wrote_header)
		return -1;

	w->timescale = timescale;
	w->def_duration = default_frame_duration;

	if ((c = mk_createContext(w, w->root, EBML_ID_HEADER)) == NULL) /* EBML */
		return -1;
	//CHECK(mk_writeUInt(c, EBML_ID_EBMLVERSION, 1));       // EBMLVersion
	//CHECK(mk_writeUInt(c, EBML_ID_EBMLREADVERSION, 1));   // EBMLReadVersion
	//CHECK(mk_writeUInt(c, EBML_ID_EBMLMAXIDLENGTH, 4));   // EBMLMaxIDLength
	//CHECK(mk_writeUInt(c, EBML_ID_EBMLMAXSIZELENGTH, 8)); // EBMLMaxSizeLength
	if(w->format == WEBM_FORMAT)
		CHECK(mk_writeStr(c, EBML_ID_DOCTYPE, "webm"));     // DocType
	else
		CHECK(mk_writeStr(c, EBML_ID_DOCTYPE, "matroska"));     // DocType
	CHECK(mk_writeUInt(c, EBML_ID_DOCTYPEVERSION, 2));      // DocTypeVersion
	CHECK(mk_writeUInt(c, EBML_ID_DOCTYPEREADVERSION, 2));  // DocTypeReadversion
	pos=c->d_cur;
	CHECK(mk_closeContext(c, &pos));

	/* SEGMENT starts at position 24 (matroska) 20(webm) */
	if ((c = mk_createContext(w, w->root, MATROSKA_ID_SEGMENT)) == NULL)
		return -1;
	pos+=4; /*pos=28(24)*/

	w->segment_size_ptr = pos;
	/*needs full segment size here (including clusters) but we only know the header size for now.(2 bytes)*/
	/*add extra (6) bytes - reserve a total of 8 bytes for segment size in ebml format =6+2(header size)  */
	mk_appendContextData(c, empty, extra);
	pos+=extra+2;/*pos=36(32) account 2 bytes from header size -these will only be available after closing c context*/

	w->seekhead_pos = pos; /* SeekHead Position */
	if ((ti = mk_createContext(w, c, MATROSKA_ID_SEEKHEAD)) == NULL)    /* SeekHead */
		return -1;
	if ((ti2 = mk_createContext(w, ti, MATROSKA_ID_SEEKENTRY)) == NULL) /* Seek */
		return -1;
	CHECK(mk_writeUInt (ti2, MATROSKA_ID_SEEKID, MATROSKA_ID_INFO));    /* seekID */
	CHECK(mk_writeUInt(ti2, MATROSKA_ID_SEEKPOSITION, 4135-(w->seekhead_pos)));
	CHECK(mk_closeContext(ti2, 0));

	if ((ti2 = mk_createContext(w, ti, MATROSKA_ID_SEEKENTRY)) == NULL) /* Seek */
		return -1;
	CHECK(mk_writeUInt(ti2, MATROSKA_ID_SEEKID, MATROSKA_ID_TRACKS));   /* seekID */
	CHECK(mk_writeUInt(ti2, MATROSKA_ID_SEEKPOSITION, 4220-(w->seekhead_pos)));
	CHECK(mk_closeContext(ti2, 0));
	pos = ti->d_cur + w->segment_size_ptr + 2;/* add 2 bytes from the header size */
	CHECK(mk_closeContext(ti, &pos)); /* pos=71 (67) */
	/* allways start Segment info at pos 4135
	* this will be overwritten by seek entries for cues and the final seekhead   */
	CHECK(mk_writeVoid(c, 4135-(pos+3))); /* account 3 bytes from Void ID (1) and size(2) */

	/* Segment Info at position 4135 (fixed)*/
	pos=4135;
	if ((ti = mk_createContext(w, c, MATROSKA_ID_INFO)) == NULL)
		return -1;
	CHECK(mk_writeUInt(ti, MATROSKA_ID_TIMECODESCALE, w->timescale));
	CHECK(mk_writeStr(ti, MATROSKA_ID_MUXINGAPP, "Guvcview Muxer-2009.11"));
	CHECK(mk_writeStr(ti, MATROSKA_ID_WRITINGAPP, writingApp));
	/* signed 8 byte integer in nanoseconds
	* with 0 indicating the precise beginning of the millennium (at 2001-01-01T00:00:00,000000000 UTC)
	* value: ns_time - 978307200000000000  ( Unix Epoch is 01/01/1970 ) */
	UINT64 date = ns_time() - 978307200000000000LL;
	CHECK(mk_writeUIntRaw(ti, MATROSKA_ID_DATEUTC, date)); /* always use the full 8 bytes */
	/*generate seg uid - 16 byte random int*/
	GRand* rand_uid= g_rand_new_with_seed(2);
	int seg_uid[4] = {0,0,0,0};
	seg_uid[0] = g_rand_int_range(rand_uid, G_MININT32, G_MAXINT32);
	seg_uid[1] = g_rand_int_range(rand_uid, G_MININT32, G_MAXINT32);
	seg_uid[2] = g_rand_int_range(rand_uid, G_MININT32, G_MAXINT32);
	seg_uid[3] = g_rand_int_range(rand_uid, G_MININT32, G_MAXINT32);
	CHECK(mk_writeBin(ti, MATROSKA_ID_SEGMENTUID, seg_uid, 16));
	CHECK(mk_writeFloat(ti, MATROSKA_ID_DURATION, 0)); //Track Duration
	/*ti->d_cur - float size(4) + EBML header size(24) + extra empty bytes for segment size(6)*/
	w->duration_ptr = ti->d_cur + 20 + extra;
	CHECK(mk_closeContext(ti, &w->duration_ptr)); /* add ti->parent->d_cur to duration_ptr */

	/*segment tracks start at 4220 (fixed)*/
	pos=4220;
	if ((ti = mk_createContext(w, c, MATROSKA_ID_TRACKS)) == NULL)
		return -1;
	/*VIDEO TRACK entry start at 4226 = 4220 + 4 + 2(tracks header size)*/
	pos+=6;
	if ((ti2 = mk_createContext(w, ti, MATROSKA_ID_TRACKENTRY)) == NULL)
		return -1;
	CHECK(mk_writeUInt(ti2, MATROSKA_ID_TRACKNUMBER, 1));        /* TrackNumber */

	int track_uid1 = g_rand_int_range(rand_uid, G_MININT32, G_MAXINT32);
	CHECK(mk_writeUInt(ti2, MATROSKA_ID_TRACKUID, track_uid1));  /* Track UID  */
	/* TrackType 1 -video 2 -audio */
	CHECK(mk_writeUInt(ti2, MATROSKA_ID_TRACKTYPE, MATROSKA_TRACK_TYPE_VIDEO));
	CHECK(mk_writeUInt(ti2, MATROSKA_ID_TRACKFLAGENABLED, 1));   /* enabled    */
	CHECK(mk_writeUInt(ti2, MATROSKA_ID_TRACKFLAGDEFAULT, 1));   /* default    */
	CHECK(mk_writeUInt(ti2, MATROSKA_ID_TRACKFLAGFORCED, 0));    /* forced     */
	CHECK(mk_writeUInt(ti2, MATROSKA_ID_TRACKFLAGLACING, 0));    /* FlagLacing */
	CHECK(mk_writeUInt(ti2, MATROSKA_ID_TRACKMINCACHE, 1));      /* MinCache   */
	CHECK(mk_writeFloat(ti2, MATROSKA_ID_TRACKTIMECODESCALE, 1));/* Timecode scale (float) */
	CHECK(mk_writeUInt(ti2, MATROSKA_ID_TRACKMAXBLKADDID, 0));   /* Max Block Addition ID  */
	CHECK(mk_writeStr(ti2, MATROSKA_ID_CODECID, codecID));       /* CodecID    */
	CHECK(mk_writeUInt(ti2, MATROSKA_ID_CODECDECODEALL, 1));     /* Codec Decode All       */

	/* CodecPrivate (40 bytes) */
	if (codecPrivateSize)
		CHECK(mk_writeBin(ti2, MATROSKA_ID_CODECPRIVATE, codecPrivate, codecPrivateSize));

	/* Default video frame duration - for fixed frame rate only (reset when closing matroska file)
	* not required by the spec, but at least vlc seems to need it to work properly
	* default duration position will be set after closing the current context */
	int64_t delta_pos = ti2->d_cur;
	CHECK(mk_writeUIntRaw(ti2, MATROSKA_ID_TRACKDEFAULTDURATION, w->def_duration));

	if ((v = mk_createContext(w, ti2, MATROSKA_ID_TRACKVIDEO)) == NULL) /* Video track */
		return -1;
	CHECK(mk_writeUInt(v, MATROSKA_ID_VIDEOPIXELWIDTH, width));
	CHECK(mk_writeUInt(v, MATROSKA_ID_VIDEOPIXELHEIGHT, height));
	CHECK(mk_writeUInt(v, MATROSKA_ID_VIDEOFLAGINTERLACED, 0));  /* interlaced flag */
	CHECK(mk_writeUInt(v, MATROSKA_ID_VIDEODISPLAYWIDTH, d_width));
	CHECK(mk_writeUInt(v, MATROSKA_ID_VIDEODISPLAYHEIGHT, d_height));
	int64_t delta_pos1=v->d_cur;
	CHECK(mk_closeContext(v, &delta_pos1));
	delta_pos = delta_pos1-delta_pos; /* video track header size + default duration entry */
	pos+=ti2->d_cur;
	CHECK(mk_closeContext(ti2, &pos));
	/* default duration position = current postion - (video track header size + def dur. entry) */
	w->def_duration_ptr =  pos-delta_pos;

	if (SampRate > 0)
	{
		/*AUDIO TRACK entry */
		if ((ti3 = mk_createContext(w, ti, MATROSKA_ID_TRACKENTRY)) == NULL)
			return -1;
		CHECK(mk_writeUInt(ti3, MATROSKA_ID_TRACKNUMBER, 2));        /* TrackNumber            */

		int track_uid2 = g_rand_int_range(rand_uid, G_MININT32, G_MAXINT32);
		CHECK(mk_writeUInt(ti3, MATROSKA_ID_TRACKUID, track_uid2));
		/* TrackType 1 -video 2 -audio */
		CHECK(mk_writeUInt(ti3, MATROSKA_ID_TRACKTYPE, MATROSKA_TRACK_TYPE_AUDIO));
		CHECK(mk_writeUInt(ti3, MATROSKA_ID_TRACKFLAGENABLED, 1));   /* enabled                */
		CHECK(mk_writeUInt(ti3, MATROSKA_ID_TRACKFLAGDEFAULT, 1));   /* default                */
		CHECK(mk_writeUInt(ti3, MATROSKA_ID_TRACKFLAGFORCED, 0));    /* forced                 */
		CHECK(mk_writeUInt(ti3, MATROSKA_ID_TRACKFLAGLACING, 0));    /* FlagLacing             */
		CHECK(mk_writeUInt(ti3, MATROSKA_ID_TRACKMINCACHE, 0));      /* MinCache               */
		CHECK(mk_writeFloat(ti3, MATROSKA_ID_TRACKTIMECODESCALE, 1));/* Timecode scale (float) */
		CHECK(mk_writeUInt(ti3, MATROSKA_ID_TRACKMAXBLKADDID, 0));   /* Max Block Addition ID  */
		CHECK(mk_writeStr(ti3, MATROSKA_ID_CODECID, AcodecID));      /* CodecID                */
		CHECK(mk_writeUInt(ti3, MATROSKA_ID_CODECDECODEALL, 1));     /* Codec Decode All       */

		if (default_aframe_duration) /* for fixed sample rate */
			CHECK(mk_writeUInt(ti3, MATROSKA_ID_TRACKDEFAULTDURATION, default_aframe_duration)); /* DefaultDuration audio*/

		if ((a = mk_createContext(w, ti3, MATROSKA_ID_TRACKAUDIO)) == NULL) /* Audio track */
			return -1;
		CHECK(mk_writeFloat(a, MATROSKA_ID_AUDIOSAMPLINGFREQ, SampRate));
		CHECK(mk_writeUInt(a, MATROSKA_ID_AUDIOCHANNELS, channels));
		if (bitsSample > 0) /* for pcm and aac (16) */
			CHECK(mk_writeUInt(a, MATROSKA_ID_AUDIOBITDEPTH, bitsSample));
		CHECK(mk_closeContext(a, 0));
		/* AudioCodecPrivate */
		if (AcodecPrivateSize)
			CHECK(mk_writeBin(ti3, MATROSKA_ID_CODECPRIVATE, AcodecPrivate, AcodecPrivateSize));
		CHECK(mk_closeContext(ti3, 0));
	}
	else
	{
		w->video_only = 1;
	}

	g_rand_free(rand_uid); /* free random uid generator */
	CHECK(mk_closeContext(ti, 0));
	CHECK(mk_writeVoid(c, 1024));
	CHECK(mk_closeContext(c, 0));
	CHECK(mk_flushContextData(w->root));
	w->cluster_index = 1;
	w->cluster_pos = g_renew(int64_t, w->cluster_pos, w->cluster_index);
	w->cluster_pos[0] = ftello64(w->fp) -36;
	w->wrote_header = TRUE;

	return 0;
}

static int mk_closeCluster(mk_Writer *w)
{
	if (w->cluster == NULL)
		return 0;
	CHECK(mk_closeContext(w->cluster, 0));
	w->cluster = NULL;
	CHECK(mk_flushContextData(w->root));
	w->cluster_index++;
	w->cluster_pos = g_renew(int64_t, w->cluster_pos, w->cluster_index);
	w->cluster_pos[w->cluster_index-1] = ftello64(w->fp) - 36;
	return 0;
}


static int mk_flushFrame(mk_Writer *w)
{
	//int64_t ref = 0;
	int64_t delta = 0;
	int64_t fsize, bgsize;
	unsigned char c_delta_flags[3];

	if (!w->in_frame)
		return 0;

	delta = w->frame_tc / w->timescale - w->cluster_tc_scaled;

	if (w->cluster == NULL)
	{
		w->cluster_tc_scaled = w->frame_tc / w->timescale ;
		w->cluster = mk_createContext(w, w->root, MATROSKA_ID_CLUSTER); /* New Cluster */
		if (w->cluster == NULL)
			return -1;

		CHECK(mk_writeUInt(w->cluster, MATROSKA_ID_CLUSTERTIMECODE, w->cluster_tc_scaled)); /* Timecode */

		delta = 0;
		w->block_n=0;
	}

	fsize = w->frame ? w->frame->d_cur : 0;
	bgsize = fsize + 4 + mk_ebmlSizeSize(fsize + 4) + 1;
	//if (!w->keyframe)
	//{
	//	ref = w->prev_frame_tc_scaled - w->cluster_tc_scaled - delta;
	//	bgsize += 1 + 1 + mk_ebmlSIntSize(ref);
	//}

	CHECK(mk_writeID(w->cluster, MATROSKA_ID_BLOCKGROUP)); /* BlockGroup */
	CHECK(mk_writeSize(w->cluster, bgsize));
	CHECK(mk_writeID(w->cluster, MATROSKA_ID_BLOCK)); /* Block */
	w->block_n++;
	CHECK(mk_writeSize(w->cluster, fsize + 4));
	CHECK(mk_writeSize(w->cluster, 1)); /* track number (for guvcview 1-video  2-audio) */

	c_delta_flags[0] = delta >> 8; //timestamp ref to cluster
	c_delta_flags[1] = delta;      //timestamp ref to cluster
	c_delta_flags[2] = 0; // frame flags
	CHECK(mk_appendContextData(w->cluster, c_delta_flags, 3)); /* block timecode */
	if (w->frame)
	{
		CHECK(mk_appendContextData(w->cluster, w->frame->data, w->frame->d_cur));
		w->frame->d_cur = 0;
	}
	//if (!w->keyframe)
	//	CHECK(mk_writeSInt(w->cluster, MATROSKA_ID_BLOCKREFERENCE, ref)); /* ReferenceBlock */

	w->in_frame = FALSE; /* current frame processed */

	w->prev_frame_tc_scaled = w->cluster_tc_scaled + delta;

	/*******************************/
	if (delta > 32767ll || delta < -32768ll || (w->cluster->d_cur) > CLSIZE)
	{
		CHECK(mk_closeCluster(w));
	}
	/*******************************/

	return 0;
}


static int mk_flushAudioFrame(mk_Writer *w)
{
	int64_t delta = 0;
	int64_t fsize, bgsize;
	unsigned char c_delta_flags[3];
	//unsigned char flags = 0x04; //lacing
	//unsigned char framesinlace = 0x07; //FIXME:  total frames -1

	delta = w->audio_frame_tc / w->timescale - w->cluster_tc_scaled;
	/* make sure we have a cluster */
	if (w->cluster == NULL)
	{
		w->cluster_tc_scaled = w->audio_frame_tc / w->timescale ;
		w->cluster = mk_createContext(w, w->root, MATROSKA_ID_CLUSTER); /* Cluster */
		if (w->cluster == NULL)
			return -1;

		CHECK(mk_writeUInt(w->cluster, MATROSKA_ID_CLUSTERTIMECODE, w->cluster_tc_scaled)); /* Timecode */

		delta = 0;
		w->block_n=0;
	}


	if (!w->audio_in_frame)
		return 0;

	fsize = w->audio_frame ? w->audio_frame->d_cur : 0;
	bgsize = fsize + 4 + mk_ebmlSizeSize(fsize + 4) + 1;
	CHECK(mk_writeID(w->cluster, MATROSKA_ID_BLOCKGROUP)); /* BlockGroup */
	CHECK(mk_writeSize(w->cluster, bgsize));
	CHECK(mk_writeID(w->cluster, MATROSKA_ID_BLOCK)); /* Block */
	w->block_n++;
	if(w->audio_block == 0)
	{
		w->audio_block = w->block_n;
	}
	CHECK(mk_writeSize(w->cluster, fsize + 4));
	CHECK(mk_writeSize(w->cluster, 2)); /* track number (1-video  2-audio) */
	c_delta_flags[0] = delta >> 8;
	c_delta_flags[1] = delta;
	c_delta_flags[2] = 0;
	CHECK(mk_appendContextData(w->cluster, c_delta_flags, 3)); /* block timecode (delta to cluster tc) */
	if (w->audio_frame)
	{
		CHECK(mk_appendContextData(w->cluster, w->audio_frame->data, w->audio_frame->d_cur));
		w->audio_frame->d_cur = 0;
	}

	w->audio_in_frame = FALSE; /* current frame processed */

	w->audio_prev_frame_tc_scaled = w->cluster_tc_scaled + delta;

	/*******************************/
	if (delta > 32767ll || delta < -32768ll || (w->cluster->d_cur) > CLSIZE)
	{
		CHECK(mk_closeCluster(w));
	}
	/*******************************/

	return 0;
}

static int write_cues(mk_Writer *w)
{
	mk_Context *cpe, *tpe;
	//printf("writng cues\n");
	w->cues = mk_createContext(w, w->root, MATROSKA_ID_CUES); /* Cues */
	if (w->cues == NULL)
		return -1;
	cpe = mk_createContext(w, w->cues, MATROSKA_ID_POINTENTRY); /* Cues */
	if (cpe == NULL)
		return -1;
	CHECK(mk_writeUInt(cpe, MATROSKA_ID_CUETIME, 0)); /* Cue Time */
	tpe = mk_createContext(w, cpe, MATROSKA_ID_CUETRACKPOSITION); /* track position */
	if (tpe == NULL)
		return -1;
	CHECK(mk_writeUInt(tpe, MATROSKA_ID_CUETRACK, 1)); /* Cue track video */
	CHECK(mk_writeUInt(tpe, MATROSKA_ID_CUECLUSTERPOSITION, w->cluster_pos[0]));
	CHECK(mk_writeUInt(tpe, MATROSKA_ID_CUEBLOCKNUMBER, 1));
	CHECK(mk_closeContext(tpe, 0));
	if(!(w->video_only) && (w->audio_block > 0))
	{
		tpe = mk_createContext(w, cpe, MATROSKA_ID_CUETRACKPOSITION); /* track position */
		if (tpe == NULL)
			return -1;
		CHECK(mk_writeUInt(tpe, MATROSKA_ID_CUETRACK, 2)); /* Cue track audio */
		CHECK(mk_writeUInt(tpe, MATROSKA_ID_CUECLUSTERPOSITION, w->cluster_pos[0]));
		CHECK(mk_writeUInt(tpe, MATROSKA_ID_CUEBLOCKNUMBER, w->audio_block));
		CHECK(mk_closeContext(tpe, 0));
	}
	CHECK(mk_closeContext(cpe, 0));
	CHECK(mk_closeContext(w->cues, 0));
	if(mk_flushContextData(w->root) < 0)
		return -1;
	return 0;
}

static int write_SeekHead(mk_Writer *w)
{
	mk_Context *sk,*se;
	int i=0;

	if ((sk = mk_createContext(w, w->root, MATROSKA_ID_SEEKHEAD)) == NULL) /* SeekHead */
		return -1;
	for(i=0; i<(w->cluster_index-1); i++)
	{
		if ((se = mk_createContext(w, sk, MATROSKA_ID_SEEKENTRY)) == NULL) /* Seek */
			return -1;
		CHECK(mk_writeUInt(se, MATROSKA_ID_SEEKID, MATROSKA_ID_CLUSTER));  /* seekID */
		CHECK(mk_writeUInt(se, MATROSKA_ID_SEEKPOSITION, w->cluster_pos[i])); /* FIXME:  SeekPosition (should not be hardcoded) */
		CHECK(mk_closeContext(se, 0));
	}
	CHECK(mk_closeContext(sk, 0));
	if(mk_flushContextData(w->root) < 0)
		return -1;
	return 0;
}

static int write_SegSeek(mk_Writer *w, int64_t cues_pos, int64_t seekHeadPos)
{
	mk_Context *sh, *se;

	if ((sh = mk_createContext(w, w->root, MATROSKA_ID_SEEKHEAD)) == NULL) /* SeekHead */
		return -1;
	if ((se = mk_createContext(w, sh, MATROSKA_ID_SEEKENTRY)) == NULL)     /* Seek */
		return -1;
	CHECK(mk_writeUInt (se, MATROSKA_ID_SEEKID, MATROSKA_ID_INFO));        /* seekID */
	CHECK(mk_writeUInt(se, MATROSKA_ID_SEEKPOSITION, 4135-(w->seekhead_pos)));
	CHECK(mk_closeContext(se, 0));

	if ((se = mk_createContext(w, sh, MATROSKA_ID_SEEKENTRY)) == NULL)     /* Seek */
		return -1;
	CHECK(mk_writeUInt(se, MATROSKA_ID_SEEKID, MATROSKA_ID_TRACKS));       /* seekID */
	CHECK(mk_writeUInt(se, MATROSKA_ID_SEEKPOSITION, 4220-(w->seekhead_pos)));
	CHECK(mk_closeContext(se, 0));

	//printf("cues@%d seekHead@%d\n", cues_pos, seekHeadPos);
	if ((se = mk_createContext(w, sh, MATROSKA_ID_SEEKENTRY)) == NULL) /* Seek */
		return -1;
	CHECK(mk_writeUInt(se, MATROSKA_ID_SEEKID, MATROSKA_ID_SEEKHEAD)); /* seekID */
	CHECK(mk_writeUInt(se, MATROSKA_ID_SEEKPOSITION, seekHeadPos));
	CHECK(mk_closeContext(se, 0));
	if ((se = mk_createContext(w, sh, MATROSKA_ID_SEEKENTRY)) == NULL) /* Seek */
		return -1;
	CHECK(mk_writeUInt(se, MATROSKA_ID_SEEKID, MATROSKA_ID_CUES));  /* seekID */
	CHECK(mk_writeUInt(se, MATROSKA_ID_SEEKPOSITION, cues_pos));
	CHECK(mk_closeContext(se, 0));
	CHECK(mk_closeContext(sh, 0));

	if(mk_flushContextData(w->root) < 0)
	return -1;

	CHECK(mk_writeVoid(w->root, 4135 - (ftello64(w->fp)+3)));
	if(mk_flushContextData(w->root) < 0)
		return -1;
	return 0;
}

void mk_setDef_Duration(mk_Writer *w, UINT64 def_duration)
{
	w->def_duration = def_duration;
}

int mk_startFrame(mk_Writer *w)
{
	if (mk_flushFrame(w) < 0)
		return -1;

	w->in_frame = TRUE; /*first frame will have size zero (don't write it)*/
	w->keyframe = FALSE;

	return 0;
}

int mk_startAudioFrame(mk_Writer *w)
{
	if (mk_flushAudioFrame(w) < 0)
		return -1;

	return 0;
}

int mk_setFrameFlags(mk_Writer *w,int64_t timestamp, int keyframe)
{
	if (!w->in_frame)
		return -1;

	w->frame_tc = timestamp;
	w->keyframe = (keyframe != 0);

	if (w->max_frame_tc < timestamp)
		w->max_frame_tc = timestamp;

	return 0;
}

int mk_setAudioFrameFlags(mk_Writer *w,int64_t timestamp, int keyframe)
{
	if (!w->audio_in_frame)
		return -1;

	w->audio_frame_tc = timestamp;

	return 0;
}


int mk_addFrameData(mk_Writer *w, const void *data, int64_t size)
{
	//if (!w->in_frame)
	//	return -1;

	if (w->frame == NULL)
		if ((w->frame = mk_createContext(w, NULL, 0)) == NULL)
			return -1;

	w->in_frame = TRUE;

	return mk_appendContextData(w->frame, data, size);
}

int mk_addAudioFrameData(mk_Writer *w, const void *data, int64_t size)
{
	//if (!w->audio_in_frame)
	//	return -1;

	if (w->audio_frame == NULL)
		if ((w->audio_frame = mk_createContext(w, NULL, 0)) == NULL)
			return -1;

	w->audio_in_frame = TRUE;

	return mk_appendContextData(w->audio_frame, data, size);
}

int mk_close(mk_Writer *w)
{
	int ret = 0;
	if (mk_flushFrame(w) < 0 || mk_flushAudioFrame(w) < 0 || mk_closeCluster(w) < 0)
		ret = -1;
	if (w->wrote_header)
	{
		/* move to end of file */
		fseeko(w->fp, 0, SEEK_END);
		/* store last position */
		int64_t CuesPos = ftello64(w->fp) - 36;
		//printf("cues at %llu\n",(unsigned long long) CuesPos);
		write_cues(w);
		/* move to end of file */
		fseeko(w->fp, 0, SEEK_END);
		int64_t SeekHeadPos = ftello64(w->fp) - 36;
		//printf("SeekHead at %llu\n",(unsigned long long) SeekHeadPos);
		/* write seekHead */
		write_SeekHead(w);
		/* move to end of file */
		fseeko(w->fp, 0, SEEK_END);
		int64_t lLastPos = ftello64(w->fp);
		int64_t seg_size = lLastPos - (w->segment_size_ptr);
		seg_size |= 0x0100000000000000LL;
		/* move to segment entry */
		fseeko(w->fp, w->segment_size_ptr, SEEK_SET);
		if (mk_writeSegPos (w->root, seg_size ) < 0 || mk_flushContextData(w->root) < 0)
			ret = -1;
		/* move to seekentries */
		fseeko(w->fp, w->seekhead_pos, SEEK_SET);
		write_SegSeek (w, CuesPos, SeekHeadPos);
		/* move to default video frame duration entry and set the correct value*/
		fseeko(w->fp, w->def_duration_ptr, SEEK_SET);
		if ((mk_writeUIntRaw(w->root, MATROSKA_ID_TRACKDEFAULTDURATION, w->def_duration)) < 0 ||
			mk_flushContextData(w->root) < 0)
				ret = -1;

		/* move to segment duration entry */
		fseeko(w->fp, w->duration_ptr, SEEK_SET);
		if (mk_writeFloatRaw(w->root, (float)(double)(w->max_frame_tc/ w->timescale)) < 0 ||
			mk_flushContextData(w->root) < 0)
				ret = -1;
	}
	mk_destroyContexts(w);
	g_free(w->cluster_pos);
	fflush(w->fp); /* flush the file stream to the file system */
	if(fsync(fileno(w->fp)) || fclose(w->fp)) /* make sure we actually write do disk and then close */
		perror("MATROSKA ERROR - couldn't write to matroska file\n");
	g_free(w);
	w = NULL;
	printf("closed matroska file\n");
	return ret;
}

