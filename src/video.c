/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
#           Flemming Frandsen <dren.dk@gmail.com>                               #
#                             Add VU meter OSD                                  #
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
#include "timers.h"
#include "osd.h"

#define __AMUTEX &pdata->mutex
#define __VMUTEX &videoIn->mutex
#define __GMUTEX &global->mutex

static Uint32 SDL_VIDEO_Flags =
        SDL_ANYFORMAT | SDL_RESIZABLE;

static const SDL_VideoInfo *info;

static SDL_Overlay * video_init(void *data, SDL_Surface **pscreen)
{
    struct ALL_DATA *all_data = (struct ALL_DATA *) data;
    struct GLOBAL *global = all_data->global;

    int width = global->width;
    int height = global->height;

    if (*pscreen == NULL) //init SDL
    {
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
            g_print("Video driver: %s\n", driver);
        }

        info = SDL_GetVideoInfo();

        if (info->wm_available && global->debug) g_print("A window manager is available\n");

        if (info->hw_available)
        {
            if (global->debug)
                g_print("Hardware surfaces are available (%dK video memory)\n", info->video_mem);

            SDL_VIDEO_Flags |= SDL_HWSURFACE;
            SDL_VIDEO_Flags |= SDL_DOUBLEBUF;
        }
        else
        {
            SDL_VIDEO_Flags |= SDL_SWSURFACE;
        }

        if (info->blit_hw)
        {
            if (global->debug) g_print("Copy blits between hardware surfaces are accelerated\n");

            SDL_VIDEO_Flags |= SDL_ASYNCBLIT;
        }

        if(!global->desktop_w) global->desktop_w = info->current_w; //get desktop width
        if(!global->desktop_h) global->desktop_h = info->current_h; //get desktop height

        if (global->debug)
        {
            if (info->blit_hw_CC) g_print ("Colorkey blits between hardware surfaces are accelerated\n");
            if (info->blit_hw_A) g_print("Alpha blits between hardware surfaces are accelerated\n");
            if (info->blit_sw) g_print ("Copy blits from software surfaces to hardware surfaces are accelerated\n");
            if (info->blit_sw_CC) g_print ("Colorkey blits from software surfaces to hardware surfaces are accelerated\n");
            if (info->blit_sw_A) g_print("Alpha blits from software surfaces to hardware surfaces are accelerated\n");
            if (info->blit_fill) g_print("Color fills on hardware surfaces are accelerated\n");
        }

        SDL_WM_SetCaption(global->WVcaption, NULL);

        /* enable key repeat */
        SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,SDL_DEFAULT_REPEAT_INTERVAL);
    }
    /*------------------------------ SDL init video ---------------------*/
    if(global->debug)
        g_print("(Desktop resolution = %ix%i)\n", global->desktop_w, global->desktop_h);
    g_print("Checking video mode %ix%i@32bpp : ", width, height);
    int bpp = SDL_VideoModeOK(
        width,
        height,
        32,
        SDL_VIDEO_Flags);

    if(!bpp)
    {
        g_print("Not available \n");
        /*resize video mode*/
        if ((width > global->desktop_w) || (height > global->desktop_h))
        {
            width = global->desktop_w; /*use desktop video resolution*/
            height = global->desktop_h;
        }
        else
        {
            width = 800;
            height = 600;
        }
        g_print("Resizing to %ix%i\n", width, height);

    }
    else
    {
        g_print("OK \n");
        if ((bpp != 32) && global->debug) g_print("recomended color depth = %i\n", bpp);
        global->bpp = bpp;
    }

    *pscreen = SDL_SetVideoMode(
        width,
        height,
        global->bpp,
        SDL_VIDEO_Flags);

    if(*pscreen == NULL)
    {
        return (NULL);
    }
    //use requested resolution for overlay even if not available as video mode
    SDL_Overlay* overlay=NULL;
    overlay = SDL_CreateYUVOverlay(global->width, global->height,
        SDL_YUY2_OVERLAY, *pscreen);

    SDL_ShowCursor(SDL_DISABLE);
    return (overlay);
}

