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
#include <string.h>
#include <signal.h>
#include <sys/stat.h>

#include "gview.h"
#include "gviewv4l2core.h"
#include "gviewrender.h"
#include "gviewencoder.h"
#include "core_time.h"

#include "../config.h"
#include "video_capture.h"
#include "options.h"
#include "config.h"
#include "gui.h"
#include "core_io.h"

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
			quit_callback(NULL);
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

	/*parse command line options*/
	if(options_parse(argc, argv))
		return 0;

	/*get command line options*/
	options_t *my_options = options_get();

	char *config_path = smart_cat(getenv("HOME"), '/', ".config/guvcview2");
	mkdir(config_path, 0777);

	char *device_name = get_file_basename(my_options->device);

	char *config_file = smart_cat(config_path, '/', device_name);

	/*clean strings*/
	if(config_path)
		free(config_path);
	if(device_name)
		free(device_name);

	/*load config data*/
	config_load(config_file);

	/*update config with options*/
	config_update(my_options);

	/*get config data*/
	config_t *my_config = config_get();

	debug_level = my_options->verbosity;
	v4l2core_set_verbosity(debug_level);

	v4l2_dev_t *device = v4l2core_init_dev(my_options->device);

	/*select capture method*/
	if(strcasecmp(my_config->capture, "read") == 0)
		v4l2core_set_capture_method(device, IO_READ);
	else
		v4l2core_set_capture_method(device, IO_MMAP);

	/*select render API*/
	int render = RENDER_SDL1;

	if(strcasecmp(my_config->render, "none") == 0)
		render = RENDER_NONE;
	else if(strcasecmp(my_config->render, "sdl1") == 0)
		render = RENDER_SDL1;

	/*select gui API*/
	int gui = GUI_GTK3;

	if(strcasecmp(my_config->gui, "none") == 0)
		gui = GUI_NONE;
	else if(strcasecmp(my_config->gui, "gtk3") == 0)
		gui = GUI_GTK3;

	/*select audio API*/
	int audio = AUDIO_PORTAUDIO;

	if(strcasecmp(my_config->audio, "none") == 0)
		audio = AUDIO_NONE;
	else if(strcasecmp(my_config->audio, "port") == 0)
		audio = AUDIO_PORTAUDIO;
#if HAS_PULSEAUDIO
	else if(strcasecmp(my_config->audio, "pulse") == 0)
		audio = AUDIO_PULSE;
#endif

	if(device)
		set_render_flag(render);
	else
	{
		options_clean();
		return -1;
	}

	/*select video codec*/
	if(debug_level > 1)
		printf("GUVCVIEW: setting video codec to '%s'\n", my_config->video_codec);
	int vcodec_ind = encoder_get_video_codec_ind_4cc(my_config->video_codec);
	if(vcodec_ind < 0)
	{
		fprintf(stderr, "GUVCVIEW: invalid video codec '%s' using raw input\n", my_config->video_codec);
		vcodec_ind = 0;
	}
	set_video_codec_ind(vcodec_ind);

	/*select audio codec*/
	if(debug_level > 1)
		printf("GUVCVIEW: setting audio codec to '%s'\n", my_config->audio_codec);
	int acodec_ind = encoder_get_audio_codec_ind_name(my_config->audio_codec);
	if(acodec_ind < 0)
	{
		fprintf(stderr, "GUVCVIEW: invalid audio codec '%s' using pcm input\n", my_config->audio_codec);
		acodec_ind = 0;
	}
	set_audio_codec_ind(acodec_ind);

	/*check if need to load a profile*/
	if(my_options->prof_filename)
		v4l2core_load_control_profile(device, my_options->prof_filename);

	/*create the inital audio context (stored staticly in video_capture)*/
	create_audio_context(audio);

	encoder_set_verbosity(debug_level);
	/*init the encoder*/
	encoder_init();

	/*start capture thread if not in control_panel mode*/
	if(!my_options->control_panel)
	{
		capture_loop_data_t cl_data;
		cl_data.options = (void *) my_options;
		cl_data.config = (void *) my_config;
		cl_data.device = (void *) device;

		int ret = __THREAD_CREATE(&capture_thread, capture_loop, (void *) &cl_data);

		if(ret)
			fprintf(stderr, "GUVCVIEW: Video thread creation failed\n");
	}

	gui_attach(device, gui, 800, 600, my_options->control_panel);

	/*run the gui loop*/
	gui_run();

	if(!my_options->control_panel)
		__THREAD_JOIN(capture_thread);

	/*closes the audio context (stored staticly in video_capture)*/
	close_audio_context();

	if(device)
		v4l2core_close_dev(device);

    /*save config before cleaning the options*/
	config_save(config_file);

	if(config_file)
		free(config_file);

	options_clean();

	if(debug_level > 0)
		printf("GUVCVIEW: good bye\n");

	return 0;
}
