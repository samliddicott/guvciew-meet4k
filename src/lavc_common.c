/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           George Sedov <radist.morse@gmail.com>                               #
#                   - Threaded encoding                                         #
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

#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gstdio.h>
#include <string.h>
#include "lavc_common.h"
#include "v4l2uvc.h"
#include "vcodecs.h"
#include "acodecs.h"
#include "sound.h"

#   define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])

int avpriv_split_xiph_headers(uint8_t *extradata, int extradata_size,
                         int first_header_size, uint8_t *header_start[3],
                         int header_len[3])
{
    int i;

     if (extradata_size >= 6 && AV_RB16(extradata) == first_header_size) {
        int overall_len = 6;
        for (i=0; i<3; i++) {
            header_len[i] = AV_RB16(extradata);
            extradata += 2;
            header_start[i] = extradata;
            extradata += header_len[i];
            if (overall_len > extradata_size - header_len[i])
                return -1;
            overall_len += header_len[i];
        }
    } else if (extradata_size >= 3 && extradata_size < INT_MAX - 0x1ff && extradata[0] == 2) {
        int overall_len = 3;
        extradata++;
        for (i=0; i<2; i++, extradata++) {
            header_len[i] = 0;
            for (; overall_len < extradata_size && *extradata==0xff; extradata++) {
                header_len[i] += 0xff;
                overall_len   += 0xff + 1;
            }
            header_len[i] += *extradata;
            overall_len   += *extradata;
            if (overall_len > extradata_size)
                return -1;
        }
        header_len[2] = extradata_size - overall_len;
        header_start[0] = extradata;
        header_start[1] = header_start[0] + header_len[0];
        header_start[2] = header_start[1] + header_len[1];
    } else {
        return -1;
    }
    return 0;
}

static void yuv422to420p(BYTE* pic, struct lavcData* data )
{
	int i,j;
	int width = data->codec_context->width;
	int height = data->codec_context->height;
	int linesize=width*2;
	int size = width * height;

	BYTE *y;
	BYTE *y1;
	BYTE *u;
	BYTE* v;
	y = data->tmpbuf;
	y1 = data->tmpbuf + width;
	u = data->tmpbuf + size;
	v = u + size/4;

	for(j=0;j<(height-1);j+=2)
	{
		for(i=0;i<(linesize-3);i+=4)
		{
			*y++ = pic[i+j*linesize];
			*y++ = pic[i+2+j*linesize];
			*y1++ = pic[i+(j+1)*linesize];
			*y1++ = pic[i+2+(j+1)*linesize];
			*u++ = (pic[i+1+j*linesize] + pic[i+1+(j+1)*linesize])>>1; // div by 2
			*v++ = (pic[i+3+j*linesize] + pic[i+3+(j+1)*linesize])>>1;
		}
		y += width;
		y1 += width;//2 lines
	}

	data->picture->data[0] = data->tmpbuf;                    //Y
	data->picture->data[1] = data->tmpbuf + size;             //U
	data->picture->data[2] = data->picture->data[1] + size/4; //V
	data->picture->linesize[0] = data->codec_context->width;
	data->picture->linesize[1] = data->codec_context->width / 2;
	data->picture->linesize[2] = data->codec_context->width / 2;
}

static void nv12to420p(BYTE* pic, struct lavcData* data )
{
	int width = data->codec_context->width;
	int height = data->codec_context->height;
	int size = width * height;

	/*FIXME: do we really need this or can we use pic directly ?*/
	data->tmpbuf = memcpy(data->tmpbuf, pic, (width*height*3)/2);

	data->picture->data[0] = data->tmpbuf;                    //Y
	data->picture->data[1] = data->tmpbuf + size;             //U
	data->picture->data[2] = data->picture->data[1] + size/4; //V
	data->picture->linesize[0] = data->codec_context->width;
	data->picture->linesize[1] = data->codec_context->width / 2;
	data->picture->linesize[2] = data->codec_context->width / 2;
}

