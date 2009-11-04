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
#include <glib/gprintf.h>
#include <stdlib.h>

#include "../config.h"
#include "defs.h"
#include "create_video.h"
#include "v4l2uvc.h"
#include "avilib.h"
#include "globals.h"
#include "sound.h"
#include "mp2.h"
#include "ms_time.h"
#include "string_utils.h"
#include "callbacks.h"
#include "vcodecs.h"
#include "video_format.h"
#include "audio_effects.h"
#include "globals.h"

/*--------------------------- controls enable/disable --------------------------*/
/* sound controls*/
void 
set_sensitive_snd_contrls (const int flag, struct GWIDGET *gwidget)
{
	gtk_widget_set_sensitive (gwidget->SndSampleRate, flag);
	gtk_widget_set_sensitive (gwidget->SndDevice, flag);
	gtk_widget_set_sensitive (gwidget->SndNumChan, flag);
	gtk_widget_set_sensitive (gwidget->SndComp, flag);
}

/*video controls*/
void 
set_sensitive_vid_contrls (const int flag, const int sndEnable, struct GWIDGET *gwidget)
{
	/* sound and video compression controls */
	gtk_widget_set_sensitive (gwidget->VidCodec, flag);
	gtk_widget_set_sensitive (gwidget->SndEnable, flag); 
	gtk_widget_set_sensitive (gwidget->VidInc, flag);
	gtk_widget_set_sensitive (gwidget->VidFormat, flag);/*video format combobox*/
	/* resolution and input format combos   */
	gtk_widget_set_sensitive (gwidget->Resolution, flag);
	gtk_widget_set_sensitive (gwidget->InpType, flag);
	/* Video File entry and open button     */
	gtk_widget_set_sensitive (gwidget->VidFNameEntry, flag);
	gtk_widget_set_sensitive (gwidget->VidFileButt, flag);
	
	if(sndEnable > 0) 
	{
		set_sensitive_snd_contrls(flag, gwidget);
	}
	gwidget->vid_widget_state = flag;
}

