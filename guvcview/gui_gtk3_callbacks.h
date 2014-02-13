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

#ifndef GUI_GTK3_CALLBACKS_H
#define GUI_GTK3_CALLBACKS_H

#include <gtk/gtk.h>
#include <glib.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>

#include "gviewv4l2core.h"

/*
 * delete event (close window)
 * args:
 *   widget - pointer to event widget
 *   event - pointe to event data
 *   data - pointer to user data
 *
 * asserts:
 *   none
 *
 * returns: error code
 */
int delete_event (GtkWidget *widget, GdkEventConfigure *event, void *data);

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
void camera_button_menu_changed (GtkWidget *item, void *data);

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
void control_defaults_clicked (GtkWidget *item, void *data);

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
void controls_profile_clicked (GtkWidget *item, void *data);

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
void pan_tilt_step_changed (GtkSpinButton *spin, void *data);

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
void button_PanTilt1_clicked (GtkButton * Button, void *data);

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
void button_PanTilt2_clicked (GtkButton * Button, void *data);

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
void button_clicked (GtkButton * Button, void *data);

/*
 * a string control apply button clicked
 * args:
 *    button - button that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    control->string is not null
 *
 * returns: none
 */
void string_button_clicked(GtkButton * Button, void *data);

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
void int64_button_clicked(GtkButton * Button, void *data);

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
void bitmask_button_clicked(GtkButton * Button, void *data);

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
void slider_changed (GtkRange * range, void *data);

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
void spin_changed (GtkSpinButton * spin, void *data);

/*
 * combo box chaged event
 * args:
 *    combo - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void combo_changed (GtkComboBox * combo, void *data);

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
void bayer_pix_ord_changed (GtkComboBox * combo, void *data);

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
void check_changed (GtkToggleButton *toggle, void *data);

/*
 * device list box changed event
 * args:
 *    wgtDevices - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void devices_changed (GtkComboBox *wgtDevices, void *data);

/*
 * frame rate list box changed event
 * args:
 *    wgtFrameRate - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void frame_rate_changed (GtkComboBox *wgtFrameRate, void *data);

/*
 * resolution list box changed event
 * args:
 *    wgtResolution - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void resolution_changed (GtkComboBox *wgtResolution, void *data);

/*
 * device pixel format list box changed event
 * args:
 *    wgtInpType - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void format_changed(GtkComboBox *wgtInpType, void *data);

/*
 * render fx filter changed event
 * args:
 *    toggle - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void render_fx_filter_changed(GtkToggleButton *toggle, void *data);

/*
 * software autofocus checkbox changed event
 * args:
 *    toggle - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void autofocus_changed (GtkToggleButton * toggle, void *data);

/*
 * software autofocus button clicked event
 * args:
 *    button - widget that generated the event
 *    data - pointer to user data
 *
 * asserts:
 *    none
 *
 * returns: none
 */
void setfocus_clicked (GtkButton * button, void *data);

#endif
