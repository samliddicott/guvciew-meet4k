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

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <pthread.h>
#include <stdlib.h>
#include <math.h>

#include "../config.h"
#include "defs.h"
#include "create_video.h"
#include "v4l2uvc.h"
#include "uvc_h264.h"
#include "avilib.h"
#include "globals.h"
#include "sound.h"
#include "acodecs.h"
#include "ms_time.h"
#include "string_utils.h"
#include "callbacks.h"
#include "vcodecs.h"
#include "video_format.h"
#include "audio_effects.h"


#define __AMUTEX &pdata->mutex
#define __VMUTEX &videoIn->mutex
#define __GMUTEX &global->mutex
#define __FMUTEX &global->file_mutex
#define __GCOND  &global->IO_cond

/*video capture can only start after buffer allocation*/
static void alloc_videoBuff(struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	int i = 0;
	int framesize = global->height*global->width*2; /*yuyv (maximum size)*/
	if((global->fps > 0) && (global->fps_num > 0))
	    global->video_buff_size = (global->fps * 3) / (global->fps_num * 2); /* 1,5 seconds of video in buffer*/
	if (global->video_buff_size < 16) global->video_buff_size = 16; /*keep at least 16 frames*/

	/*alloc video ring buffer*/
	__LOCK_MUTEX(__GMUTEX);
		if (global->videoBuff == NULL)
		{
			/*alloc video frames to videoBuff*/
			global->videoBuff = g_new0(VidBuff, global->video_buff_size);
			for(i=0;i<global->video_buff_size;i++)
			{
				global->videoBuff[i].frame = g_new0(BYTE,framesize);
				global->videoBuff[i].used = FALSE;
			}
		}
		else
		{
			/*free video frames to videoBuff*/
			for(i=0;i<global->video_buff_size;i++)
			{
				if(global->videoBuff[i].frame) g_free(global->videoBuff[i].frame);
				global->videoBuff[i].frame = NULL;
			}
			g_free(global->videoBuff);

			/*alloc video frames to videoBuff*/
			global->videoBuff = g_new0(VidBuff,global->video_buff_size);
			for(i=0;i<global->video_buff_size;i++)
			{
				global->videoBuff[i].frame = g_new0(BYTE,framesize);
				global->videoBuff[i].used = FALSE;
			}
		}
		//reset indexes
		global->r_ind=0;
		global->w_ind=0;
	__UNLOCK_MUTEX(__GMUTEX);
}