static void nv21to420p(BYTE* pic, struct lavcData* data )
{
	int width = data->codec_context->width;
	int height = data->codec_context->height;
	int size = width * height;

	/*FIXME: do we really need this or can we use pic directly ?*/
	data->tmpbuf = memcpy(data->tmpbuf, pic, (width*height*3)/2);

	data->picture->data[0] = data->tmpbuf;                    //Y
	data->picture->data[2] = data->tmpbuf + size;             //V
	data->picture->data[1] = data->picture->data[2] + size/4; //U
	data->picture->linesize[0] = data->codec_context->width;
	data->picture->linesize[1] = data->codec_context->width / 2;
	data->picture->linesize[2] = data->codec_context->width / 2;
}

int encode_lavc_frame (BYTE *picture_buf, struct lavcData* data , int format, struct VideoFormatData *videoF)
{
	int out_size = 0;

	if(!data->codec_context)
	{
		data->flush_done = 1;
		return out_size;
	}
	videoF->vblock_align = data->codec_context->block_align;
	//videoF->avi->time_base_num = data->codec_context->time_base.num;
	//videoF->avi->time_base_den = data->codec_context->time_base.den;

	//convert to 4:2:0
	switch (format)
	{
		case V4L2_PIX_FMT_NV12:
			nv12to420p(picture_buf, data );
			break;

		case V4L2_PIX_FMT_NV21:
			nv21to420p(picture_buf, data );
			break;

		default:
			yuv422to420p(picture_buf, data );
			break;
	}
	/* encode the image */

	//videoF->frame_number++;

	if(!data->monotonic_pts) //generate a real pts based on the frame timestamp
		data->picture->pts += ((videoF->vpts - videoF->old_vpts)/1000) * 90;
	else  //generate a true monotonic pts based on the codec fps
		data->picture->pts += (data->codec_context->time_base.num*1000/data->codec_context->time_base.den) * 90;

	videoF->old_vpts = videoF->vpts;

	if(data->flush_delayed_frames)
	{
		//pkt.size = 0;
		if(!data->flushed_buffers)
		{
			avcodec_flush_buffers(data->codec_context);
			data->flushed_buffers = 1;
		}

 	}

#if LIBAVCODEC_VER_AT_LEAST(54,01)
	AVPacket pkt;
    int got_packet = 0;
    av_init_packet(&pkt);
	pkt.data = data->outbuf;
	pkt.size = data->outbuf_size;

    //if(data->outbuf_size < FF_MIN_BUFFER_SIZE)
    //{
	//	av_log(avctx, AV_LOG_ERROR, "buffer smaller than minimum size\n");
    //    return -1;
    //}
 	int ret = 0;
 	if(!data->flush_delayed_frames)
    	ret = avcodec_encode_video2(data->codec_context, &pkt, data->picture, &got_packet);
   	else
   		ret = avcodec_encode_video2(data->codec_context, &pkt, NULL, &got_packet);

    if (!ret && got_packet && data->codec_context->coded_frame)
    {
		// Do we really need to set this ???
    	data->codec_context->coded_frame->pts       = pkt.pts;
        data->codec_context->coded_frame->key_frame = !!(pkt.flags & AV_PKT_FLAG_KEY);
    }

	videoF->vdts = pkt.dts;
	videoF->vflags = pkt.flags;
	videoF->vduration = pkt.duration;

    /* free any side data since we cannot return it */
    if (pkt.side_data_elems > 0)
    {
    	int i;
        for (i = 0; i < pkt.side_data_elems; i++)
        	av_free(pkt.side_data[i].data);
        av_freep(&pkt.side_data);
        pkt.side_data_elems = 0;
    }

    out_size = pkt.size;
#else
	if(!data->flush_delayed_frames)
		out_size = avcodec_encode_video(data->codec_context, data->outbuf, data->outbuf_size, data->picture);
	else
		out_size = avcodec_encode_video(data->codec_context, data->outbuf, data->outbuf_size, NULL);

	videoF->vflags = 0;
	if (data->codec_context->coded_frame->key_frame)
		videoF->vflags |= AV_PKT_FLAG_KEY;
	videoF->vdts = AV_NOPTS_VALUE;
	videoF->vduration = videoF->vpts - videoF->old_vpts;
#endif

	 if(data->flush_delayed_frames && out_size == 0)
    	data->flush_done = 1;

	if(out_size == 0 && data->index_of_df < 0)
	{
	    data->delayed_pts[data->delayed_frames] = videoF->vpts;
	    data->delayed_frames++;
	    if(data->delayed_frames > MAX_DELAYED_FRAMES)
	    {
	    	data->delayed_frames = MAX_DELAYED_FRAMES;
	    	printf("WARNING: Maximum of %i delayed video frames reached...\n", MAX_DELAYED_FRAMES);
	    }
	}
	else
	{
		if(data->delayed_frames > 0)
		{
			if(data->index_of_df < 0)
			{
				data->index_of_df = 0;
				printf("WARNING: video codec is using %i delayed video frames\n", data->delayed_frames);
			}
			INT64 pts = videoF->vpts;
			videoF->vpts = data->delayed_pts[data->index_of_df];
			data->delayed_pts[data->index_of_df] = pts;
			data->index_of_df++;
			if(data->index_of_df >= data->delayed_frames)
				data->index_of_df = 0;
		}
	}
	return (out_size);
}

