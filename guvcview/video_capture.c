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
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
/* support for internationalization - i18n */
#include <locale.h>
#include <libintl.h>

#include "gviewv4l2core.h"
#include "gviewrender.h"
#include "gviewencoder.h"
#include "gview.h"
#include "video_capture.h"
#include "options.h"
#include "core_io.h"
#include "gui.h"

/*flags*/
extern int debug_level;

static int render = RENDER_SDL1; /*render API*/
static int quit = 0; /*terminate flag*/
static int save_image = 0; /*save image flag*/
static int save_video = 0; /*save video flag*/

static uint64_t my_photo_timer = 0; /*timer count*/

static int restart = 0; /*restart flag*/

static char render_caption[20]; /*render window caption*/

static uint32_t my_render_mask = REND_FX_YUV_NOFILT; /*render fx filter mask*/

static uint32_t my_audio_mask = AUDIO_FX_NONE; /*audio fx filter mask*/

/*continues focus*/
static int do_soft_autofocus = 0;
/*single time focus (can happen during continues focus)*/
static int do_soft_focus = 0;

/*pointer to audio context data*/
static audio_context_t *my_audio_ctx = NULL;

static __THREAD_TYPE encoder_thread;

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
 * get render fx mask
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: render fx mask
 */
uint32_t get_render_fx_mask()
{
	return my_render_mask;
}

/*
 * set render fx mask
 * args:
 *    new_mask - fx mask value
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void set_render_fx_mask(uint32_t new_mask)
{
	my_render_mask = new_mask;
}

/*
 * get audio fx mask
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: audio fx mask
 */
uint32_t get_audio_fx_mask()
{
	return my_audio_mask;
}

/*
 * set audio fx mask
 * args:
 *    new_mask - new audio fx filter mask
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void set_audio_fx_mask(uint32_t new_mask)
{
	my_audio_mask = new_mask;
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
 * stops the photo timed capture
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void stop_photo_timer()
{
	my_photo_timer = 0;
	gui_set_image_capture_button_label(_("Cap. Image (I)"));
}

/*
 * checks if photo timed capture is on
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: 1 if on; 0 if off
 */
