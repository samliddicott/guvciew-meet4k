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
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>
#include <libusb.h>

#include "uvc_h264.h"
#include "v4l2_dyna_ctrls.h"
#include "string_utils.h"
#include "callbacks.h"


#define USB_VIDEO_CONTROL		    0x01
#define USB_VIDEO_CONTROL_INTERFACE	0x24
#define USB_VIDEO_CONTROL_XU_TYPE	0x06


/* get the unit id for GUID_UVCX_H264_XU by using libusb */
uint8_t xu_get_unit_id (uint64_t busnum, uint64_t devnum)
{
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
							const guint8 *ptr = NULL;

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
									return unit_id;
								}
								ptr += desc->bLength;
							}
						}
					}
				}
			}
		}
		libusb_unref_device (device);
	}

	return unit;
}

/*
 * check for uvc h264 support by querying UVCX_VERSION
 * although geting a unit id > 0 from xu_get_unit_id
 * should be enought
 */
int has_h264_support(int hdevice, uint8_t unit_id)
{
	if(unit_id <= 0)
	{
		g_printerr("device doesn't seem to support uvc H264\n");
		return 0;
	}

	uvcx_version_t uvcx_version;

	if(query_xu_control(hdevice, unit_id, UVCX_VERSION, UVC_GET_CUR, sizeof(uvcx_version), &uvcx_version) < 0)
	{
		g_printerr("device doesn't seem to support uvc H264 in unit_id %d\n", unit_id);
		return 0;
	}
	else
	{
		g_printerr("device seems to support uvc H264 (version: %d) in unit_id %d\n", uvcx_version.wVersion, unit_id);
		return 1;
	}
}

/*
 * called if uvc h264 is supported
 * adds h264 to the format list
 */
