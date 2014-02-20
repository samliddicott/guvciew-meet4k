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

static int render = RENDER_SDL1; /*render API*/
static int quit = 0; /*terminate flag*/
static int save_image = 0; /*save image flag*/


static int restart = 0; /*restart flag*/

static char render_caption[20]; /*render window caption*/

static uint32_t mask = REND_FX_YUV_NOFILT; /*render fx filter mask*/

/*continues focus*/
static int do_soft_autofocus = 0;
/*single time focus (can happen during continues focus)*/
static int do_soft_focus = 0;

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
 * set render flag
 * args:
 *    value - flag value
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void set_render_fx_mask(uint32_t new_mask)
{
	mask = new_mask;
}

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
void set_soft_autofocus(int value)
{
	do_soft_autofocus = value;
}

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
void set_soft_focus(int value)
{
	v4l2core_soft_autofocus_set_focus();

	do_soft_focus = value;
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

	int format = v4l2core_fourcc_2_v4l2_pixelformat(my_options->format);

	/*prepare format*/
	v4l2core_prepare_new_format(device, format);
	/*prepare resolution*/
	v4l2core_prepare_new_resolution(device, my_options->width, my_options->height);
	/*try to set the video stream format on the device*/
	int ret = v4l2core_update_current_format(device);

	if(ret != E_OK)
	{
		fprintf(stderr, "GUCVIEW: could not set the defined stream format\n");
		fprintf(stderr, "GUCVIEW: trying first listed stream format\n");

		v4l2core_prepare_valid_format(device);
		v4l2core_prepare_valid_resolution(device);
		ret = v4l2core_update_current_format(device);

		if(ret != E_OK)
		{
			fprintf(stderr, "GUCVIEW: also could not set the first listed stream format\n");
			return ((void *) -1);
		}
	}

	render_set_verbosity(debug_level);
	if(render_init(render, device->format.fmt.pix.width, device->format.fmt.pix.height) < 0)
			render = RENDER_NONE;
	else
	{
		render_set_event_callback(EV_QUIT, &quit_callback, NULL);
	}


	v4l2core_start_stream(device);

	while(!quit)
	{
		if(restart)
		{
			restart = 0; /*reset*/
			v4l2core_stop_stream(device);

			/*close render*/
			render_close();

			v4l2core_clean_buffers(device);

			/*try new format (values prepared by the request callback)*/
			ret = v4l2core_update_current_format(device);
			/*try to set the video stream format on the device*/
			if(ret != E_OK)
			{
				fprintf(stderr, "GUCVIEW: could not set the defined stream format\n");
				fprintf(stderr, "GUCVIEW: trying first listed stream format\n");

				v4l2core_prepare_valid_format(device);
				v4l2core_prepare_valid_resolution(device);
				ret = v4l2core_update_current_format(device);

				if(ret != E_OK)
				{
					fprintf(stderr, "GUCVIEW: also could not set the first listed stream format\n");
					return ((void *) -1);
				}
			}

			/*restart the render with new format*/
			if(render_init(render, device->format.fmt.pix.width, device->format.fmt.pix.height) < 0)
				render = RENDER_NONE;
			else
			{
				render_set_event_callback(EV_QUIT, &quit_callback, NULL);
			}


			if(debug_level > 0)
				printf("GUVCVIEW: reset to pixelformat=%x width=%i and height=%i\n", device->requested_fmt, device->format.fmt.pix.width, device->format.fmt.pix.height);

			v4l2core_start_stream(device);

		}

		if( v4l2core_get_frame(device) == E_OK)
		{
			/*decode the raw frame*/
			if(v4l2core_frame_decode(device) != E_OK)
			{
				fprintf(stderr, "GUVCIEW: Error - Couldn't decode frame\n");
				//quit_callback(NULL);
				//continue;
			}

			/*run software autofocus (must be called after frame_decode)*/
			if(do_soft_autofocus || do_soft_focus)
				do_soft_focus = v4l2core_soft_autofocus_run(device);

			/*render the decoded frame*/
			snprintf(render_caption, 20, "SDL Video - %2.2f", v4l2core_get_realfps());
			render_set_caption(render_caption);
			render_frame(device->yuv_frame, mask);

			if(save_image)
			{
				if(my_options->img_filename)
					free(my_options->img_filename);

				/*get_photo_[name|path] always return a non NULL value*/
				char *name = strdup(get_photo_name());
				char *path = strdup(get_photo_path());

				if(get_photo_sufix_flag())
				{
					char *new_name = add_file_suffix(path, name);
					free(name); /*free old name*/
					name = new_name; /*replace with suffixed name*/
				}
				int pathsize = strlen(path);
				if(path[pathsize] != '/')
					my_options->img_filename = smart_cat(path, '/', name);
				else
					my_options->img_filename = smart_cat(path, 0, name);

				if(debug_level > 1)
					printf("GUVCVIEW: saving image to %s\n", my_options->img_filename);

				v4l2core_save_image(device, my_options->img_filename, get_photo_format());

				free(path);
				free(name);

				save_image = 0; /*reset*/
			}

		}
	}

	v4l2core_stop_stream(device);

	render_close();

	return ((void *) 0);
}
