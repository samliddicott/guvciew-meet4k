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

#include <SDL/SDL.h>
#include <assert.h>

#include "defs.h"

extern int verbosity;

static int desktop_w = 0;
static int desktop_h = 0;

static SDL_Surface *pscreen = NULL;
static SDL_Overlay *poverlay = NULL;

static SDL_Rect drect;

static Uint32 SDL_VIDEO_Flags =
        SDL_ANYFORMAT | SDL_RESIZABLE;

static const SDL_VideoInfo *info;

/*
 * initialize sdl video
 * args:
 *   width - video width
 *   height - video height
 * 
 * asserts:
 *   none
 * 
 * returns: pointer to SDL_Overlay
 */ 
static SDL_Overlay * video_init(int width, int height)
{
	if(verbosity > 0)
		printf("RENDER: Initializing SDL1 render\n");
		
    if (pscreen == NULL) //init SDL
    {
        char driver[128];
        /*----------------------------- Test SDL capabilities ---------------------*/
        if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0)
        {
            fprintf(stderr, "RENDER: Couldn't initialize SDL: %s\n", SDL_GetError());
            return NULL;
        }

        /*use hardware acceleration as default*/
        if ( ! getenv("SDL_VIDEO_YUV_HWACCEL") ) putenv("SDL_VIDEO_YUV_HWACCEL=1");
        if ( ! getenv("SDL_VIDEO_YUV_DIRECT") ) putenv("SDL_VIDEO_YUV_DIRECT=1");
        

        if (SDL_VideoDriverName(driver, sizeof(driver)) && verbosity > 0)
        {
            printf("RENDER: Video driver: %s\n", driver);
        }

        info = SDL_GetVideoInfo();

        if (info->wm_available && verbosity > 0) printf("RENDER: A window manager is available\n");

        if (info->hw_available)
        {
            if (verbosity > 0)
                printf("RENDER: Hardware surfaces are available (%dK video memory)\n", info->video_mem);

            SDL_VIDEO_Flags |= SDL_HWSURFACE;
            SDL_VIDEO_Flags |= SDL_DOUBLEBUF;
        }
        else
        {
            SDL_VIDEO_Flags |= SDL_SWSURFACE;
        }

        if (info->blit_hw)
        {
            if (verbosity > 0) printf("RENDER: Copy blits between hardware surfaces are accelerated\n");

            SDL_VIDEO_Flags |= SDL_ASYNCBLIT;
        }
		
       desktop_w = info->current_w; //get desktop width
       desktop_h = info->current_h; //get desktop height
       
       if(!desktop_w) desktop_w = 800;
       if(!desktop_h) desktop_h = 600;

        if (verbosity > 0)
        {
            if (info->blit_hw_CC) printf("Colorkey blits between hardware surfaces are accelerated\n");
            if (info->blit_hw_A) printf("Alpha blits between hardware surfaces are accelerated\n");
            if (info->blit_sw) printf("Copy blits from software surfaces to hardware surfaces are accelerated\n");
            if (info->blit_sw_CC) printf("Colorkey blits from software surfaces to hardware surfaces are accelerated\n");
            if (info->blit_sw_A) printf("Alpha blits from software surfaces to hardware surfaces are accelerated\n");
            if (info->blit_fill) printf("Color fills on hardware surfaces are accelerated\n");
        }

        SDL_WM_SetCaption("Guvcview Video", NULL);

        /* enable key repeat */
        SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,SDL_DEFAULT_REPEAT_INTERVAL);
    }
    
    /*------------------------------ SDL init video ---------------------*/
    if(verbosity > 0)
    {
        printf("RENDER: Desktop resolution = %ix%i\n", desktop_w, desktop_h);
        printf("RENDER: Checking video mode %ix%i@32bpp : ", width, height);
    }
       
    int bpp = SDL_VideoModeOK(
        width,
        height,
        32,
        SDL_VIDEO_Flags);

    if(!bpp)
    {
        fprintf(stderr, "RENDER: resolution not available\n");
        /*resize video mode*/
        if ((width > desktop_w) || (height > desktop_h))
        {
            width = desktop_w; /*use desktop video resolution*/
            height = desktop_h;
        }
        else
        {
            width = 800;
            height = 600;
        }
        fprintf(stderr, "RENDER: resizing to %ix%i\n", width, height);

    }
    else
    {
        if ((bpp != 32) && verbosity > 0) printf("RENDER: recomended color depth = %i\n", bpp);
    }

    pscreen = SDL_SetVideoMode(
        width,
        height,
        bpp,
        SDL_VIDEO_Flags);

    if(pscreen == NULL)
    {
        return (NULL);
    }
    /*use requested resolution for overlay even if not available as video mode*/
    SDL_Overlay* overlay=NULL;
    overlay = SDL_CreateYUVOverlay(width, height,
        SDL_YUY2_OVERLAY, pscreen);

    SDL_ShowCursor(SDL_DISABLE);
    return (overlay);
}

/*
 * init sdl1 render
 * args:
 * 
 * asserts:
 * 
 * returns: error code (0 ok)
 */ 
 int init_render_sdl1(int width, int height)
 {
	poverlay = video_init(width, height);
	 
	if(poverlay == NULL)
	{
		fprintf(stderr, "RENDER: Couldn't create yuv overlay (try disabling hardware accelaration)\n");
		return -1;
	}

	assert(pscreen != NULL);
	
	drect.x = 0;
	drect.y = 0;
	drect.w = pscreen->w;
	drect.h = pscreen->h;
	
	return 0;
 }

/*
 * render a frame
 * args:
 *   frame - pointer to frame data (yuyv format)
 *   size - frame size in bytes
 * 
 * asserts:
 *   poverlay is not nul
 *   frame is not null
 * 
 * returns: error code 
 */  
int render_sdl1_frame(uint8_t *frame, int size)
{
	/*asserts*/
	assert(poverlay != NULL);
	assert(frame != NULL);
	
	uint8_t *p = (uint8_t *) poverlay->pixels[0];
	
	 SDL_LockYUVOverlay(poverlay);
     memcpy(p, frame, size);
     SDL_UnlockYUVOverlay(poverlay);
     SDL_DisplayYUVOverlay(poverlay, &drect);
}

/*
 * clean sdl1 render data
 * args:
 *   none
 * 
 * asserts:
 *   none
 * 
 * returns: none 
 */
void render_sdl1_clean()
{
	if(poverlay)
		SDL_FreeYUVOverlay(poverlay);
	poverlay = NULL;
	
	SDL_Quit();
}
 
