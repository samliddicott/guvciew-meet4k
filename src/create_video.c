/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
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
#include <stdlib.h>

#include "../config.h"
#include "defs.h"
#include "create_video.h"
#include "v4l2uvc.h"
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
#include "globals.h"

/*video capture can only start after buffer allocation*/
static void alloc_videoBuff(struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	int i = 0;
	int framesize = global->height*global->width*2; /*yuyv (maximum size)*/
	if((global->fps > 0) && (global->fps_num > 0))
	    global->video_buff_size = (global->fps * 3) / (global->fps_num * 2); /* 1,5 seconds of video in buffer*/
	if (global->video_buff_size < 2) global->video_buff_size = 2; /*keep at least two frames*/
	
	/*alloc video ring buffer*/
	g_mutex_lock(global->mutex);
		if (global->videoBuff == NULL)
		{
			/*alloc video frames to videoBuff*/
			global->videoBuff = g_new0(VidBuff, global->video_buff_size);
			for(i=0;i<global->video_buff_size;i++)
			{
				global->videoBuff[i].frame = g_new0(BYTE,framesize);
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
			}
		}
		//reset indexes
		global->r_ind=0;
		global->w_ind=0;
	g_mutex_unlock(global->mutex);
}

static int initVideoFile(struct ALL_DATA *all_data, void * lavc_aud_data)
{
	struct lavcAData** lavc_audio_data = (struct lavcAData**) lavc_aud_data;
	struct GWIDGET *gwidget = all_data->gwidget;
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct VideoFormatData *videoF = all_data->videoF;
	
	const char *compression= get_vid4cc(global->VidCodec);
	videoF->vcodec = get_vcodec_id(global->VidCodec);
	videoF->acodec = CODEC_ID_NONE;
	videoF->keyframe = 0;
	int ret = 0;
	
	g_mutex_lock(videoIn->mutex);
		gboolean capVid = videoIn->capVid;
	g_mutex_unlock(videoIn->mutex);
	
	/*alloc video ring buffer*/
	alloc_videoBuff(all_data);
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			if(videoF->AviOut != NULL)
			{
				g_free(videoF->AviOut);
				videoF->AviOut = NULL;
			}
			videoF->AviOut = g_new0(struct avi_t, 1);
			
			if(AVI_open_output_file(videoF->AviOut, videoIn->VidFName)<0) 
			{
				g_printerr("Error: Couldn't create Video.\n");
				capVid = FALSE; /*don't start video capture*/
				g_mutex_lock(videoIn->mutex);
					videoIn->capVid = capVid;
				g_mutex_unlock(videoIn->mutex);
				pdata->capVid = capVid;
				return(-1);
			} 
			else 
			{
				AVI_set_video(videoF->AviOut, global->width, global->height, 
					global->fps, compression);
		  
				/* start video capture*/
				capVid = TRUE;
				g_mutex_lock(videoIn->mutex);
					videoIn->capVid = capVid;
				g_mutex_unlock(videoIn->mutex);
				pdata->capVid = capVid;
				
				/* start sound capture*/
				if(global->Sound_enable > 0) 
				{
					/*get channels and sample rate*/
					set_sound(global, pdata, (void *) lavc_audio_data);
					/*set audio header for avi*/
					AVI_set_audio(videoF->AviOut, global->Sound_NumChan, 
						global->Sound_SampRate,
						get_aud_bit_rate(get_ind_by4cc(global->Sound_Format)), /*bit rate*/
						get_aud_bits(get_ind_by4cc(global->Sound_Format)),     /*sample size - only used for PCM*/
						global->Sound_Format);
					/* Initialize sound (open stream)*/
					if(init_sound (pdata)) 
					{
						g_printerr("error opening portaudio\n");
						global->Sound_enable=0;
						if(!(global->no_display))
                        {
						    gdk_threads_enter();
						    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->SndEnable),0);
						    gdk_flush();
						    gdk_threads_leave();
						}
						else
                            capture_vid(NULL, all_data);
					} 
				}
			}
			break;
			
		case MKV_FORMAT:
			if(global->Sound_enable > 0) 
			{
				/*set channels, sample rate and allocate buffers*/
				set_sound(global, pdata, (void *) lavc_audio_data);
			}
			if(init_FormatContext((void *) all_data)<0)
			{
				capVid = FALSE;
				g_mutex_lock(videoIn->mutex);
					videoIn->capVid = capVid;
				g_mutex_unlock(videoIn->mutex);
				pdata->capVid = capVid;
				return (-1);
			}
			
			videoF->old_apts = 0;
			videoF->apts = 0;
			videoF->vpts = 0;
			
			/* start video capture*/
			capVid = TRUE;
			g_mutex_lock(videoIn->mutex);
				videoIn->capVid = capVid;
			g_mutex_unlock(videoIn->mutex);
			pdata->capVid = capVid;
			
			
			/* start sound capture*/
			if(global->Sound_enable > 0) 
			{
				/* Initialize sound (open stream)*/
				if(init_sound (pdata)) 
				{
					g_printerr("error opening portaudio\n");
					global->Sound_enable=0;
					if(!(global->no_display))
                    {
					    /*will this work with the checkbox disabled?*/
					    gdk_threads_enter();
					    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->SndEnable),0);
					    gdk_flush();
					    gdk_threads_leave();
					}
					else
					    capture_vid(NULL, all_data);
				}
			}
			break;
			
		default:
			
			break;
	}
	
	return (ret);
}


