/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
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

/* support for internationalization - i18n */
#include <math.h>
#include <libusb.h>
#include <stdlib.h>
#include <stdio.h>
#include <linux/videodev2.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "uvc_h264.h"
#include "v4l2_formats.h"

#define USB_VIDEO_CONTROL		    0x01
#define USB_VIDEO_CONTROL_INTERFACE	0x24
#define USB_VIDEO_CONTROL_XU_TYPE	0x06

extern int verbosity;

/*
 * print probe/commit data
 * args:
 *   data - pointer to probe/commit config data
 *
 * asserts:
 *   data is not null
 *
 * returns: void
 */
static void print_probe_commit_data(uvcx_video_config_probe_commit_t *data)
{
	/*asserts*/
	assert(data != NULL);

	printf("uvcx_video_config_probe_commit:\n");
	printf("\tFrameInterval: %i\n", data->dwFrameInterval);
	printf("\tBitRate: %i\n", data->dwBitRate);
	printf("\tHints: 0x%X\n", data->bmHints);
	printf("\tConfigurationIndex: %i\n", data->wConfigurationIndex);
	printf("\tWidth: %i\n", data->wWidth);
	printf("\tHeight: %i\n", data->wHeight);
	printf("\tSliceUnits: %i\n", data->wSliceUnits);
	printf("\tSliceMode: %i\n", data->wSliceMode);
	printf("\tProfile: %i\n", data->wProfile);
	printf("\tIFramePeriod: %i\n", data->wIFramePeriod);
	printf("\tEstimatedVideoDelay: %i\n",data->wEstimatedVideoDelay);
	printf("\tEstimatedMaxConfigDelay: %i\n",data->wEstimatedMaxConfigDelay);
	printf("\tUsageType: %i\n",data->bUsageType);
	printf("\tRateControlMode: %i\n",data->bRateControlMode);
	printf("\tTemporalScaleMode: %i\n",data->bTemporalScaleMode);
	printf("\tSpatialScaleMode: %i\n",data->bSpatialScaleMode);
	printf("\tSNRScaleMode: %i\n",data->bSNRScaleMode);
	printf("\tStreamMuxOption: %i\n",data->bStreamMuxOption);
	printf("\tStreamFormat: %i\n",data->bStreamFormat);
	printf("\tEntropyCABAC: %i\n",data->bEntropyCABAC);
	printf("\tTimestamp: %i\n",data->bTimestamp);
	printf("\tNumOfReorderFrames: %i\n",data->bNumOfReorderFrames);
	printf("\tPreviewFlipped: %i\n",data->bPreviewFlipped);
	printf("\tView: %i\n",data->bView);
	printf("\tReserved1: %i\n",data->bReserved1);
	printf("\tReserved2: %i\n",data->bReserved2);
	printf("\tStreamID: %i\n",data->bStreamID);
	printf("\tSpatialLayerRatio: %i\n",data->bSpatialLayerRatio);
	printf("\tLeakyBucketSize: %i\n",data->wLeakyBucketSize);
}

/*
 * resets the h264 encoder
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: 0 on success or error code on fail
 */
static int uvcx_video_encoder_reset(v4l2_dev *vd)
{
	/*assertions*/
	assert(vd != NULL);

	uvcx_encoder_reset encoder_reset_req = {0};

	int err = 0;

	if((err = query_xu_control(vd, vd->h264_unit_id, UVCX_ENCODER_RESET, UVC_SET_CUR, &encoder_reset_req)) < 0)
		fprintf(stderr, "V4L2_CORE: (UVCX_ENCODER_RESET) error: %s\n", strerror(errno));

	return err;
}

/*
 * probes the h264 encoder config
 * args:
 *   vd - pointer to video device data
 *   query - probe query
 *   uvcx_video_config - pointer to probe/commit config data
 *
 * asserts:
 *   vd is not null
 *
 * returns: 0 on success or error code on fail
 */
static int uvcx_video_probe(v4l2_dev* vd, uint8_t query, uvcx_video_config_probe_commit_t *uvcx_video_config)
{
	/*assertions*/
	assert(vd != NULL);

	int err = 0;


	if((err = query_xu_control(vd, vd->h264_unit_id, UVCX_VIDEO_CONFIG_PROBE, query, uvcx_video_config)) < 0)
		fprintf(stderr, "V4L2_CORE: (UVCX_VIDEO_CONFIG_PROBE) error: %s\n", strerror(errno));

	return err;
}

