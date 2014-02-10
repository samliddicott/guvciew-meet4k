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

	int format_index = get_frame_format_index(device, device->requested_fmt);
	int resolu_index = get_format_resolution_index(
		device,
		device->requested_fmt,
		device->format.fmt.pix.width,
		device->format.fmt.pix.height);

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


	GtkWidget *wgtDevices = gtk_combo_box_text_new ();
	gtk_widget_set_halign (wgtDevices, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (wgtDevices, TRUE);
	if (device->num_devices < 1)
	{
		/*use current*/
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(wgtDevices),
			device->videodevice);
		gtk_combo_box_set_active(GTK_COMBO_BOX(wgtDevices),0);
	}
	else
	{
		for(i = 0; i < (device->num_devices); i++)
		{
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(wgtDevices),
				device->list_devices[i].name);
			if(device->list_devices[i].current)
				gtk_combo_box_set_active(GTK_COMBO_BOX(wgtDevices),i);
		}
	}
	gtk_grid_attach (GTK_GRID(video_controls_grid), wgtDevices, 1, line, 1 ,1);
	gtk_widget_show (wgtDevices);
	g_signal_connect (GTK_COMBO_BOX_TEXT(wgtDevices), "changed",
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
		printf("GUVCVIEW: frame rates of %dº resolution=%d \n",
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


	//GtkWidget *ShowFPS=gtk_check_button_new_with_label (_(" Show"));
	//gtk_grid_attach (GTK_GRID(video_controls_grid), ShowFPS, 2, line, 1, 1);

	//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ShowFPS),(FpsCount > 0));
	//gtk_widget_show (ShowFPS);
	//g_signal_connect (GTK_CHECK_BUTTON(ShowFPS), "toggled",
	//	G_CALLBACK (show_FPS_changed), device);


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

	g_object_set_data (G_OBJECT (wgtFrameRate), "control_fps", wgtResolution);

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

	g_object_set_data (G_OBJECT (wgtFrameRate), "control_fps", wgtInpType);
	g_object_set_data (G_OBJECT (wgtResolution), "control_resolution", wgtInpType);

	g_signal_connect (GTK_COMBO_BOX_TEXT(wgtInpType), "changed",
		G_CALLBACK (format_changed), device);




	/*add control grid to parent container*/
	gtk_container_add(GTK_CONTAINER(parent), video_controls_grid);

	return 0;
}