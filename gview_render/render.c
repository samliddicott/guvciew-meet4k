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

/*******************************************************************************#
#                                                                               #
#  Render library                                                               #
#                                                                               #
********************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
/* support for internationalization - i18n */
#include <locale.h>
#include <libintl.h>

#include "gviewrender.h"
#include "render_sdl1.h"


int verbosity = 0;

static int render_api = RENDER_SDL1;

static render_events_t render_events_list[] =
{
	{
		.id = EV_QUIT,
		.callback = NULL,
		.data = NULL
		
	},
	{
		.id = EV_KEY_UP,
		.callback = NULL
	},
	{
		.id = EV_KEY_DOWN,
		.callback = NULL,
		.data = NULL
	},
	{
		.id = EV_KEY_LEFT,
		.callback = NULL,
		.data = NULL
	},
	{
		.id = EV_KEY_RIGHT,
		.callback = NULL,
		.data = NULL
	},
	{
		.id = EV_KEY_SPACE,
		.callback = NULL,
		.data = NULL
	},
	{
		.id = -1, /*end of list*/
		.callback = NULL,
		.data = NULL
	}
};

/*
 * set verbosity
 * args:
 *   value - verbosity value
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void set_render_verbosity(int value)
{
	verbosity = value;
}

/*
 * render initialization
 * args:
 *   render - render API to use (RENDER_NONE, RENDER_SDL1, ...)
 *   width - render width
 *   height - render height
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int render_init(int render, int width, int height)
{

	int ret = 0;

	render_api = render;

	switch(render_api)
	{
		case RENDER_NONE:
			break;

		case RENDER_SDL1:
		default:
			ret = init_render_sdl1(width, height);
			break;
	}

	return ret;
}

/*
 * render a frame
 * args:
 *   frame - pointer to frame data (yuyv format)
 *   size - frame size in bytes
 *
 * asserts:
 *   frame is not null
 *   size is valid
 *
 * returns: error code
 */
int render_frame(uint8_t *frame, int size)
{
	/*asserts*/
	assert(frame != NULL);

	int ret = 0;
	switch(render_api)
	{
		case RENDER_NONE:
			break;

		case RENDER_SDL1:
		default:
			ret = render_sdl1_frame(frame, size);
			render_sdl1_dispatch_events();
			break;
	}

	return ret;
}

/*
 * set caption
 * args:
 *   caption - string with render window caption
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void set_render_caption(const char* caption)
{
	switch(render_api)
	{
		case RENDER_NONE:
			break;

		case RENDER_SDL1:
		default:
			set_render_sdl1_caption(caption);
			break;
	}
}


/*
 * clean render data
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void render_clean()
{
	switch(render_api)
	{
		case RENDER_NONE:
			break;

		case RENDER_SDL1:
		default:
			render_sdl1_clean();
			break;
	}
}

/*
 * get event index on render_events_list
 * args:
 *    id - event id
 * 
 * asserts:
 *    none
 * 
 * returns: event index or -1 on error 
 */ 
int render_get_event_index(int id)
{
	int i = 0;
	while(render_events_list[i].id >= 0)
	{
		if(render_events_list[i].id == id)
			return i;
			
		i++;
	}
	return -1;
}

/*
 * set event callback
 * args:
 *    id - event id
 *    callback_function - pointer to callback function
 *    data - pointer to user data (passed to callback)
 * 
 * asserts:
 *    none
 * 
 * returns: error code
 */ 
int render_set_event_callback(int id, render_event_callback callback_function, void *data)
{
	int index = render_get_event_index(id);
	if(index < 0)
		return index;
	
	render_events_list[index].callback = callback_function;
	render_events_list[index].data = data;
		
	return 0;
}

/*
 * call the event callback for event id
 * args:
 *    id - event id
 * 
 * asserts:
 *    none
 * 
 * returns: error code 
 */ 
int render_call_event_callback(int id)
{
	int index = render_get_event_index(id);
	
	if(index < 0)
		return index;
	
	if(render_events_list[index].callback == NULL)
		return -1;
		
	int ret = render_events_list[index].callback(render_events_list[index].data);
	
	return ret;
}