/*
 * commits the h264 encoder config
 * args:
 *   vd - pointer to video device data
 *   uvcx_video_config - pointer to probe/commit config data
 *
 * asserts:
 *   vd is not null
 *
 * returns: 0 on success or error code on fail
 */
static int uvcx_video_commit(v4l2_dev* vd, uvcx_video_config_probe_commit_t *uvcx_video_config)
{
	/*assertions*/
	assert(vd != NULL);

	int err = 0;

	if((err = query_xu_control(vd, vd->h264_unit_id, UVCX_VIDEO_CONFIG_COMMIT, UVC_SET_CUR, uvcx_video_config)) < 0)
		fprintf(stderr, "V4L2_CORE: (UVCX_VIDEO_CONFIG_COMMIT) error: %s\n", strerror(errno));

	return err;
}


/*
 * gets the uvc h264 xu control unit id, if any
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *   vd->list_devices is not null
 *
 * returns: unit id or 0 if none
 *  (also sets vd->h264_unit_id)
 */
uint8_t get_uvc_h624_unit_id (v4l2_dev *vd)
{
	/*asserts*/
	assert(vd != NULL);
	assert(vd->list_devices != NULL);

	uint64_t busnum = vd->list_devices[vd->this_device].busnum;
	uint64_t devnum = vd->list_devices[vd->this_device].devnum;

    /* use libusb */
	libusb_context *usb_ctx = NULL;
    libusb_device **device_list = NULL;
    libusb_device *device = NULL;
    ssize_t cnt;
    int i, j, k;

	static const uint8_t guid[16] = GUID_UVCX_H264_XU;
	uint8_t unit = 0;

    if (usb_ctx == NULL)
      libusb_init (&usb_ctx);

    cnt = libusb_get_device_list (usb_ctx, &device_list);
    for (i = 0; i < cnt; i++)
    {
		uint64_t dev_busnum = libusb_get_bus_number (device_list[i]);
		uint64_t dev_devnum = libusb_get_device_address (device_list[i]);
		if (busnum == dev_busnum &&	devnum == dev_devnum)
		{
			device = libusb_ref_device (device_list[i]);
			break;
		}
	}

	libusb_free_device_list (device_list, 1);

	if (device)
	{
		struct libusb_device_descriptor desc;

		 if (libusb_get_device_descriptor (device, &desc) == 0)
		 {
			for (i = 0; i < desc.bNumConfigurations; ++i)
			{
				struct libusb_config_descriptor *config = NULL;

				if (libusb_get_config_descriptor (device, i, &config) == 0)
				{
					for (j = 0; j < config->bNumInterfaces; j++)
					{
						for (k = 0; k < config->interface[j].num_altsetting; k++)
						{
							const struct libusb_interface_descriptor *interface;
							const uint8_t *ptr = NULL;

							interface = &config->interface[j].altsetting[k];
							if (interface->bInterfaceClass != LIBUSB_CLASS_VIDEO ||
								interface->bInterfaceSubClass != USB_VIDEO_CONTROL)
								continue;
							ptr = interface->extra;
							while (ptr - interface->extra +
								sizeof (xu_descriptor) < interface->extra_length)
							{
								xu_descriptor *desc = (xu_descriptor *) ptr;

								if (desc->bDescriptorType == USB_VIDEO_CONTROL_INTERFACE &&
									desc->bDescriptorSubType == USB_VIDEO_CONTROL_XU_TYPE &&
									memcmp (desc->guidExtensionCode, guid, 16) == 0)
								{
									uint8_t unit_id = desc->bUnitID;

									libusb_unref_device (device);
									/*it's a match*/
									vd->h264_unit_id = unit;
									return unit_id;
								}
								ptr += desc->bLength;
							}
						}
					}
				}
				else
					fprintf(stderr, "V4L2_CORE: (libusb) couldn't get config descriptor for configuration %i\n", i);
			}
		}
		else
			fprintf(stderr, "V4L2_CORE: (libusb) couldn't get device descriptor\n");
		libusb_unref_device (device);
	}
	/*no match found*/
	vd->h264_unit_id = unit;
	return unit;
}

