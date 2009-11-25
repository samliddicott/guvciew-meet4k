/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
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

#include <portaudio.h>
#include <SDL/SDL.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#include "defs.h"
#include "video.h"
#include "guvcview.h"
#include "v4l2uvc.h"
#include "colorspaces.h"
#include "video_filters.h"
#include "jpgenc.h"
#include "autofocus.h"
#include "picture.h"
#include "ms_time.h"
#include "string_utils.h"
#include "callbacks.h"
#include "create_video.h"
#include "create_image.h"

static SDL_Overlay * video_init(void *data, SDL_Surface **pscreen)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	static Uint32 SDL_VIDEO_Flags =
		SDL_ANYFORMAT | SDL_DOUBLEBUF | SDL_RESIZABLE;
		
	if (*pscreen == NULL) //init SDL
	{
		const SDL_VideoInfo *info;
		char driver[128];
		/*----------------------------- Test SDL capabilities ---------------------*/
		if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) 
		{
			g_printerr("Couldn't initialize SDL: %s\n", SDL_GetError());
			exit(1);
		}
	
		/* For this version, we will use hardware acceleration as default*/
		if(global->hwaccel) 
		{
			if ( ! getenv("SDL_VIDEO_YUV_HWACCEL") ) putenv("SDL_VIDEO_YUV_HWACCEL=1");
			if ( ! getenv("SDL_VIDEO_YUV_DIRECT") ) putenv("SDL_VIDEO_YUV_DIRECT=1"); 
		}
		else 
		{
			if ( ! getenv("SDL_VIDEO_YUV_HWACCEL") ) putenv("SDL_VIDEO_YUV_HWACCEL=0");
			if ( ! getenv("SDL_VIDEO_YUV_DIRECT") ) putenv("SDL_VIDEO_YUV_DIRECT=0"); 
		}
	 
		if (SDL_VideoDriverName(driver, sizeof(driver)) && global->debug) 
		{
			g_printf("Video driver: %s\n", driver);
		}
	
		info = SDL_GetVideoInfo();

		if (info->wm_available && global->debug) g_printf("A window manager is available\n");

		if (info->hw_available) 
		{
			if (global->debug) 
				g_printf("Hardware surfaces are available (%dK video memory)\n", info->video_mem);

			SDL_VIDEO_Flags |= SDL_HWSURFACE;
		}
		if (info->blit_hw) 
		{
			if (global->debug) g_printf("Copy blits between hardware surfaces are accelerated\n");

			SDL_VIDEO_Flags |= SDL_ASYNCBLIT;
		}
	
		if (global->debug) 
		{
			if (info->blit_hw_CC) g_printf ("Colorkey blits between hardware surfaces are accelerated\n");
			if (info->blit_hw_A) g_printf("Alpha blits between hardware surfaces are accelerated\n");
			if (info->blit_sw) g_printf ("Copy blits from software surfaces to hardware surfaces are accelerated\n");
			if (info->blit_sw_CC) g_printf ("Colorkey blits from software surfaces to hardware surfaces are accelerated\n");
			if (info->blit_sw_A) g_printf("Alpha blits from software surfaces to hardware surfaces are accelerated\n");
			if (info->blit_fill) g_printf("Color fills on hardware surfaces are accelerated\n");
		}

		if (!(SDL_VIDEO_Flags & SDL_HWSURFACE))
		{
			SDL_VIDEO_Flags |= SDL_SWSURFACE;
		}

		SDL_WM_SetCaption(global->WVcaption, NULL); 

		/* enable key repeat */
		SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,SDL_DEFAULT_REPEAT_INTERVAL);
	}
	/*------------------------------ SDL init video ---------------------*/

	*pscreen = SDL_SetVideoMode( videoIn->width, 
		videoIn->height, 
		global->bpp,
		SDL_VIDEO_Flags);
		 
	SDL_Overlay* overlay=NULL;
	overlay = SDL_CreateYUVOverlay(videoIn->width, videoIn->height,
		SDL_YUY2_OVERLAY, *pscreen);
		
	return (overlay);
}

