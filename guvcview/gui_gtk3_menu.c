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
 * attach top menu widget
 * args:
 *   device - pointer to device data we want to attach the gui for
 *   parent - menu parent widget
 *
 * asserts:
 *   device is not null
 *   parent is not null
 *
 * returns: error code (0 -OK)
 */
int gui_attach_gtk3_menu(v4l2_dev_t *device, GtkWidget *parent)
{
	/*assertions*/
	assert(device != NULL);
	assert(parent != NULL);

	GtkWidget *menubar = gtk_menu_bar_new();
	gtk_menu_bar_set_pack_direction(GTK_MENU_BAR(menubar), GTK_PACK_DIRECTION_LTR);

	GtkWidget *controls_menu = gtk_menu_new();
	GtkWidget *controls_top = gtk_menu_item_new_with_label(_("Settings"));
	GtkWidget *controls_load = gtk_menu_item_new_with_label(_("Load Profile"));
	GtkWidget *controls_save = gtk_menu_item_new_with_label(_("Save Profile"));
	GtkWidget *controls_default = gtk_menu_item_new_with_label(_("Hardware Defaults"));

	gtk_widget_show (controls_top);
	gtk_widget_show (controls_load);
	gtk_widget_show (controls_save);
	gtk_widget_show (controls_default);

	GtkWidget *camera_button_menu = gtk_menu_new();
	GtkWidget *camera_button_top = gtk_menu_item_new_with_label(_("Camera Button"));

	GSList *camera_button_group = NULL;
	GtkWidget *def_image = gtk_radio_menu_item_new_with_label(camera_button_group, _("Capture Image"));
	g_object_set_data (G_OBJECT (def_image), "camera_default", GINT_TO_POINTER(0));
	gtk_menu_shell_append(GTK_MENU_SHELL(camera_button_menu), def_image);

	camera_button_group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (def_image));
	GtkWidget *def_video = gtk_radio_menu_item_new_with_label(camera_button_group, _("Capture Video"));
	g_object_set_data (G_OBJECT (def_video), "camera_default", GINT_TO_POINTER(1));
	gtk_menu_shell_append(GTK_MENU_SHELL(camera_button_menu), def_video);

	gtk_widget_show (camera_button_top);
	gtk_widget_show (def_image);
	gtk_widget_show (def_video);

	/*default camera button action*/
	if (get_default_camera_button_action() == 0)
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (def_image), TRUE);
	else
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (def_video), TRUE);

	g_signal_connect (GTK_RADIO_MENU_ITEM(def_image), "toggled",
		G_CALLBACK (camera_button_menu_changed), NULL);
	g_signal_connect (GTK_RADIO_MENU_ITEM(def_video), "toggled",
		G_CALLBACK (camera_button_menu_changed), NULL);

	/* profile events*/
	g_object_set_data (G_OBJECT (controls_save), "profile_dialog", GINT_TO_POINTER(1));
	g_signal_connect (GTK_MENU_ITEM(controls_save), "activate",
		G_CALLBACK (controls_profile_clicked), device);
	g_object_set_data (G_OBJECT (controls_load), "profile_dialog", GINT_TO_POINTER(0));
	g_signal_connect (GTK_MENU_ITEM(controls_load), "activate",
		G_CALLBACK (controls_profile_clicked), device);
	g_signal_connect (GTK_MENU_ITEM(controls_default), "activate",
		G_CALLBACK (control_defaults_clicked), device);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(controls_top), controls_menu);
	gtk_menu_shell_append(GTK_MENU_SHELL(controls_menu), controls_load);
	gtk_menu_shell_append(GTK_MENU_SHELL(controls_menu), controls_save);
	gtk_menu_shell_append(GTK_MENU_SHELL(controls_menu), controls_default);
	gtk_menu_shell_append(GTK_MENU_SHELL(controls_menu), camera_button_top);
	gtk_menu_shell_append(GTK_MENU_SHELL(menubar), controls_top);

	/*control_only exclusion */



	/*show the menu*/
	gtk_widget_show (menubar);
	gtk_container_set_resize_mode (GTK_CONTAINER(menubar), GTK_RESIZE_PARENT);

	/* Attach the menu to parent container*/
	gtk_box_pack_start(GTK_BOX(parent), menubar, FALSE, TRUE, 2);

	return 0;
}