/*
 * check for uvc h264 support by querying UVCX_VERSION
 * although geting a unit id > 0 from xu_get_unit_id
 * should be enought
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *   vd->fd is valid ( > 0 )
 *
 * returns: 1 if support available or 0 otherwise
 */
int check_h264_support(v4l2_dev *vd)
{
	/*assertions*/
	assert(vd != NULL);
	assert(vd->fd > 0);

	if(vd->h264_unit_id <= 0)
	{
		if(verbosity > 0)
			fprintf(stderr, "V4L2_CORE: device doesn't seem to support uvc H264 (%i)\n", vd->h264_unit_id);
		return 0;
	}

	uvcx_version_t uvcx_version;

	if(query_xu_control(vd, vd->h264_unit_id, UVCX_VERSION, UVC_GET_CUR, &uvcx_version) < 0)
	{
		if(verbosity > 0)
			fprintf(stderr, "V4L2_CORE: device doesn't seem to support uvc H264 in unit_id %d\n", vd->h264_unit_id);
		return 0;
	}

	if(verbosity > 0)
		printf("V4L2_CORE: device seems to support uvc H264 (version: %d) in unit_id %d\n", uvcx_version.wVersion, vd->h264_unit_id);
	return 1;

}

/*
 * adds h264 to the format list, if supported by device
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *   vd->list_stream_formats is not null
 *
 * returns: void
 */
void add_h264_format(v4l2_dev *vd)
{
	/*assertions*/
	assert(vd != NULL);
	assert(vd->list_stream_formats != NULL);

	if(verbosity > 0)
		printf("V4L2_CORE: checking muxed H264 format support\n");

	if(get_frame_format_index(vd, V4L2_PIX_FMT_H264) >= 0)
	{
		if(verbosity > 0)
			printf("V4L2_CORE: H264 format already in list\n");
		return; /*H264 is already in the list*/
	}

	if(get_uvc_h624_unit_id(vd) <= 0)
		return; /*no unit id found for h264*/

	if(!check_h264_support(vd))
		return; /*no XU support for h264*/

	/*add the format to the list*/
	int mjpg_index = get_frame_format_index(vd, V4L2_PIX_FMT_MJPEG);
	if(mjpg_index < 0) /*MJPG must be available for uvc H264 streams*/
		return;

	if(verbosity > 0)
		printf("V4L2_CORE: adding muxed H264 format\n");

	vd->numb_formats++; /*increment number of formats*/
	int fmtind = vd->numb_formats;

	vd->list_stream_formats = realloc(
		vd->list_stream_formats,
		fmtind * sizeof(v4l2_stream_formats));
	vd->list_stream_formats[fmtind-1].format = V4L2_PIX_FMT_H264;
	snprintf(vd->list_stream_formats[fmtind-1].fourcc , 5, "H264");
	vd->list_stream_formats[fmtind-1].list_stream_cap = NULL;
	vd->list_stream_formats[fmtind-1].numb_res = 0;


	/*add MJPG resolutions and frame rates for H264*/
	int numb_res = vd->list_stream_formats[mjpg_index].numb_res;

	int i=0, j=0;
	int res_index = 0;
	for(i=0; i < numb_res; i++)
	{
		int width = vd->list_stream_formats[mjpg_index].list_stream_cap[i].width;
		int height = vd->list_stream_formats[mjpg_index].list_stream_cap[i].height;

		res_index++;
		vd->list_stream_formats[fmtind-1].list_stream_cap = realloc(
			vd->list_stream_formats[fmtind-1].list_stream_cap,
			res_index * sizeof(v4l2_stream_cap));
		vd->list_stream_formats[fmtind-1].numb_res = res_index;
		vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].width = width;
		vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].height = height;
		vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_num = NULL;
		vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_denom = NULL;
		vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].numb_frates = 0;

		/*add frates*/
		int numb_frates =  vd->list_stream_formats[mjpg_index].list_stream_cap[i].numb_frates;
		int frate_index = 0;
		for(j=0; j < numb_frates; j++)
		{
			int framerate_num = vd->list_stream_formats[mjpg_index].list_stream_cap[i].framerate_num[j];
			int framerate_denom = vd->list_stream_formats[mjpg_index].list_stream_cap[i].framerate_denom[j];

			frate_index++;
			vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].numb_frates = frate_index;
			vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_num = realloc(
				vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_num,
				frate_index * sizeof(int));
			vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_num[frate_index-1] = framerate_num;
			vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_denom = realloc(
				vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_denom,
				frate_index * sizeof(int));
			vd->list_stream_formats[fmtind-1].list_stream_cap[res_index-1].framerate_denom[frate_index-1] = framerate_denom;
		}
	}
}