static int initVideoFile(struct ALL_DATA *all_data, void* lav_data)
{
	//struct GWIDGET *gwidget = all_data->gwidget;
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct VideoFormatData *videoF = all_data->videoF;

	io_Stream *vstream, *astream;
	struct lavcData **lavc_data = (struct lavcData **) lav_data;

	videoF->vcodec = get_vcodec_id(global->VidCodec); //global->VidCodec_ID
	videoF->acodec = get_acodec_id(global->AudCodec);
	videoF->keyframe = 0;

	__LOCK_MUTEX(__VMUTEX);
		gboolean capVid = videoIn->capVid;
	__UNLOCK_MUTEX(__VMUTEX);

	printf("initiating video file context\n");
	/*alloc video ring buffer*/
	alloc_videoBuff(all_data);

	//FIXME: don't initiate lavc on uvc h264 stream and H264 codec
	if(isLavcCodec(global->VidCodec))
		*lavc_data = init_lavc(global->width, global->height, global->fps_num, global->fps, global->VidCodec, global->format, global->Frame_Flags);

	if((*lavc_data)->monotonic_pts > 0)
		global->monotonic_pts = TRUE;

	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			if(videoF->avi != NULL)
			{
				avi_destroy_context(videoF->avi);
				videoF->avi = NULL;
			}
			videoF->avi = avi_create_context(videoIn->VidFName);

			if(!videoF->avi)
			{
				g_printerr("Error: Couldn't create AVI context.\n");
				capVid = FALSE; /*don't start video capture*/
				__LOCK_MUTEX(__VMUTEX);
					videoIn->capVid = capVid;
				__UNLOCK_MUTEX(__VMUTEX);
				pdata->capVid = capVid;
				return(-1);
			}

			vstream = avi_add_video_stream(videoF->avi,
								global->width,
								global->height,
								global->fps,
								videoF->vcodec,
								get_vid4cc(global->VidCodec));

			if(videoF->vcodec == AV_CODEC_ID_THEORA)
			{
				vstream->extra_data = (BYTE*) (*lavc_data)->codec_context->extradata;
				vstream->extra_data_size = (*lavc_data)->codec_context->extradata_size;
			}

			if(global->Sound_enable > 0)
			{
				/*get channels and sample rate*/
				set_sound(global, pdata);

				/*sample size - only used for PCM*/
				int32_t a_bits = get_aud_bits(global->AudCodec);
				/*bit rate (compressed formats)*/
				int32_t b_rate = get_aud_bit_rate(global->AudCodec);

				astream = avi_add_audio_stream(videoF->avi,
								global->Sound_NumChan,
								global->Sound_SampRate,
								a_bits,
								b_rate,
								videoF->acodec,
								global->Sound_Format);

				if(videoF->acodec == AV_CODEC_ID_VORBIS)
				{
						astream->extra_data = (BYTE*) pdata->lavc_data->codec_context->extradata;
						astream->extra_data_size = pdata->lavc_data->codec_context->extradata_size;
				}

			}
			/* add first riff header */
			avi_add_new_riff(videoF->avi);

			break;

		case WEBM_FORMAT:
		case MKV_FORMAT:
			if(videoF->mkv != NULL)
			{
				mkv_destroy_context(videoF->mkv);
				videoF->mkv = NULL;
			}
			videoF->mkv = mkv_create_context(videoIn->VidFName, global->VidFormat);

			if(!videoF->mkv)
			{
				g_printerr("Error: Couldn't create MKV context.\n");
				capVid = FALSE; /*don't start video capture*/
				__LOCK_MUTEX(__VMUTEX);
					videoIn->capVid = capVid;
				__UNLOCK_MUTEX(__VMUTEX);
				pdata->capVid = capVid;
				return(-1);
			}

			vstream = mkv_add_video_stream(videoF->mkv,
									global->width,
									global->height,
									global->fps,
									global->fps_num,
									videoF->vcodec);


			vstream->extra_data_size = set_mkvCodecPriv(videoIn, global, *lavc_data);
			if(vstream->extra_data_size > 0)
			{
				vstream->extra_data = get_mkvCodecPriv(global->VidCodec);
				if(global->format == V4L2_PIX_FMT_H264)
					vstream->h264_process = 1; //we need to process NALU marker
			}

			if(global->Sound_enable > 0)
			{
				/*get channels and sample rate*/
				set_sound(global, pdata);

				/*sample size - only used for PCM*/
				int32_t a_bits = get_aud_bits(global->AudCodec);
				/*bit rate (compressed formats)*/
				int32_t b_rate = get_aud_bit_rate(global->AudCodec);

				astream = mkv_add_audio_stream(
								videoF->mkv,
								pdata->channels,
								pdata->samprate,
								a_bits,
								b_rate,
								videoF->acodec,
								global->Sound_Format);

				astream->extra_data_size = set_mkvACodecPriv(
								global->AudCodec,
								pdata->samprate,
								pdata->channels,
								pdata->lavc_data);

				if(astream->extra_data_size > 0)
					astream->extra_data = get_mkvACodecPriv(global->AudCodec);


			}

			/** write the file header */
			mkv_write_header(videoF->mkv);

			break;

		default:
			g_printerr("Init Video File: Unsupported Format(%d)\n", global->VidFormat);
			return(-2);
			break;
	}

	/* start video capture*/
	capVid = TRUE;
	__LOCK_MUTEX(__VMUTEX);
		videoIn->capVid = capVid;
	__UNLOCK_MUTEX(__VMUTEX);
	pdata->capVid = capVid;

	/* start sound capture*/
	if(global->Sound_enable > 0 && init_sound (pdata))
	{
		//FIXME: enable capture button
		g_printerr("Audio initialization error\n");
		global->Sound_enable=0;
	}

	printf("    OK\n");
	return (0);
}

/* Called at avi capture stop       */
static void
aviClose (struct ALL_DATA *all_data)
{
	float tottime = 0;

	struct GLOBAL *global = all_data->global;
	//struct vdIn *videoIn = all_data->videoIn;
	struct VideoFormatData *videoF = all_data->videoF;
	struct paRecordData *pdata = all_data->pdata;

	if (videoF->avi)
	{
		tottime = (float) ((int64_t) (global->Vidstoptime - global->Vidstarttime) / 1000000); // convert to miliseconds

		if (global->debug) g_print("stop= %llu start=%llu \n",
			(unsigned long long) global->Vidstoptime, (unsigned long long) global->Vidstarttime);
		if (tottime > 0)
		{
			/*try to find the real frame rate*/
			videoF->avi->fps = (double) (global->framecount * 1000) / tottime;
		}
		else
		{
			/*set the hardware frame rate*/
			videoF->avi->fps = global->fps;
		}

		if (global->debug)
			g_print("VIDEO: %"PRIu64" frames in %f ms = %f fps\n",
				global->framecount, tottime, videoF->avi->fps);
		/*------------------- close audio stream and clean up -------------------*/
		if (global->Sound_enable > 0)
		{
			if (close_sound (pdata)) g_printerr("Sound Close error\n");
		}
		avi_close(videoF->avi);

		global->framecount = 0;
		global->Vidstarttime = 0;
		if (global->debug) g_print ("close avi\n");
	}

	avi_destroy_context(videoF->avi);
	pdata = NULL;
	global = NULL;
	//videoIn = NULL;
	videoF->avi = NULL;
}

