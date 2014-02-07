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

#ifndef GUI_GTK3_H
#define GUI_GTK3_H

#include <gtk/gtk.h>
#include <glib.h>
#include <assert.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>

#include "gviewv4l2core.h"
#include "video_capture.h"
#include "gui.h"
#include "gui_gtk3.h"

extern int debug_level;

/*
 * delete event (close window)
 * args:
 *   widget - pointer to event widget
 *   event - pointer to event data
 *   data - pointer to user data
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int delete_event (GtkWidget *widget, GdkEventConfigure *event, void *data)
{
	/* Terminate program */
	gui_close();
	return 0;
}

/*
 * camera_button_menu toggled event
 * args:
 *   widget - pointer to event widget
 *   data - pointer to user data
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void camera_button_menu_changed (GtkWidget *item, void *data)
{
	int flag = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (item), "camera_default"));

	if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item)))
		set_default_camera_button_action(flag);
}

/*
 * control default clicked event
 * args:
 *   widget - pointer to event widget
 *   data - pointer to user data
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void control_defaults_clicked (GtkWidget *item, void *data)
{
    v4l2_dev_t *device = (v4l2_dev_t *) data;

    set_v4l2_control_defaults(device);
}

/*
 * control profile (load/save) clicked event
 * args:
 *   widget - pointer to event widget
 *   data - pointer to user data
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void controls_profile_clicked (GtkWidget *item, void *data)
{
	v4l2_dev_t *device = (v4l2_dev_t *) data;

	GtkWidget *FileDialog;

	int save = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (item), "profile_dialog"));

	if(debug_level > 0)
		printf("GUVCVIEW: Profile dialog (%d)\n", save);

	if (save > 0)
	{
		FileDialog = gtk_file_chooser_dialog_new (_("Save Profile"),
			GTK_WINDOW(get_main_window()),
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
		gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (FileDialog), TRUE);

		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (FileDialog),
			get_profile_name());
	}
	else
	{
		FileDialog = gtk_file_chooser_dialog_new (_("Load Profile"),
			GTK_WINDOW(get_main_window()),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);
	}

	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (FileDialog),
		get_profile_path());

	if (gtk_dialog_run (GTK_DIALOG (FileDialog)) == GTK_RESPONSE_ACCEPT)
	{
		/*Save Controls Data*/
		char *filename= gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (FileDialog));
		//global->profile_FPath=splitPath(filename,global->profile_FPath);

		//if(save > 0)
		//	SaveControls(all_data);
		//else
		//	LoadControls(all_data);
	}
	gtk_widget_destroy (FileDialog);
}

/*
 * pan/tilt step changed
 * args:
 *    spin - spinbutton that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns:
 *    none
 */
void pan_tilt_step_changed (GtkSpinButton *spin, void *data)
{
	v4l2_dev_t *device = (v4l2_dev_t *) data;

	int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (spin), "control_info"));

	int val = gtk_spin_button_get_value_as_int (spin);

	if(id == V4L2_CID_PAN_RELATIVE)
		device->pan_step = val;
	if(id == V4L2_CID_TILT_RELATIVE)
		device->tilt_step = val;
}

/*
 * Pan Tilt button 1 clicked
 * args:
 *    button - button that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void button_PanTilt1_clicked (GtkButton * Button, void *data)
{
    v4l2_dev_t *device = (v4l2_dev_t *) data;

    int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (Button), "control_info"));

    v4l2_ctrl_t *control = get_v4l2_control_by_id(device, id);

	if(id == V4L2_CID_PAN_RELATIVE)
		control->value =  device->pan_step;
	else
		control->value =  device->tilt_step;

    if(set_v4l2_control_id_value(device, id))
		fprintf(stderr, "GUVCVIEW: error setting pan/tilt\n");
}

/*
 * Pan Tilt button 2 clicked
 * args:
 *    button - button that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void button_PanTilt2_clicked (GtkButton * Button, void *data)
{
    v4l2_dev_t *device = (v4l2_dev_t *) data;

    int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (Button), "control_info"));

    v4l2_ctrl_t *control = get_v4l2_control_by_id(device, id);

    if(id == V4L2_CID_PAN_RELATIVE)
		control->value =  - device->pan_step;
	else
		control->value =  - device->tilt_step;

    if(set_v4l2_control_id_value(device, id))
		fprintf(stderr, "GUVCVIEW: error setting pan/tilt\n");
}

/*
 * generic button clicked
 * args:
 *    button - button that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void button_clicked (GtkButton * Button, void *data)
{
    v4l2_dev_t *device = (v4l2_dev_t *) data;

    int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (Button), "control_info"));

    v4l2_ctrl_t *control = get_v4l2_control_by_id(device, id);

	control->value = 1;

    if(set_v4l2_control_id_value(device, id))
		fprintf(stderr, "GUVCVIEW: error setting button value\n");
}

/*
 * a string control apply button clicked
 * args:
 *    button - button that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    control->string not null
 *
 * returns: none
 */
void string_button_clicked(GtkButton * Button, void *data)
{
	v4l2_dev_t *device = (v4l2_dev_t *) data;

	int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (Button), "control_info"));
	GtkWidget *entry = (GtkWidget *) g_object_get_data (G_OBJECT (Button), "control_entry");

	v4l2_ctrl_t *control = get_v4l2_control_by_id(device, id);

	assert(control->string != NULL);

	strncpy(control->string, gtk_entry_get_text(GTK_ENTRY(entry)), control->control.maximum);

	if(set_v4l2_control_id_value(device, id))
		fprintf(stderr, "GUVCVIEW: error setting string value\n");
}