/* Called at avi capture stop       */
static void
aviClose (struct ALL_DATA *all_data)
{
	float tottime = 0;

	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct VideoFormatData *videoF = all_data->videoF;
	struct paRecordData *pdata = all_data->pdata;

	if (!(videoF->AviOut->closed))
	{
		tottime = (float) ((int64_t) (global->Vidstoptime - global->Vidstarttime) / 1000000); // convert to miliseconds
		
		if (global->debug) g_printf("stop= %llu start=%llu \n",
			(unsigned long long) global->Vidstoptime, (unsigned long long) global->Vidstarttime);
		if (tottime > 0) 
		{
			/*try to find the real frame rate*/
			videoF->AviOut->fps = (double) (global->framecount * 1000) / tottime;
		}
		else 
		{
			/*set the hardware frame rate*/   
			videoF->AviOut->fps = global->fps;
		}

		if (global->debug) g_printf("VIDEO: %d frames in %f ms = %f fps\n",global->framecount,tottime,videoF->AviOut->fps);
		/*------------------- close audio stream and clean up -------------------*/
		if (global->Sound_enable > 0) 
		{
			if (close_sound (pdata)) g_printerr("Sound Close error\n");
		} 
		AVI_close (videoF->AviOut);
		global->framecount = 0;
		global->Vidstarttime = 0;
		if (global->debug) g_printf ("close avi\n");
	}
	
	g_free(videoF->AviOut);
	pdata = NULL;
	global = NULL;
	videoIn = NULL;
	videoF->AviOut = NULL;
}