/*-------------------------------- Main Video Loop ---------------------------*/
/* run in a thread (SDL overlay)*/
void *main_loop(void *data)
{
    struct ALL_DATA *all_data = (struct ALL_DATA *) data;

    struct VidState *s = all_data->s;
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

    int width = global->width;
    int height = global->height;
    int format = global->format;

    SAMPLE vuPeak[2];  // The maximum vuLevel seen recently
    int vuPeakFreeze[2]; // The vuPeak values will be frozen for this many frames.
    vuPeak[0] = vuPeak[1] = 0;
    vuPeakFreeze[0] = vuPeakFreeze[1] = 0;

    BYTE *p = NULL;

    Control *focus_control = NULL;
    int last_focus = 0;

    if (global->AFcontrol)
    {
        focus_control = get_ctrl_by_id(s->control_list, AFdata->id);
        get_ctrl(videoIn->fd, s->control_list, AFdata->id, all_data);
        last_focus = focus_control->value;
        /*make sure we wait for focus to settle on first check*/
        if (last_focus < 0) last_focus = AFdata->f_max;
    }

    gboolean capVid = FALSE;
    gboolean signalquit = FALSE;

    /*------------------------------ SDL init video ---------------------*/
    if(!global->no_display)
    {
        overlay = video_init(data, &(pscreen));

        if(overlay == NULL)
        {
            g_print("FATAL: Couldn't create yuv overlay - please disable hardware accelaration\n");
            signalquit = TRUE; /*exit video thread*/
        }
        else
        {
            p = (unsigned char *) overlay->pixels[0];

            drect.x = 0;
            drect.y = 0;
            drect.w = pscreen->w;
            drect.h = pscreen->h;
        }
    }

    while (!signalquit)
    {
        __LOCK_MUTEX(__VMUTEX);
            capVid = videoIn->capVid;
            signalquit = videoIn->signalquit;
        __UNLOCK_MUTEX(__VMUTEX);

        /*-------------------------- Grab Frame ----------------------------------*/
        if (uvcGrab(videoIn, global, format, width, height) < 0)
        {
            g_printerr("Error grabbing image \n");
            continue;
        }
        else
        {
            if(!videoIn->timestamp)
            {
                global->skip_n++; //skip this frame
            }

            if(capVid)
            {
                if(global->framecount < 1)
                {
					/*reset video start time to first frame capture time */
					global->Vidstarttime = videoIn->timestamp;
					/** set current time for audio ts(0) reference (MONOTONIC)
					 *  only used if we have no audio capture before video
					 */
					__LOCK_MUTEX(__AMUTEX);
						pdata->ts_ref = ns_time_monotonic();
					__UNLOCK_MUTEX(__AMUTEX);
					//printf("video ts ref: %llu audio ts_ ref: %llu\n",global->Vidstarttime, pdata->ts_ref);
					global->v_ts = 0;
                }
                else
                {
                    global->v_ts = videoIn->timestamp - global->Vidstarttime;
                    /*always use the last frame time stamp for video stop time*/
                    global->Vidstoptime = videoIn->timestamp;
                }
            }

            if (global->FpsCount && !global->no_display)
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
                    focus_control->value = AFdata->focus;
                    if (set_ctrl (videoIn->fd, s->control_list, AFdata->id) != 0)
                        g_printerr("ERROR: couldn't set focus to %d\n", AFdata->focus);
                    /*number of frames until focus is stable*/
                    /*1.4 ms focus time - every 1 step*/
                    AFdata->focus_wait = (int) abs(AFdata->focus-last_focus)*1.4/(1000/global->fps)+1;
                    last_focus = AFdata->focus;
                }
                else
                {
                    if (AFdata->focus_wait == 0)
                    {
                        AFdata->sharpness=getSharpness (videoIn->framebuffer, width, height, 5);
                        if (global->debug)
                            g_print("sharp=%d focus_sharp=%d foc=%d right=%d left=%d ind=%d flag=%d\n",
                                AFdata->sharpness,AFdata->focus_sharpness,
                                AFdata->focus, AFdata->right, AFdata->left,
                                AFdata->ind, AFdata->flag);
                        AFdata->focus=getFocusVal (AFdata);
                        if ((AFdata->focus != last_focus))
                        {
                            focus_control->value = AFdata->focus;
                            if (set_ctrl (videoIn->fd, s->control_list, AFdata->id) != 0)
                                g_printerr("ERROR: couldn't set focus to %d\n",
                                    AFdata->focus);
                            /*number of frames until focus is stable*/
                            /*1.4 ms focus time - every 1 step*/
                            AFdata->focus_wait = (int) abs(AFdata->focus-last_focus)*1.4/(1000/global->fps)+1;
                        }
                        last_focus = AFdata->focus;
                    }
                    else
                    {
                        AFdata->focus_wait--;
                        if (global->debug) g_print("Wait Frame: %d\n",AFdata->focus_wait);
                    }
                }
            }
        }
        /*------------------------- Filter Frame ---------------------------------*/
        __LOCK_MUTEX(__GMUTEX);
        if(global->Frame_Flags>0)
        {
            if((global->Frame_Flags & YUV_PARTICLES)==YUV_PARTICLES)
                particles = particles_effect(videoIn->framebuffer, width, height, 20, 4, particles);

            if((global->Frame_Flags & YUV_MIRROR)==YUV_MIRROR)
                yuyv_mirror(videoIn->framebuffer, width, height);

            if((global->Frame_Flags & YUV_UPTURN)==YUV_UPTURN)
                yuyv_upturn(videoIn->framebuffer, width, height);

            if((global->Frame_Flags & YUV_NEGATE)==YUV_NEGATE)
                yuyv_negative (videoIn->framebuffer, width, height);

            if((global->Frame_Flags & YUV_MONOCR)==YUV_MONOCR)
                yuyv_monochrome (videoIn->framebuffer, width, height);

            if((global->Frame_Flags & YUV_PIECES)==YUV_PIECES)
                pieces (videoIn->framebuffer, width, height, 16 );

        }
        __UNLOCK_MUTEX(__GMUTEX);
        /*-------------------------capture Image----------------------------------*/
        if (videoIn->capImage)
        {
            /*
             * format and resolution can change(enabled) while capturing the frame
             * but you would need to be speedy gonzalez to press two buttons
             * at almost the same time :D
             */
            int ret = 0;
            if((ret=store_picture(all_data)) < 0)
                g_printerr("saved image to:%s ...Failed \n",videoIn->ImageFName);
            else if (!ret && global->debug) g_print("saved image to:%s ...OK \n",videoIn->ImageFName);

            videoIn->capImage=FALSE;
        }
        /*---------------------------capture Video---------------------------------*/
        if (capVid && !(global->skip_n))
        {
            __LOCK_MUTEX(__VMUTEX);
                if(videoIn->VidCapStop) videoIn->VidCapStop = FALSE;
            __UNLOCK_MUTEX(__VMUTEX);
            int res=0;

			/* format and resolution don't change(disabled) while capturing video
			 * store_video_frame may sleep if needed to avoid buffer overrun
			 */
            if((res=store_video_frame(all_data))<0) g_printerr("WARNING: droped frame (%i)\n",res);

        } /*video and audio capture have stopped */
        else
        {
            __LOCK_MUTEX(__VMUTEX);
                if(!(videoIn->VidCapStop)) videoIn->VidCapStop=TRUE;
            __UNLOCK_MUTEX(__VMUTEX);
        }

        /* decrease skip frame count */
        if (global->skip_n > 0)
        {
            if (global->debug && capVid) g_print("skiping frame %d...\n", global->skip_n);
            global->skip_n--;
        }

        __LOCK_MUTEX( __AMUTEX );
            if (global->Sound_enable && capVid) pdata->skip_n = global->skip_n;
        __UNLOCK_MUTEX( __AMUTEX );

        /*------------------------- Display Frame --------------------------------*/
        if(!global->no_display)
        {
			if (global->osdFlags && pdata->audio_buff[0])
			{
				draw_vu_meter(width, height, vuPeak, vuPeakFreeze, data);
			}
            SDL_LockYUVOverlay(overlay);
            memcpy(p, videoIn->framebuffer, width * height * 2);
            SDL_UnlockYUVOverlay(overlay);
            SDL_DisplayYUVOverlay(overlay, &drect);

            /*------------------------- Read Key events ------------------------------*/
            /* Poll for events */
            while( SDL_PollEvent(&event) )
            {
                //printf("event type:%i  event key:%i\n", event.type, event.key.keysym.scancode);
                if(event.type==SDL_KEYDOWN)
                {
                    if (videoIn->PanTilt)
                    {
                        switch( event.key.keysym.sym )
                        {
                            /* Keyboard event */
                            /* Pass the event data onto PrintKeyInfo() */
                            case SDLK_DOWN:
                                /*Tilt Down*/
                                uvcPanTilt (videoIn->fd, s->control_list, 0, 1);
                                break;

                            case SDLK_UP:
                                /*Tilt UP*/
                                uvcPanTilt (videoIn->fd, s->control_list, 0, -1);
                                break;

                            case SDLK_LEFT:
                                /*Pan Left*/
                                uvcPanTilt (videoIn->fd, s->control_list, 1, 1);
                                break;

                            case SDLK_RIGHT:
                                /*Pan Right*/
                                uvcPanTilt (videoIn->fd, s->control_list, 1, -1);
                                break;
                            default:
                                break;
                        }
                    }
                    switch( event.key.keysym.scancode )
                    {
                        case 220: /*webcam button*/
                            //gdk_threads_enter();
                           	if (all_data->global->default_action == 0)
                           		g_main_context_invoke(NULL, image_capture_callback, (gpointer) all_data);
							else
                            	g_main_context_invoke(NULL, video_capture_callback, (gpointer) all_data);

                            break;
                    }
                    switch( event.key.keysym.sym )
                    {
                        case SDLK_q:
                            //shutDown
                            g_timeout_add(200, shutd_timer, all_data);
                            g_print("q pressed - Quiting...\n");
                            break;
                        case SDLK_SPACE:
							{
                            if(global->AFcontrol > 0)
                                setfocus_clicked(NULL, all_data);
							}
                            break;
                        case SDLK_i:
							g_main_context_invoke(NULL, image_capture_callback, (gpointer) all_data);
							break;
						case SDLK_v:
							g_main_context_invoke(NULL, video_capture_callback, (gpointer) all_data);
							break;
                        default:
                            break;
                    }
                }
                if(event.type==SDL_VIDEORESIZE)
                {
                    pscreen =
                        SDL_SetVideoMode(event.resize.w,
                                 event.resize.h,
                                 global->bpp,
                                 SDL_VIDEO_Flags);
                    drect.w = event.resize.w;
                    drect.h = event.resize.h;
                }
                if(event.type==SDL_QUIT)
                {
                    //shutDown
                    g_timeout_add(200, shutd_timer, all_data);
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
            g_print("setting new resolution (%d x %d)\n", global->width, global->height);
            /*clean up */

            if(particles) g_free(particles);
            particles = NULL;

            if (global->debug) g_print("cleaning buffer allocations\n");
            fflush(NULL);//flush all output buffers

            if(!global->no_display)
            {
                SDL_FreeYUVOverlay(overlay);
                overlay = NULL;
            }
            /*init device*/
            restart_v4l2(videoIn, global);
            /*set new resolution for video thread*/
            width = global->width;
            height = global->height;
            format = global->format;
            /* restart SDL with new values*/
            if(!global->no_display)
            {
                overlay = video_init(data, &(pscreen));
                if(overlay == NULL)
                {
                    g_print("FATAL: Couldn't create yuv overlay - please disable hardware accelaration\n");
                    signalquit = TRUE; /*exit video thread*/
                }
                else
                {
                    if (global->debug) g_print("yuv overlay created (%ix%i).\n", overlay->w, overlay->h);
                    p = (unsigned char *) overlay->pixels[0];

                    drect.x = 0;
                    drect.y = 0;
                    drect.w = pscreen->w;
                    drect.h = pscreen->h;

                    global->change_res = FALSE;
                }
            }
            else global->change_res = FALSE;
        }

    }/*loop end*/

    __LOCK_MUTEX(__VMUTEX);
        capVid = videoIn->capVid;
    __UNLOCK_MUTEX(__VMUTEX);
    /*check if thread exited while in Video capture mode*/
    if (capVid)
    {
        /*stop capture*/
        if (global->debug) g_print("stoping Video capture\n");
        //global->Vidstoptime = ns_time_monotonic(); /*this is set in IO thread*/
        videoIn->VidCapStop=TRUE;
        capVid = FALSE;
        __LOCK_MUTEX(__VMUTEX);
            videoIn->capVid = capVid;
        __UNLOCK_MUTEX(__VMUTEX);
        __LOCK_MUTEX(__AMUTEX);
            pdata->capVid = capVid;
        __UNLOCK_MUTEX(__AMUTEX);
        /*join IO thread*/
        if (global->debug) g_print("Shuting Down IO Thread\n");
        __THREAD_JOIN( all_data->IO_thread );
        if (global->debug) g_print("IO Thread finished\n");
    }

    if (global->debug) g_print("Thread terminated...\n");
    p = NULL;
    if(particles) g_free(particles);
    particles=NULL;

    if (global->debug) g_print("cleaning Thread allocations: 100%%\n");
    fflush(NULL);//flush all output buffers

    if(!global->no_display)
    {
        if(overlay)
            SDL_FreeYUVOverlay(overlay);
        //SDL_FreeSurface(pscreen);

        SDL_Quit();
    }

    if (global->debug) g_print("Video thread completed\n");

    global = NULL;
    AFdata = NULL;
    videoIn = NULL;
    return ((void *) 0);
}



