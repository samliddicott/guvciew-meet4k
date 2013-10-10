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

#include <libusb.h>

#include "uvc_h264.h"
#include "v4l2_dyna_ctrls.h"
#include "v4l2_formats.h"
#include "string_utils.h"
#include "callbacks.h"


#define USB_VIDEO_CONTROL		    0x01
#define USB_VIDEO_CONTROL_INTERFACE	0x24
#define USB_VIDEO_CONTROL_XU_TYPE	0x06

static void print_probe_commit_data(uvcx_video_config_probe_commit_t *data)
{
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

void h264_probe_button_clicked(GtkButton * Button, video_config_probe_commit_gtkcontrols* h264_controls)
{
	uvcx_video_config_probe_commit_t config_probe_req;
	uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_GET_CUR, &config_probe_req);
	print_probe_commit_data(&config_probe_req);

	//get the control data and fill req (need fps and resolution)


}

void h264_commit_button_clicked(GtkButton * Button, video_config_probe_commit_gtkcontrols* h264_controls)
{

}

/*
 * creates the control widgets for uvcx_video_config_probe_commit
 */
video_config_probe_commit_gtkcontrols* get_uvc_h264_controls(struct vdIn *vd, struct GLOBAL *global)
{
	//get current values
	uvcx_video_config_probe_commit_t config_probe_cur;
	uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_GET_CUR, &config_probe_cur);
	//print_probe_commit_data(&config_probe_cur);

	//get Max values
	uvcx_video_config_probe_commit_t config_probe_max;
	uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_GET_MAX, &config_probe_max);

	//get Min values
	uvcx_video_config_probe_commit_t config_probe_min;
	uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_GET_MIN, &config_probe_min);

	//alloc the struct
	video_config_probe_commit_gtkcontrols* h264_controls = g_new0(video_config_probe_commit_gtkcontrols, 1);
	//add the controls and associate the callbacks

	//dwBitRate
	GtkAdjustment *adjustment =  gtk_adjustment_new (
                                	config_probe_cur.dwBitRate,
                                	config_probe_min.dwBitRate,
                                    config_probe_max.dwBitRate,
                                    1,
                                    10,
                                    0);

    h264_controls->BitRate = gtk_spin_button_new(adjustment, 1, 0);
    gtk_editable_set_editable(GTK_EDITABLE(h264_controls->BitRate), TRUE);
    gtk_widget_show (h264_controls->BitRate);

	//bmHints
	h264_controls->Hints_res = gtk_check_button_new_with_label ("Resolution");
	gtk_widget_show (h264_controls->Hints_res);
	h264_controls->Hints_prof = gtk_check_button_new_with_label ("Profile");
	gtk_widget_show (h264_controls->Hints_prof);
	h264_controls->Hints_ratecontrol = gtk_check_button_new_with_label ("Rate Control");
	gtk_widget_show (h264_controls->Hints_ratecontrol);
	h264_controls->Hints_usage = gtk_check_button_new_with_label ("Usage Type");
	gtk_widget_show (h264_controls->Hints_usage);
	h264_controls->Hints_slicemode = gtk_check_button_new_with_label ("Slice Mode");
	gtk_widget_show (h264_controls->Hints_slicemode);
	h264_controls->Hints_sliceunit = gtk_check_button_new_with_label ("Slice Unit");
	gtk_widget_show (h264_controls->Hints_sliceunit);
	h264_controls->Hints_view = gtk_check_button_new_with_label ("MVC View");
	gtk_widget_show (h264_controls->Hints_view);
	h264_controls->Hints_temporal = gtk_check_button_new_with_label ("Temporal Scale");
	gtk_widget_show (h264_controls->Hints_temporal);
	h264_controls->Hints_snr = gtk_check_button_new_with_label ("SNR Scale");
	gtk_widget_show (h264_controls->Hints_snr);
	h264_controls->Hints_spatial = gtk_check_button_new_with_label ("Spatial Scale");
	gtk_widget_show (h264_controls->Hints_spatial);
	h264_controls->Hints_spatiallayer = gtk_check_button_new_with_label ("Spatial Layer Ratio");
	gtk_widget_show (h264_controls->Hints_spatiallayer);
	h264_controls->Hints_frameinterval = gtk_check_button_new_with_label ("Frame Interval");
	gtk_widget_show (h264_controls->Hints_frameinterval);
	h264_controls->Hints_leakybucket = gtk_check_button_new_with_label ("Leaky Bucket Size");
	gtk_widget_show (h264_controls->Hints_leakybucket);
	h264_controls->Hints_bitrate = gtk_check_button_new_with_label ("Bit Rate");
	gtk_widget_show (h264_controls->Hints_bitrate);
	h264_controls->Hints_cabac = gtk_check_button_new_with_label ("CABAC");
	gtk_widget_show (h264_controls->Hints_cabac);
	h264_controls->Hints_iframe = gtk_check_button_new_with_label ("I FramePeriod");
	gtk_widget_show (h264_controls->Hints_iframe);

	//wSliceMode
	h264_controls->SliceMode = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(h264_controls->SliceMode),
								"no multiple slices");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(h264_controls->SliceMode),
								"bits/slice");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(h264_controls->SliceMode),
								"Mbits/slice");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(h264_controls->SliceMode),
								"slices/frame");
	gtk_widget_show (h264_controls->SliceMode);

	//wSliceUnits
	GtkAdjustment *adjustment1 =  gtk_adjustment_new (
                                	config_probe_cur.wSliceUnits,
                                	config_probe_min.wSliceUnits,
                                    config_probe_max.wSliceUnits,
                                    1,
                                    10,
                                    0);

    h264_controls->SliceUnits = gtk_spin_button_new(adjustment1, 1, 0);
    gtk_editable_set_editable(GTK_EDITABLE(h264_controls->SliceUnits), TRUE);
    gtk_widget_show (h264_controls->SliceUnits);

	//wProfile
	h264_controls->Profile = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(h264_controls->Profile),
								"Baseline Profile");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(h264_controls->Profile),
								"Main Profile");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(h264_controls->Profile),
								"High Profile");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(h264_controls->Profile),
								"Scalable Baseline Profile");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(h264_controls->Profile),
								"Scalable High Profile");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(h264_controls->Profile),
								"Multiview High Profile");
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(h264_controls->Profile),
								"Stereo High Profile");
	gtk_widget_show (h264_controls->Profile);

	//wProfile (Bits 0-7)
	int cur_flags = config_probe_cur.wProfile & 0x000000FF;
	int max_flags = config_probe_max.wProfile & 0x000000FF;
	int min_flags = config_probe_min.wProfile & 0x000000FF;

	GtkAdjustment *adjustment2 =  gtk_adjustment_new (
                                	cur_flags,
                                	min_flags,
                                    max_flags,
                                    1,
                                    10,
                                    0);

    h264_controls->Profile_flags = gtk_spin_button_new(adjustment1, 1, 0);
    gtk_editable_set_editable(GTK_EDITABLE(h264_controls->Profile_flags), TRUE);
    gtk_widget_show (h264_controls->Profile_flags);



	h264_controls->probe_button = gtk_button_new_with_label("PROBE");
	g_signal_connect (GTK_BUTTON(h264_controls->probe_button), "clicked",
                                G_CALLBACK (h264_probe_button_clicked), h264_controls);
	gtk_widget_show(h264_controls->probe_button);

	h264_controls->commit_button = gtk_button_new_with_label("COMMIT");
	g_signal_connect (GTK_BUTTON(h264_controls->commit_button), "clicked",
                                G_CALLBACK (h264_commit_button_clicked), h264_controls);
	gtk_widget_show(h264_controls->commit_button);
}

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

	if(query_xu_control(hdevice, unit_id, UVCX_VERSION, UVC_GET_CUR, &uvcx_version) < 0)
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
	int mjpg_index = get_FormatIndex(vd->listFormats, V4L2_PIX_FMT_MJPEG);
	if(mjpg_index < 0) //MJPG must be available for uvc H264 streams
		return;

	set_SupPixFormatUvcH264();

	vd->listFormats->numb_formats++; //increment number of formats
	int fmtind = vd->listFormats->numb_formats;

	vd->listFormats->listVidFormats = g_renew(VidFormats, vd->listFormats->listVidFormats, fmtind);
	vd->listFormats->listVidFormats[fmtind-1].format = V4L2_PIX_FMT_H264;
	g_snprintf(vd->listFormats->listVidFormats[fmtind-1].fourcc ,5,"H264");
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
		config_probe_test.wHeight = height;

		if(!uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_SET_CUR, &config_probe_test))
			continue;

		if(!uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_GET_CUR, &config_probe_test))
			continue;

		if(config_probe_test.wWidth != width || config_probe_test.wHeight != height)
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
			uint32_t frame_interval = (framerate_num * 1000000000LL / framerate_denom)/100;
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
	uvcx_video_config_probe_commit_t config_probe_req;

	uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_GET_CUR, &config_probe_cur);

	config_probe_req = config_probe_cur;

	config_probe_req.wWidth = global->width;
	config_probe_req.wHeight = global->height;
	//in 100ns units
	uint32_t frame_interval = (global->fps_num * 1000000000LL / global->fps)/100;
	config_probe_req.dwFrameInterval = frame_interval;

	//probe the format
	uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_SET_CUR, &config_probe_req);
	uvcx_video_probe(vd->fd, global->uvc_h264_unit, UVC_GET_CUR, &config_probe_req);

	if(config_probe_req.wWidth != global->width)
	{
		fprintf(stderr, "H264 config probe: requested width %i but got %i\n",
			global->width, config_probe_req.wWidth);

		global->width = config_probe_req.wWidth;
	}
	if(config_probe_req.wHeight != global->height)
	{
		fprintf(stderr, "H264 config probe: requested height %i but got %i\n",
			global->height, config_probe_req.wHeight);

		global->height = config_probe_req.wHeight;
	}
	if(config_probe_req.dwFrameInterval != frame_interval)
	{
		fprintf(stderr, "H264 config probe: requested frame interval %i but got %i\n",
			frame_interval, config_probe_req.dwFrameInterval);
	}
	//commit the format
	print_probe_commit_data(&config_probe_req);
	uvcx_video_commit(vd->fd, global->uvc_h264_unit, &config_probe_req);
}

int uvcx_video_probe(int hdevice, uint8_t unit_id, uint8_t query, uvcx_video_config_probe_commit_t *uvcx_video_config)
{
	int err = 0;


	if((err = query_xu_control(hdevice, unit_id, UVCX_VIDEO_CONFIG_PROBE, query, uvcx_video_config)) < 0)
	{
		perror("UVCX_VIDEO_CONFIG_PROBE error");
		return err;
	}

	return err;
}

int uvcx_video_commit(int hdevice, uint8_t unit_id, uvcx_video_config_probe_commit_t *uvcx_video_config)
{
	int err = 0;

	if((err = query_xu_control(hdevice, unit_id, UVCX_VIDEO_CONFIG_COMMIT, UVC_SET_CUR, uvcx_video_config)) < 0)
	{
		perror("UVCX_VIDEO_CONFIG_COMMIT error");
		return err;
	}

	return err;
}