int encode_lavc_audio_frame (void *audio_buf, struct lavcAData* data, struct VideoFormatData *videoF)
{
	int out_size = 0;
	int ret = 0;

	videoF->ablock_align = data->codec_context->block_align;

	/* encode the audio */
#if LIBAVCODEC_VER_AT_LEAST(53,34)
	AVPacket pkt;
	int got_packet;
	av_init_packet(&pkt);
	pkt.data = data->outbuf;
	pkt.size = data->outbuf_size;

	data->frame->nb_samples  = data->codec_context->frame_size;
	int samples_size = av_samples_get_buffer_size(NULL, data->codec_context->channels,
		                                          data->frame->nb_samples,
                                                  data->codec_context->sample_fmt, 1);

    avcodec_fill_audio_frame(data->frame, data->codec_context->channels,
                                   data->codec_context->sample_fmt,
                                   (const uint8_t *) audio_buf, samples_size, 1);
	if(!data->monotonic_pts) //generate a real pts based on the frame timestamp
		data->frame->pts += ((videoF->apts - videoF->old_apts)/1000) * 90;
	else  //generate a true monotonic pts based on the codec fps
		data->frame->pts += (data->codec_context->time_base.num*1000/data->codec_context->time_base.den) * 90;

	ret = avcodec_encode_audio2(data->codec_context, &pkt, data->frame, &got_packet);

	if (!ret && got_packet && data->codec_context->coded_frame)
    {
    	data->codec_context->coded_frame->pts       = pkt.pts;
        data->codec_context->coded_frame->key_frame = !!(pkt.flags & AV_PKT_FLAG_KEY);
    }

	videoF->adts = pkt.dts;
	videoF->aflags = pkt.flags;
	videoF->aduration = pkt.duration;

	/* free any side data since we cannot return it */
	//ff_packet_free_side_data(&pkt);
	if (data->frame && data->frame->extended_data != data->frame->data)
		av_freep(data->frame->extended_data);

	out_size = pkt.size;
#else
	out_size = avcodec_encode_audio(data->codec_context, data->outbuf, data->outbuf_size, audio_buf);
	videoF->adts = AV_NOPTS_VALUE;
	videoF->aflags = 0;
	if (data->codec_context->coded_frame->key_frame)
		videoF->aflags |= AV_PKT_FLAG_KEY;
	videoF->aduration = videoF->apts - videoF->old_apts;;
#endif
	return (out_size);
}