static void closeVideoFile(struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct paRecordData *pdata = all_data->pdata;
	
	int i=0;
	/*we are streaming so we need to lock a mutex*/
	gboolean capVid = FALSE;
	g_mutex_lock(videoIn->mutex);
		videoIn->capVid = capVid; /*flag video thread to stop recording frames*/
	g_mutex_unlock(videoIn->mutex);
	g_mutex_lock(pdata->mutex);
		pdata->capVid = capVid;
	g_mutex_unlock(pdata->mutex);
	/*wait for flag from video thread that recording has stopped    */
	/*wait on videoIn->VidCapStop by sleeping for 200 loops of 10 ms*/
	/*(test VidCapStop == TRUE on every loop)*/
	int stall = wait_ms(&(videoIn->VidCapStop), TRUE, videoIn->mutex, 10, 200);
	if( !(stall > 0) )
	{
		g_printerr("video capture stall on exit(%d) - timeout\n",
			videoIn->VidCapStop);
	}
	
	/*free video buffer allocations*/
	g_mutex_lock(global->mutex);
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
	g_mutex_unlock(global->mutex);
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			aviClose(all_data);
			break;
			
		case MKV_FORMAT:
			if(clean_FormatContext ((void*) all_data))
				g_printerr("matroska close returned a error\n");
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
	void *lavc_data,
	VidBuff *proc_buff)
{
	struct GLOBAL *global = all_data->global;
	struct GWIDGET *gwidget = all_data->gwidget;
	
	GThread *press_butt_thread = NULL;
	int ret=0;

	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			/*all video controls are now disabled so related values cannot be changed*/
			if(!(global->VidButtPress)) //if this is set AVI reached it's limit size
				ret = compress_frame(all_data, jpeg_struct, lavc_data, proc_buff);

			if (ret)
			{
				if (AVI_getErrno () == AVI_ERR_SIZELIM)
				{
					if (!(global->VidButtPress))
					{
						global->VidButtPress = TRUE;
						GError *err1 = NULL;
						/*avi file limit reached - must end capture close file and start new one*/
						if( (press_butt_thread =g_thread_create((GThreadFunc) split_avi, 
							all_data, //data
							FALSE,    //joinable - no need waiting for thread to finish
							&err1)    //error
						) == NULL)  
						{
							/*thread failed to start - stop video capture   */
							/*can't restart since we need IO thread to stop */
							g_printerr("Thread create failed: %s!!\n", err1->message );
							g_error_free ( err1 ) ;
							printf("stoping video capture\n");
							if(!(global->no_display))
                            {
							    gdk_threads_enter();
							    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
							    gdk_flush();
							    gdk_threads_leave();
							}
							else
							    capture_vid(NULL, all_data);
						}
						g_printf("AVI file size limit reached - restarted capture on new file\n");
					}
				} 
				else 
				{
					g_printerr ("write error on avi out \n");
				}
			}
			break;
		
		
		case MKV_FORMAT:
			//global->framecount++;
			
			ret = compress_frame(all_data, jpeg_struct, lavc_data, proc_buff);
			break;
		
		default:
			break;
	}
	return (0);
}

