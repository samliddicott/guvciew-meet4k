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

#include "gviewv4l2core.h"
#include "gviewrender.h"
#include "video_capture.h"
#include "options.h"
#include "core_io.h"
#include "gui.h"

/*flags*/
extern int debug_level;

static int render = RENDER_NONE; /*render API*/
static int quit = 0; /*terminate flag*/
static int save_image = 0; /*save image flag*/


static int restart = 0; /*restart flag*/

static char render_caption[20]; /*render window caption*/

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
 * request format update
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void request_format_update()
{
	restart = 1;
}

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
int quit_callback(void *data)
{
	/*make sure we only call gui_close once*/
	if(!quit)
		gui_close();

	quit = 1;

	return 0;
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
 *    data - pointer to user data (device data + options data)
 *
 * asserts:
 *    device data is not null
 *
 * returns: pointer to return code
 */
void *capture_loop(void *data)
{
	capture_loop_data_t *cl_data = (capture_loop_data_t *) data;
	v4l2_dev_t *device = (v4l2_dev_t *) cl_data->device;
	options_t *my_options = (options_t *) cl_data->options;

	/*asserts*/
	assert(device != NULL);

	/*reset quit flag*/
	quit = 0;

	int format = fourcc_2_v4l2_pixelformat(my_options->format);

	/*prepare format*/
	prepare_new_format(device, format);
	/*prepare resolution*/
	prepare_new_resolution(device, my_options->width, my_options->height);
	/*try to set the video stream format on the device*/
	int ret = update_current_format(device);

	if(ret != E_OK)
	{
		fprintf(stderr, "GUCVIEW: could not set the defined stream format\n");
		fprintf(stderr, "GUCVIEW: trying first listed stream format\n");

		prepare_valid_format(device);
		prepare_valid_resolution(device);
		ret = update_current_format(device);

		if(ret != E_OK)
		{
			fprintf(stderr, "GUCVIEW: also could not set the first listed stream format\n");
			return ((void *) -1);
		}
	}

	/*yuyv frame has 2 bytes per pixel*/
	int yuv_frame_size = device->format.fmt.pix.width * device->format.fmt.pix.height << 1;

	if(render != RENDER_NONE)
	{
		set_render_verbosity(debug_level);
		if(render_init(RENDER_SDL1, device->format.fmt.pix.width, device->format.fmt.pix.height) < 0)
			render = RENDER_NONE;
		else
		{
			render_set_event_callback(EV_QUIT, &quit_callback, NULL);
		}
	}

	start_video_stream(device);

	while(!quit)
	{
		if(restart)
		{
			restart = 0; /*reset*/
			stop_video_stream(device);

			/*close render*/
			if(render)
				render_clean();

			clean_v4l2_buffers(device);

			/*try new format (values prepared by the request callback)*/
			ret = update_current_format(device);
			/*try to set the video stream format on the device*/
			if(ret != E_OK)
			{
				fprintf(stderr, "GUCVIEW: could not set the defined stream format\n");
				fprintf(stderr, "GUCVIEW: trying first listed stream format\n");

				prepare_valid_format(device);
				prepare_valid_resolution(device);
				ret = update_current_format(device);

				if(ret != E_OK)
				{
					fprintf(stderr, "GUCVIEW: also could not set the first listed stream format\n");
					return ((void *) -1);
				}
			}

			/*restart the render with new format*/
			if(render != RENDER_NONE)
			{
				if(render_init(RENDER_SDL1, device->format.fmt.pix.width, device->format.fmt.pix.height) < 0)
					render = RENDER_NONE;
				else
				{
					render_set_event_callback(EV_QUIT, &quit_callback, NULL);
				}
			}

			if(debug_level > 0)
				printf("GUVCVIEW: reset to pixelformat=%x width=%i and height=%i\n", device->requested_fmt, device->format.fmt.pix.width, device->format.fmt.pix.height);

			start_video_stream(device);

		}

		if( get_v4l2_frame(device) == E_OK)
		{
			/*decode the raw frame*/
			if(frame_decode(device) != E_OK)
			{
				fprintf(stderr, "GUVCIEW: Error - Couldn't decode image\n");
				//quit_callback(NULL);
				continue;
			}

			/*render the decoded frame*/
			if(render != RENDER_NONE)
			{
				snprintf(render_caption, 20, "SDL Video - %2.2f", get_v4l2_realfps());
				set_render_caption(render_caption);
				render_frame(device->yuv_frame, yuv_frame_size);
			}

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

	if(render != RENDER_NONE)
		render_clean();

	return ((void *) 0);
}