int clean_lavc (void* arg)
{
	struct lavcData** data= (struct lavcData**) arg;
	//int enc_frames =0;
	if(*data)
	{
		if((*data)->priv_data != NULL)
			g_free((*data)->priv_data);

		//close codec
		if((*data)->codec_context)
		{
			//enc_frames = (*data)->codec_context->real_pict_num;
			if(!(*data)->flushed_buffers)
			{
				avcodec_flush_buffers((*data)->codec_context);
				(*data)->flushed_buffers = 1;
			}
			avcodec_close((*data)->codec_context);
#if LIBAVCODEC_VER_AT_LEAST(53,6)
		//free private options;
		struct lavcData *pdata = *data;
		av_dict_free(&(pdata->private_options));
#endif
		//free codec context
		g_free((*data)->codec_context);
	}
		(*data)->codec_context = NULL;
		if((*data)->tmpbuf)
			g_free((*data)->tmpbuf);
		if((*data)->outbuf)
			g_free((*data)->outbuf);
		if((*data)->picture)
			g_free((*data)->picture);
		g_free(*data);
		*data = NULL;
	}
	return (0);
}

int clean_lavc_audio (void* arg)
{
	struct lavcAData** data= (struct lavcAData**) arg;
	//int enc_frames =0;
	if(*data)
	{
		if((*data)->priv_data != NULL)
			g_free((*data)->priv_data);
		//enc_frames = (*data)->codec_context->real_pict_num;
		avcodec_flush_buffers((*data)->codec_context);
		//close codec
		avcodec_close((*data)->codec_context);
		//free codec context
		g_free((*data)->codec_context);
		(*data)->codec_context = NULL;
		g_free((*data)->outbuf);
		g_free((*data)->frame);
		g_free(*data);
		*data = NULL;
	}
	return (0);
}

struct lavcData* init_lavc(int width, int height, int fps_num, int fps_den, int codec_ind, int format, int frame_flags)
{
	//allocate
	struct lavcData* data = g_new0(struct lavcData, 1);
	data->priv_data = NULL;

	vcodecs_data *defaults = get_codec_defaults(codec_ind);
	data->monotonic_pts = defaults->monotonic_pts;

	if( format == V4L2_PIX_FMT_H264 &&
		get_vcodec_id(codec_ind) == AV_CODEC_ID_H264 &&
		frame_flags == 0)
		return(data); //we only need the private data in this case

	data->codec_context = NULL;

	data->codec = NULL;
	// find the video encoder
	//try specific codec (by name)
	data->codec = avcodec_find_encoder_by_name(defaults->codec_name);

	if(!data->codec) //if it fails try any codec with matching AV_CODEC_ID
		data->codec = avcodec_find_encoder(defaults->codec_id);

	if (!data->codec)
	{
		fprintf(stderr, "ffmpeg codec not found\n");
		g_free(data);
		return(NULL);
	}
#if LIBAVCODEC_VER_AT_LEAST(53,6)
	data->codec_context = avcodec_alloc_context3(data->codec);
	avcodec_get_context_defaults3 (data->codec_context, data->codec);
#else
	data->codec_context = avcodec_alloc_context();
#endif
	data->codec_id = defaults->codec_id;

	//alloc picture
	data->picture= avcodec_alloc_frame();
	data->picture->pts = 0;
	// define bit rate (lower = more compression but lower quality)
	data->codec_context->bit_rate = defaults->bit_rate;
	// resolution must be a multiple of two
	data->codec_context->width = width;
	data->codec_context->height = height;

	data->codec_context->flags |= defaults->flags;
	if (defaults->num_threads > 0)
		data->codec_context->thread_count = defaults->num_threads;

	/*
	* mb_decision
	*0 (FF_MB_DECISION_SIMPLE) Use mbcmp (default).
	*1 (FF_MB_DECISION_BITS)   Select the MB mode which needs the fewest bits (=vhq).
	*2 (FF_MB_DECISION_RD)     Select the MB mode which has the best rate distortion.
	*/
	data->codec_context->mb_decision = defaults->mb_decision;
	/*use trellis quantization*/
	data->codec_context->trellis = defaults->trellis;

