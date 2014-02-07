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

/* The main window*/
static GtkWidget *main_window = NULL;

static GtkWidget *status_bar = NULL;
static int status_warning_id = 0;

/*
 * adds a message to the status bar
 * args:
 *    message - message string
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void set_status_message_gtk3(const char *message)
{

}

/*
 * GUI initialization
 * args:
 *   device - pointer to device data we want to attach the gui for
 *   gui - gui API to use (GUI_NONE, GUI_GTK3, ...)
 *   width - window width
 *   height - window height
 *
 * asserts:
 *   device is not null
 *
 * returns: error code (0 -OK)
 */
int gui_attach_gtk3(v4l2_dev_t *device, int width, int height)
{
	/*asserts*/
	assert(device != NULL);

	if(!gtk_init_check(NULL, NULL))
	{
		fprintf(stderr, "GUVCVIEW: (GUI) Gtk3 can't open display\n");
		return -1;
	}

	g_set_application_name(_("Guvcview Video Capture"));

	/* make sure the type is realized so that we can change the properties*/
	g_type_class_unref (g_type_class_ref (GTK_TYPE_BUTTON));
	/* make sure gtk-button-images property is set to true (defaults to false in karmic)*/
	g_object_set (gtk_settings_get_default (), "gtk-button-images", TRUE, NULL);

	/* Create a main window */
	main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (main_window), _("Guvcview"));

	/* get screen resolution */
	GdkScreen* screen = NULL;
	screen = gtk_window_get_screen(GTK_WINDOW(main_window));
	int desktop_width = gdk_screen_get_width(screen);
	int desktop_height = gdk_screen_get_height(screen);

	if(debug_level > 0)
		printf("GUVCVIEW: (GUI) Screen resolution is (%d x %d)\n", desktop_width, desktop_height);

	if((width > desktop_width) && (desktop_width > 0))
		width = desktop_width;
	if((height > desktop_height) && (desktop_height > 0))
		height = desktop_height;

	gtk_window_resize(GTK_WINDOW(main_window), width, height);

	/* Add delete event handler */
	g_signal_connect(GTK_WINDOW(main_window), "delete_event", G_CALLBACK(delete_event), NULL);

	/*----------------------- Main table --------------------------------------*/
	GtkWidget *maintable = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_widget_show (maintable);

	/*tab box container*/
	GtkWidget *tab_box = gtk_notebook_new();
	gtk_widget_show (tab_box);

	GtkWidget *scroll_1 = gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(scroll_1), GTK_CORNER_TOP_LEFT);
	gtk_widget_show(scroll_1);

	/*
	 * viewport is only needed for gtk < 3.8
	 * for 3.8 and above controls tab can be directly added to scroll1
	 */
	GtkWidget* viewport = gtk_viewport_new(NULL,NULL);
	gtk_widget_show(viewport);

	gtk_container_add(GTK_CONTAINER(scroll_1), viewport);

	/*----------------------- Top Menu ----------------------------------------*/

	gui_attach_gtk3_menu(device, maintable);

	/*----------------------- Image controls Tab ------------------------------*/

	gui_attach_gtk3_v4l2ctrls(device, viewport);

	GtkWidget *tab_1 = gtk_grid_new();
    GtkWidget *tab_1_label = gtk_label_new(_("Image Controls"));
	gtk_widget_show (tab_1_label);
	/** check for files */
	gchar *tab_1_icon_path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/image_controls.png",NULL);
	/** don't test for file - use default empty image if load fails */
	/** get icon image*/
	GtkWidget *tab_1_icon = gtk_image_new_from_file(tab_1_icon_path);
	g_free(tab_1_icon_path);
	gtk_widget_show (tab_1_icon);
	gtk_grid_attach (GTK_GRID(tab_1), tab_1_icon, 0, 0, 1, 1);
	gtk_grid_attach (GTK_GRID(tab_1), tab_1_label, 1, 0, 1, 1);

	gtk_widget_show (tab_1);

	gtk_notebook_append_page(GTK_NOTEBOOK(tab_box), scroll_1, tab_1);



	/** Attach the notebook (tabs) */
	gtk_box_pack_start(GTK_BOX(maintable), tab_box, TRUE, TRUE, 2);

	/*-------------------------- Status bar ------------------------------------*/
	status_bar = gtk_statusbar_new();
	status_warning_id = gtk_statusbar_get_context_id (GTK_STATUSBAR(status_bar), "warning");

    gtk_widget_show(status_bar);
	/** add the status bar*/
	gtk_box_pack_start(GTK_BOX(maintable), status_bar, FALSE, FALSE, 2);


	/* attach to main window container */
	gtk_container_add (GTK_CONTAINER (main_window), maintable);

	gtk_widget_show (main_window);

	return 0;
}

/*
 * run the GUI loop
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int gui_run_gtk3()
{

	int ret = 0;

	gtk_main();

	return ret;
}

/*
 * closes and cleans the GTK3 GUI
 * args:
 *   none
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void gui_close_gtk3()
{
	gtk_main_quit();
}