static int write_audio_frame (struct ALL_DATA *all_data, void *lavc_adata, AudBuff *proc_buff)
{
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct VideoFormatData *videoF = all_data->videoF;
	struct GWIDGET *gwidget = all_data->gwidget;
	
	int ret =0;
	GThread *press_butt_thread;
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			if(!(global->VidButtPress)) //if this is set AVI reached it's limit size
				ret = compress_audio_frame(all_data, lavc_adata, proc_buff);
		
			if (ret) 
			{	
				if (AVI_getErrno () == AVI_ERR_SIZELIM) 
				{
					if (!(global->VidButtPress))
					{
						global->VidButtPress = TRUE;
						GError *err1 = NULL;
						/*avi file limit reached - must end capture close file and start new one*/
						if( (press_butt_thread =g_thread_create((GThreadFunc) split_avi, 
							all_data, //data
							FALSE,    //joinable - no need waiting for thread to finish
							&err1)    //error
						) == NULL)  
						{
							/*thread failed to start - stop video capture   */
							/*can't restart since we need IO thread to stop */
							g_printerr("Thread create failed: %s!!\n", err1->message );
							g_error_free ( err1 ) ;
							printf("stoping video capture\n");
							if(!(global->no_display))
                            {
							    gdk_threads_enter();
							    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
							    gdk_flush();
							    gdk_threads_leave();
							}
							else
							    capture_vid(NULL, all_data);
						}
					
						//split_avi(all_data);/*blocking call*/
						g_printf("AVI file size limit reached - restarted capture on new file\n");
					}
				} 
				else 
				{
					g_printerr ("write error on avi out \n");
				}
					
			}
			break;
		case MKV_FORMAT:
			g_mutex_lock( pdata->mutex ); //why do we need this ???
				/*set pts*/
				videoF->apts = proc_buff->time_stamp;
				/*write audio chunk*/
				ret = compress_audio_frame(all_data, lavc_adata, proc_buff);
			g_mutex_unlock( pdata->mutex );
			break;
			
		default:
			
			break;
	}
	return (0);
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
			if (!(videoF->AviOut->track[0].audio_bytes)) 
			{ 
				/*only 1 audio stream*/
				/*time diff for audio-video*/
				int synctime= (int) (pdata->delay + pdata->snd_begintime - pdata->ts_ref)/1000000; /*convert to miliseconds*/
				if (global->debug) g_printf("shift sound by %d ms\n", synctime);
				if(synctime>10 && synctime<2000) 
				{ 	/*only sync between 10ms and 2 seconds*/
					if(global->Sound_Format == PA_FOURCC) 
					{	/*shift sound by synctime*/
						UINT32 shiftFrames = abs(synctime * global->Sound_SampRate / 1000);
						UINT32 shiftSamples = shiftFrames * global->Sound_NumChan;
						if (global->debug) g_printf("shift sound forward by %d samples\n", 
							shiftSamples);
						short *EmptySamp;
						EmptySamp=g_new0(short, shiftSamples);
						AVI_write_audio(videoF->AviOut,(BYTE *) EmptySamp,shiftSamples*sizeof(short));
						g_free(EmptySamp);
					} 
					else
					{
						/*use lavc encoder*/
					}
				}
			}
			break;
		
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
	//try to balance buffer overrun in read/write operations 
	if(w_ind >= r_ind)
		diff_ind = w_ind - r_ind;
	else
		diff_ind = (buff_size - r_ind) + w_ind;

	if(diff_ind <= 15) sched_sleep = 0; /* full throttle (must wait for audio at least 10 frames) */
	else if (diff_ind <= 20) sched_sleep = (diff_ind-15) * 2; /* <= 10  ms ~1 frame @ 90  fps */
	else if (diff_ind <= 25) sched_sleep = (diff_ind-15) * 3; /* <= 30  ms ~1 frame @ 30  fps */
	else if (diff_ind <= 30) sched_sleep = (diff_ind-15) * 4; /* <= 60  ms ~1 frame @ 15  fps */
	else if (diff_ind <= 35) sched_sleep = (diff_ind-15) * 5; /* <= 100 ms ~1 frame @ 10  fps */
	else if (diff_ind <= 40) sched_sleep = (diff_ind-15) * 6; /* <= 130 ms ~1 frame @ 7,5 fps */
	else sched_sleep = (diff_ind-15) * 7;                     /* <= 210 ms ~1 frame @ 5   fps */
	
	return sched_sleep;
}

static void process_audio(struct ALL_DATA *all_data, 
			AudBuff *aud_proc_buff,
			void *lavc_adata, 
			struct audio_effects **aud_eff)
{
	struct paRecordData *pdata = all_data->pdata;

	g_mutex_lock( pdata->mutex );
		gboolean used = pdata->audio_buff[pdata->r_ind].used;
	g_mutex_unlock( pdata->mutex );
  
	/*read at most 10 audio Frames (1152 * channels  samples each)*/
	if(used)
	{
		g_mutex_lock( pdata->mutex );
			memcpy(aud_proc_buff->frame, pdata->audio_buff[pdata->r_ind].frame, pdata->aud_numSamples*sizeof(SAMPLE));
			pdata->audio_buff[pdata->r_ind].used = FALSE;
			aud_proc_buff->time_stamp = pdata->audio_buff[pdata->r_ind].time_stamp;
			NEXT_IND(pdata->r_ind, AUDBUFF_SIZE);	
		g_mutex_unlock( pdata->mutex ); /*now we should be able to unlock the audio mutex*/	
		sync_audio_frame(all_data, aud_proc_buff);	
		/*run effects on data*/
		/*echo*/
		if((pdata->snd_Flags & SND_ECHO)==SND_ECHO) 
		{
			Echo(pdata, aud_proc_buff, *aud_eff, 300, 0.5);
		}
		else
		{
			close_DELAY((*aud_eff)->ECHO);
			(*aud_eff)->ECHO = NULL;
		}
		/*fuzz*/
		if((pdata->snd_Flags & SND_FUZZ)==SND_FUZZ) 
		{
			Fuzz(pdata, aud_proc_buff, *aud_eff);
		}
		else
		{
			close_FILT((*aud_eff)->HPF);
			(*aud_eff)->HPF = NULL;
		}
		/*reverb*/
		if((pdata->snd_Flags & SND_REVERB)==SND_REVERB) 
		{
			Reverb(pdata, aud_proc_buff, *aud_eff, 50);
		}
		else
		{
			close_REVERB(*aud_eff);
		}
		/*wahwah*/
		if((pdata->snd_Flags & SND_WAHWAH)==SND_WAHWAH) 
		{
			WahWah (pdata, aud_proc_buff, *aud_eff, 1.5, 0, 0.7, 0.3, 2.5);
		}
		else
		{
			close_WAHWAH((*aud_eff)->wahData);
			(*aud_eff)->wahData = NULL;
		}
		/*Ducky*/
		if((pdata->snd_Flags & SND_DUCKY)==SND_DUCKY) 
		{
			change_pitch(pdata, aud_proc_buff, *aud_eff, 2);
		}
		else
		{
			close_pitch (*aud_eff);
		}
			
		write_audio_frame(all_data, lavc_adata, aud_proc_buff);
	}
}