	//motion estimation method epzs
	data->codec_context->me_method = defaults->me_method;

	data->codec_context->dia_size = defaults->dia;
	data->codec_context->pre_dia_size = defaults->pre_dia;
	data->codec_context->pre_me = defaults->pre_me;
	data->codec_context->me_pre_cmp = defaults->me_pre_cmp;
	data->codec_context->me_cmp = defaults->me_cmp;
	data->codec_context->me_sub_cmp = defaults->me_sub_cmp;
	data->codec_context->me_subpel_quality = defaults->subq; //NEW
	data->codec_context->refs = defaults->framerefs;         //NEW
	data->codec_context->last_predictor_count = defaults->last_pred;

	data->codec_context->mpeg_quant = defaults->mpeg_quant; //h.263
	data->codec_context->qmin = defaults->qmin;             // best detail allowed - worst compression
	data->codec_context->qmax = defaults->qmax;             // worst detail allowed - best compression
	data->codec_context->max_qdiff = defaults->max_qdiff;
	data->codec_context->max_b_frames = defaults->max_b_frames;

	data->codec_context->qcompress = defaults->qcompress;
	data->codec_context->qblur = defaults->qblur;
	data->codec_context->strict_std_compliance = FF_COMPLIANCE_NORMAL;
	data->codec_context->codec_id = defaults->codec_id;

#if !LIBAVCODEC_VER_AT_LEAST(53,0)
#define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
#endif
	data->codec_context->codec_type = AVMEDIA_TYPE_VIDEO;

	data->codec_context->pix_fmt = PIX_FMT_YUV420P; //only yuv420p available for mpeg
	if(defaults->fps)
		data->codec_context->time_base = (AVRational){1,defaults->fps}; //use codec properties fps
	else if (fps_den >= 5)
		data->codec_context->time_base = (AVRational){fps_num,fps_den}; //default fps (for gspca this is 1/1)
	else data->codec_context->time_base = (AVRational){1,15}; //fallback to 15 fps (e.g gspca)

    if(defaults->gop_size > 0)
        data->codec_context->gop_size = defaults->gop_size;
    else
        data->codec_context->gop_size = data->codec_context->time_base.den;

	if(defaults->codec_id == AV_CODEC_ID_H264)
	{
	    data->codec_context->me_range = 16;
	    //the first compressed frame will be empty (1 frame out of sync)
	    //but avoids x264 warning on lookaheadless mb-tree
#if LIBAVCODEC_VER_AT_LEAST(53,6)
	    av_dict_set(&data->private_options, "rc_lookahead", "1", 0);
#else
	    data->codec_context->rc_lookahead=1;
#endif
        //TODO:
        // add rc_lookahead to codec properties and handle it gracefully by
        // fixing the frames timestamps => shift them by rc_lookahead frames
	}

	// open codec
#if LIBAVCODEC_VER_AT_LEAST(53,6)
	if (avcodec_open2(data->codec_context, data->codec, &data->private_options) < 0)
#else
	if (avcodec_open(data->codec_context, data->codec) < 0)
#endif
	{
		fprintf(stderr, "could not open codec\n");
		g_free(data);
		return(NULL);
	}
	//alloc tmpbuff (yuv420p)
	data->tmpbuf = g_new0(BYTE, (width*height*3)/2);
	//alloc outbuf
	data->outbuf_size = 240000;//1792
	data->outbuf = g_new0(BYTE, data->outbuf_size);

	data->delayed_frames = 0;
	data->index_of_df = -1;

	data->flushed_buffers = 0;
	data->flush_delayed_frames = 0;
	data->flush_done = 0;

	return(data);
}

struct lavcAData* init_lavc_audio(struct paRecordData *pdata, int codec_ind)
{
	//allocate
	pdata->lavc_data = g_new0(struct lavcAData, 1);

	pdata->lavc_data->priv_data = NULL;

	pdata->lavc_data->codec_context = NULL;
	acodecs_data *defaults = get_aud_codec_defaults(codec_ind);

