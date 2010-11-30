/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
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
#include "matroska.h"

#define CLSIZE    1048576
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

mk_Writer *mk_createWriter(const char *filename)
{
	mk_Writer *w = g_new0(mk_Writer, 1);

	if (w == NULL)
		return NULL;

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
	CHECK(mk_writeStr(c, EBML_ID_DOCTYPE, "matroska"));     // DocType
	CHECK(mk_writeUInt(c, EBML_ID_DOCTYPEVERSION, 2));      // DocTypeVersion
	CHECK(mk_writeUInt(c, EBML_ID_DOCTYPEREADVERSION, 2));  // DocTypeReadversion
	pos=c->d_cur;
	CHECK(mk_closeContext(c, &pos));

	/* SEGMENT starts at position 24  */
	if ((c = mk_createContext(w, w->root, MATROSKA_ID_SEGMENT)) == NULL)
		return -1;
	pos+=4; /*pos=28*/

	w->segment_size_ptr = pos;
	/*needs full segment size here (including clusters) but we only know the header size for now.(2 bytes)*/
	/*add extra (6) bytes - reserve a total of 8 bytes for segment size in ebml format =6+2(header size)  */
	mk_appendContextData(c, empty, extra);
	pos+=extra+2;/*(pos=36) account 2 bytes from header size -these will only be available after closing c context*/

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
	CHECK(mk_closeContext(ti, &pos)); /* pos=71 */
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
	w->cluster_pos[0] = ftello(w->fp) -36;
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
	w->cluster_pos[w->cluster_index-1] = ftello(w->fp) - 36;
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

	CHECK(mk_writeVoid(w->root, 4135 - (ftello(w->fp)+3)));
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
		int64_t CuesPos = ftello (w->fp) - 36;
		//printf("cues at %llu\n",(unsigned long long) CuesPos);
		write_cues(w);
		/* move to end of file */
		fseeko(w->fp, 0, SEEK_END);
		int64_t SeekHeadPos = ftello (w->fp) - 36;
		//printf("SeekHead at %llu\n",(unsigned long long) SeekHeadPos);
		/* write seekHead */
		write_SeekHead(w);
		/* move to end of file */
		fseeko(w->fp, 0, SEEK_END);
		int64_t lLastPos = ftello (w->fp);
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