int initVideoFile(struct ALL_DATA *all_data)
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct VideoFormatData *videoF = all_data->videoF;
	
	const char *compression= get_vid4cc(global->VidCodec);
	videoF->vcodec = get_vcodec_id(global->VidCodec);
	videoF->acodec = CODEC_ID_NONE;
	int ret = 0;
	int i=0;
	
	/*allocate video buffer*/
	if (global->videoBuff == NULL)
	{
		int framesize= videoIn->height*videoIn->width*2; /*yuyv (maximum size)*/
		/*alloc video frames to videoBuff*/
		global->videoBuff = g_new0(VidBuff,VIDBUFF_SIZE);
		for(i=0;i<VIDBUFF_SIZE;i++)
		{
			global->videoBuff[i].frame = g_new0(BYTE,framesize);
		}
	}
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			if(videoF->AviOut != NULL)
			{
				g_free(videoF->AviOut);
				videoF->AviOut = NULL;
			}
			videoF->AviOut = g_new0(struct avi_t, 1);
			videoF->keyframe = 1;
			
			if(AVI_open_output_file(videoF->AviOut, videoIn->VidFName)<0) 
			{
				g_printerr("Error: Couldn't create Video.\n");
				videoIn->capVid = FALSE;
				pdata->capVid = videoIn->capVid;
				return(-1);
			} 
			else 
			{
				/*disabling sound and video compression controls*/
				set_sensitive_vid_contrls(FALSE, global->Sound_enable, gwidget);
		
				AVI_set_video(videoF->AviOut, videoIn->width, videoIn->height, 
					videoIn->fps,compression);
		  
				/* start video capture*/
				videoIn->capVid = TRUE;
				pdata->capVid = videoIn->capVid;
				
				/* start sound capture*/
				if(global->Sound_enable > 0) 
				{
					/*get channels and sample rate*/
					set_sound(global,pdata);
					/*set audio header for avi*/
					AVI_set_audio(videoF->AviOut, global->Sound_NumChan, 
						global->Sound_SampRate,
						global->Sound_bitRate,
						16, /*only used for PCM*/
						global->Sound_Format);
					/* Initialize sound (open stream)*/
					if(init_sound (pdata)) 
					{
						g_printerr("error opening portaudio\n");
						global->Sound_enable=0;
						gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->SndEnable),0);
					} 
					else 
					{
						if (global->Sound_Format == ISO_FORMAT_MPEG12) 
						{
							init_MP2_encoder(pdata, global->Sound_bitRate);    
						}
					}
				
				}
			}
			break;
			
		case MKV_FORMAT:
			/* start sound capture*/
			if(global->Sound_enable > 0) 
			{
				/*set channels, sample rate and allocate buffers*/
				set_sound(global,pdata);
			}
			init_FormatContext((void *) all_data);
			/* start sound capture*/
			if(global->Sound_enable > 0) 
			{
				/* Initialize sound (open stream)*/
				if(init_sound (pdata)) 
				{
					g_printerr("error opening portaudio\n");
					global->Sound_enable=0;
					gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->SndEnable),0);
				} 
				else 
				{
					if (global->Sound_Format == ISO_FORMAT_MPEG12) 
					{
						init_MP2_encoder(pdata, global->Sound_bitRate);
					}
				}
			}
			/*disabling sound and video compression controls*/
			set_sensitive_vid_contrls(FALSE, global->Sound_enable, gwidget);
			
			videoF->keyframe = 1;
			videoF->old_apts = 0;
			videoF->apts = 0;
			videoF->vpts = 0;
			
			/* start video capture*/
			videoIn->capVid = TRUE;
			pdata->capVid = videoIn->capVid;
			break;
		default:
			
			break;
	}

	GError *err1 = NULL;
	/*start IO thread*/
	if( (all_data->IO_thread = g_thread_create_full((GThreadFunc) IO_loop, 
		(void *) all_data,       //data - argument supplied to thread
		global->stack_size,       //stack size
		TRUE,                     //joinable
		FALSE,                    //bound
		G_THREAD_PRIORITY_NORMAL, //priority - no priority for threads in GNU-Linux
		&err1)                    //error
		) == NULL)
	{
		g_printerr("Thread create failed: %s!!\n", err1->message );
		g_error_free ( err1 ) ;
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
		
		if (global->debug) g_printf("stop= %llu start=%llu \n",global->Vidstoptime,global->Vidstarttime);
		if (tottime > 0) 
		{
			/*try to find the real frame rate*/
			videoF->AviOut->fps = (double) (global->framecount * 1000) / tottime;
		}
		else 
		{
			/*set the hardware frame rate*/   
			videoF->AviOut->fps=videoIn->fps;
		}

		if (global->debug) g_printf("VIDEO: %d frames in %f ms = %f fps\n",global->framecount,tottime,videoF->AviOut->fps);
		/*------------------- close audio stream and clean up -------------------*/
		if (global->Sound_enable > 0) 
		{
			/*wait for audio to finish*/
			int stall = wait_ms( &pdata->streaming, FALSE, 10, 30 );
			if(!(stall)) 
			{
				g_printerr("WARNING:sound capture stall (still streaming(%d) \n",
					pdata->streaming);
				pdata->streaming = 0;
			}
			/*write any available audio data*/  
			if(pdata->audio_flag)
			{
				g_printerr("writing %d bytes of audio data\n",pdata->snd_numBytes);
				g_mutex_lock( pdata->mutex);
					if(global->Sound_Format == PA_FOURCC)
					{
						if(pdata->vid_sndBuff) 
						{
							Float2Int16(pdata);
							AVI_write_audio(videoF->AviOut,(BYTE *) pdata->vid_sndBuff1,pdata->snd_numSamples*2);
						}
					}
					else if (global->Sound_Format == ISO_FORMAT_MPEG12)
					{
						int size_mp2=0;
						if(pdata->vid_sndBuff && pdata->mp2Buff) 
						{
							size_mp2 = MP2_encode(pdata,0);
							AVI_write_audio(videoF->AviOut,pdata->mp2Buff,size_mp2);
							pdata->flush = 1; /*flush mp2 encoder*/
							size_mp2 = MP2_encode(pdata,0);
							AVI_write_audio(videoF->AviOut,pdata->mp2Buff,size_mp2);
							pdata->flush = 0;
						}
					}
				g_mutex_unlock( pdata->mutex );
			}
			pdata->audio_flag = 0; /*all audio should have been writen by now*/
			
			if (close_sound (pdata)) g_printerr("Sound Close error\n");
			if(global->Sound_Format == ISO_FORMAT_MPEG12) close_MP2_encoder();
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


void closeVideoFile(struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	struct paRecordData *pdata = all_data->pdata;
	struct GWIDGET *gwidget = all_data->gwidget;
	
	int i=0;
	videoIn->capVid = FALSE; /*flag video thread to stop recording frames*/
	pdata->capVid = videoIn->capVid;
	/*wait for flag from video thread that recording has stopped    */
	/*wait on videoIn->VidCapStop by sleeping for 200 loops of 10 ms*/
	/*(test VidCapStop == TRUE on every loop)*/
	int stall = wait_ms(&(videoIn->VidCapStop), TRUE, 10, 200);
	if( !(stall > 0) )
	{
		g_printerr("video capture stall on exit(%d) - timeout\n",
			videoIn->VidCapStop);
	}
	global->Vidstoptime = ns_time();
	
	/*join IO thread*/
	g_thread_join( all_data->IO_thread );
	
	/*free video buffer allocations*/
	if (global->videoBuff != NULL)
	{
		/*free video frames to videoBuff*/
		for(i=0;i<VIDBUFF_SIZE;i++)
		{
			g_free(global->videoBuff[i].frame);
			global->videoBuff[i].frame = NULL;
		}
		g_free(global->videoBuff);
		global->videoBuff = NULL;
	}
	
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

	/*enabling sound and video compression controls*/
	set_sensitive_vid_contrls(TRUE, global->Sound_enable, gwidget);
	global->Vidstoptime = 0;
	global->Vidstarttime = 0;
	global->framecount = 0;
}

int write_video_data(struct ALL_DATA *all_data, BYTE *buff, int size, QWORD v_ts)
{
	struct VideoFormatData *videoF = all_data->videoF;
	struct GLOBAL *global = all_data->global;
	
	int ret =0;
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			ret = AVI_write_frame (videoF->AviOut, buff, size, videoF->keyframe);
			break;
		
		case MKV_FORMAT:
			videoF->vpts = v_ts;
			ret = write_video_packet (buff, size, global->fps, videoF);
			break;
			
		default:
			
			break;
	}
	
	return (ret);
}

int write_video_frame (struct ALL_DATA *all_data, 
	void *jpeg_struct, 
	void *lavc_data,
	VidBuff *proc_buff)
{
	struct GLOBAL *global = all_data->global;
	struct VideoFormatData *videoF = all_data->videoF;
	
	int ret=0;
	GThread *press_butt_thread;
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			/*all video controls are now disabled so related values cannot be changed*/
			ret = compress_frame(all_data, jpeg_struct, lavc_data, proc_buff);

			if (ret)
			{
				if (AVI_getErrno () == AVI_ERR_SIZELIM)
				{
					if (!(global->VidButtPress))
					{
						
						GError *err1 = NULL;
					
						/*avi file limit reached - must end capture close file and start new one*/
						if( (press_butt_thread =g_thread_create((GThreadFunc) split_avi, 
							all_data, //data
							FALSE,    //joinable - no need waiting for thread to finish
							&err1)    //error
						) == NULL)  
						{
							g_printerr("Thread create failed: %s!!\n", err1->message );
							g_error_free ( err1 ) ;
							printf("using blocking method\n");
							split_avi(all_data); /*blocking call*/
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
		   
			global->framecount++;
			if (videoF->keyframe) videoF->keyframe=0; /*resets key frame*/
			
			break;
		
		
		case MKV_FORMAT:
			global->framecount++;
			
			ret = compress_frame(all_data, jpeg_struct, lavc_data, proc_buff);
			break;
		
		default:
			break;
	}
	return (0);
}

int write_audio_frame (struct ALL_DATA *all_data)
{
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct VideoFormatData *videoF = all_data->videoF;
	
	int ret =0;
	GThread *press_butt_thread;
	
	switch (global->VidFormat)
	{
		case AVI_FORMAT:
			g_mutex_lock( pdata->mutex );
				/*write audio chunk*/
				if(global->Sound_Format == PA_FOURCC) 
				{
					Float2Int16(pdata); /*convert from float sample to 16 bit PCM*/
					ret=AVI_write_audio(videoF->AviOut,(BYTE *) pdata->vid_sndBuff1,pdata->snd_numSamples*2);
				}
				else if(global->Sound_Format == ISO_FORMAT_MPEG12)
				{
					int size_mp2 = MP2_encode(pdata,0);
					ret=AVI_write_audio(videoF->AviOut,pdata->mp2Buff,size_mp2);
				}
				pdata->audio_flag=0;
			g_mutex_unlock( pdata->mutex );
			//videoF->keyframe = 1; /*marks next frame as key frame*/
		
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
							g_printerr("Thread create failed: %s!!\n", err1->message );
							g_error_free ( err1 ) ;
							printf("using blocking method\n");
							split_avi(all_data); /*blocking call*/
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
			g_mutex_lock( pdata->mutex );
				/*set pts*/
				videoF->apts = videoF->old_apts;
				videoF->old_apts = pdata->a_ts - global->Vidstarttime;
				/*write audio chunk*/
				if(global->Sound_Format == PA_FOURCC) 
				{
					Float2Int16(pdata); /*convert from float sample to 16 bit PCM*/
					ret = write_audio_packet ((BYTE *) pdata->vid_sndBuff1, pdata->snd_numSamples*2, pdata->samprate, videoF);
				}
				else if(global->Sound_Format == ISO_FORMAT_MPEG12)
				{
					int size_mp2 = MP2_encode(pdata,0);
					ret = write_audio_packet (pdata->mp2Buff, size_mp2, pdata->samprate, videoF);
				}
				pdata->audio_flag=0;
			g_mutex_unlock( pdata->mutex );
			break;
			
		default:
			
			break;
	}
	return (0);
}

int sync_audio_frame(struct ALL_DATA *all_data)
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
				int synctime= (int) (pdata->snd_begintime - global->Vidstarttime)/1000000; /*convert to miliseconds*/
				if (global->debug) g_printf("shift sound by %d ms\n", synctime);
				if(synctime>10 && synctime<5000) 
				{ 	/*only sync between 10ms and 5 seconds*/
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
					else if(global->Sound_Format == ISO_FORMAT_MPEG12) 
					{
						int size_mp2 = MP2_encode(pdata, synctime);
						if (global->debug) g_printf("shift sound forward by %d bytes\n",size_mp2);
						AVI_write_audio(videoF->AviOut,pdata->mp2Buff,size_mp2);
					}
				}
			}
			break;
		
		case MKV_FORMAT:
			//sync audio 
		/*
			if(!(videoF->old_apts)) 
			{
				int synctime= pdata->snd_begintime - global->Vidstarttime; 
				if (global->debug) g_printf("shift sound by %d ms\n",synctime);
				if(synctime>10 && synctime<5000) 
				{ 	//only sync between 10ms and 5 seconds
					//shift sound by synctime
					UINT32 shiftFrames = abs(synctime * global->Sound_SampRate / 1000);
					UINT32 shiftSamples = shiftFrames * global->Sound_NumChan;
					if (global->debug) g_printf("shift sound forward by %d samples\n", shiftSamples);
					videoF->old_apts = shiftSamples;
				} 
				else
					videoF->old_apts = 1;
			}
		 */
			break;
		
		default:
			break;
	}
	
	return (0);
}

#define NEXT_IND(ind,size) ind++;if(ind>=size) ind=0
//#define PREV_IND(ind,size) ind--;if(ind<0) ind=size-1

static int buff_scheduler(int w_ind, int r_ind)
{
	int diff_ind = 0;
	int vid_sleep = 0;
	//try to balance buffer overrun in read/write operations 
	if(w_ind >= r_ind)
		diff_ind = w_ind - r_ind;
	else
		diff_ind = (VIDBUFF_SIZE - r_ind) + w_ind;

	if(diff_ind <= 2) vid_sleep = 0;                  /*    full throtle             */
	else if (diff_ind <= 5)  vid_sleep = diff_ind * 2;/* <= 10  ms ~1 frame @ 90  fps*/
	else if (diff_ind <= 10) vid_sleep = diff_ind * 3;/* <= 30  ms ~1 frame @ 30  fps*/
	else if (diff_ind <= 15) vid_sleep = diff_ind * 4;/* <= 60  ms ~1 frame @ 15  fps*/
	else if (diff_ind <= 20) vid_sleep = diff_ind * 5;/* <= 100 ms ~1 frame @ 10  fps*/
	else if (diff_ind <= 25) vid_sleep = diff_ind * 6;/* <= 130 ms ~1 frame @ 7,5 fps*/
	else vid_sleep = diff_ind * 7;                    /* <= 210 ms ~1 frame @ 5   fps*/
	
	return vid_sleep;
}

int store_video_frame(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;

	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	int ret = 0;
	
	g_mutex_lock(global->mutex);
	if (!global->videoBuff[global->w_ind].used)
	{
		global->videoBuff[global->w_ind].time_stamp = global->v_ts;
		/*store frame at index*/
		if((global->VidCodec == CODEC_MJPEG) &&
			(global->Frame_Flags==0) &&
			(videoIn->formatIn==V4L2_PIX_FMT_MJPEG))
		{
			/*store MJPEG frame*/
			global->videoBuff[global->w_ind].bytes_used = videoIn->buf.bytesused;
			memcpy(global->videoBuff[global->w_ind].frame, 
				videoIn->tmpbuffer, 
				global->videoBuff[global->w_ind].bytes_used);
		}
		else
		{
			/*store YUYV frame*/
			global->videoBuff[global->w_ind].bytes_used = videoIn->height*videoIn->width*2;
			memcpy(global->videoBuff[global->w_ind].frame, 
				videoIn->framebuffer, 
				global->videoBuff[global->w_ind].bytes_used);
		}
		global->videoBuff[global->w_ind].used = TRUE;
		
		global->vid_sleep = buff_scheduler(global->w_ind, global->r_ind);
		
		NEXT_IND(global->w_ind, VIDBUFF_SIZE);
	}
	else
	{
		if(global->debug) g_printerr("WARNING: buffer full waiting for free space\n");
		/*wait for IO_cond at least 200ms*/
		GTimeVal *timev;
		timev = g_new0(GTimeVal, 1);
		g_get_current_time(timev);
		g_time_val_add(timev,200*1000); /*200 ms*/
		if(g_cond_timed_wait(global->IO_cond, global->mutex, timev))
		{
			/*try to store the frame again*/
			if (!global->videoBuff[global->w_ind].used)
			{
				global->videoBuff[global->w_ind].time_stamp = global->v_ts;
				/*store frame at index*/
				if((global->VidCodec == CODEC_MJPEG) &&
					(global->Frame_Flags==0) &&
					(videoIn->formatIn==V4L2_PIX_FMT_MJPEG))
				{
					/*store MJPEG frame*/
					global->videoBuff[global->w_ind].bytes_used = videoIn->buf.bytesused;
					memcpy(global->videoBuff[global->w_ind].frame, 
						videoIn->tmpbuffer, 
						global->videoBuff[global->w_ind].bytes_used);
				}
				else
				{
					/*store YUYV frame*/
					global->videoBuff[global->w_ind].bytes_used = videoIn->height*videoIn->width*2;
					memcpy(global->videoBuff[global->w_ind].frame, 
						videoIn->framebuffer, 
						global->videoBuff[global->w_ind].bytes_used);
				}
				global->videoBuff[global->w_ind].used = TRUE;
				NEXT_IND(global->w_ind, VIDBUFF_SIZE);
			}
			else ret = -1;/*drop frame*/
			
		}
		else ret = -2;/*drop frame*/
		
		g_free(timev);
		
	}
	g_mutex_unlock(global->mutex);
	
	return ret;
}

void *IO_loop(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;

	struct JPEG_ENCODER_STRUCTURE *jpeg_struct=NULL;
	struct lavcData *lavc_data = NULL;
	struct audio_effects *aud_eff = init_audio_effects ();
	
	//int a_ind=0; /*audio buffer current index*/
	gboolean finished=FALSE;

	//buffer to be processed
	int frame_size = videoIn->height*videoIn->width*2;
	VidBuff *proc_buff = g_new0(VidBuff, 1);
	proc_buff->frame = g_new0(BYTE, frame_size);
	
	if(global->debug) g_printf("IO thread started...OK\n");

	/*IO loop*/
	while(!finished)
	{
		/*process the video*/
		g_mutex_lock(global->mutex);
			if (global->videoBuff[global->r_ind].used)
			{
				/*read video Frame*/
				proc_buff->bytes_used = global->videoBuff[global->r_ind].bytes_used;
				memcpy(proc_buff->frame, global->videoBuff[global->r_ind].frame, proc_buff->bytes_used);
				proc_buff->time_stamp = global->videoBuff[global->r_ind].time_stamp;
				global->videoBuff[global->r_ind].used = FALSE;
				/*signals an empty slot in the video buffer*/
				g_cond_broadcast(global->IO_cond);
				
				NEXT_IND(global->r_ind,VIDBUFF_SIZE);
			g_mutex_unlock(global->mutex);

				/*process video Frame*/
				write_video_frame(all_data, (void *) &(jpeg_struct), (void *) &(lavc_data), proc_buff);
			}
			else
			{
			g_mutex_unlock(global->mutex);
				if (videoIn->capVid)
				{
					/*video buffer underrun            */
					/*wait for next frame (sleep 20 ms)*/
					sleep_ms(20);
				}
				else 
				{
					finished = TRUE; /*all frames processed and no longer capturing so finish*/
				}
			}

		/*now process the audio*/
		/*----------------------- add audio -----------------------------*/
		if ((global->Sound_enable) && (pdata->audio_flag>0)) 
		{
			g_mutex_lock( pdata->mutex );
				sync_audio_frame(all_data);
				//if(global->debug) g_printf("audio: %lu frames per buffer and %d total samples\n",
				//	pdata->framesPerBuffer, pdata->numSamples);
				/*run effects on data*/
				/*echo*/
				if((pdata->snd_Flags & SND_ECHO)==SND_ECHO) 
				{
					Echo(pdata, aud_eff, 300, 0.5);
				}
				else
				{
					close_DELAY(aud_eff->ECHO);
					aud_eff->ECHO = NULL;
				}
				/*fuzz*/
				if((pdata->snd_Flags & SND_FUZZ)==SND_FUZZ) 
				{
					Fuzz(pdata, aud_eff);
				}
				else
				{
					close_FILT(aud_eff->HPF);
					aud_eff->HPF = NULL;
				}
				/*reverb*/
				if((pdata->snd_Flags & SND_REVERB)==SND_REVERB) 
				{
					Reverb(pdata, aud_eff, 50);
				}
				else
				{
					close_REVERB(aud_eff);
				}
				/*wahwah*/
				if((pdata->snd_Flags & SND_WAHWAH)==SND_WAHWAH) 
				{
					WahWah (pdata, aud_eff, 1.5, 0, 0.7, 0.3, 2.5);
				}
				else
				{
					close_WAHWAH(aud_eff->wahData);
					aud_eff->wahData = NULL;
				}
				/*Ducky*/
				if((pdata->snd_Flags & SND_DUCKY)==SND_DUCKY) 
				{
					change_pitch(pdata, aud_eff, 2);
				}
				else
				{
					close_pitch (aud_eff);
				}
			g_mutex_unlock( pdata->mutex );
				
			write_audio_frame(all_data);
		}
	}
	
	/*free proc buffer*/
	g_free(proc_buff->frame);
	g_free(proc_buff);
	
	if(lavc_data != NULL)
	{
		int nf = clean_lavc(&lavc_data);
		if(global->debug) g_printf(" total frames: %d  -- encoded: %d\n", global->framecount, nf);
		lavc_data = NULL;
	}
	g_free(jpeg_struct);
	
	close_audio_effects (aud_eff);
	/*make sure video thread returns to full throtle*/
	global->vid_sleep = 0;

	if(global->debug) g_printf("IO thread finished...OK\n");
	
	return ((void *) 0);
}