static void
mkvClose(struct ALL_DATA *all_data)
{
	float tottime = 0;

	struct GLOBAL *global = all_data->global;
	//struct vdIn *videoIn = all_data->videoIn;
	struct VideoFormatData *videoF = all_data->videoF;
	struct paRecordData *pdata = all_data->pdata;

	if (videoF->mkv)
	{
		tottime = (float) ((int64_t) (global->Vidstoptime - global->Vidstarttime) / 1000000); // convert to miliseconds

		if (global->debug) g_print("stop= %llu start=%llu \n",
			(unsigned long long) global->Vidstoptime, (unsigned long long) global->Vidstarttime);

		if (global->debug)
			g_print("VIDEO: %"PRIu64" frames in %f ms \n", global->framecount, tottime);
		/*------------------- close audio stream and clean up -------------------*/
		if (global->Sound_enable > 0)
		{
			if (close_sound (pdata)) g_printerr("Sound Close error\n");
		}
		mkv_close(videoF->mkv);

		global->framecount = 0;
		global->Vidstarttime = 0;
		if (global->debug) g_print ("close mkv\n");
	}

	mkv_destroy_context(videoF->mkv);
	pdata = NULL;
	global = NULL;
	//videoIn = NULL;
	videoF->mkv = NULL;
}


static void closeVideoFile(struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct paRecordData *pdata = all_data->pdata;

	int i=0;
	/*we are streaming so we need to lock a mutex*/
	gboolean capVid = FALSE;
	__LOCK_MUTEX(__VMUTEX);
		videoIn->capVid = capVid; /*flag video thread to stop recording frames*/
	__UNLOCK_MUTEX(__VMUTEX);
	__LOCK_MUTEX(__AMUTEX);
		pdata->capVid = capVid;
	__UNLOCK_MUTEX(__AMUTEX);
	/*wait for flag from video thread that recording has stopped    */
	/*wait on videoIn->VidCapStop by sleeping for 200 loops of 10 ms*/
	/*(test VidCapStop == TRUE on every loop)*/
	int stall = wait_ms(&(videoIn->VidCapStop), TRUE, __VMUTEX, 10, 200);
	if( !(stall > 0) )
	{
		g_printerr("video capture stall on exit(%d) - timeout\n",
			videoIn->VidCapStop);
	}

	/*free video buffer allocations*/
	__LOCK_MUTEX(__GMUTEX);
		//reset the indexes
		global->r_ind=0;
		global->w_ind=0;
		if (global->videoBuff != NULL)
		{
			/*free video frames to videoBuff*/
			for(i=0;i<global->video_buff_size;i++)
			{
				g_free(global->videoBuff[i].frame);
				global->videoBuff[i].frame = NULL;
			}
			g_free(global->videoBuff);
			global->videoBuff = NULL;
		}
	__UNLOCK_MUTEX(__GMUTEX);

	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			aviClose(all_data);
			break;

		case WEBM_FORMAT:
		case MKV_FORMAT:
			mkvClose(all_data);
			break;

		default:

			break;
	}

	global->Vidstoptime = 0;
	global->Vidstarttime = 0;
	global->framecount = 0;
}

static int write_video_frame (struct ALL_DATA *all_data,
	void *jpeg_struct,
	struct lavcData *lavc_data,
	VidBuff *proc_buff)
{
	struct GLOBAL *global = all_data->global;
	//struct GWIDGET *gwidget = all_data->gwidget;

	int ret=0;

	//printf("proc_buff: size: %i, ts:%llu keyframe:%i\n", proc_buff->bytes_used, proc_buff->time_stamp, proc_buff->keyframe);
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			ret = compress_frame(all_data, jpeg_struct, lavc_data, proc_buff);
			break;

		case WEBM_FORMAT:
		case MKV_FORMAT:
			ret = compress_frame(all_data, jpeg_struct, lavc_data, proc_buff);
			break;

		default:
			break;
	}
	return (ret);
}

static int write_audio_frame (struct ALL_DATA *all_data)
{
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct VideoFormatData *videoF = all_data->videoF;
	//struct GWIDGET *gwidget = all_data->gwidget;

	int ret =0;

	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			ret = compress_audio_frame(all_data);
			break;

		case WEBM_FORMAT:
		case MKV_FORMAT:
			__LOCK_MUTEX( __AMUTEX ); //why do we need this ???
				/*set pts*/
				videoF->apts = pdata->audio_buff[pdata->br_ind][pdata->r_ind].time_stamp;
				/*write audio chunk*/
				ret = compress_audio_frame(all_data);
			__UNLOCK_MUTEX( __AMUTEX );
			break;

		default:

			break;
	}
	return (ret);
}

