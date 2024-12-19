/*******************************************************************************#
#           uvc Meet4K support for OBSBOT Meet 4K                               #
#       for guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Sam Liddicott <sam@liddicott.com>                                   #
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
#include <math.h>
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

#include "uvc_meet4k.h"


extern int debug_level;
extern int is_control_panel;

/*
 * Meet4k control widgets
 */
GtkWidget *BackgroundMode = NULL;
GtkWidget *CameraAngle = NULL;
GtkWidget *Background = NULL;

/*
 * meet4K effect callback
 * args:
 *   combo - widget that caused the event
 *   data  - user data
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void meet4k_camera_effect_changed(GtkComboBox *combo, void *data)
{
	uint8_t camera_effect = (uint8_t) (gtk_combo_box_get_active (combo));
	meet4kcore_set_camera_effect(get_v4l2_device_handler(), camera_effect);
}

/*
 * meet4K camera angle mode callback
 * args:
 *   combo - widget that caused the event
 *   data  - user data
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void meet4k_camera_angle_changed(GtkComboBox *combo, void *data)
{
	uint8_t camera_angle = (uint8_t) (gtk_combo_box_get_active (combo));

	meet4kcore_set_camera_angle(get_v4l2_device_handler(), camera_angle);
}

/*
 * meet4K background mode mode callback
 * args:
 *   combo - widget that caused the event
 *   data  - user data
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void meet4k_background_changed(GtkComboBox *combo, void *data)
{
	uint8_t bg_mode = *(gtk_combo_box_get_active_id(combo));
	meet4kcore_set_bg_mode(get_v4l2_device_handler(), bg_mode);
    if (1 != gtk_combo_box_get_active(GTK_COMBO_BOX(BackgroundMode)))
        gtk_combo_box_set_active(GTK_COMBO_BOX(BackgroundMode), 1);
}

void meet4k_blur_level_changed(GtkRange *range, void *data)
{
	uint8_t blur = (uint8_t) gtk_range_get_value (range);
	meet4kcore_set_blur_level(get_v4l2_device_handler(), blur);
    if (2 != gtk_combo_box_get_active(GTK_COMBO_BOX(Background)))
      gtk_combo_box_set_active(GTK_COMBO_BOX(Background), 2);
}

/*
 * meet4K hdr mode callback
 * args:
 *   combo - widget that caused the event
 *   data  - user data
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void meet4k_hdr_mode_changed(GtkToggleButton *toggle, void *data)
{
	//uint8_t hdr_mode = (uint8_t) (GPOINTER_TO_INT(g_object_get_data (G_OBJECT (toggle), "meet4k_hdr_mode")));
	uint8_t hdr_mode = gtk_toggle_button_get_active(toggle)?1:0;
	meet4kcore_set_hdr_mode(get_v4l2_device_handler(), hdr_mode);
}

/*
 * meet4K face ae mode mode callback
 * args:
 *   combo - widget that caused the event
 *   data  - user data
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void meet4k_face_ae_mode_changed(GtkToggleButton *toggle, void *data)
{
	uint8_t ae_mode = gtk_toggle_button_get_active(toggle)?1:0;
	meet4kcore_set_face_ae_mode(get_v4l2_device_handler(), ae_mode);
}

/*
 * meet4K face nr mode mode callback
 * args:
 *   combo - widget that caused the event
 *   data  - user data
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void meet4k_nr_mode_changed(GtkToggleButton *toggle, void *data)
{
	uint8_t nr_mode = gtk_toggle_button_get_active(toggle)?1:0;
	meet4kcore_set_nr_mode(get_v4l2_device_handler(), nr_mode);
}

void meet4k_button_rotate_changed(GtkToggleButton *toggle, void *data)
{
	uint8_t rotate_mode = gtk_toggle_button_get_active(toggle)?1:0;
	meet4kcore_set_button_mode(get_v4l2_device_handler(), rotate_mode);
}

/*
 * update controls from commit probe data
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: none
 */