int check_photo_timer()
{
	return ( (my_photo_timer > 0) ? 1 : 0 );
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
 * sets the save video flag
 * args:
 *    value - save_video flag value
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void video_capture_save_video(int value)
{
	save_video = value;
}

/*
 * sets the save video flag
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: save_video flag
 */
int video_capture_get_save_video()
{
	return save_video;
}


/*
 * create an audio context
 * args:
 *    api - audio api
 *
 * asserts:
 *    none
 *
 * returns: pointer to audio context data
 */
audio_context_t *create_audio_context(int api)
{
	close_audio_context();

	my_audio_ctx = audio_init(api);

	return my_audio_ctx;
}

/*
 * get audio context
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: pointer to audio context data (or NULL if no audio)
 */
audio_context_t *get_audio_context()
{
	return my_audio_ctx;
}

/*
 * close the audio context
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void close_audio_context()
{
	if(my_audio_ctx != NULL)
		audio_close(my_audio_ctx);

	my_audio_ctx = NULL;
}
/*
 * encoder loop (should run in a separate thread)
 * args:
 *    data - pointer to user data
 *
 * asserts:
 *   none
 *
 * returns: pointer to return code
 */
static void *encoder_loop(void *data)
{
	v4l2_dev_t *device = (v4l2_dev_t *) data;

	audio_context_t *audio_ctx = get_audio_context();

	audio_buff_t *audio_buff = NULL;

	int channels = 0;
	int samprate = 0;

	if(audio_ctx)
	{
		channels = audio_ctx->channels;
		samprate = audio_ctx->samprate;
	}

	if(debug_level > 0)
		printf("ENCODER: audio [channels= %i; samprate= %i] \n",
			channels, samprate);

	encoder_context_t *encoder_ctx = encoder_get_context(
		device->requested_fmt,
		get_video_codec_ind(),
		get_audio_codec_ind(),
		get_video_muxer(),
		device->format.fmt.pix.width,
		device->format.fmt.pix.height,
		device->fps_num,
		device->fps_denom,
		channels,
		samprate);

	/*store external SPS and PPS data if needed*/
	if(get_video_codec_ind() == 0 && /*raw - direct input*/
		device->requested_fmt == V4L2_PIX_FMT_H264)
	{
		/*request a IDR (key) frame*/
		v4l2core_h264_request_idr(device);

		if(debug_level > 0)
			printf("GUVCVIEW: storing external pps and sps data in encoder context\n");
		encoder_ctx->h264_pps_size = device->h264_PPS_size;
		if(encoder_ctx->h264_pps_size > 0)
		{
			encoder_ctx->h264_pps = calloc(device->h264_PPS_size, sizeof(uint8_t));
			memcpy(encoder_ctx->h264_pps, device->h264_PPS, device->h264_PPS_size);
		}

		encoder_ctx->h264_sps_size = device->h264_SPS_size;
		if(encoder_ctx->h264_sps_size > 0)
		{
			encoder_ctx->h264_sps = calloc(device->h264_SPS_size, sizeof(uint8_t));
			memcpy(encoder_ctx->h264_sps, device->h264_SPS, device->h264_SPS_size);
		}
	}

	uint32_t current_frame = 0;
	if(device->requested_fmt == V4L2_PIX_FMT_H264)
	{
		/* store framerate */
		current_frame = v4l2core_get_h264_frame_rate_config(device);
	}


	char *video_filename = NULL;
	/*get_video_[name|path] always return a non NULL value*/
	char *name = strdup(get_video_name());
	char *path = strdup(get_video_path());

	if(get_video_sufix_flag())
	{
		char *new_name = add_file_suffix(path, name);
		free(name); /*free old name*/
		name = new_name; /*replace with suffixed name*/
	}
	int pathsize = strlen(path);
	if(path[pathsize] != '/')
		video_filename = smart_cat(path, '/', name);
	else
		video_filename = smart_cat(path, 0, name);

	if(debug_level > 1)
		printf("GUVCVIEW: saving video to %s\n", video_filename);

	encoder_muxer_init(encoder_ctx, video_filename);

	/*start video capture*/
	video_capture_save_video(1);

	/*start audio capture*/
	if(encoder_ctx->enc_audio_ctx != NULL && channels > 0)
	{
		int frame_size = encoder_ctx->enc_audio_ctx->codec_context->frame_size;

		audio_ctx->capture_buff_size = frame_size * channels;
		audio_start(audio_ctx);
		/*
		 * alloc the buffer after audio_start
		 * otherwise capture_buff_size may not
		 * be correct
		 * make big enough for float samples (32 bit) although it may
		 * contain int16 samples (16 bit)
		 */
		audio_buff = calloc(1, sizeof(audio_buff_t));
		audio_buff->data = calloc(audio_ctx->capture_buff_size, sizeof(sample_t));
	}

	int treshold = 102400; /*100 Mbytes*/
	int64_t last_check_pts = 0; /*last pts when disk supervisor called*/

	int sample_type = SAMPLE_TYPE_INT16;
	switch(encoder_ctx->enc_audio_ctx->codec_context->sample_fmt)
	{
		case AV_SAMPLE_FMT_FLTP:
			sample_type = SAMPLE_TYPE_FLOATP;
			break;
		case AV_SAMPLE_FMT_FLT:
			sample_type = SAMPLE_TYPE_FLOAT;
			break;
		case AV_SAMPLE_FMT_S16P:
			sample_type = SAMPLE_TYPE_INT16P;
			break;
		default:
			sample_type = SAMPLE_TYPE_INT16;
			break;
	}

	while(video_capture_get_save_video())
	{
		encoder_process_next_video_buffer(encoder_ctx);

		/*
		 * encode audio frames up to video timestamp or error
		 * if we are using delayed video frames only encode
		 * up to 4 audio buffers
		 */
		if(encoder_ctx->enc_audio_ctx != NULL && channels > 0)
		{
			int loop_counter = 0;
			int ret = 0;
			do
			{
				loop_counter++;
				ret = audio_get_next_buffer(audio_ctx, audio_buff,
					sample_type, my_audio_mask);

				if(ret == 0)
				{
					encoder_ctx->enc_audio_ctx->pts = audio_buff->timestamp;

					encoder_process_audio_buffer(encoder_ctx, audio_buff->data);
				}
			}
			while(ret == 0 &&
				((encoder_ctx->enc_video_ctx->delayed_frames > 0 && loop_counter < 4)||
				 (encoder_ctx->enc_video_ctx->delayed_frames <= 0 &&
				  encoder_ctx->enc_audio_ctx->pts <= encoder_ctx->enc_video_ctx->pts)));
		}

		/*disk supervisor*/
		if(encoder_ctx->enc_video_ctx->pts - last_check_pts > 2 * NSEC_PER_SEC)
		{
			last_check_pts = encoder_ctx->enc_video_ctx->pts;

			if(!encoder_disk_supervisor(treshold, path))
			{
				/*stop capture*/
				gui_set_video_capture_button_status(0);
			}
		}
	}

	/*stop audio capture*/
	if(encoder_ctx->enc_audio_ctx != NULL && channels > 0)
		audio_stop(audio_ctx);

	encoder_muxer_close(encoder_ctx);

	encoder_close(encoder_ctx);

	if(device->requested_fmt == V4L2_PIX_FMT_H264)
	{
		/* restore framerate */
		v4l2core_set_h264_frame_rate_config(device, current_frame);
	}

	/*clean string*/
	free(video_filename);
	free(path);
	free(name);

	return ((void *) 0);
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

	uint64_t my_last_photo_time = 0; /*timer count*/
	int my_photo_npics = 0;/*no npics*/

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

	/*add a photo capture timer*/
	if(my_options->photo_timer > 0)
	{
		my_photo_timer = NSEC_PER_SEC * my_options->photo_timer;
		my_last_photo_time = v4l2core_time_get_timestamp(); /*timer count*/
	}

	if(my_options->photo_npics > 0)
		my_photo_npics = my_options->photo_npics;

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
			render_frame(device->yuv_frame, my_render_mask);

			if(check_photo_timer())
			{
				if((device->timestamp - my_last_photo_time) > my_photo_timer)
				{
					save_image = 1;
					my_last_photo_time = device->timestamp;

					if(my_options->photo_npics > 0)
					{
						if(my_photo_npics > 0)
							my_photo_npics--;
						else
							stop_photo_timer(); /*close timer*/
					}
				}
			}

			if(save_image)
			{
				char *img_filename = NULL;

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
					img_filename = smart_cat(path, '/', name);
				else
					img_filename = smart_cat(path, 0, name);

				if(debug_level > 1)
					printf("GUVCVIEW: saving image to %s\n", img_filename);

				v4l2core_save_image(device, img_filename, get_photo_format());

				free(path);
				free(name);
				free(img_filename);

				save_image = 0; /*reset*/
			}

			if(video_capture_get_save_video())
			{
				int size = device->format.fmt.pix.width * device->format.fmt.pix.height * 2;
				uint8_t *input_frame = device->yuv_frame;
				/*
				 * TODO: check codec_id, format and frame flags
				 * (we may want to store a compressed format
				 */
				if(get_video_codec_ind() == 0)
				{
					switch(device->requested_fmt)
					{
						case  V4L2_PIX_FMT_H264:
							input_frame = device->h264_frame;
							size = (int) device->h264_frame_size;
							break;
						case V4L2_PIX_FMT_MJPEG:
							input_frame = device->raw_frame;
							size = (int) device->raw_frame_size;
							break;
					}

				}
				encoder_add_video_frame(input_frame, size, device->timestamp, device->isKeyframe);

				int time_sched = encoder_buff_scheduler(); /*nanosec*/
				if(time_sched > 0)
				{
					switch(device->requested_fmt)
					{
						case  V4L2_PIX_FMT_H264:
						{
							uint32_t framerate = time_sched; /*nanosec*/
							v4l2core_set_h264_frame_rate_config(device, framerate);
							break;
						}
						default:
						{
							struct timespec req = {
								.tv_sec = 0,
								.tv_nsec = time_sched};/*nanosec*/
							nanosleep(&req, NULL);
							break;
						}
					}
				}
			}

		}
	}

	v4l2core_stop_stream(device);

	render_close();

	return ((void *) 0);
}

/*
 * start the encoder thread
 * args:
 *   data - pointer to user data
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int start_encoder_thread(void *data)
{
	int ret = __THREAD_CREATE(&encoder_thread, encoder_loop, data);

	return ret;
}

/*
 * stop the encoder thread
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int stop_encoder_thread()
{
	video_capture_save_video(0);

	__THREAD_JOIN(encoder_thread);

	return 0;
}