	// find the audio encoder
	//try specific codec (by name)
	pdata->lavc_data->codec = avcodec_find_encoder_by_name(defaults->codec_name);

	fprintf(stderr, "AUDIO: codec %s selected\n", defaults->codec_name);

	if(!pdata->lavc_data->codec) //if it fails try any codec with matching AV_CODEC_ID
	{
		fprintf(stderr, "ffmpeg audio codec %s not found\n", defaults->codec_name);
		pdata->lavc_data->codec = avcodec_find_encoder(defaults->codec_id);

		if (!pdata->lavc_data->codec)
		{
			fprintf(stderr, "ffmpeg no audio codec for ID: %i found\n", defaults->codec_id);
			g_free(pdata->lavc_data);
			pdata->lavc_data = NULL;
			return(NULL);
		}
	}

#if LIBAVCODEC_VER_AT_LEAST(53,6)
	pdata->lavc_data->codec_context = avcodec_alloc_context3(pdata->lavc_data->codec);
	avcodec_get_context_defaults3 (pdata->lavc_data->codec_context, pdata->lavc_data->codec);
#else
	pdata->lavc_data->codec_context = avcodec_alloc_context();
#endif

	// define bit rate (lower = more compression but lower quality)
	pdata->lavc_data->codec_context->bit_rate = defaults->bit_rate;
	pdata->lavc_data->codec_context->profile = defaults->profile; /*for AAC*/

	pdata->lavc_data->codec_context->flags |= defaults->flags;

	pdata->lavc_data->codec_context->sample_rate = pdata->samprate;
	pdata->lavc_data->codec_context->channels = pdata->channels;

#ifdef AV_CH_LAYOUT_MONO
	if(pdata->channels < 2)
		pdata->lavc_data->codec_context->channel_layout = AV_CH_LAYOUT_MONO;
	else
		pdata->lavc_data->codec_context->channel_layout = AV_CH_LAYOUT_STEREO;
#endif

	pdata->lavc_data->codec_context->cutoff = 0; /*automatic*/

	pdata->lavc_data->codec_context->sample_fmt = defaults->sample_format;

    pdata->lavc_data->codec_context->codec_id = defaults->codec_id;

#if !LIBAVCODEC_VER_AT_LEAST(53,0)
#define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO
#endif
	pdata->lavc_data->codec_context->codec_type = AVMEDIA_TYPE_AUDIO;

	// open codec
#if LIBAVCODEC_VER_AT_LEAST(53,6)
	if (avcodec_open2(pdata->lavc_data->codec_context, pdata->lavc_data->codec, NULL) < 0)
#else
	if (avcodec_open(pdata->lavc_data->codec_context, pdata->lavc_data->codec) < 0)
#endif
	{
		switch(defaults->sample_format)
		{
			case AV_SAMPLE_FMT_S16:
				defaults->sample_format = AV_SAMPLE_FMT_FLT;
				pdata->lavc_data->codec_context->sample_fmt = defaults->sample_format;
				fprintf(stderr, "could not open codec...trying again with float sample format\n");
				break;
			case AV_SAMPLE_FMT_FLT:
				defaults->sample_format = AV_SAMPLE_FMT_S16;
				pdata->lavc_data->codec_context->sample_fmt = defaults->sample_format;
				fprintf(stderr, "could not open codec...trying again with int16 sample format\n");
				break;
		}
		#if LIBAVCODEC_VER_AT_LEAST(53,6)
		if (avcodec_open2(pdata->lavc_data->codec_context, pdata->lavc_data->codec, NULL) < 0)
		#else
		if (avcodec_open(pdata->lavc_data->codec_context, pdata->lavc_data->codec) < 0)
		#endif
		{
			fprintf(stderr, "could not open codec...giving up\n");
			g_free(pdata->lavc_data);
			pdata->lavc_data = NULL;
			return(NULL);
		}
	}

	/* the codec gives us the frame size, in samples */
	int frame_size = pdata->lavc_data->codec_context->frame_size;
	g_print("Audio frame size is %d samples for selected codec\n", frame_size);

