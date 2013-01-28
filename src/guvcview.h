/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
#           George Sedov <radist.morse@gmail.com>                               #
#                             Threaded encoding                                 #
#                             default action selector for Webcam button         #
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

#ifndef GUVCVIEW_H
#define GUVCVIEW_H

#include "v4l2uvc.h"
#include "sound.h"
#include "autofocus.h"
#include "video_format.h"

/* Must set this as global so they */
/* can be set from any callback.   */

struct GWIDGET
{
	/* The main window*/
	GtkWidget *mainwin;
	/* A restart Dialog */
	GtkWidget *restartdialog;
	/*Paned containers*/
	GtkWidget *maintable;
	GtkWidget *boxv;
	GtkWidget *boxh;

	//group list for menu video codecs
	GSList *vgroup;
	//group list for menu audio codecs
	GSList *agroup;

	GtkWidget *status_bar;

	GtkWidget *label_SndAPI;
	GtkWidget *SndAPI;
	GtkWidget *SndEnable;
	GtkWidget *SndSampleRate;
	GtkWidget *SndDevice;
	GtkWidget *SndNumChan;
	GtkWidget *SndComp;
	/*must be called from main loop if capture timer enabled*/
	GtkWidget *ImageFNameEntry;
	GtkWidget *ImgFileButt;
	GtkWidget *VidFileButt;
	GtkWidget *ImageType;
	GtkWidget *CapImageButt;
	GtkWidget *ImageInc;
	GtkWidget *ImageIncLabel;
	GtkWidget *CapVidButt;
	GtkWidget *VidButton_Img;
	GtkWidget *VidFNameEntry;
	GtkWidget *VidIncLabel;
	GtkWidget *VidInc;
	GtkWidget *VidFormat;
	GtkWidget *Resolution;
	GtkWidget *InpType;
	GtkWidget *FrameRate;
	GtkWidget *Devices;
	GtkWidget *FileDialog;
	GtkWidget *lavc_button;
	GtkWidget *lavc_aud_button;
	GtkWidget *jpeg_comp;
	GtkWidget *quitButton;

	GtkWidget *TakeImageByDefault;
	GtkWidget *TakeVidByDefault;

	gboolean vid_widget_state;
};

struct ALL_DATA
{
	struct paRecordData *pdata;
	struct GLOBAL *global;
	struct focusData *AFdata;
	struct vdIn *videoIn;
	struct VideoFormatData *videoF;
	struct GWIDGET *gwidget;
	struct VidState *s;
	__THREAD_TYPE video_thread;
	__THREAD_TYPE audio_thread;
	__THREAD_TYPE IO_thread;
};

#endif
