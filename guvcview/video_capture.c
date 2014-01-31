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

/*******************************************************************************#
#                                                                               #
#  V4L2 core library                                                            #
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

#include "v4l2_core.h"
#include "core_io.h"

/*flags*/
extern int verbosity;

static int render = 0; /*flag if we should render frames*/
static int quit = 0; /*terminate flag*/
static int save_image = 0; /*save image flag*/

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
void set_render_flag(int value)
{
	render = value;
}

/*
 * terminate capture loop
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void video_capture_quit()
{
	quit = 1;
}

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
void video_capture_save_image()
{
	save_image = 1;
}

/*
 * capture loop (should run in a separate thread)
 * args:
 *    data - pointer to user data (device data)
 *
 * asserts:
 *    device data is not null
 *
 * returns: pointer to return code
 */
void *capture_loop(void *data)
{
	v4l2_dev* device = (v4l2_dev *) data;
	/*asserts*/
	assert(device != NULL);

	start_video_stream(device);

	while(!quit)
	{
		if( get_v4l2_frame(device) == E_OK)
		{
			if(save_image)
			{
				/*debug*/
				char test_filename[20];
				snprintf(test_filename, 20, "rawframe-%u.raw", (uint) device->frame_index);

				save_data_to_file(test_filename, device->raw_frame, device->raw_frame_size);

				save_image = 0; /*reset*/
			}
		}
	}

	stop_video_stream(device);

	return ((void *) 0);
}