static int sync_audio_frame(struct ALL_DATA *all_data, AudBuff *proc_buff)
{
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct VideoFormatData *videoF = all_data->videoF;

	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			/*first audio data - sync with video (audio stream capture can take               */
			/*a bit longer to start)                                                          */
			/*no need of locking the audio mutex yet, since we are not reading from the buffer*/
			if (!get_stream(videoF->avi->stream_list, 1)->audio_strm_length)
			{
				/*only 1 audio stream*/
				/*time diff for audio-video*/
				int synctime= (int) (pdata->delay + pdata->snd_begintime - pdata->ts_ref)/1000000; /*convert to miliseconds*/
				if (global->debug) g_print("shift sound by %d ms\n", synctime);
				if(synctime>10 && synctime<2000)
				{ 	/*only sync between 10ms and 2 seconds*/
					if(global->Sound_Format == PA_FOURCC)
					{	/*shift sound by synctime*/
						UINT32 shiftFrames = abs(synctime * global->Sound_SampRate / 1000);
						UINT32 shiftSamples = shiftFrames * global->Sound_NumChan;
						if (global->debug) g_print("shift sound forward by %d samples\n",
							shiftSamples);
						short *EmptySamp;
						EmptySamp=g_new0(short, shiftSamples);
						avi_write_packet(videoF->avi, 1, (BYTE *) EmptySamp, shiftSamples*sizeof(short), 0, -1, 0);
						//AVI_write_audio(videoF->AviOut,(BYTE *) EmptySamp,shiftSamples*sizeof(short));
						g_free(EmptySamp);
					}
					else
					{
						/*use lavc encoder*/
					}
				}
			}
			break;

		case WEBM_FORMAT:
		case MKV_FORMAT:

			break;

		default:
			break;
	}

	return (0);
}

static int buff_scheduler(int w_ind, int r_ind, int buff_size)
{
	int diff_ind = 0;
	int sched_sleep = 0;
	/* try to balance buffer overrun in read/write operations */
	if(w_ind >= r_ind)
		diff_ind = w_ind - r_ind;
	else
		diff_ind = (buff_size - r_ind) + w_ind;

	int th = (int) lround((double) buff_size * 0.7);

	if(diff_ind <= th) /* from 0 to 50 ms (down below 20 fps)*/
		sched_sleep = (int) lround((double) (diff_ind * 71) / buff_size);
	else               /*from 50 to 210 ms (down below 5 fps)*/
		sched_sleep = (int) lround((double) ((diff_ind * 320) / buff_size) - 110);

	if(sched_sleep < 0) sched_sleep = 0; /*clip to positive values just in case*/

	//g_printf("diff index: %i sleep %i\n",diff_ind, sched_sleep);
	return sched_sleep;
}

static int get_audio_flag(struct paRecordData *pdata)
{
	int flag = 0;
	__LOCK_MUTEX(__AMUTEX);
		flag = pdata->audio_buff_flag[pdata->br_ind];
	__UNLOCK_MUTEX(__AMUTEX);
	return flag;
}

static gboolean is_audio_processing(struct paRecordData *pdata, gboolean set_processing)
{
	int flag = get_audio_flag(pdata);

	if((set_processing) && (flag == AUD_PROCESS))
	{
		__LOCK_MUTEX(__AMUTEX);
			pdata->audio_buff_flag[pdata->br_ind] = AUD_PROCESSING;
		__UNLOCK_MUTEX(__AMUTEX);

		flag = AUD_PROCESSING;
	}

	if(flag == AUD_PROCESSING)
		return TRUE;
	else
		return FALSE;
}

//static gboolean is_audioTS_lessThan_videoTS(void *data)
//{
//	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
//	struct paRecordData *pdata = all_data->pdata;
//	struct GLOBAL *global = all_data->global;
//
//	__LOCK_MUTEX(__AMUTEX);
//		QWORD audioTS = pdata->audio_buff[pdata->br_ind][pdata->r_ind].time_stamp;
//	__UNLOCK_MUTEX(__AMUTEX);
//
//	__LOCK_MUTEX( __GMUTEX );
//		QWORD videoTS = global->videoBuff[global->r_ind].time_stamp;
//	__UNLOCK_MUTEX( __GMUTEX );
//
//	if (audioTS < videoTS)
//		return TRUE;
//	else
//		return FALSE;
//}

static gboolean process_audio(struct ALL_DATA *all_data,
			struct audio_effects **aud_eff)
{
	struct vdIn *videoIn = all_data->videoIn;
	struct paRecordData *pdata = all_data->pdata;