/*
 * sets h264 muxed format (must not be called while streaming)
 * args:
 *   vd - pointer to video device data
 *
 * asserts:
 *   vd is not null
 *
 * returns: void
 */
void set_h264_muxed_format(v4l2_dev *vd)
{
	uvcx_video_config_probe_commit_t *config_probe_req = &(vd->h264_config_probe_req);

	/* reset the encoder*/
	uvcx_video_encoder_reset(vd);

	/*
	 * Get default values (safe)
	 */
	uvcx_video_probe(vd, UVC_GET_DEF, config_probe_req);

	/*set resolution*/
	config_probe_req->wWidth = vd->format.fmt.pix.width;
	config_probe_req->wHeight = vd->format.fmt.pix.height;
	/*set frame rate in 100ns units*/
	uint32_t frame_interval = (vd->fps_num * 1000000000LL / vd->fps_denom)/100;
	config_probe_req->dwFrameInterval = frame_interval;

	/*set the aux stream (h264)*/
	config_probe_req->bStreamMuxOption = STREAMMUX_H264;

	/*probe the format*/
	uvcx_video_probe(vd, UVC_SET_CUR, config_probe_req);
	uvcx_video_probe(vd, UVC_GET_CUR, config_probe_req);

	if(config_probe_req->wWidth != vd->format.fmt.pix.width)
	{
		fprintf(stderr, "V4L2_CORE: H264 config probe: requested width %i but got %i\n",
			vd->format.fmt.pix.width, config_probe_req->wWidth);

		vd->format.fmt.pix.width = config_probe_req->wWidth;
	}
	if(config_probe_req->wHeight != vd->format.fmt.pix.height)
	{
		fprintf(stderr, "V4L2_CORE: H264 config probe: requested height %i but got %i\n",
			vd->format.fmt.pix.height, config_probe_req->wHeight);

		vd->format.fmt.pix.height = config_probe_req->wHeight;
	}
	if(config_probe_req->dwFrameInterval != frame_interval)
	{
		fprintf(stderr, "V4L2_CORE: H264 config probe: requested frame interval %i but got %i\n",
			frame_interval, config_probe_req->dwFrameInterval);
	}
	/*commit the format*/
	uvcx_video_commit(vd, config_probe_req);

	/*print probe/commit data*/
	print_probe_commit_data(config_probe_req);
}