//static void update_meet4k_controls()

int gui_attach_gtk3_meet4kctrls (GtkWidget *parent)
{
	/*assertions*/
	assert(parent != NULL);

	if(debug_level > 1)
		printf("GUVCVIEW (Gtk3): attaching Meet4k controls\n");

	GtkWidget *meet4k_controls_grid = gtk_grid_new();
	gtk_widget_show (meet4k_controls_grid);

	gtk_grid_set_column_homogeneous (GTK_GRID(meet4k_controls_grid), FALSE);
	gtk_widget_set_hexpand (meet4k_controls_grid, TRUE);
	gtk_widget_set_halign (meet4k_controls_grid, GTK_ALIGN_FILL);

	gtk_grid_set_row_spacing (GTK_GRID(meet4k_controls_grid), 4);
	gtk_grid_set_column_spacing (GTK_GRID (meet4k_controls_grid), 4);
	gtk_container_set_border_width (GTK_CONTAINER (meet4k_controls_grid), 2);

	int line = 0;

	/* Camera effect */
	line++;

	GtkWidget* label_BackgroundMode = gtk_label_new(_("Virtual Background Mode:"));
#if GTK_VER_AT_LEAST(3,15)
	gtk_label_set_xalign(GTK_LABEL(label_BackgroundMode), 1);
	gtk_label_set_yalign(GTK_LABEL(label_BackgroundMode), 0.5);
#else
	gtk_misc_set_alignment (GTK_MISC (label_BackgroundMode), 1, 0.5);
#endif
	gtk_grid_attach (GTK_GRID(meet4k_controls_grid), label_BackgroundMode, 0, line, 1, 1);
	gtk_widget_show (label_BackgroundMode);

	uint8_t min_backgroundmode = 0;
	uint8_t max_backgroundmode = 3;


	BackgroundMode = gtk_combo_box_text_new();
	if(max_backgroundmode >= 1 && min_backgroundmode < 2)
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(BackgroundMode),
										_("OFF"));
	if(max_backgroundmode >= 2 && min_backgroundmode < 3)
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(BackgroundMode),
										_("Virtual"));

	if(max_backgroundmode >= 3 && min_backgroundmode < 4)
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(BackgroundMode),
										_("Track"));

	uint8_t cur_BackgroundMode = meet4kcore_get_camera_effect(get_v4l2_device_handler());
	int BackgroundMode_index = cur_BackgroundMode;
	if(BackgroundMode_index < 0)
		BackgroundMode_index = 0;

	gtk_combo_box_set_active(GTK_COMBO_BOX(BackgroundMode), BackgroundMode_index);

	//connect signal
	g_signal_connect (GTK_COMBO_BOX_TEXT(BackgroundMode), "changed",
			G_CALLBACK (meet4k_camera_effect_changed), NULL);

	gtk_grid_attach (GTK_GRID(meet4k_controls_grid), BackgroundMode, 1, line, 1 ,1);
	gtk_widget_show (BackgroundMode);

	/* Camera angle */
	line++;

	GtkWidget* label_CameraAngle = gtk_label_new(_("Camera Angle:"));
#if GTK_VER_AT_LEAST(3,15)
	gtk_label_set_xalign(GTK_LABEL(label_CameraAngle), 1);
	gtk_label_set_yalign(GTK_LABEL(label_CameraAngle), 0.5);
#else
	gtk_misc_set_alignment (GTK_MISC (label_CameraAngle), 1, 0.5);
