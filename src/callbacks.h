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

#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <gtk/gtk.h>
#include "guvcview.h"

#ifndef GDK_KEY_WebCam
#define GDK_KEY_WebCam 0x1008ff8f
#endif

void
WARN_DIALOG(const char *warn_title, const char* warn_msg, struct ALL_DATA *all_data);

void
ERR_DIALOG(const char *err_title, const char* err_msg, struct ALL_DATA *all_data);

void
file_chooser (GtkWidget * FileButt, struct ALL_DATA *all_data);

void
lavc_properties(GtkMenuItem * codec_prop, struct ALL_DATA *all_data);

void
lavc_audio_properties(GtkMenuItem * codec_prop, struct ALL_DATA *all_data);

/* sound controls*/
void
set_sensitive_snd_contrls (const int flag, struct GWIDGET *gwidget);

/*video controls*/
void
set_sensitive_vid_contrls (const int flag, const int sndEnable, struct GWIDGET *gwidget);

gint
delete_event (GtkWidget *widget, GdkEventConfigure *event, void *data);

void
set_sensitive_img_contrls (const int flag, struct GWIDGET *gwidget);

gboolean
key_pressed (GtkWidget *win, GdkEventKey *event, struct ALL_DATA *all_data);

void
vidClose (struct ALL_DATA *all_data);

void
slider_changed (GtkRange * range, struct ALL_DATA *all_data);

void
spin_changed (GtkSpinButton * spin, struct ALL_DATA *all_data);

void
set_jpeg_comp_clicked (GtkButton * jpeg_comp, struct ALL_DATA *all_data);

void
autofocus_changed (GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
check_changed (GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
pix_ord_changed (GtkComboBox * combo, struct ALL_DATA *all_data);

void
combo_changed (GtkComboBox * combo, struct ALL_DATA *all_data);

void
button_clicked (GtkButton * Button, struct ALL_DATA *all_data);

void
setfocus_clicked (GtkButton * FocusButton, struct ALL_DATA *all_data);

void
button_PanTilt1_clicked (GtkButton * Button, struct ALL_DATA *all_data);

void
button_PanTilt2_clicked (GtkButton * Button, struct ALL_DATA *all_data);

void
quitButton_clicked (GtkButton * quitButton, struct ALL_DATA *all_data);

void
Devices_changed (GtkComboBox * Devices, struct ALL_DATA *all_data);

void
resolution_changed (GtkComboBox * Resolution, struct ALL_DATA *all_data);

void
InpType_changed(GtkComboBox * ImpType, struct ALL_DATA *all_data);

void
FrameRate_changed (GtkComboBox * FrameRate, struct ALL_DATA *all_data);

void
SndSampleRate_changed (GtkComboBox * SampleRate, struct ALL_DATA *all_data);

void
SndAPI_changed (GtkComboBox * SoundAPI, struct ALL_DATA *all_data);

void
SndDevice_changed (GtkComboBox * SoundDevice, struct ALL_DATA *all_data);

void
SndNumChan_changed (GtkComboBox * SoundChan, struct ALL_DATA *all_data);

void
AudCodec_menu_changed (GtkRadioMenuItem *acodec_item, struct ALL_DATA *all_data);

void
VidCodec_menu_changed (GtkRadioMenuItem *vcodec_item, struct ALL_DATA *all_data);

void
SndEnable_changed (GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
FiltEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
osdChanged(GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
EffEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
image_prefix_toggled(GtkWidget * toggle, struct ALL_DATA *all_data);

void
video_prefix_toggled(GtkWidget * toggle, struct ALL_DATA *all_data);

void
capture_image (GtkButton *ImageButt, struct ALL_DATA *all_data);

void
capture_vid (GtkToggleButton *VidButt, struct ALL_DATA *all_data);

void
camera_button_menu_changed (GtkWidget *item, struct ALL_DATA *all_data);

void
Profile_clicked (GtkWidget *item, struct ALL_DATA *all_data);

void
Defaults_clicked (GtkWidget *item, struct ALL_DATA *all_data);

void
ShowFPS_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data);

gboolean 
image_capture_callback (gpointer data);

gboolean 
video_capture_callback (gpointer data);

#endif