/*

uint8_t uvcx_get_video_rate_control_mode(int hdevice, uint8_t unit_id, uint8_t query)
{
	uvcx_rate_control_mode_t rate_control_mode_req;
	rate_control_mode_req.wLayerID = 0;

	int err = 0;

	if((err = query_xu_control(hdevice, unit_id, UVCX_RATE_CONTROL_MODE, query, &rate_control_mode_req)) < 0)
	{
		perror("UVCX_RATE_CONTROL_MODE: query error");
		return err;
	}

	return rate_control_mode_req.bRateControlMode;
}

int uvcx_set_video_rate_control_mode(int hdevice, uint8_t unit_id, uint8_t rate_mode)
{
	uvcx_rate_control_mode_t rate_control_mode_req;
	rate_control_mode_req.wLayerID = 0;
	rate_control_mode_req.bRateControlMode = rate_mode;

	int err = 0;

	if((err = query_xu_control(hdevice, unit_id, UVCX_RATE_CONTROL_MODE, UVC_SET_CUR, &rate_control_mode_req)) < 0)
	{
		perror("UVCX_ENCODER_RESET: SET_CUR error");
	}

	return err;
}

uint8_t uvcx_get_temporal_scale_mode(int hdevice, uint8_t unit_id, uint8_t query)
{
	uvcx_temporal_scale_mode_t temporal_scale_mode_req;
	temporal_scale_mode_req.wLayerID = 0;

	int err = 0;

	if((err = query_xu_control(hdevice, unit_id, UVCX_TEMPORAL_SCALE_MODE, query, &temporal_scale_mode_req)) < 0)
	{
		perror("UVCX_TEMPORAL_SCALE_MODE: query error");
		return err;
	}

	return temporal_scale_mode_req.bTemporalScaleMode;
}

int uvcx_set_temporal_scale_mode(int hdevice, uint8_t unit_id, uint8_t scale_mode)
{
	uvcx_temporal_scale_mode_t temporal_scale_mode_req;
	temporal_scale_mode_req.wLayerID = 0;
	temporal_scale_mode_req.bTemporalScaleMode = scale_mode;

	int err = 0;

	if((err = query_xu_control(hdevice, unit_id, UVCX_TEMPORAL_SCALE_MODE, UVC_SET_CUR, &temporal_scale_mode_req)) < 0)
	{
		perror("UVCX_TEMPORAL_SCALE_MODE: SET_CUR error");
	}

	return err;
}

uint8_t uvcx_get_spatial_scale_mode(int hdevice, uint8_t unit_id, uint8_t query)
{
	uvcx_spatial_scale_mode_t spatial_scale_mode_req;
	spatial_scale_mode_req.wLayerID = 0;

	int err = 0;

	if((err = query_xu_control(hdevice, unit_id, UVCX_SPATIAL_SCALE_MODE, query, &spatial_scale_mode_req)) < 0)
	{
		perror("UVCX_SPATIAL_SCALE_MODE: query error");
		return err;
	}

	return spatial_scale_mode_req.bSpatialScaleMode;
}

int uvcx_set_spatial_scale_mode(int hdevice, uint8_t unit_id, uint8_t scale_mode)
{
	uvcx_spatial_scale_mode_t spatial_scale_mode_req;
	spatial_scale_mode_req.wLayerID = 0;
	spatial_scale_mode_req.bSpatialScaleMode = scale_mode;

	int err = 0;

	if((err = query_xu_control(hdevice, unit_id, UVCX_SPATIAL_SCALE_MODE, UVC_SET_CUR, &spatial_scale_mode_req)) < 0)
	{
		perror("UVCX_SPATIAL_SCALE_MODE: SET_CUR error");
	}

	return err;
}

int uvcx_request_frame_type(int hdevice, uint8_t unit_id, uint16_t type)
{
	uvcx_picture_type_control_t picture_type_req;
	picture_type_req.wLayerID = 0;
	picture_type_req.wPicType = type;

	int err = 0;

	if((err = query_xu_control(hdevice, unit_id, UVCX_PICTURE_TYPE_CONTROL, UVC_SET_CUR, &picture_type_req)) < 0)
	{
		perror("UVCX_PICTURE_TYPE_CONTROL: SET_CUR error");
	}

	return err;

}

uint32_t uvcx_get_frame_rate_config(int hdevice, uint8_t unit_id, uint8_t query)
{
	uvcx_framerate_config_t framerate_req;
	framerate_req.wLayerID = 0;

	int err = 0;

	if((err = query_xu_control(hdevice, unit_id, UVCX_FRAMERATE_CONFIG, query, &framerate_req)) < 0)
	{
		perror("UVCX_FRAMERATE_CONFIG: query error");
		return err;
	}

	return framerate_req.dwFrameInterval;
}

int uvcx_set_frame_rate_config(int hdevice, uint8_t unit_id, uint32_t framerate)
{
	uvcx_framerate_config_t framerate_req;
	framerate_req.wLayerID = 0;
	framerate_req.dwFrameInterval = framerate;

	int err = 0;

	if((err = query_xu_control(hdevice, unit_id, UVCX_FRAMERATE_CONFIG, UVC_SET_CUR, &framerate_req)) < 0)
	{
		perror("UVCX_FRAMERATE_CONFIG: SET_CUR error");
	}

	return err;
}

*/