	__LOCK_MUTEX(__VMUTEX);
		gboolean capVid = videoIn->capVid;
	__UNLOCK_MUTEX(__VMUTEX);

	/*read audio Frames (1152 * channels  samples each)*/

	gboolean finish = FALSE;

	/*used - doesn't need locking as current buffer must be on AUD_PROCESSING state*/
	if(is_audio_processing(pdata, TRUE) &&
		pdata->audio_buff[pdata->br_ind][pdata->r_ind].used)
	{
		sync_audio_frame(all_data, &pdata->audio_buff[pdata->br_ind][pdata->r_ind]);
		/*run effects on data*/
		/*echo*/
		if((pdata->snd_Flags & SND_ECHO)==SND_ECHO)
		{
			Echo(pdata, &pdata->audio_buff[pdata->br_ind][pdata->r_ind], *aud_eff, 300, 0.5);
		}
		else
		{
			close_DELAY((*aud_eff)->ECHO);
			(*aud_eff)->ECHO = NULL;
		}
		/*fuzz*/
		if((pdata->snd_Flags & SND_FUZZ)==SND_FUZZ)
		{
			Fuzz(pdata, &pdata->audio_buff[pdata->br_ind][pdata->r_ind], *aud_eff);
		}
		else
		{
			close_FILT((*aud_eff)->HPF);
			(*aud_eff)->HPF = NULL;
		}
		/*reverb*/
		if((pdata->snd_Flags & SND_REVERB)==SND_REVERB)
		{
			Reverb(pdata, &pdata->audio_buff[pdata->br_ind][pdata->r_ind], *aud_eff, 50);
		}
		else
		{
			close_REVERB(*aud_eff);
		}
		/*wahwah*/
		if((pdata->snd_Flags & SND_WAHWAH)==SND_WAHWAH)
		{
			WahWah (pdata, &pdata->audio_buff[pdata->br_ind][pdata->r_ind], *aud_eff, 1.5, 0, 0.7, 0.3, 2.5);
		}
		else
		{
			close_WAHWAH((*aud_eff)->wahData);
			(*aud_eff)->wahData = NULL;
		}
		/*Ducky*/
		if((pdata->snd_Flags & SND_DUCKY)==SND_DUCKY)
		{
			change_pitch(pdata, &pdata->audio_buff[pdata->br_ind][pdata->r_ind], *aud_eff, 2);
		}
		else
		{
			close_pitch (*aud_eff);
		}

		write_audio_frame(all_data);

		pdata->audio_buff[pdata->br_ind][pdata->r_ind].used = FALSE;
		NEXT_IND(pdata->r_ind, AUDBUFF_SIZE);

		/*start of new buffer block*/
		if(pdata->r_ind == 0)
		{
			__LOCK_MUTEX(__AMUTEX);
				pdata->audio_buff_flag[pdata->br_ind] = AUD_READY;
				NEXT_IND(pdata->br_ind, AUDBUFF_NUM);
			__UNLOCK_MUTEX(__AMUTEX);
		}

	}
	else
	{
		if (capVid)
		{
			/*video buffer underrun            */
			/*wait for next frame (sleep 10 ms)*/
			sleep_ms(10);
		}
		else
		{
			finish = TRUE; /*all frames processed and no longer capturing so finish*/
		}
	}

	return finish;
}

static gboolean process_video(struct ALL_DATA *all_data,
				VidBuff *proc_buff,
				struct lavcData *lavc_data,
				struct JPEG_ENCODER_STRUCTURE **jpeg_struct)
{
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct paRecordData *pdata = all_data->pdata;
	int64_t max_drift = 0, audio_drift = 0;

	if (global->Sound_enable) {
		__LOCK_MUTEX(__AMUTEX);
			audio_drift = pdata->ts_drift;
		__UNLOCK_MUTEX(__AMUTEX);
		int fps = global->fps;
		if(fps < 5)
			fps = 15;
		max_drift = 1000000000 / fps;	/* one frame */
	}

	__LOCK_MUTEX(__VMUTEX);
		gboolean capVid = videoIn->capVid;
	__UNLOCK_MUTEX(__VMUTEX);

	gboolean finish = FALSE;

	__LOCK_MUTEX(__GMUTEX);
    	gboolean used = global->videoBuff[global->r_ind].used;
    __UNLOCK_MUTEX(__GMUTEX);