/*
 * a int64 control apply button clicked
 * args:
 *    button - button that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void int64_button_clicked(GtkButton * Button, void *data)
{
	v4l2_dev_t *device = (v4l2_dev_t *) data;

	int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (Button), "control_info"));
	GtkWidget *entry = (GtkWidget *) g_object_get_data (G_OBJECT (Button), "control_entry");

	v4l2_ctrl_t *control = get_v4l2_control_by_id(device, id);

	char* text_input = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	text_input = g_strstrip(text_input);
	if( g_str_has_prefix(text_input,"0x")) //hex format
	{
		text_input = g_strcanon(text_input,"0123456789ABCDEFabcdef", '');
		control->value64 = g_ascii_strtoll(text_input, NULL, 16);
	}
	else //decimal or hex ?
	{
		text_input = g_strcanon(text_input,"0123456789ABCDEFabcdef", '');
		control->value64 = g_ascii_strtoll(text_input, NULL, 0);
	}
	g_free(text_input);

	if(set_v4l2_control_id_value(device, id))
		fprintf(stderr, "GUVCVIEW: error setting string value\n");

}

/*
 * a bitmask control apply button clicked
 * args:
 *    button - button that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void bitmask_button_clicked(GtkButton * Button, void *data)
{
	v4l2_dev_t *device = (v4l2_dev_t *) data;

	int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (Button), "control_info"));
	GtkWidget *entry = (GtkWidget *) g_object_get_data (G_OBJECT (Button), "control_entry");

	v4l2_ctrl_t *control = get_v4l2_control_by_id(device, id);

	char* text_input = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry)));
	text_input = g_strcanon(text_input,"0123456789ABCDEFabcdef", '');
	control->value = (int32_t) g_ascii_strtoll(text_input, NULL, 16);
	g_free(text_input);

	if(set_v4l2_control_id_value(device, id))
		fprintf(stderr, "GUVCVIEW: error setting string value\n");
}

/*
 * slider changed event
 * args:
 *    range - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void slider_changed (GtkRange * range, void *data)
{
    v4l2_dev_t *device = (v4l2_dev_t *) data;

    int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (range), "control_info"));
    v4l2_ctrl_t *control = get_v4l2_control_by_id(device, id);

    int val = (int) gtk_range_get_value (range);

    control->value = val;

    if(set_v4l2_control_id_value(device, id))
		fprintf(stderr, "GUVCVIEW: error setting slider value\n");

   /*
    if(widget2)
    {
        //disable widget signals
        g_signal_handlers_block_by_func(GTK_SPIN_BUTTON(widget2),
            G_CALLBACK (spin_changed), data);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON(widget2), control->value);
        //enable widget signals
        g_signal_handlers_unblock_by_func(GTK_SPIN_BUTTON(widget2),
            G_CALLBACK (spin_changed), data);
    }
	*/
}

/*
 * spin changed event
 * args:
 *    spin - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void spin_changed (GtkSpinButton * spin, void *data)
{
	v4l2_dev_t *device = (v4l2_dev_t *) data;

    int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (range), "control_info"));
    v4l2_ctrl_t *control = get_v4l2_control_by_id(device, id);

	int val = gtk_spin_button_get_value_as_int (spin);
    control->value = val;

     if(set_v4l2_control_id_value(device, id))
		fprintf(stderr, "GUVCVIEW: error setting spin value\n");

	/*
    if(widget)
    {
        //disable widget signals
        g_signal_handlers_block_by_func(GTK_SCALE (widget),
            G_CALLBACK (slider_changed), data);
        gtk_range_set_value (GTK_RANGE (widget), control->value);
        //enable widget signals
        g_signal_handlers_unblock_by_func(GTK_SCALE (widget),
            G_CALLBACK (slider_changed), data);
    }
	*/
}

/*
 * combo box changed event
 * args:
 *    combo - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void combo_changed (GtkComboBox * combo, void *data)
{
	v4l2_dev_t *device = (v4l2_dev_t *) data;

    int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (combo), "control_info"));
    v4l2_ctrl_t *control = get_v4l2_control_by_id(device, id);

    int index = gtk_combo_box_get_active (combo);
    control->value = control->menu[index].index;

	if(set_v4l2_control_id_value(device, id))
		fprintf(stderr, "GUVCVIEW: error setting menu value\n");
}

/*
 * bayer pixel order combo box changed event
 * args:
 *    combo - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void bayer_pix_ord_changed (GtkComboBox * combo, void *data)
{
	v4l2_dev_t *device = (v4l2_dev_t *) data;

	//int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (combo), "control_info"));

	int index = gtk_combo_box_get_active (combo);
	device->bayer_pix_order = index;
}

/*
 * check box changed event
 * args:
 *    toggle - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void check_changed (GtkToggleButton *toggle, void *data)
{
    v4l2_dev_t *device = (v4l2_dev_t *) data;

    int id = GPOINTER_TO_INT(g_object_get_data (G_OBJECT (toggle), "control_info"));
    v4l2_ctrl_t *control = get_v4l2_control_by_id(device, id);

    int val = gtk_toggle_button_get_active (toggle) ? 1 : 0;

    control->value = val;

	if(set_v4l2_control_id_value(device, id))
		fprintf(stderr, "GUVCVIEW: error setting menu value\n");

    if(id == V4L2_CID_DISABLE_PROCESSING_LOGITECH)
    {
        if (control->value > 0)
			device->isbayer = 1;
        else
			device->isbayer = 0;

        /*must restart stream for changes to take effect*/
        stop_video_stream(device);
        start_video_stream(device);
    }
}

#endif