	pdata->lavc_data->monotonic_pts = defaults->monotonic_pts;

	//alloc outbuf
	pdata->lavc_data->outbuf_size = pdata->outbuf_size;
	pdata->lavc_data->outbuf = g_new0(BYTE, pdata->lavc_data->outbuf_size);

#if LIBAVCODEC_VER_AT_LEAST(53,34)
	pdata->lavc_data->frame= avcodec_alloc_frame();
	avcodec_get_frame_defaults(pdata->lavc_data->frame);
#endif
	return(pdata->lavc_data);
}

// H264 decoder
gboolean has_h264_decoder()
{
	if(avcodec_find_decoder(CODEC_ID_H264))
		return TRUE;
	else
		return FALSE;
}


struct h264_decoder_context* init_h264_decoder(int width, int height)
{
	struct h264_decoder_context* h264_ctx = g_new0(struct h264_decoder_context, 1);

	h264_ctx->codec = avcodec_find_decoder(CODEC_ID_H264);
	if(!h264_ctx->codec)
	{
		fprintf(stderr, "H264 decoder: codec not found (please install libavcodec-extra for H264 support)\n");
		g_free(h264_ctx);
		return NULL;
	}

#if LIBAVCODEC_VER_AT_LEAST(53,6)
	h264_ctx->context = avcodec_alloc_context3(h264_ctx->codec);
	avcodec_get_context_defaults3 (h264_ctx->context, h264_ctx->codec);
#else
	h264_ctx->context = avcodec_alloc_context();
	avcodec_get_context_defaults(h264_ctx->context);
#endif


	h264_ctx->context->flags2 |= CODEC_FLAG2_FAST;
	h264_ctx->context->pix_fmt = PIX_FMT_YUV420P;
	h264_ctx->context->width = width;
	h264_ctx->context->height = height;
	//h264_ctx->context->dsp_mask = (FF_MM_MMX | FF_MM_MMXEXT | FF_MM_SSE);
#if LIBAVCODEC_VER_AT_LEAST(53,6)
	if (avcodec_open2(h264_ctx->context, h264_ctx->codec, NULL) < 0)
#else
	if (avcodec_open(h264_ctx->context, h264_ctx->codec) < 0)
#endif
	{
		fprintf(stderr, "H264 decoder: couldn't open codec\n");
		avcodec_close(h264_ctx->context);
		g_free(h264_ctx->context);
		g_free(h264_ctx);
		return NULL;
	}

	h264_ctx->picture = avcodec_alloc_frame();
	avcodec_get_frame_defaults(h264_ctx->picture);
	h264_ctx->pic_size = avpicture_get_size(h264_ctx->context->pix_fmt, width, height);
	h264_ctx->width = width;
	h264_ctx->height = height;

	//decodedOut = (uint8_t *)malloc(pic_size);

	return h264_ctx;
}

int decode_h264(uint8_t *out_buf, uint8_t *in_buf, int buf_size, struct h264_decoder_context* h264_ctx)
{
	AVPacket avpkt;

	avpkt.size = buf_size;
	avpkt.data = in_buf;

	int got_picture = 0;
	int len = avcodec_decode_video2(h264_ctx->context, h264_ctx->picture, &got_picture, &avpkt);

	if(len < 0)
	{
		fprintf(stderr, "H264 decoder: error while decoding frame\n");
		return len;
	}

	if(got_picture)
	{
		avpicture_layout((AVPicture *) h264_ctx->picture, h264_ctx->context->pix_fmt
			, h264_ctx->width, h264_ctx->height, out_buf, h264_ctx->pic_size);
		return len;
	}
	else
		return 0;

}

void close_h264_decoder(struct h264_decoder_context* h264_ctx)
{
	if(h264_ctx == NULL)
		return;

	avcodec_close(h264_ctx->context);

	g_free(h264_ctx->context);
	g_free(h264_ctx->picture);

	g_free(h264_ctx);
}