	if (used)
	{
		__LOCK_MUTEX(__GMUTEX);
	    		/*read video Frame*/
			proc_buff->bytes_used = global->videoBuff[global->r_ind].bytes_used;
			memcpy(proc_buff->frame, global->videoBuff[global->r_ind].frame, proc_buff->bytes_used);
			proc_buff->time_stamp = global->videoBuff[global->r_ind].time_stamp;
			proc_buff->keyframe =  global->videoBuff[global->r_ind].keyframe;
			global->videoBuff[global->r_ind].used = FALSE;
			/*signals an empty slot in the video buffer*/
			__COND_BCAST(__GCOND);
			//printf("proc_buff bytes used %i\n", proc_buff->bytes_used);
			NEXT_IND(global->r_ind,global->video_buff_size);
			audio_drift -= global->av_drift;
		__UNLOCK_MUTEX(__GMUTEX);

		/* fprintf(stderr, "audio drift = %lli ms\n", audio_drift / 1000000); */
		/*process video Frame*/
		if (audio_drift > max_drift)
		{
			/* audio is behind (this should be compensated when capturing audio) */
			g_print("audio drift (%lli ms): but not dropping/shifting frame\n", (long long int) audio_drift/1000000);
			//__LOCK_MUTEX(__GMUTEX);
			//	global->av_drift += max_drift; /* shift for matroska/webm */
			//__UNLOCK_MUTEX(__GMUTEX);

		}
		else if (audio_drift < -1 * max_drift)
		{
			/* audio is ahead (are we over compensating when capturing audio?) */
			g_print("audio drift: duplicating/shifting frame\n");
			__LOCK_MUTEX(__GMUTEX);
				global->av_drift -= max_drift; /* shift for matroska/webm */
			__UNLOCK_MUTEX(__GMUTEX);

			switch (global->VidFormat)
			{
				case AVI_FORMAT:
					/* duplicate the frame */
					write_video_frame(all_data, (void *) jpeg_struct, lavc_data, proc_buff);
					break;

				default:
					break;
			}
		}

		/*write frame*/
		write_video_frame(all_data, (void *) jpeg_struct, lavc_data, proc_buff);
	}
	else
	{
		if (capVid)
		{
			/*video buffer underrun            */
			/*wait for next frame (sleep 10 ms)*/
			/*FIXME: this is not good on streaming formats (we can loose key frames)*/
			sleep_ms(10);
		}
		else if (lavc_data != NULL && lavc_data->codec_context != NULL) //if we are using a lavc encoder flush the last frames
		{
			//flush video encoder
			lavc_data->flush_delayed_frames = 1;
			write_video_frame(all_data, (void *) jpeg_struct, lavc_data, proc_buff);
			finish = lavc_data->flush_done; /*all frames processed and no longer capturing so finish*/
		}
		else //finish
			finish = TRUE;
	}
	return finish;
}

/* this function can only be called after a lock on global->mutex */
static void store_at_index(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;

	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;


	global->videoBuff[global->w_ind].time_stamp = global->v_ts - global->av_drift;

	/*store frame at index*/
	if ((global->VidCodec == CODEC_MJPEG) && (global->Frame_Flags==0) &&
		(global->format==V4L2_PIX_FMT_MJPEG))
	{
		/*store MJPEG frame*/
		global->videoBuff[global->w_ind].bytes_used = videoIn->buf.bytesused;
		memcpy(global->videoBuff[global->w_ind].frame,
			videoIn->tmpbuffer,
			global->videoBuff[global->w_ind].bytes_used);
	}
	else if ((global->VidCodec >= CODEC_LAVC) && (global->Frame_Flags==0) &&
		((global->format==V4L2_PIX_FMT_NV12) || (global->format==V4L2_PIX_FMT_NV21)))
	{
		/*store yuv420p frame*/
		global->videoBuff[global->w_ind].bytes_used = videoIn->buf.bytesused;
		memcpy(global->videoBuff[global->w_ind].frame,
			videoIn->tmpbuffer,
			global->videoBuff[global->w_ind].bytes_used);
	}
	else if ( global->format == V4L2_PIX_FMT_H264 &&
			  global->VidCodec_ID == AV_CODEC_ID_H264 &&
			  global->Frame_Flags == 0
			)
	{
		/*store H264 frame*/
		global->videoBuff[global->w_ind].bytes_used = videoIn->buf.bytesused;
		memcpy(global->videoBuff[global->w_ind].frame,
			videoIn->h264_frame,
			global->videoBuff[global->w_ind].bytes_used);
		global->videoBuff[global->w_ind].keyframe = videoIn->isKeyframe;
		/* use monotonic timestamp instead of real timestamp */
		//if(global->monotonic_pts)
		//	global->videoBuff[global->w_ind].time_stamp = global->framecount * floor(1E9/global->fps);
	}
	else
	{
		/*store YUYV frame*/
		global->videoBuff[global->w_ind].bytes_used = global->height*global->width*2;
		memcpy(global->videoBuff[global->w_ind].frame,
			videoIn->framebuffer,
			global->videoBuff[global->w_ind].bytes_used);
	}
	global->videoBuff[global->w_ind].used = TRUE;

	//printf("CODECID: %i (%i) format: %i (%i) size: %i (%i)\n",global->VidCodec_ID, AV_CODEC_ID_H264, global->format, V4L2_PIX_FMT_H264, global->videoBuff[global->w_ind].bytes_used, videoIn->buf.bytesused);
}

