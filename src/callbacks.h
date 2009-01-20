/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <pthread.h>
#include <SDL/SDL.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <SDL/SDL_syswm.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include "../config.h"

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <portaudio.h>

#include "v4l2uvc.h"
#include "avilib.h"
#include "globals.h"
#include "colorspaces.h"
#include "sound.h"
#include "jpgenc.h"
#include "autofocus.h"
#include "picture.h"
#include "ms_time.h"
#include "string_utils.h"
#include "options.h"
#include "guvcview.h"
#include "video.h"
#include "mp2.h"
#include "profile.h"
#include "close.h"
#include "timers.h"

void 
ERR_DIALOG(const char *err_title, const char* err_msg, struct ALL_DATA *all_data);

gint
delete_event (GtkWidget *widget, GdkEventConfigure *event, void *data);

void 
set_sensitive_snd_contrls (const int flag, struct GWIDGET *gwidget);

void 
set_sensitive_avi_contrls (const int flag, const int sndEnable, struct GWIDGET *gwidget);

void 
set_sensitive_img_contrls (const int flag, struct GWIDGET *gwidget);

void
aviClose (struct ALL_DATA *all_data);

void
slider_changed (GtkRange * range, struct ALL_DATA *all_data);

void
spin_changed (GtkSpinButton * spin, struct ALL_DATA *all_data);

void
autofocus_changed (GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
check_changed (GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
reversePan_changed (GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
bayer_changed (GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
pix_ord_changed (GtkComboBox * combo, struct ALL_DATA *all_data);

void
combo_changed (GtkComboBox * combo, struct ALL_DATA *all_data);

void
setfocus_clicked (GtkButton * FocusButton, struct ALL_DATA *all_data);

void
PanLeft_clicked (GtkButton * PanLeft, struct ALL_DATA *all_data);

void
PanRight_clicked (GtkButton * PanRight, struct ALL_DATA *all_data);

void
TiltUp_clicked (GtkButton * TiltUp, struct ALL_DATA *all_data);

void
TiltDown_clicked (GtkButton * TiltDown, struct ALL_DATA *all_data);

void
PReset_clicked (GtkButton * PReset, struct ALL_DATA *all_data);

void
TReset_clicked (GtkButton * PTReset, struct ALL_DATA *all_data);

void
PTReset_clicked (GtkButton * PTReset, struct ALL_DATA *all_data);

void
quitButton_clicked (GtkButton * quitButton, struct ALL_DATA *all_data);

void 
Devices_changed (GtkComboBox * Devices, struct ALL_DATA *all_data);

void
resolution_changed (GtkComboBox * Resolution, struct ALL_DATA *all_data);

void
ImpType_changed(GtkComboBox * ImpType, struct ALL_DATA *all_data);

void
FrameRate_changed (GtkComboBox * FrameRate, struct ALL_DATA *all_data);

void
SndSampleRate_changed (GtkComboBox * SampleRate, struct ALL_DATA *all_data);

void
ImageType_changed (GtkComboBox * ImageType, struct ALL_DATA *all_data);

void
SndDevice_changed (GtkComboBox * SoundDevice, struct ALL_DATA *all_data);

void
SndNumChan_changed (GtkComboBox * SoundChan, struct ALL_DATA *all_data);

void
SndComp_changed (GtkComboBox * SoundComp, struct ALL_DATA *all_data);

void
AVIComp_changed (GtkComboBox * AVIComp, struct ALL_DATA *all_data);

void
SndEnable_changed (GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
FiltMirrorEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
FiltUpturnEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
FiltNegateEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
FiltMonoEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
EffEchoEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
EffFuzzEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
EffRevEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
EffWahEnable_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data);

void 
ImageInc_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
AVIInc_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data);

void
capture_image (GtkButton *ImageButt, struct ALL_DATA *all_data);

void
capture_avi (GtkButton *AVIButt, struct ALL_DATA *all_data);

void
SProfileButton_clicked (GtkButton * SProfileButton, struct ALL_DATA *all_data);

void
LProfileButton_clicked (GtkButton * LProfileButton, struct ALL_DATA *all_data);

void *
split_avi(void *data);

void 
ShowFPS_changed(GtkToggleButton * toggle, struct ALL_DATA *all_data);

#endif