static gboolean process_video(struct ALL_DATA *all_data, 
				VidBuff *proc_buff, 
				struct lavcData **lavc_data, 
				struct JPEG_ENCODER_STRUCTURE **jpeg_struct)
{
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct paRecordData *pdata = all_data->pdata;
	int64_t max_drift = 0, audio_drift = 0;
	
	if (global->Sound_enable) {
		g_mutex_lock(pdata->mutex);
			audio_drift = pdata->ts_drift;
		g_mutex_unlock(pdata->mutex);
		max_drift = 1000000000 / global->fps;	/* one frame */
	}

	g_mutex_lock(videoIn->mutex);
		gboolean capVid = videoIn->capVid;
	g_mutex_unlock(videoIn->mutex);
	
	gboolean finish = FALSE;
	
	g_mutex_lock(global->mutex);
    		gboolean used = global->videoBuff[global->r_ind].used;
    	g_mutex_unlock(global->mutex);
	if (used)
	{
		g_mutex_lock(global->mutex);
	    		/*read video Frame*/
			proc_buff->bytes_used = global->videoBuff[global->r_ind].bytes_used;
			memcpy(proc_buff->frame, global->videoBuff[global->r_ind].frame, proc_buff->bytes_used);
			proc_buff->time_stamp = global->videoBuff[global->r_ind].time_stamp;
			global->videoBuff[global->r_ind].used = FALSE;
			/*signals an empty slot in the video buffer*/
			g_cond_broadcast(global->IO_cond);
				
			NEXT_IND(global->r_ind,global->video_buff_size);
			audio_drift -= global->av_drift;
		g_mutex_unlock(global->mutex);

		/* fprintf(stderr, "audio drift = %lli ms\n", audio_drift / 1000000); */
		/*process video Frame*/
		if (audio_drift > max_drift)
		{
			/* audio delayed */
			g_printf("audio drift: dropping/shifting frame\n");
			g_mutex_lock(global->mutex);
				global->av_drift += max_drift;
			g_mutex_unlock(global->mutex);

			switch (global->VidFormat)
			{
				case AVI_FORMAT:
					/* drop frame */
					break;

				case MKV_FORMAT:
					write_video_frame(all_data, (void *) jpeg_struct, (void *) lavc_data, proc_buff);
					break;

				default:
					break;
			}
		}
		else if (audio_drift < -1 * max_drift)
		{
			/* audio too fast */
			g_printf("audio drift: duplicating/shifting frame\n");
			g_mutex_lock(global->mutex);
				global->av_drift -= max_drift;
			g_mutex_unlock(global->mutex);

			switch (global->VidFormat)
			{
				case AVI_FORMAT:
					/* write frame twice */
					write_video_frame(all_data, (void *) jpeg_struct, (void *) lavc_data, proc_buff);
					write_video_frame(all_data, (void *) jpeg_struct, (void *) lavc_data, proc_buff);
					break;

				case MKV_FORMAT:
					write_video_frame(all_data, (void *) jpeg_struct, (void *) lavc_data, proc_buff);
					break;

				default:
					break;
			}
		}
		else
			write_video_frame(all_data, (void *) jpeg_struct, (void *) lavc_data, proc_buff);
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
	else if ((global->VidCodec == CODEC_LAVC) && (global->Frame_Flags==0) &&
		((global->format==V4L2_PIX_FMT_NV12) || (global->format==V4L2_PIX_FMT_NV21)))
	{
		/*store yuv420p frame*/
		global->videoBuff[global->w_ind].bytes_used = videoIn->buf.bytesused;
		memcpy(global->videoBuff[global->w_ind].frame, 
			videoIn->tmpbuffer, 
			global->videoBuff[global->w_ind].bytes_used);
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
}

int store_video_frame(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	struct GLOBAL *global = all_data->global;
	
	int ret = 0;
	int producer_sleep = 0;
	
	g_mutex_lock(global->mutex);
	
	if(!global->videoBuff)
	{
		g_printerr("WARNING: video ring buffer not allocated yet - dropping frame.");
		g_mutex_unlock(global->mutex);
		return(-1);
	}
	
	if (!global->videoBuff[global->w_ind].used)
	{
		store_at_index(data);
		producer_sleep = buff_scheduler(global->w_ind, global->r_ind, global->video_buff_size);
		NEXT_IND(global->w_ind, global->video_buff_size);
	}
	else
	{
		if(global->debug) g_printerr("WARNING: buffer full waiting for free space\n");
		/*wait for IO_cond at least 200ms*/
		GTimeVal *timev;
		timev = g_new0(GTimeVal, 1);
		g_get_current_time(timev); 
		g_time_val_add(timev,100*1000); /*100 ms*/
		/* WARNING: if system time changes it can cause undesired behaviour */
		if(g_cond_timed_wait(global->IO_cond, global->mutex, timev))
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
		
		g_free(timev);
	}
	if(!ret) global->framecount++;
	
	g_mutex_unlock(global->mutex);
	
	/*-------------if needed, make the thread sleep for a while----------------*/
	if(producer_sleep) sleep_ms(producer_sleep);
	
	return ret;
}

void *IO_loop(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	
	struct GWIDGET *gwidget = all_data->gwidget;
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;

	struct JPEG_ENCODER_STRUCTURE *jpg_data=NULL;
	struct lavcData *lavc_data = NULL;
	struct lavcAData *lavc_audio_data = NULL;
	struct audio_effects *aud_eff = NULL;
	
	gboolean finished=FALSE;
	g_mutex_lock(videoIn->mutex);
		videoIn->IOfinished=FALSE;
	g_mutex_unlock(videoIn->mutex);
	
	gboolean capVid=FALSE;
    
	gboolean failed = FALSE;
	int proc_flag = 0;
	int diff_ind=0;
	
	//buffers to be processed (video and audio)
	int frame_size=0;
	VidBuff *proc_buff = NULL;
	AudBuff *aud_proc_buff = NULL;
	
	if(initVideoFile(all_data, (void *) &(lavc_audio_data))<0)
	{
		g_printerr("Cap Video failed\n");
		if(!(global->no_display))
        {
		    gdk_threads_enter();
		    /*disable signals for video capture callback*/
		    g_signal_handlers_block_by_func(GTK_TOGGLE_BUTTON(gwidget->CapVidButt), G_CALLBACK (capture_vid), all_data);
		    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
		    gtk_button_set_label(GTK_BUTTON(gwidget->CapVidButt),_("Cap. Video"));
		    /*enable signals for video capture callback*/
		    g_signal_handlers_unblock_by_func(GTK_TOGGLE_BUTTON(gwidget->CapVidButt), G_CALLBACK (capture_vid), all_data);
		    gdk_flush();
		    gdk_threads_leave();
		
		    /*enabling sound and video compression controls*/
		    set_sensitive_vid_contrls(TRUE, global->Sound_enable, gwidget);
		}
		
		finished = TRUE;
		failed = TRUE;
	}
	else
	{
		if(global->debug) g_printf("IO thread started...OK\n");
		frame_size = global->height*global->width*2;
		proc_buff = g_new0(VidBuff, 1);
		proc_buff->frame = g_new0(BYTE, frame_size);
	
		if (global->Sound_enable) 
		{
			aud_eff=init_audio_effects ();
			aud_proc_buff = g_new0(AudBuff, 1);
			aud_proc_buff->frame = g_new0(SAMPLE, pdata->aud_numSamples);
			global->av_drift = 0;
			pdata->ts_drift = 0;
		}
	}
	
	if(!failed)
	{
		while(!finished)
		{
			if(global->Sound_enable)
			{
				g_mutex_lock(videoIn->mutex);
					capVid = videoIn->capVid;
				g_mutex_unlock(videoIn->mutex);

				g_mutex_lock( pdata->mutex );
				g_mutex_lock( global->mutex );
					//check read/write index delta in frames 
					if(global->w_ind >= global->r_ind)
						diff_ind = global->w_ind - global->r_ind;
					else
						diff_ind = (global->video_buff_size - global->r_ind) + global->w_ind;
			
					if( (pdata->audio_buff[pdata->r_ind].used && global->videoBuff[global->r_ind].used) &&
						(pdata->audio_buff[pdata->r_ind].time_stamp < global->videoBuff[global->r_ind].time_stamp))
					{
						proc_flag = 1;	//process audio
					}
					else if(pdata->audio_buff[pdata->r_ind].used && global->videoBuff[global->r_ind].used)
					{
						proc_flag = 2;    //process video
					}
					else if (diff_ind < 10 && capVid)
					{
						proc_flag = 3;	//sleep -wait for audio (at most 10 video frames)
					}
					else if(pdata->audio_buff[pdata->r_ind].used)
					{	
						proc_flag = 1;    //process audio
					}
					else proc_flag = 2;    //process video
				g_mutex_unlock( global->mutex );
				g_mutex_unlock( pdata->mutex );
			
			
				switch(proc_flag)
				{
					case 1:
						//g_printf("processing audio frame\n");
						process_audio(all_data, aud_proc_buff, (void *) &lavc_audio_data, &(aud_eff));
						break;
					case 2:
						//g_printf("processing video frame\n");
						finished = process_video (all_data, proc_buff, &(lavc_data), &(jpg_data));
						break;
					default:
						sleep_ms(10); /*make the thread sleep for 10ms*/
						break;
				}
			}
			else
			{
				//process video
				finished = process_video (all_data, proc_buff, &(lavc_data), &(jpg_data));
			}
		
			if(finished)
			{
				//wait for audio to finish
				int stall = wait_ms(&(pdata->streaming), FALSE, pdata->mutex, 10, 25);
				if( !(stall > 0) )
				{
					g_printerr("audio streaming stalled - timeout\n");
				}
				//process any remaining audio
				if(global->Sound_enable) process_audio(all_data, aud_proc_buff,(void *) &lavc_audio_data, &(aud_eff));
			}
		}
	
		/*finish capture*/
		closeVideoFile(all_data);
	
		/*free proc buffer*/
		g_free(proc_buff->frame);
		g_free(proc_buff);
		if(aud_proc_buff) 
		{
			g_free(aud_proc_buff->frame);
			g_free(aud_proc_buff);
		}
		if (global->Sound_enable) close_audio_effects (aud_eff);
	}
	
	if(lavc_data != NULL)
	{
		clean_lavc(&lavc_data);
		lavc_data = NULL;
	}
	if(lavc_audio_data != NULL)
	{
		clean_lavc_audio(&lavc_audio_data);
		lavc_audio_data = NULL;
	}
	if(jpg_data != NULL) g_free(jpg_data);
	jpg_data = NULL;
	
	if(global->jpeg != NULL) g_free(global->jpeg); //jpeg buffer used in encoding
	global->jpeg = NULL;
	
	if(global->debug) g_printf("IO thread finished...OK\n");

	global->VidButtPress = FALSE;
	g_mutex_lock(videoIn->mutex);
		videoIn->IOfinished=TRUE;
	g_mutex_unlock(videoIn->mutex);
	
	return ((void *) 0);
}