/* called from main video loop*
 * stores current frame in video ring buffer
 */
int store_video_frame(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;

	int ret = 0;
	int producer_sleep = 0;

	__LOCK_MUTEX(__GMUTEX);

	if(!global->videoBuff)
	{
		g_printerr("WARNING: video ring buffer not allocated yet - dropping frame.");
		__UNLOCK_MUTEX(__GMUTEX);
		return(-1);
	}

	if( global->VidCodec_ID == AV_CODEC_ID_H264 &&
		global->format == V4L2_PIX_FMT_H264 &&
		global->Frame_Flags == 0)
	{
		if(videoIn->h264_last_IDR_size <= 0)
		{
			g_printerr("WARNING: h264 video stream hasn't produce a IDR frame yet - dropping frame.");
			g_printerr("        Requesting a IDR frame\n");
			uvcx_request_frame_type(videoIn->fd, global->uvc_h264_unit, PICTURE_TYPE_IDR_FULL);
			return (-1);
		}

		if( global->framecount < 1 &&
			!videoIn->isKeyframe )
		{
			if (!global->videoBuff[global->w_ind].used) //it's the first frame (should allways be true)
			{
				//printf("storing last IDR at video buf ind %i\n", global->w_ind);
				//char test_filename[20];
				//snprintf(test_filename, 20, "frame_last_IDR.raw");
				//SaveBuff (test_filename, videoIn->h264_last_IDR_size, videoIn->h264_last_IDR);

				//should we add SPS and PPS NALU first??
				//store the last keyframe first (use current timestamp)
				global->videoBuff[global->w_ind].time_stamp = global->v_ts;
				global->videoBuff[global->w_ind].bytes_used = videoIn->h264_last_IDR_size;
				memcpy( global->videoBuff[global->w_ind].frame,
					videoIn->h264_last_IDR,
					global->videoBuff[global->w_ind].bytes_used);
				global->videoBuff[global->w_ind].keyframe = TRUE;
				global->videoBuff[global->w_ind].used = TRUE;
				NEXT_IND(global->w_ind, global->video_buff_size);
				global->framecount++;
			}
		}
	}

	if (!global->videoBuff[global->w_ind].used)
	{
		store_at_index(data);
		if(global->format == V4L2_PIX_FMT_H264)
		{
			// for stream based formats change frame rate instead of dropping frames
			// by making the producer thread sleep
			producer_sleep = 0;
			h264_framerate_balance(all_data);
		}
		else
			producer_sleep = buff_scheduler(global->w_ind, global->r_ind, global->video_buff_size);
		NEXT_IND(global->w_ind, global->video_buff_size);
	}
	else
	{
		if(global->debug) g_printerr("WARNING: buffer full waiting for free space\n");
		/*wait for IO_cond at least 100ms*/
		struct timespec endtime;
		clock_gettime(CLOCK_REALTIME, &endtime);
		endtime.tv_nsec += 10000;
		if(__COND_TIMED_WAIT(__GCOND, __GMUTEX, &endtime))
		{
			/*try to store the frame again*/
			if (!global->videoBuff[global->w_ind].used)
			{
				store_at_index(data);
                		producer_sleep = buff_scheduler(global->w_ind, global->r_ind, global->video_buff_size);
				NEXT_IND(global->w_ind, global->video_buff_size);
			}
			else ret = -2;/*drop frame*/

		}
		else ret = -3;/*drop frame*/
	}
	if(!ret) global->framecount++;

	__UNLOCK_MUTEX(__GMUTEX);

	/*-------------if needed, make the thread sleep for a while----------------*/
	if(producer_sleep) sleep_ms(producer_sleep);

	return ret;
}

void *Audio_loop(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	struct GLOBAL *global = all_data->global;
	struct paRecordData *pdata = all_data->pdata;
	struct vdIn *videoIn = all_data->videoIn;

	struct audio_effects *aud_eff = init_audio_effects ();
	global->av_drift = 0;
	pdata->ts_drift = 0;

	gboolean capVid = TRUE;
	gboolean finished = FALSE;
	int max_loops = 60;

	while(!finished)
	{
		__LOCK_MUTEX(__VMUTEX);
			capVid = videoIn->capVid;
		__UNLOCK_MUTEX(__VMUTEX);

		finished = process_audio(all_data, &(aud_eff));

		if(!capVid)
		{
			/* if capture has stopped then limit the number of iterations
			 * fixes any possible loop lock on process_audio
			 */
			max_loops--;
			if(max_loops < 1)
				finished = TRUE;
		}
	}

	close_audio_effects (aud_eff);

	return ((void *) 0);
}