/*-------------------------------- Main Video Loop ---------------------------*/ 
/* run in a thread (SDL overlay)*/
void *main_loop(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
	
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct focusData *AFdata = all_data->AFdata;
	struct vdIn *videoIn = all_data->videoIn;

	struct particle* particles = NULL; //for the particles video effect
	
	SDL_Event event;
	/*the main SDL surface*/
	SDL_Surface *pscreen = NULL;
	SDL_Overlay *overlay = NULL;
	SDL_Rect drect;
	
	BYTE *p = NULL;
	
	int last_focus = 0;
	if (global->AFcontrol) 
	{
		last_focus = get_focus(videoIn->fd);
		/*make sure we wait for focus to settle on first check*/
		if (last_focus < 0) last_focus=255;
	}
	
	/*------------------------------ SDL init video ---------------------*/
	overlay = video_init(data, &(pscreen));
	p = (unsigned char *) overlay->pixels[0];
	
	drect.x = 0;
	drect.y = 0;
	drect.w = pscreen->w;
	drect.h = pscreen->h;
	
	gboolean capVid = FALSE;
	gboolean signalquit = FALSE;
	
	while (!signalquit) 
	{
		g_mutex_lock(videoIn->mutex);
			capVid = videoIn->capVid;
			signalquit = videoIn->signalquit;
		g_mutex_unlock(videoIn->mutex);
		
		/*-------------------------- Grab Frame ----------------------------------*/
		if (uvcGrab(videoIn) < 0) 
		{
			g_printerr("Error grabbing image \n");
			signalquit=TRUE;
			g_snprintf(global->WVcaption,20,"GUVCVideo - CRASHED");
			SDL_WM_SetCaption(global->WVcaption, NULL);
			g_thread_exit((void *) -2);
		} 
		else 
		{
			if(!videoIn->timestamp)
			{
				global->skip_n++; //skip this frame
			}
			/*reset video start time to first frame capture time */  
			if(capVid)
			{
		    		if(global->framecount < 1)
				{
					global->Vidstarttime = videoIn->timestamp;
				   	/*use current system time for audio ts(0) reference*/
					g_mutex_lock(pdata->mutex);
						pdata->ts_ref = ns_time();
				    	g_mutex_unlock(pdata->mutex);
				    	
					global->v_ts = 0;
				}
				else
				{
					global->v_ts = videoIn->timestamp - global->Vidstarttime;
					/*always use the last frame time stamp for video stop time*/
		    			global->Vidstoptime = videoIn->timestamp;
				}
			}
		    	//g_printf("v_ts = %llu \n",global->v_ts);

			if (global->FpsCount) 
			{/* sets fps count in window title bar */
				global->frmCount++;
				if (global->DispFps>0) 
				{ /*set every 2 sec*/
					g_snprintf(global->WVcaption,24,"GUVCVideo - %3.2f fps",global->DispFps);
					SDL_WM_SetCaption(global->WVcaption, NULL);
					global->frmCount=0;/*resets*/
					global->DispFps=0;
				}
			}
			
			/*---------------- autofocus control ------------------*/
		
			if (global->AFcontrol && (global->autofocus || AFdata->setFocus)) 
			{ /*AFdata = NULL if no focus control*/
				if (AFdata->focus < 0) 
				{
					/*starting autofocus*/
					AFdata->focus = AFdata->left; /*start left*/
					if (set_focus (videoIn->fd, AFdata->focus) != 0) 
						g_printerr("ERROR: couldn't set focus to %d\n", AFdata->focus);
					/*number of frames until focus is stable*/
					/*1.4 ms focus time - every 1 step*/
					AFdata->focus_wait = (int) abs(AFdata->focus-last_focus)*1.4/(1000/videoIn->fps)+1;
					last_focus = AFdata->focus;
				} 
				else 
				{
					if (AFdata->focus_wait == 0) 
					{
						AFdata->sharpness=getSharpness (videoIn->framebuffer, videoIn->width, 
							videoIn->height, 5);
						if (global->debug) 
							g_printf("sharp=%d focus_sharp=%d foc=%d right=%d left=%d ind=%d flag=%d\n",
								AFdata->sharpness,AFdata->focus_sharpness,
								AFdata->focus, AFdata->right, AFdata->left, 
								AFdata->ind, AFdata->flag);
						AFdata->focus=getFocusVal (AFdata);
						if ((AFdata->focus != last_focus)) 
						{
							if (set_focus (videoIn->fd, AFdata->focus) != 0) 
								g_printerr("ERROR: couldn't set focus to %d\n", 
									AFdata->focus);
							/*number of frames until focus is stable*/
							/*1.4 ms focus time - every 1 step*/
							AFdata->focus_wait = (int) abs(AFdata->focus-last_focus)*1.4/(1000/videoIn->fps)+1;
						}
						last_focus = AFdata->focus;
					} 
					else 
					{
						AFdata->focus_wait--;
						if (global->debug) g_printf("Wait Frame: %d\n",AFdata->focus_wait);
					}
				}
			}
		}
		/*------------------------- Filter Frame ---------------------------------*/
		g_mutex_lock(global->mutex);
		if(global->Frame_Flags>0)
		{
			if((global->Frame_Flags & YUV_PARTICLES)==YUV_PARTICLES)
				particles = particles_effect(videoIn->framebuffer, videoIn->width, videoIn->height, 20, 4, particles);
			
			if((global->Frame_Flags & YUV_MIRROR)==YUV_MIRROR) 
				yuyv_mirror(videoIn->framebuffer,videoIn->width,videoIn->height);
			
			if((global->Frame_Flags & YUV_UPTURN)==YUV_UPTURN)
				yuyv_upturn(videoIn->framebuffer,videoIn->width,videoIn->height);
				
			if((global->Frame_Flags & YUV_NEGATE)==YUV_NEGATE)
				yuyv_negative (videoIn->framebuffer,videoIn->width,videoIn->height);
				
			if((global->Frame_Flags & YUV_MONOCR)==YUV_MONOCR) 
				yuyv_monochrome (videoIn->framebuffer,videoIn->width,videoIn->height);
		   
			if((global->Frame_Flags & YUV_PIECES)==YUV_PIECES)
				pieces (videoIn->framebuffer, videoIn->width, videoIn->height, 16 );
			
		}
		g_mutex_unlock(global->mutex);
		/*-------------------------capture Image----------------------------------*/
		if (videoIn->capImage)
		{
			if(store_picture(all_data) < 0)
				g_printerr("saved image to:%s ...Failed \n",videoIn->ImageFName);
			else if (global->debug) g_printf("saved image to:%s ...OK \n",videoIn->ImageFName);
			
			videoIn->capImage=FALSE;
		}
		/*---------------------------capture Video---------------------------------*/
		if (capVid && !(global->skip_n))
		{
			g_mutex_lock(videoIn->mutex);
				if(videoIn->VidCapStop) videoIn->VidCapStop = FALSE;
			g_mutex_unlock(videoIn->mutex);
			int res=0;
			if((res=store_video_frame(all_data))<0) g_printerr("WARNING: droped frame (%i)\n",res);
			
		} /*video and audio capture have stopped */
		else
		{
			g_mutex_lock(videoIn->mutex);
				if(!(videoIn->VidCapStop)) videoIn->VidCapStop=TRUE;
			g_mutex_unlock(videoIn->mutex);
		}

		//decrease skip frame count
		if (global->skip_n > 0)
		{
			if (global->debug && capVid) g_printf("skiping frame %d...\n", global->skip_n);
			global->skip_n--;
		}

		g_mutex_lock( pdata->mutex );
			if (global->Sound_enable && capVid) pdata->skip_n = global->skip_n;
		g_mutex_unlock( pdata->mutex );
		
		/*------------------------- Display Frame --------------------------------*/
		SDL_LockYUVOverlay(overlay);
		memcpy(p, videoIn->framebuffer,
			videoIn->width * (videoIn->height) * 2);
		SDL_UnlockYUVOverlay(overlay);
		SDL_DisplayYUVOverlay(overlay, &drect);
		
		/*------------------------- Read Key events ------------------------------*/
		if (videoIn->PanTilt) 
		{
			/* Poll for events */
			while( SDL_PollEvent(&event) )
			{
				if(event.type==SDL_KEYDOWN) 
				{
					switch( event.key.keysym.sym )
					{
						/* Keyboard event */
						/* Pass the event data onto PrintKeyInfo() */
						case SDLK_DOWN:
							/*Tilt Down*/
							uvcPanTilt (videoIn->fd,0,INCPANTILT*(global->TiltStep),0);
							break;
							
						case SDLK_UP:
							/*Tilt UP*/
							uvcPanTilt (videoIn->fd,0,-INCPANTILT*(global->TiltStep),0);
							break;
							
						case SDLK_LEFT:
							/*Pan Left*/
							uvcPanTilt (videoIn->fd,-INCPANTILT*(global->PanStep),0,0);
							break;
							
						case SDLK_RIGHT:
							/*Pan Right*/
							uvcPanTilt (videoIn->fd,INCPANTILT*(global->PanStep),0,0);
							break;
						default:
							break;
					}
				}

			}
		}
		
		/* if set make the thread sleep - default no sleep (full throttle)*/
		if(global->vid_sleep) sleep_ms(global->vid_sleep);
		
		/*------------------------------------------*/
		/*  restart video (new resolution/format)   */
		/*------------------------------------------*/
		if (global->change_res)
		{
			g_printf("setting new resolution (%d x %d)\n",global->width,global->height);
			/*clean up */
			
			if(particles) g_free(particles);
			particles = NULL;
			
			if (global->debug) g_printf("cleaning buffer allocations\n");
			fflush(NULL);//flush all output buffers 
			
			SDL_FreeYUVOverlay(overlay);
			overlay = NULL;
			/*init device*/
			restart_v4l2(videoIn, global);
			
			/* restart SDL with new values*/
			overlay = video_init(data, &(pscreen));
			p = (unsigned char *) overlay->pixels[0];
	
			drect.x = 0;
			drect.y = 0;
			drect.w = pscreen->w;
			drect.h = pscreen->h;
			global->change_res = FALSE;
		}

	}/*loop end*/

	g_mutex_lock(videoIn->mutex);
		capVid = videoIn->capVid;
	g_mutex_unlock(videoIn->mutex);
	/*check if thread exited while in Video capture mode*/
	if (capVid) 
	{
		/*stop capture*/
		if (global->debug) g_printf("stoping Video capture\n");
		//global->Vidstoptime = ns_time(); /*this is set in IO thread*/
		videoIn->VidCapStop=TRUE;
		capVid = FALSE;
		g_mutex_lock(videoIn->mutex);
			videoIn->capVid = capVid;
		g_mutex_unlock(videoIn->mutex);
		g_mutex_lock(pdata->mutex);
			pdata->capVid = capVid;
		g_mutex_unlock(pdata->mutex);
		/*join IO thread*/
		if (global->debug) g_printf("Shuting Down IO Thread\n");
		g_thread_join( all_data->IO_thread );
		if (global->debug) g_printf("IO Thread finished\n");
	}
	
	if (global->debug) g_printf("Thread terminated...\n");
	p = NULL;
	if(particles) g_free(particles);
	particles=NULL;

	if (global->debug) g_printf("cleaning Thread allocations: 100%%\n");
	fflush(NULL);//flush all output buffers 
	
	SDL_FreeYUVOverlay(overlay);
	//SDL_FreeSurface(pscreen);

	SDL_Quit();   

	if (global->debug) g_printf("SDL Quit\n");

	global = NULL;
	AFdata = NULL;
	videoIn = NULL;
	return ((void *) 0);
}



