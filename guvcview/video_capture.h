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
#ifndef VIDEO_CAPTURE_H
#define VIDEO_CAPTURE_H

#include <inttypes.h>
#include <sys/types.h>

typedef struct _capture_loop_data_t
{
	void *options;
	void *device;
} capture_loop_data_t;

/*
 * set render flag
 * args:
 *    value - flag value
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void set_render_flag(int value);

/*
 * set render fx mask
 * args:
 *    new_mask - new render fx filter mask
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void set_render_fx_mask(uint32_t new_mask);

/*
 * set software autofocus flag
 * args:
 *    value - flag value
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void set_soft_autofocus(int value);

/*
 * set software focus flag
 * args:
 *    value - flag value
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void set_soft_focus(int value);


/*
 * quit callback
 * args:
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: error code
 */
int quit_callback(void *data);

/*
 * sets the save image flag
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void video_capture_save_image();

/*
 * request format update
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void request_format_update();

/*
 * capture loop (should run in a separate thread)
 * args:
 *    data - pointer to user data
 *
 * asserts:
 *    device data is not null
 *
 * returns: pointer to return code
 */
void *capture_loop(void *data);

#endif
