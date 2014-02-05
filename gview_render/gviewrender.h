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
 *   render - render API to use (SDL1, SDL2, ...)
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
