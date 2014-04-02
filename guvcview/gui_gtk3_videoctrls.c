/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
#           Flemming Frandsen <dren.dk@gmail.com>                               #
#                             Add VU meter OSD                                  #
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
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
/* support for internationalization - i18n */
#include <locale.h>
#include <libintl.h>

#include "gui_gtk3.h"
#include "gui_gtk3_callbacks.h"
#include "gui.h"
/*add this last to avoid redefining _() and N_()*/
#include "gview.h"
#include "gviewrender.h"
#include "video_capture.h"


extern int debug_level;

/*
 * attach v4l2 video controls tab widget
 * args:
 *   device - pointer to device data we want to attach the gui for
 *   parent - tab parent widget
 *
 * asserts:
 *   device is not null
 *   parent is not null
 *
 * returns: error code (0 -OK)
 */
int gui_attach_gtk3_videoctrls(v4l2_dev_t *device, GtkWidget *parent)
{
	/*assertions*/
	assert(device != NULL);
	assert(parent != NULL);

	if(debug_level > 1)
		printf("GUVCVIEW: attaching video controls\n");

	int format_index = v4l2core_get_frame_format_index(device, device->requested_fmt);

	if(format_index < 0)
	{
		gui_error(device, "Guvcview error", "invalid pixel format", 0);
		printf("GUVCVIEW: invalid pixel format\n");
	}

	int resolu_index = v4l2core_get_format_resolution_index(
		device,
		format_index,
		device->format.fmt.pix.width,
		device->format.fmt.pix.height);

	if(resolu_index < 0)
	{
		gui_error(device, "Guvcview error", "invalid resolution index", 0);
		printf("GUVCVIEW: invalid resolution index\n");
	}

	GtkWidget *video_controls_grid = gtk_grid_new();
	gtk_widget_show (video_controls_grid);

	gtk_grid_set_column_homogeneous (GTK_GRID(video_controls_grid), FALSE);
	gtk_widget_set_hexpand (video_controls_grid, TRUE);
	gtk_widget_set_halign (video_controls_grid, GTK_ALIGN_FILL);

	gtk_grid_set_row_spacing (GTK_GRID(video_controls_grid), 4);
	gtk_grid_set_column_spacing (GTK_GRID (video_controls_grid), 4);
	gtk_container_set_border_width (GTK_CONTAINER (video_controls_grid), 2);

	char temp_str[20];
	int line = 0;
	int i =0;

	/*---- Devices ----*/
	GtkWidget *label_Device = gtk_label_new(_("Device:"));
	gtk_misc_set_alignment (GTK_MISC (label_Device), 1, 0.5);

	gtk_grid_attach (GTK_GRID(video_controls_grid), label_Device, 0, line, 1, 1);

	gtk_widget_show (label_Device);

	set_wgtDevices_gtk3(gtk_combo_box_text_new());
	gtk_widget_set_halign (get_wgtDevices_gtk3(), GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (get_wgtDevices_gtk3(), TRUE);

	v4l2_device_list *device_list = v4l2core_get_device_list();

	if (device_list->num_devices < 1)
	{
		/*use current*/
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(get_wgtDevices_gtk3()),
			device->videodevice);
		gtk_combo_box_set_active(GTK_COMBO_BOX(get_wgtDevices_gtk3()),0);
	}
	else
	{
		for(i = 0; i < (device_list->num_devices); i++)
		{
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(get_wgtDevices_gtk3()),
				device_list->list_devices[i].name);
			if(device_list->list_devices[i].current)
				gtk_combo_box_set_active(GTK_COMBO_BOX(get_wgtDevices_gtk3()),i);
		}
	}
	gtk_grid_attach (GTK_GRID(video_controls_grid), get_wgtDevices_gtk3(), 1, line, 1 ,1);
	gtk_widget_show (get_wgtDevices_gtk3());
	g_signal_connect (GTK_COMBO_BOX_TEXT(get_wgtDevices_gtk3()), "changed",
		G_CALLBACK (devices_changed), device);

	/*---- Frame Rate ----*/
	line++;

	GtkWidget *label_FPS = gtk_label_new(_("Frame Rate:"));
	gtk_misc_set_alignment (GTK_MISC (label_FPS), 1, 0.5);
	gtk_widget_show (label_FPS);

	gtk_grid_attach (GTK_GRID(video_controls_grid), label_FPS, 0, line, 1, 1);

	GtkWidget *wgtFrameRate = gtk_combo_box_text_new ();
	gtk_widget_set_halign (wgtFrameRate, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (wgtFrameRate, TRUE);

	gtk_widget_show (wgtFrameRate);
	gtk_widget_set_sensitive (wgtFrameRate, TRUE);

	int deffps=0;

	if (debug_level > 0)
		printf("GUVCVIEW: frame rates of resolution index %d = %d \n",
			resolu_index+1,
			device->list_stream_formats[format_index].list_stream_cap[resolu_index].numb_frates);
	for ( i = 0 ; i < device->list_stream_formats[format_index].list_stream_cap[resolu_index].numb_frates ; ++i)
	{
		g_snprintf(
			temp_str,
			18,
			"%i/%i fps",
			device->list_stream_formats[format_index].list_stream_cap[resolu_index].framerate_denom[i],
			device->list_stream_formats[format_index].list_stream_cap[resolu_index].framerate_num[i]);

		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(wgtFrameRate), temp_str);

		if (( device->fps_num == device->list_stream_formats[format_index].list_stream_cap[resolu_index].framerate_num[i]) &&
			( device->fps_denom == device->list_stream_formats[format_index].list_stream_cap[resolu_index].framerate_denom[i]))
				deffps=i;//set selected
	}

	gtk_grid_attach (GTK_GRID(video_controls_grid), wgtFrameRate, 1, line, 1 ,1);

	gtk_combo_box_set_active(GTK_COMBO_BOX(wgtFrameRate), deffps);

	if (deffps==0)
	{
		if (device->list_stream_formats[format_index].list_stream_cap[resolu_index].framerate_denom)
			device->fps_denom = device->list_stream_formats[format_index].list_stream_cap[resolu_index].framerate_denom[0];

		if (device->list_stream_formats[format_index].list_stream_cap[resolu_index].framerate_num)
			device->fps_num = device->list_stream_formats[format_index].list_stream_cap[resolu_index].framerate_num[0];

		if(debug_level > 0)
			printf("GUVCVIEW: fps is set to %i/%i\n", device->fps_num, device->fps_denom);
	}

	g_signal_connect (GTK_COMBO_BOX_TEXT(wgtFrameRate), "changed",
		G_CALLBACK (frame_rate_changed), device);

	/*try to sync the device fps (capture thread must have started by now)*/
	v4l2core_request_framerate_update (device);

	/*---- Resolution ----*/
	line++;

	GtkWidget *label_Resol = gtk_label_new(_("Resolution:"));
	gtk_misc_set_alignment (GTK_MISC (label_Resol), 1, 0.5);
	gtk_grid_attach (GTK_GRID(video_controls_grid), label_Resol, 0, line, 1, 1);
	gtk_widget_show (label_Resol);

	GtkWidget *wgtResolution = gtk_combo_box_text_new ();
	gtk_widget_set_halign (wgtResolution, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (wgtResolution, TRUE);

	gtk_widget_show (wgtResolution);
	gtk_widget_set_sensitive (wgtResolution, TRUE);

	int defres=0;

	if (debug_level > 0)
		printf("GUVCVIEW: resolutions of format(%d) = %d \n",
			format_index+1,
			device->list_stream_formats[format_index].numb_res);
	for(i = 0 ; i < device->list_stream_formats[format_index].numb_res ; i++)
	{
		if (device->list_stream_formats[format_index].list_stream_cap[i].width > 0)
		{
			g_snprintf(
				temp_str,
				18,
				"%ix%i",
				device->list_stream_formats[format_index].list_stream_cap[i].width,
				device->list_stream_formats[format_index].list_stream_cap[i].height);

			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(wgtResolution), temp_str);

			if ((device->format.fmt.pix.width == device->list_stream_formats[format_index].list_stream_cap[i].width) &&
				(device->format.fmt.pix.height == device->list_stream_formats[format_index].list_stream_cap[i].height))
					defres=i;//set selected resolution index
		}
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(wgtResolution), defres);

	gtk_grid_attach (GTK_GRID(video_controls_grid), wgtResolution, 1, line, 1 ,1);

	g_object_set_data (G_OBJECT (wgtResolution), "control_fps", wgtFrameRate);

	g_signal_connect (wgtResolution, "changed",
		G_CALLBACK (resolution_changed), device);


	/*---- Input Format ----*/
	line++;

	GtkWidget *label_InpType = gtk_label_new(_("Camera Output:"));
	gtk_misc_set_alignment (GTK_MISC (label_InpType), 1, 0.5);

	gtk_widget_show (label_InpType);

	gtk_grid_attach (GTK_GRID(video_controls_grid), label_InpType, 0, line, 1, 1);

	GtkWidget *wgtInpType= gtk_combo_box_text_new ();
	gtk_widget_set_halign (wgtInpType, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (wgtInpType, TRUE);

	gtk_widget_show (wgtInpType);
	gtk_widget_set_sensitive (wgtInpType, TRUE);

	int fmtind=0;
	for (fmtind=0; fmtind < device->numb_formats; fmtind++)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(wgtInpType), device->list_stream_formats[fmtind].fourcc);
		if(device->requested_fmt == device->list_stream_formats[fmtind].format)
			gtk_combo_box_set_active(GTK_COMBO_BOX(wgtInpType), fmtind); /*set active*/
	}

	gtk_grid_attach (GTK_GRID(video_controls_grid), wgtInpType, 1, line, 1 ,1);

	//g_object_set_data (G_OBJECT (wgtInpType), "control_fps", wgtFrameRate);
	g_object_set_data (G_OBJECT (wgtInpType), "control_resolution", wgtResolution);

	g_signal_connect (GTK_COMBO_BOX_TEXT(wgtInpType), "changed",
		G_CALLBACK (format_changed), device);


	/* ----- Filter controls -----*/
	line++;
	GtkWidget *label_videoFilters = gtk_label_new(_("---- Video Filters ----"));
	gtk_misc_set_alignment (GTK_MISC (label_videoFilters), 0.5, 0.5);

	gtk_grid_attach (GTK_GRID(video_controls_grid), label_videoFilters, 0, line, 3, 1);
	gtk_widget_show (label_videoFilters);

	/*filters grid*/
	line++;
	GtkWidget *table_filt = gtk_grid_new();
	gtk_grid_set_row_spacing (GTK_GRID (table_filt), 4);
	gtk_grid_set_column_spacing (GTK_GRID (table_filt), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table_filt), 4);
	gtk_widget_set_size_request (table_filt, -1, -1);

	gtk_widget_set_halign (table_filt, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (table_filt, TRUE);
	gtk_grid_attach (GTK_GRID(video_controls_grid), table_filt, 0, line, 3, 1);
	gtk_widget_show (table_filt);

	/* Mirror FX */
	GtkWidget *FiltMirrorEnable = gtk_check_button_new_with_label (_(" Mirror"));
	g_object_set_data (G_OBJECT (FiltMirrorEnable), "filt_info", GINT_TO_POINTER(REND_FX_YUV_MIRROR));
	gtk_widget_set_halign (FiltMirrorEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (FiltMirrorEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_filt), FiltMirrorEnable, 0, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltMirrorEnable),
		(get_render_fx_mask() & REND_FX_YUV_MIRROR) > 0);
	gtk_widget_show (FiltMirrorEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltMirrorEnable), "toggled",
		G_CALLBACK (render_fx_filter_changed), device);
	/* Upturn FX */
	GtkWidget *FiltUpturnEnable = gtk_check_button_new_with_label (_(" Invert"));
	g_object_set_data (G_OBJECT (FiltUpturnEnable), "filt_info", GINT_TO_POINTER(REND_FX_YUV_UPTURN));
	gtk_widget_set_halign (FiltUpturnEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (FiltUpturnEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_filt), FiltUpturnEnable, 1, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltUpturnEnable),
		(get_render_fx_mask() & REND_FX_YUV_UPTURN) > 0);
	gtk_widget_show (FiltUpturnEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltUpturnEnable), "toggled",
		G_CALLBACK (render_fx_filter_changed), device);
	/* Negate FX */
	GtkWidget *FiltNegateEnable = gtk_check_button_new_with_label (_(" Negative"));
	g_object_set_data (G_OBJECT (FiltNegateEnable), "filt_info", GINT_TO_POINTER(REND_FX_YUV_NEGATE));
	gtk_widget_set_halign (FiltNegateEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (FiltNegateEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_filt), FiltNegateEnable, 2, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltNegateEnable),
		(get_render_fx_mask() & REND_FX_YUV_NEGATE) >0 );
	gtk_widget_show (FiltNegateEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltNegateEnable), "toggled",
		G_CALLBACK (render_fx_filter_changed), device);
	/* Mono FX */
	GtkWidget *FiltMonoEnable = gtk_check_button_new_with_label (_(" Mono"));
	g_object_set_data (G_OBJECT (FiltMonoEnable), "filt_info", GINT_TO_POINTER(REND_FX_YUV_MONOCR));
	gtk_widget_set_halign (FiltMonoEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (FiltMonoEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_filt), FiltMonoEnable, 3, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltMonoEnable),
		(get_render_fx_mask() & REND_FX_YUV_MONOCR) > 0);
	gtk_widget_show (FiltMonoEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltMonoEnable), "toggled",
		G_CALLBACK (render_fx_filter_changed), device);

	/* Pieces FX */
	GtkWidget *FiltPiecesEnable = gtk_check_button_new_with_label (_(" Pieces"));
	g_object_set_data (G_OBJECT (FiltPiecesEnable), "filt_info", GINT_TO_POINTER(REND_FX_YUV_PIECES));
	gtk_widget_set_halign (FiltPiecesEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (FiltPiecesEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_filt), FiltPiecesEnable, 4, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltPiecesEnable),
		(get_render_fx_mask() & REND_FX_YUV_PIECES) > 0);
	gtk_widget_show (FiltPiecesEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltPiecesEnable), "toggled",
		G_CALLBACK (render_fx_filter_changed), device);

	/* Particles */
	GtkWidget *FiltParticlesEnable = gtk_check_button_new_with_label (_(" Particles"));
	g_object_set_data (G_OBJECT (FiltParticlesEnable), "filt_info", GINT_TO_POINTER(REND_FX_YUV_PARTICLES));
	gtk_widget_set_halign (FiltParticlesEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (FiltParticlesEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_filt), FiltParticlesEnable, 5, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltParticlesEnable),
		(get_render_fx_mask() & REND_FX_YUV_PARTICLES) > 0);
	gtk_widget_show (FiltParticlesEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltParticlesEnable), "toggled",
		G_CALLBACK (render_fx_filter_changed), device);


	/*add control grid to parent container*/
	gtk_container_add(GTK_CONTAINER(parent), video_controls_grid);

	return 0;
}
