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

#ifndef GVIEWRENDER_H
#define GVIEWRENDER_H

#include <inttypes.h>
#include <sys/types.h>

#define RENDER_NONE     (0)
#define RENDER_SDL1     (1)

#define EV_QUIT      (0)
#define EV_KEY_UP    (1)
#define EV_KEY_DOWN  (2)
#define EV_KEY_LEFT  (3)
#define EV_KEY_RIGHT (4)
#define EV_KEY_SPACE (5)


typedef int (*render_event_callback)(void *data);

typedef struct _render_events_t
{
	int id;
	render_event_callback callback;
	void *data;
	
} render_events_t;
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
void set_render_verbosity(int value);

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
int render_init(int render, int width, int height);

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
void set_render_caption(const char* caption);

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
int render_frame(uint8_t *frame, int size);

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
int render_get_event_index(int id);

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
int render_set_event_callback(int id, render_event_callback callback_function, void *data);

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
int render_call_event_callback(int id);

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
void render_clean();

#endif