#endif
	gtk_grid_attach (GTK_GRID(meet4k_controls_grid), label_CameraAngle, 0, line, 1, 1);
	gtk_widget_show (label_CameraAngle);

	uint8_t min_CameraAngle = 0;
	uint8_t max_CameraAngle = 3;


	CameraAngle = gtk_combo_box_text_new();
	if(max_CameraAngle >= 1 && min_CameraAngle < 2)
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(CameraAngle),
										_("86 degrees"));
	if(max_CameraAngle >= 2 && min_CameraAngle < 3)
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(CameraAngle),
										_("78 degrees"));

	if(max_CameraAngle >= 3 && min_CameraAngle < 4)
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT(CameraAngle),
										_("65 degrees"));

	uint8_t cur_CameraAngle = meet4kcore_get_camera_angle(get_v4l2_device_handler());
	int CameraAngle_index = cur_CameraAngle;
	if(CameraAngle_index < 0)
		CameraAngle_index = 0;

	gtk_combo_box_set_active(GTK_COMBO_BOX(CameraAngle), CameraAngle_index);

	//connect signal
	g_signal_connect (GTK_COMBO_BOX_TEXT(CameraAngle), "changed",
			G_CALLBACK (meet4k_camera_angle_changed), NULL);

	gtk_grid_attach (GTK_GRID(meet4k_controls_grid), CameraAngle, 1, line, 1 ,1);
	gtk_widget_show (CameraAngle);

	/* Camera bg mode */
	line++;

	GtkWidget* label_Background = gtk_label_new(_("Background:"));
#if GTK_VER_AT_LEAST(3,15)
	gtk_label_set_xalign(GTK_LABEL(label_Background), 1);
	gtk_label_set_yalign(GTK_LABEL(label_Background), 0.5);
#else
	gtk_misc_set_alignment (GTK_MISC (label_Background), 1, 0.5);
#endif
	gtk_grid_attach (GTK_GRID(meet4k_controls_grid), label_Background, 0, line, 1, 1);
	gtk_widget_show (label_Background);

	uint8_t min_Background = 0;
	uint8_t max_Background = 3;


	Background = gtk_combo_box_text_new();
	if(max_Background >= 1 && min_Background < 2)
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(Background),
									"\x01", _("Color"));
	if(max_Background >= 2 && min_Background < 3)
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(Background),
									"\x11", _("Image"));
	if(max_Background >= 3 && min_Background < 4)
		gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(Background),
									"\x12", _("Blur"));

	char cur_Background[] = { (char)meet4kcore_get_bg_mode(get_v4l2_device_handler()), 0};
	gtk_combo_box_set_active_id(GTK_COMBO_BOX(Background), cur_Background);

	//connect signal
	g_signal_connect (GTK_COMBO_BOX_TEXT(Background), "changed",
			G_CALLBACK (meet4k_background_changed), NULL);

	gtk_grid_attach (GTK_GRID(meet4k_controls_grid), Background, 1, line, 1 ,1);
	gtk_widget_show (Background);

	/*blur*/
	line++;

	GtkWidget *label_Blur = gtk_label_new(_("Blur:"));
#if GTK_VER_AT_LEAST(3,15)
	gtk_label_set_xalign(GTK_LABEL(label_Blur), 1);
	gtk_label_set_yalign(GTK_LABEL(label_Blur), 0.5);
#else
	gtk_misc_set_alignment (GTK_MISC (label_Blur), 1, 0.5);