void check_uvc_h264_format(struct vdIn *vd, struct GLOBAL *global)
{
	if(global->uvc_h264_unit < 0)
		return;

	if(get_FormatIndex(vd->listFormats, V4L2_PIX_FMT_H264) >= 0)
		return; //H264 is already in the list

	//add format to the list
	int mjpg_index = get_FormatIndex(vd->listFormats, V4L2_PIX_FMT_MJPG);
	if(mjpg_index < 0) //MJPG must be available for uvc H264 streams
		return;

	set_SupPixFormatUvcH264();

	vd->listFormats->numb_formats++; //increment number of formats
	int fmtind = vd->listFormats->numb_formats;

	vd->listFormats->listVidFormats = g_renew(VidFormats, vd->listFormats->listVidFormats, fmtind);
	vd->listFormats->listVidFormats[fmtind-1].format = V4L2_PIX_FMT_H264;
	vd->listFormats->listVidFormats[fmtind-1].fourcc = "H264";
	vd->listFormats->listVidFormats[fmtind-1].listVidCap = NULL;
	vd->listFormats->listVidFormats[fmtind-1].numb_res = 0;
	//enumerate frame sizes with UVCX_VIDEO_CONFIG_PROBE
	//get PROBE info (def, max, min, cur)
	uvcx_video_config_probe_commit_t config_probe_def;
	uvcx_video_config_probe_commit_t config_probe_max;
	uvcx_video_config_probe_commit_t config_probe_min;
	uvcx_video_config_probe_commit_t config_probe_cur;
	uvcx_video_config_probe_commit_t config_probe_test;

	uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_GET_DEF, &config_probe_def);
	uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_GET_MAX, &config_probe_max);
	uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_GET_MIN, &config_probe_min);
	uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_GET_CUR, &config_probe_cur);

	//check MJPG resolutions and frame rates for H264
	int numb_res = vd->listFormats->listVidFormats[mjpg_index].numb_res;

	int i=0, j=0;
	int res_index = 0;
	for(i=0; i < numb_res; i++)
	{
		config_probe_test = config_probe_def;

		int width = vd->listFormats->listVidFormats[mjpg_index].listVidCap[i].width;
		int height = vd->listFormats->listVidFormats[mjpg_index].listVidCap[i].height;

		config_probe_test.wWidth = width;
		config_probe_test.wheight = height;

		if(!uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_SET_CUR, &config_probe_test))
			continue;

		if(!uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_GET_CUR, &config_probe_test))
			continue;

		if(config_probe_test.wWidth != width || config_probe_test.wheight != height)
		{
			fprintf(stderr, "H264 resolution %ix%i not supported\n", width, height);
			continue;
		}

		res_index++;
		vd->listFormats->listVidFormats[fmtind-1].listVidCap = g_renew(VidCap,
			vd->listFormats->listVidFormats[fmtind-1].listVidCap, res_index);
		vd->listFormats->listVidFormats[fmtind-1].numb_res = res_index;
		vd->listFormats->listVidFormats[fmtind-1].listVidCap[res_index-1].width = width;
		vd->listFormats->listVidFormats[fmtind-1].listVidCap[res_index-1].height = height;
		vd->listFormats->listVidFormats[fmtind-1].listVidCap[res_index-1].framerate_num = NULL;
		vd->listFormats->listVidFormats[fmtind-1].listVidCap[res_index-1].framerate_denom = NULL;
		vd->listFormats->listVidFormats[fmtind-1].listVidCap[res_index-1].numb_frates = 0;

		//check frates
		int numb_frates = vd->listFormats->listVidFormats[mjpg_index].listVidCap[i].numb_frates;
		int frate_index = 0;
		for(j=0; j < numb_frates; j++)
		{
			int framerate_num = vd->listFormats->listVidFormats[mjpg_index].listVidCap[i].framerate_num[j];
			int framerate_denom = vd->listFormats->listVidFormats[mjpg_index].listVidCap[i].framerate_denom[j];
			//in 100ns units
			uint32_t frame_interval = (framerate_denom * 1000000000LL / framerate_num)/100;
			config_probe_test.dwFrameInterval = frame_interval;

			if(!uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_SET_CUR, &config_probe_test))
				continue;

			if(!uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_GET_CUR, &config_probe_test))
				continue;

			if(config_probe_test.dwFrameInterval != frame_interval)
			{
				fprintf(stderr, "H264 resolution %ix%i with frame_rate %i not supported\n", width, height, frame_interval);
				continue;
			}

			frate_index++;
			vd->listFormats->listVidFormats[fmtind-1].listVidCap[res_index-1].numb_frates = frate_index;
			vd->listFormats->listVidFormats[fmtind-1].listVidCap[res_index-1].framerate_num = g_renew(int,
				vd->listFormats->listVidFormats[fmtind-1].listVidCap[res_index-1].framerate_num, frate_index);
			vd->listFormats->listVidFormats[fmtind-1].listVidCap[res_index-1].framerate_num[frate_index-1] = framerate_num;
			vd->listFormats->listVidFormats[fmtind-1].listVidCap[res_index-1].framerate_denom = g_renew(int,
				vd->listFormats->listVidFormats[fmtind-1].listVidCap[res_index-1].framerate_denom, frate_index);
			vd->listFormats->listVidFormats[fmtind-1].listVidCap[res_index-1].framerate_denom[frate_index-1] = framerate_denom;
		}
	}

	//return config video probe to current state
	uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_SET_CUR, &config_probe_cur);
}

void commit_uvc_h264_format(struct vdIn *vd, struct GLOBAL *global)
{
	uvcx_video_config_probe_commit_t config_probe_cur;

	uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_GET_CUR, &config_probe_cur);


	uvcx_video_commit(vd->fd, global->uvc_h264_unit, &config_probe_cur);
}

int uvcx_video_probe(int hdevice, uint8_t unit_id, uint8_t query, uvcx_video_config_probe_commit_t *uvcx_video_config)
{
	int err = 0;

	if((err = query_xu_control(hdevice, unit_id, UVCX_VIDEO_CONFIG_PROBE, query, sizeof(uvcx_video_config_probe_commit_t), uvcx_video_config)) < 0)
	{
		perror("UVCX_VIDEO_CONFIG_PROBE error");
		return err;
	}

	return err;
}

int uvcx_video_commit(int hdevice, uint8_t unit_id, uvcx_video_config_probe_commit_t *uvcx_video_config)
{
	int err = 0;

	if((err = query_xu_control(hdevice, unit_id, UVCX_VIDEO_CONFIG_COMMIT, UVC_SET_CUR, sizeof(uvcx_video_config_probe_commit_t), uvcx_video_config)) < 0)
	{
		perror("UVCX_VIDEO_CONFIG_COMMIT error");
		return err;
	}

	return err;
}



