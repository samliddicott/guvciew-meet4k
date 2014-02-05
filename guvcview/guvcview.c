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

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "gviewv4l2core.h"
#include "gviewrender.h"
#include "core_time.h"

#include "video_capture.h"
#include "options.h"

int debug_level = 0;

static __THREAD_TYPE capture_thread;

/*
 * signal callback
 * args:
 *    signum - signal number
 *
 * return: none
 */
void signal_callback_handler(int signum)
{
	printf("GUVCVIEW Caught signal %d\n", signum);

	switch(signum)
	{
		case SIGINT:
			/* Terminate program */
			video_capture_quit();
			break;

		case SIGUSR1:
			/* (start/stop) record video */
			break;

		case SIGUSR2:
			/* save image */
			video_capture_save_image();
			break;
	}
}

int main(int argc, char *argv[])
{

	// Register signal and signal handler
	signal(SIGINT,  signal_callback_handler);
	signal(SIGUSR1, signal_callback_handler);
	signal(SIGUSR2, signal_callback_handler);

	if(options_parse(argc, argv))
		return 0;

	options_t *my_options = options_get();


	set_v4l2_verbosity(my_options->verbosity);

	v4l2_dev* device = init_v4l2_dev(my_options->device);

	if(device)
		set_render_flag(RENDER_SDL1);
	else
		return -1;

	int format = fourcc_2_v4l2_pixelformat(my_options->format);

	/*set format*/
	int format_index = get_frame_format_index(device, format);

	if(format_index < 0)
		format_index = 0;

	int pixelformat = device->list_stream_formats[format_index].format;

	/*get width and height*/
	int resolution_index = get_format_resolution_index(device, format_index, my_options.width, my_options.height);

	if(resolution_index < 0)
		resolution_index = 0;

	int width  = device->list_stream_formats[format_index].list_stream_cap[resolution_index].width;
	int height = device->list_stream_formats[format_index].list_stream_cap[resolution_index].height;

	int ret = try_video_stream_format(device, width, height, pixelformat);
	/*try to set the video stream format on the device*/
	if(ret != E_OK)
	{
		fprintf(stderr, "GUCVIEW: could not set the defined stream format\n");
		fprintf(stderr, "GUCVIEW: trying first listed stream format\n");
		device->current_format = 0;
		pixelformat = device->list_stream_formats[device->current_format].format;
		width  = device->list_stream_formats[device->current_format].list_stream_cap[0].width;
		height = device->list_stream_formats[device->current_format].list_stream_cap[0].height;

		ret = try_video_stream_format(device, width, height, pixelformat);
		if(ret != E_OK)
			fprintf(stderr, "GUCVIEW: also could not set the first listed stream format\n");

	}

	if(ret == E_OK)
		ret = __THREAD_CREATE(&capture_thread, capture_loop, (void *) device);

	if(ret)
		fprintf(stderr, "GUVCVIEW: Video thread creation failed\n");

	__THREAD_JOIN(capture_thread);

	if(device)
		close_v4l2_dev(device);

	return 0;
}
