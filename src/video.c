/*******************************************************************************#
#	    guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
#										#
# This program is free software; you can redistribute it and/or modify         	#
# it under the terms of the GNU General Public License as published by   	#
# the Free Software Foundation; either version 2 of the License, or           	#
# (at your option) any later version.                                          	#
#                                                                              	#
# This program is distributed in the hope that it will be useful,              	#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             	#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 	#
#                                                                              	#
# You should have received a copy of the GNU General Public License           	#
# along with this program; if not, write to the Free Software                  	#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	#
#                                                                              	#
********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <linux/videodev.h>
#include <string.h>
#include <pthread.h>
#include <portaudio.h>
#include <SDL/SDL.h>

#include "defs.h"
#include "video.h"
#include "guvcview.h"
#include "v4l2uvc.h"
#include "avilib.h"
#include "globals.h"
#include "colorspaces.h"
#include "sound.h"
#include "jpgenc.h"
#include "autofocus.h"
#include "picture.h"
#include "ms_time.h"
#include "string_utils.h"
#include "mp2.h"
#include "callbacks.h"

/*-------------------------------- Main Video Loop ---------------------------*/ 
/* run in a thread (SDL overlay)*/
void *main_loop(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct focusData *AFdata = all_data->AFdata;
	struct vdIn *videoIn = all_data->videoIn;
	struct avi_t *AviOut = all_data->AviOut;

	pthread_t pth_press_butt;
	pthread_attr_t pth_press_attr;
	
	pthread_attr_init(&pth_press_attr);
	pthread_attr_setdetachstate(&pth_press_attr, PTHREAD_CREATE_DETACHED);
	
	SDL_Event event;
	/*the main SDL surface*/
	SDL_Surface *pscreen = NULL;
	SDL_Overlay *overlay=NULL;
	SDL_Rect drect;
	const SDL_VideoInfo *info;
	char driver[128];
	
        struct JPEG_ENCODER_STRUCTURE *jpeg_struct=NULL;
	
	BYTE *p = NULL;
	BYTE *pim= NULL;
	BYTE *pavi=NULL;
	
	
    	int keyframe = 1;
    	
    	int last_focus = 0;
    	if (global->AFcontrol) {
    		last_focus = get_focus(videoIn);
    		if (last_focus < 0) last_focus=255; /*make sure we wait for focus to settle on first check*/
		//printf("last_focus is %d and focus is %d\n",last_focus, AFdata->focus);
	}
    
	/*gets the stack size for the thread (DEBUG)*/ 
	//pthread_attr_getstacksize (&attr, &videostacksize);
	//if (global->debug) printf("Video Thread: stack size = %d bytes \n", (int) videostacksize);
	
	static Uint32 SDL_VIDEO_Flags =
		SDL_ANYFORMAT | SDL_DOUBLEBUF | SDL_RESIZABLE;
 
	/*----------------------------- Test SDL capabilities ---------------------*/
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
		printf("Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}
	
	/* For this version, we will use hardware acceleration as default*/
	if(global->hwaccel) {
		if ( ! getenv("SDL_VIDEO_YUV_HWACCEL") ) {
			putenv("SDL_VIDEO_YUV_HWACCEL=1");
		}
		if ( ! getenv("SDL_VIDEO_YUV_DIRECT") ) {
			putenv("SDL_VIDEO_YUV_DIRECT=1"); 
		}
	 } else {
		if ( ! getenv("SDL_VIDEO_YUV_HWACCEL") ) {
			putenv("SDL_VIDEO_YUV_HWACCEL=0");
		}
		if ( ! getenv("SDL_VIDEO_YUV_DIRECT") ) {
			putenv("SDL_VIDEO_YUV_DIRECT=0"); 
		}
	 }
	 
	if (SDL_VideoDriverName(driver, sizeof(driver))) {
		if (global->debug) printf("Video driver: %s\n", driver);
	}
	info = SDL_GetVideoInfo();

	if (info->wm_available) {
		if (global->debug) printf("A window manager is available\n");
	}
	if (info->hw_available) {
		if (global->debug) printf("Hardware surfaces are available (%dK video memory)\n",
		   info->video_mem);
		SDL_VIDEO_Flags |= SDL_HWSURFACE;
	}
	if (info->blit_hw) {
		if (global->debug) printf("Copy blits between hardware surfaces are accelerated\n");
		SDL_VIDEO_Flags |= SDL_ASYNCBLIT;
	}
    	if (global->debug) {
		if (info->blit_hw_CC) {
			printf ("Colorkey blits between hardware surfaces are accelerated\n");
		}
		if (info->blit_hw_A) {
			printf("Alpha blits between hardware surfaces are accelerated\n");
		}
		if (info->blit_sw) {
			printf ("Copy blits from software surfaces to hardware surfaces are accelerated\n");
		}
		if (info->blit_sw_CC) {
			printf ("Colorkey blits from software surfaces to hardware surfaces are accelerated\n");
		}
		if (info->blit_sw_A) {
			printf("Alpha blits from software surfaces to hardware surfaces are accelerated\n");
		}
		if (info->blit_fill) {
			printf("Color fills on hardware surfaces are accelerated\n");
		}
	}

	if (!(SDL_VIDEO_Flags & SDL_HWSURFACE)){
		SDL_VIDEO_Flags |= SDL_SWSURFACE;
	}
        
        SDL_WM_SetCaption(global->WVcaption, NULL); 
   
   	/* enable key repeat */
   	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,SDL_DEFAULT_REPEAT_INTERVAL);
	 
	/*------------------------------ SDL init video ---------------------*/
	pscreen =
	SDL_SetVideoMode(videoIn->width, videoIn->height, global->bpp,
			 SDL_VIDEO_Flags);
	switch (global->format) {
	    case V4L2_PIX_FMT_SGBRG8: /*converted to YUYV*/
	    case V4L2_PIX_FMT_YUV420: /*converted to YUYV*/
	    case V4L2_PIX_FMT_YUYV:
		overlay = SDL_CreateYUVOverlay(videoIn->width, videoIn->height,
				 SDL_YUY2_OVERLAY, pscreen);
		break;
	    case V4L2_PIX_FMT_UYVY:
		overlay = SDL_CreateYUVOverlay(videoIn->width, videoIn->height,
				 SDL_UYVY_OVERLAY, pscreen);
		break;
	    default:
		overlay = SDL_CreateYUVOverlay(videoIn->width, videoIn->height,
				 SDL_YUY2_OVERLAY, pscreen);
		break;
	}	
	p = (unsigned char *) overlay->pixels[0];
	
	drect.x = 0;
	drect.y = 0;
	drect.w = pscreen->w;
	drect.h = pscreen->h;
	
	while (videoIn->signalquit) {
	 /*-------------------------- Grab Frame ----------------------------------*/
	 if (uvcGrab(videoIn) < 0) {
		printf("Error grabbing image \n");
		videoIn->signalquit=0;
		snprintf(global->WVcaption,20,"GUVCVideo - CRASHED");
		SDL_WM_SetCaption(global->WVcaption, NULL);
		pthread_exit((void *) 2);
	 } else {
		/*reset video start time to first frame capture time */  
		if((global->framecount==0) && videoIn->capAVI) global->AVIstarttime = ms_time();
		
		if (global->FpsCount) {/* sets fps count in window title bar */
			global->frmCount++;
			if (global->DispFps>0) { /*set every 2 sec*/
				snprintf(global->WVcaption,24,"GUVCVideo - %3.2f fps",global->DispFps);
				SDL_WM_SetCaption(global->WVcaption, NULL);
				global->frmCount=0;/*resets*/
				global->DispFps=0;
			}				
		}
	
	     	/*---------------- autofocus control ------------------*/
		
		if (global->AFcontrol && (global->autofocus || AFdata->setFocus)) { /*AFdata = NULL if no focus control*/
			if (AFdata->focus < 0) {
			    /*starting autofocus*/
			    AFdata->focus = AFdata->left; /*start left*/
			    if (set_focus (videoIn, AFdata->focus) != 0) printf("ERROR: couldn't set focus to %d\n", AFdata->focus);
			    /*number of frames until focus is stable*/
			    AFdata->focus_wait = (int) abs(AFdata->focus-last_focus)*1.4/(1000/videoIn->fps)+1; /*1.4 ms focus time - every 1 step*/
			    last_focus = AFdata->focus;
			} else {
		    		if (AFdata->focus_wait == 0) {
		    			//AFdata->sharpness=getSharpMeasure (videoIn->framebuffer, videoIn->width, videoIn->height, 6);
					//printf("sharp 1: %i ", AFdata->sharpness);
					AFdata->sharpness=getSharpness (videoIn->framebuffer, videoIn->width, videoIn->height, 5);
					if (global->debug) printf("sharp=%d focus_sharp=%d foc=%d right=%d left=%d ind=%d flag=%d\n",AFdata->sharpness,AFdata->focus_sharpness,AFdata->focus, AFdata->right, AFdata->left, AFdata->ind, AFdata->flag);
		    			AFdata->focus=getFocusVal (AFdata);
					if ((AFdata->focus != last_focus)) {
			    			if (set_focus (videoIn, AFdata->focus) != 0) printf("ERROR: couldn't set focus to %d\n", AFdata->focus);
			    			/*number of frames until focus is stable*/
			    			AFdata->focus_wait = (int) abs(AFdata->focus-last_focus)*1.4/(1000/videoIn->fps)+1; /*1.4 ms focus time - every 1 step*/
					}
		    			last_focus = AFdata->focus;
				} else {
					AFdata->focus_wait--;
			    		if (global->debug) printf("Wait Frame: %d\n",AFdata->focus_wait);
				}
			}
		    
		}
	 }
	 /*------------------------- Filter Frame ---------------------------------*/
	 if(global->Frame_Flags>0){
		if((global->Frame_Flags & YUV_MIRROR)==YUV_MIRROR) {
			switch (global->format) {
			    case V4L2_PIX_FMT_SGBRG8: /*converted to YUYV*/
			    case V4L2_PIX_FMT_YUV420: /*converted to YUYV*/
			    case V4L2_PIX_FMT_YUYV: 
				yuyv_mirror(videoIn->framebuffer,videoIn->width,videoIn->height);
				break;
			    case V4L2_PIX_FMT_UYVY:
				uyvy_mirror(videoIn->framebuffer,videoIn->width,videoIn->height);
				break;
			    default:
				yuyv_mirror(videoIn->framebuffer,videoIn->width,videoIn->height);
				break;
			}
		}
		if((global->Frame_Flags & YUV_UPTURN)==YUV_UPTURN)
			yuyv_upturn(videoIn->framebuffer,videoIn->width,videoIn->height);
		if((global->Frame_Flags & YUV_NEGATE)==YUV_NEGATE)
			yuyv_negative (videoIn->framebuffer,videoIn->width,videoIn->height);
		if((global->Frame_Flags & YUV_MONOCR)==YUV_MONOCR) {
			switch (global->format) {
			    case V4L2_PIX_FMT_SGBRG8: /*converted to YUYV*/
			    case V4L2_PIX_FMT_YUV420: /*converted to YUYV*/
			    case V4L2_PIX_FMT_YUYV: 
				yuyv_monochrome (videoIn->framebuffer,videoIn->width,videoIn->height);
				break;
			    case V4L2_PIX_FMT_UYVY:
				uyvy_monochrome (videoIn->framebuffer,videoIn->width,videoIn->height);
				break;
			    default:
				yuyv_monochrome (videoIn->framebuffer,videoIn->width,videoIn->height);
				break;
			 }
		}
	 }
	
	 /*-------------------------capture Image----------------------------------*/
	 //char fbasename[20];
	 if (videoIn->capImage){
		 switch(global->imgFormat) {
		 case 0:/*jpg*/
			/* Save directly from MJPG frame */	 
			if((global->Frame_Flags==0) && (videoIn->formatIn==V4L2_PIX_FMT_MJPEG)) {
				if(SaveJPG(videoIn->ImageFName,videoIn->buf.bytesused,videoIn->tmpbuffer)) {
					fprintf (stderr,"Error: Couldn't capture Image to %s \n",
						videoIn->ImageFName);		
				}
				
			} else { /* use built in encoder */
				if (!global->jpeg){ 
					if((global->jpeg = (BYTE*)malloc(global->jpeg_bufsize))==NULL) {
						printf("couldn't allocate memory for: jpeg buffer\n");
						exit(1);
					}				
				}
				if(!jpeg_struct) {
					if((jpeg_struct =(struct JPEG_ENCODER_STRUCTURE *) calloc(1, sizeof(struct JPEG_ENCODER_STRUCTURE)))==NULL){
						printf("couldn't allocate memory for: jpeg encoder struct\n");
						exit(1); 
					} else {
						/* Initialization of JPEG control structure */
						initialization (jpeg_struct,videoIn->width,videoIn->height);
	
						/* Initialization of Quantization Tables  */
						initialize_quantization_tables (jpeg_struct);
					}
				} 
				
				int uyv=0;
				if (global->format == V4L2_PIX_FMT_UYVY) uyv=1;
				
				global->jpeg_size = encode_image(videoIn->framebuffer, global->jpeg, 
								jpeg_struct,1, videoIn->width, videoIn->height,uyv);
								
				if(SaveBuff(videoIn->ImageFName,global->jpeg_size,global->jpeg)) { 
					fprintf (stderr,"Error: Couldn't capture Image to %s \n",
					videoIn->ImageFName);		
				}
			}
			break;
		 case 1:/*bmp*/
			if(pim==NULL) {  
				 /*24 bits -> 3bytes     32 bits ->4 bytes*/
				if((pim= malloc((pscreen->w)*(pscreen->h)*3))==NULL){
					printf("Couldn't allocate memory for: pim\n");
					videoIn->signalquit=0;
					pthread_exit((void *) 3);		
				}
			}
			
			if (global->format == V4L2_PIX_FMT_UYVY) {
				uyvy2bgr(videoIn->framebuffer,pim,videoIn->width,videoIn->height);
			} else {
				yuyv2bgr(videoIn->framebuffer,pim,videoIn->width,videoIn->height);
			}
			
			if(SaveBPM(videoIn->ImageFName, videoIn->width, videoIn->height, 24, pim)) {
				  fprintf (stderr,"Error: Couldn't capture Image to %s \n",
				  videoIn->ImageFName);
			} 
			else {	  
				//printf ("Capture BMP Image to %s \n",videoIn->ImageFName);
			}
			break;
		 case 2:/*png*/
			if(pim==NULL) {  
				 /*24 bits -> 3bytes     32 bits ->4 bytes*/
				if((pim= malloc((pscreen->w)*(pscreen->h)*3))==NULL){
					printf("Couldn't allocate memory for: pim\n");
					videoIn->signalquit=0;
					pthread_exit((void *) 3);		
				}
			}
			
			if (global->format == V4L2_PIX_FMT_UYVY) {
				uyvy2rgb(videoIn->framebuffer,pim,videoIn->width,videoIn->height);
			} else {
				yuyv2rgb(videoIn->framebuffer,pim,videoIn->width,videoIn->height);
			}
			write_png(videoIn->ImageFName, videoIn->width, videoIn->height,pim);
		 }
		 videoIn->capImage=FALSE;
		 if (global->debug) printf("saved image to:%s\n",videoIn->ImageFName);
	  }
	  
	  /*---------------------------capture AVI---------------------------------*/
	  if (videoIn->capAVI){
	   long framesize;
	   int ret=0;
	   switch (global->AVIFormat) 
	   {
		   
		case 0: /*MJPG*/
			/* save MJPG frame */   
			if((global->Frame_Flags==0) && (videoIn->formatIn==V4L2_PIX_FMT_MJPEG)) 
			{
				//printf("avi write frame\n");
				ret = AVI_write_frame (AviOut, videoIn->tmpbuffer, videoIn->buf.bytesused, keyframe);
			} 
			else 
			{  /* use built in encoder */ 
				if (!global->jpeg){ 
					if((global->jpeg = (BYTE*)malloc(global->jpeg_bufsize))==NULL) {
						printf("couldn't allocate memory for: jpeg buffer\n");
						exit(1);
					}				
				}
				if(!jpeg_struct) {
					if((jpeg_struct =(struct JPEG_ENCODER_STRUCTURE *) calloc(1, sizeof(struct JPEG_ENCODER_STRUCTURE)))==NULL){
						printf("couldn't allocate memory for: jpeg encoder struct\n");
						exit(1); 
					} else {
						/* Initialization of JPEG control structure */
						initialization (jpeg_struct,videoIn->width,videoIn->height);
	
						/* Initialization of Quantization Tables  */
						initialize_quantization_tables (jpeg_struct);
					}
				} 
				
				int uyv=0;
				if (global->format == V4L2_PIX_FMT_UYVY) uyv=1;
				
				global->jpeg_size = encode_image(videoIn->framebuffer, global->jpeg, 
								jpeg_struct,1, videoIn->width, videoIn->height,uyv);
			
				ret = AVI_write_frame (AviOut, global->jpeg, global->jpeg_size, keyframe);
			}
			break;
		case 1:
			framesize=(pscreen->w)*(pscreen->h)*2; /*YUY2/UYVY -> 2 bytes per pixel */
			ret = AVI_write_frame (AviOut, p, framesize, keyframe);
			break;
		case 2:
			framesize=(pscreen->w)*(pscreen->h)*3; /*DIB 24/32 -> 3/4 bytes per pixel*/ 
			if(pavi==NULL){
				if((pavi= malloc(framesize))==NULL){
					printf("Couldn't allocate memory for: pim\n");
					videoIn->signalquit=0;
					pthread_exit((void *) 3);
				}
			}
			if (global->format == V4L2_PIX_FMT_UYVY) {
				uyvy2bgr(videoIn->framebuffer,pavi,videoIn->width,videoIn->height);
			} else {
				yuyv2bgr(videoIn->framebuffer,pavi,videoIn->width,videoIn->height);
			}
			ret = AVI_write_frame (AviOut,pavi, framesize, keyframe);
			break;
	   }
		
	   if (ret) 
	   {
		   	if (AVI_getErrno () == AVI_ERR_SIZELIM) {
				/*avi file limit reached - must end capture close file and start new one*/
				
				int rc = pthread_create(&pth_press_butt, &pth_press_attr, split_avi, all_data); 
				if (rc) {
				    printf("ERROR; return code from pthread_create(press_butt) is %d\n", rc);	       
				    split_avi(all_data); /*blocking call*/
				}   
				   
				printf("AVI file size limit reached - restarted capture on new file\n");
			} else {
				printf ("write error on avi out \n");
			}				
	   }
		   
		global->framecount++;
		if (keyframe) keyframe=0; /*resets key frame*/   
		/*----------------------- add audio -----------------------------*/
		if ((global->Sound_enable) && (pdata->audio_flag>0)) 
		{
		    /*first audio data - sync with video (audio stream capture can take a bit longer to start)*/
		    /* no need of locking the mutex yet, since we are not reading from the buffer*/
		    if (!(AviOut->track[0].audio_bytes)) 
		    { /*only 1 audio stream*/
	       		int synctime= pdata->snd_begintime - global->AVIstarttime; /*time diff for audio-video*/
			if (global->debug) printf("shift sound by %d ms\n",synctime);
			if(synctime>10 && synctime<5000) 
			{ /*only sync between 100ms and 5 seconds*/
			    if(global->Sound_Format == WAVE_FORMAT_PCM) 
			    {/*shift sound by synctime*/
			        UINT32 shiftFrames = abs(synctime * global->Sound_SampRate / 1000);
				UINT32 shiftSamples = shiftFrames * global->Sound_NumChan;
				if (global->debug) printf("shift sound forward by %d frames\n", shiftSamples);
				SAMPLE EmptySamp[shiftSamples];
				int i;
				for(i=0; i<shiftSamples; i++) EmptySamp[i]=0;/*init to zero - silence*/
				AVI_write_audio(AviOut,(BYTE *) &EmptySamp,shiftSamples*sizeof(SAMPLE));
			    } 
			    else if(global->Sound_Format == ISO_FORMAT_MPEG12) 
			    {
				    int size_mp2 = MP2_encode(pdata, synctime);
				    if (global->debug) printf("shift sound forward by %d bytes\n",size_mp2);
				    AVI_write_audio(AviOut,pdata->mp2Buff,size_mp2);
	       		    }
			}
		    }
		    /*write audio chunk*/
		    /*lock the mutex because we are reading from the audio buffer*/
		    if(global->Sound_Format == WAVE_FORMAT_PCM) 
		    {
			pthread_mutex_lock( &pdata->mutex );
				ret=AVI_write_audio(AviOut,(BYTE *) pdata->avi_sndBuff,pdata->snd_numBytes);
			pthread_mutex_unlock( &pdata->mutex );
		    }
		    else if(global->Sound_Format == ISO_FORMAT_MPEG12)
	            {
			pthread_mutex_lock( &pdata->mutex );
				int size_mp2 = MP2_encode(pdata,0);
				ret=AVI_write_audio(AviOut,pdata->mp2Buff,size_mp2);
			pthread_mutex_unlock( &pdata->mutex );
		    }
		    
		    pdata->audio_flag=0;
		    keyframe = 1; /*marks next frame as key frame*/
		    
		    if (ret) 
		    {	
	    		if (AVI_getErrno () == AVI_ERR_SIZELIM) 
			{
				/*avi file limit reached - must end capture close file and start new one*/
				/*from a thread - non blocking                                          */
				int rc = pthread_create(&pth_press_butt, &pth_press_attr, split_avi,all_data); 
				if (rc) {
				    printf("ERROR; return code from pthread_create(press_butt) is %d\n", rc);	       
				    split_avi(all_data); /*blocking call*/
				}   
				   
				printf("AVI file size limit reached - restarted capture on new file\n");
			} 
			else 
			{
				printf ("write error on avi out \n");
			}
		    }
		}   
	   
	
	   } /*video and audio capture have stopped but there is still audio available in the buffers*/
	   else if (pdata->audio_flag && !(pdata->streaming)) 
	   {
		/*write last audio data to avi*/
		/*even if max file size reached we still have an extra 20M available*/
		
		/*perform a mutex lock on the buffers to make sure they are not released*/
		/*while performing read operations- close_sound() may be running          */
		pthread_mutex_lock( &pdata->mutex);
		
		if(global->Sound_Format == WAVE_FORMAT_PCM)
		{
			if(pdata->avi_sndBuff) {
				AVI_write_audio(AviOut,(BYTE *) pdata->avi_sndBuff,pdata->snd_numBytes);
			}
		}
	        else if (global->Sound_Format == ISO_FORMAT_MPEG12)
		{
			int size_mp2=0;
			if(pdata->avi_sndBuff && pdata->mp2Buff) {
				size_mp2 = MP2_encode(pdata,0);
				AVI_write_audio(AviOut,pdata->mp2Buff,size_mp2);
				/*flush mp2 buffer*/	
				pdata->recording=0;
				size_mp2 = MP2_encode(pdata,0);
				AVI_write_audio(AviOut,pdata->mp2Buff,size_mp2);
			}
		}
		
		pthread_mutex_unlock( &pdata->mutex );
		/* can safely close sound now, no more data to record*/
		pdata->audio_flag=0;
		pdata->recording=0;
	   }
	   
	/*------------------------- Display Frame --------------------------------*/
	 SDL_LockYUVOverlay(overlay);
	 memcpy(p, videoIn->framebuffer,
		   videoIn->width * (videoIn->height) * 2);
	 SDL_UnlockYUVOverlay(overlay);
	 SDL_DisplayYUVOverlay(overlay, &drect);
	 
	/*sleep for a while*/
	if(global->vid_sleep)
		SDL_Delay(global->vid_sleep);
		
	/*------------------------- Read Key events ------------------------------*/
	if (videoIn->PanTilt) {
		/* Poll for events */
    		while( SDL_PollEvent(&event) ){
			if(event.type==SDL_KEYDOWN) {   
                		switch( event.key.keysym.sym ){
                    		/* Keyboard event */
                    		/* Pass the event data onto PrintKeyInfo() */
                    		case SDLK_DOWN:
					/*Tilt Down*/
					uvcPanTilt (videoIn,0,INCPANTILT*(global->TiltStep),0);
				break;
                    		case SDLK_UP:
                        		/*Tilt UP*/
					uvcPanTilt (videoIn,0,-INCPANTILT*(global->TiltStep),0);
                        		break;
		    		case SDLK_LEFT:
					/*Pan Left*/
					uvcPanTilt (videoIn,-INCPANTILT*(global->PanStep),0,0);
					break;
		    		case SDLK_RIGHT:
					/*Pan Right*/
					uvcPanTilt (videoIn,INCPANTILT*(global->PanStep),0,0);
					break;
                    		default:
                        		break;
                		}
			}

        	}
	}

	
  }/*loop end*/
  
  pthread_attr_destroy(&pth_press_attr);
	
  /*check if thread exited while AVI in capture mode*/
  if (videoIn->capAVI) {
	global->AVIstoptime = ms_time();
	videoIn->capAVI = FALSE;
	pdata->capAVI = videoIn->capAVI;   
  }	   
  if (global->debug) printf("Thread terminated...\n");
  
  p = NULL;
  if (jpeg_struct != NULL) free(jpeg_struct);
  jpeg_struct=NULL;
  if(pim!=NULL) free(pim);
  pim=NULL;
  if(pavi!=NULL) free(pavi);
  pavi=NULL;
  if (global->debug) printf("cleaning Thread allocations: 100%%\n");
  fflush(NULL);//flush all output buffers 

  SDL_FreeYUVOverlay(overlay);	
  SDL_Quit();   
	
  if (global->debug) printf("SDL Quit\n");
	
  global = NULL;
  AFdata = NULL;
  videoIn = NULL;
  AviOut = NULL;
  pthread_exit((void *) 0);
}