#endif

	gtk_grid_attach (GTK_GRID(meet4k_controls_grid), label_Blur, 0, line, 1, 1);
	gtk_widget_show (label_Blur);

	uint8_t blur_level = meet4kcore_get_blur_level(get_v4l2_device_handler());

	if(debug_level > 2)
		printf("GUVCVIEW: blur level is set to %d\n", blur_level);

	GtkAdjustment *blur_adjustment =  gtk_adjustment_new (
		(double)blur_level,
		0.0,
		64.0,
		1.0,
		10.0,
		0.0);

	GtkWidget *blur_scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, blur_adjustment);
	gtk_scale_set_digits(GTK_SCALE(blur_scale), 3);
	gtk_scale_set_value_pos(GTK_SCALE(blur_scale), GTK_POS_RIGHT);
	gtk_widget_set_halign (blur_scale, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (blur_scale, TRUE);
	gtk_grid_attach(GTK_GRID(meet4k_controls_grid), blur_scale, 1, line, 1, 1);
	gtk_widget_show (blur_scale);

	gtk_widget_set_sensitive (blur_scale, TRUE);

	g_signal_connect (GTK_SCALE(blur_scale), "value-changed",
		G_CALLBACK (meet4k_blur_level_changed), NULL);

	/* modes grid*/
	line++;
	GtkWidget *table_modes = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID (table_modes), 4);
	gtk_grid_set_column_spacing(GTK_GRID (table_modes), 4);
	gtk_container_set_border_width(GTK_CONTAINER (table_modes), 4);
	gtk_widget_set_size_request(table_modes, -1, -1);

	gtk_widget_set_halign(table_modes, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand(table_modes, TRUE);
	gtk_grid_attach (GTK_GRID(meet4k_controls_grid), table_modes, 0, line, 3, 1);
	gtk_widget_show (table_modes);

	/* HDR Mode */
	GtkWidget *HDRModeEnable = gtk_check_button_new_with_label (_("HDR Mode"));
	g_object_set_data (G_OBJECT (HDRModeEnable), "meet4k_hdr_mode", GINT_TO_POINTER(1));
	gtk_widget_set_halign (HDRModeEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (HDRModeEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_modes), HDRModeEnable, 1, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(HDRModeEnable), meet4kcore_get_hdr_mode(get_v4l2_device_handler()));
	gtk_widget_show (HDRModeEnable);
	g_signal_connect (GTK_CHECK_BUTTON(HDRModeEnable), "toggled", G_CALLBACK (meet4k_hdr_mode_changed), NULL);

	/* AE Mode */
	GtkWidget *AEModeEnable = gtk_check_button_new_with_label (_("Face Enhance"));
	g_object_set_data (G_OBJECT (AEModeEnable), "meet4k_ae_mode", GINT_TO_POINTER(1));
	gtk_widget_set_halign (AEModeEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (AEModeEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_modes), AEModeEnable, 2, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(AEModeEnable), meet4kcore_get_face_ae_mode(get_v4l2_device_handler()));
	gtk_widget_show (AEModeEnable);
	g_signal_connect (GTK_CHECK_BUTTON(AEModeEnable), "toggled", G_CALLBACK (meet4k_face_ae_mode_changed), NULL);

	/* Button Mode */
	GtkWidget *ButtonRotateEnable = gtk_check_button_new_with_label (_("Button Rotate"));
	g_object_set_data (G_OBJECT (ButtonRotateEnable), "meet4k_nr_mode", GINT_TO_POINTER(1));
	gtk_widget_set_halign (ButtonRotateEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (ButtonRotateEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_modes), ButtonRotateEnable, 3, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ButtonRotateEnable), meet4kcore_get_button_mode(get_v4l2_device_handler()));
	gtk_widget_show (ButtonRotateEnable);
	g_signal_connect (GTK_CHECK_BUTTON(ButtonRotateEnable), "toggled", G_CALLBACK (meet4k_button_rotate_changed), NULL);

	/* NR Mode */
	GtkWidget *NRModeEnable = gtk_check_button_new_with_label (_("Noise Reduction (audio)"));
	g_object_set_data (G_OBJECT (NRModeEnable), "meet4k_nr_mode", GINT_TO_POINTER(1));
	gtk_widget_set_halign (NRModeEnable, GTK_ALIGN_FILL);
	gtk_widget_set_hexpand (NRModeEnable, TRUE);
	gtk_grid_attach(GTK_GRID(table_modes), NRModeEnable, 4, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(NRModeEnable), meet4kcore_get_nr_mode(get_v4l2_device_handler()));
	gtk_widget_show (NRModeEnable);
	g_signal_connect (GTK_CHECK_BUTTON(NRModeEnable), "toggled", G_CALLBACK (meet4k_nr_mode_changed), NULL);



	gtk_widget_show(meet4k_controls_grid);

	/*add control grid to parent container*/
	gtk_container_add(GTK_CONTAINER(parent), meet4k_controls_grid);

//	gui_gtk3_update_controls_state();


	return 0;
}