void *IO_loop(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;

	struct GWIDGET *gwidget = all_data->gwidget;
	struct GLOBAL *global = all_data->global;

	struct vdIn *videoIn = all_data->videoIn;

	struct JPEG_ENCODER_STRUCTURE *jpg_data=NULL;
	struct lavcData *lavc_data = NULL;
	//struct audio_effects *aud_eff = NULL;

	gboolean capVid = TRUE;
	gboolean finished=FALSE;
	int max_loops = 60;

	__LOCK_MUTEX(__VMUTEX);
		videoIn->IOfinished=FALSE;
	__UNLOCK_MUTEX(__VMUTEX);

	gboolean failed = FALSE;
	gboolean audio_failed = FALSE;

	//video buffers to be processed
	int frame_size=0;
	VidBuff *proc_buff = NULL;

	/* make sure we have a IDR frame (and SPS+PPS data) before start capturing */
	if( global->VidCodec_ID == AV_CODEC_ID_H264 &&
		global->Frame_Flags == 0 &&
		global->format == V4L2_PIX_FMT_H264 &&
		videoIn->h264_last_IDR_size <= 0)
	{
		g_printerr("WARNING: h264 video stream hasn't produce a IDR frame yet - waiting\n");
		g_printerr("        Requesting a IDR frame\n");
		uvcx_request_frame_type(videoIn->fd, global->uvc_h264_unit, PICTURE_TYPE_IDR_FULL);

		/*waiting at most 3 seconds (30*100 ms)*/
		int max_loops = 30;
		while(videoIn->h264_last_IDR_size <= 0 || max_loops > 0)
		{
			/*sleep for 100 ms*/
			sleep_ms(100);
			max_loops--;
		}
	}

	if(initVideoFile(all_data, &(lavc_data))<0)
	{
		g_printerr("Cap Video failed\n");
		if(!(global->no_display))
        {
		    /*disable signals for video capture callback*/
		    g_signal_handlers_block_by_func(GTK_TOGGLE_BUTTON(gwidget->CapVidButt), G_CALLBACK (capture_vid), all_data);
		    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
		    gtk_button_set_label(GTK_BUTTON(gwidget->CapVidButt),_("Cap. Video"));
		    /*enable signals for video capture callback*/
		    g_signal_handlers_unblock_by_func(GTK_TOGGLE_BUTTON(gwidget->CapVidButt), G_CALLBACK (capture_vid), all_data);
		    gdk_flush();

		    /*enabling sound and video compression controls*/
		    set_sensitive_vid_contrls(TRUE, global->Sound_enable, gwidget);
		}

		finished = TRUE;
		failed = TRUE;
	}
	else
	{
		if(global->debug) g_print("IO thread started...OK\n");
		frame_size = global->height*global->width*2;
		proc_buff = g_new0(VidBuff, 1);
		proc_buff->frame = g_new0(BYTE, frame_size);

		if (global->Sound_enable)
		{
			/*start audio process thread*/
			if( __THREAD_CREATE(&all_data->audio_thread, Audio_loop, (void *) all_data))
			{
				g_printerr("Audio thread creation failed\n");
				audio_failed = TRUE;
			}
		}
	}

	/*process video frames*/
	if(!failed)
	{
		while(!finished)
		{

			__LOCK_MUTEX(__VMUTEX);
				capVid = videoIn->capVid;
			__UNLOCK_MUTEX(__VMUTEX);

			finished = process_video (all_data, proc_buff, lavc_data, &(jpg_data));

			if(!capVid)
			{
				/* if capture has stopped then limit the number of iterations
				 * fixes any possible loop lock on process_video
				 */
				max_loops--;
				if(max_loops < 1)
					finished = TRUE;
			}
		}

		if (global->Sound_enable && !audio_failed)
		{
			/* join audio thread*/
			__THREAD_JOIN( all_data->audio_thread );
		}

		/*finish capture*/
		closeVideoFile(all_data);

		/*free proc buffer*/
		g_free(proc_buff->frame);
		g_free(proc_buff);

		/* reset fps since it may have changed during capture (stream base formats) */
		videoIn->setFPS = 1;
	}

	if(lavc_data != NULL)
	{
		clean_lavc(&lavc_data);
		lavc_data = NULL;
	}

	if(jpg_data != NULL) g_free(jpg_data);
	jpg_data = NULL;

	if(global->jpeg != NULL) g_free(global->jpeg); //jpeg buffer used in encoding
	global->jpeg = NULL;

	if(global->debug) g_print("IO thread finished...OK\n");

	global->VidButtPress = FALSE;
	__LOCK_MUTEX(__VMUTEX);
		videoIn->IOfinished=TRUE;
	__UNLOCK_MUTEX(__VMUTEX);

	return ((void *) 0);
}

