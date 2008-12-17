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

#ifndef GUVCVIEW_H
#define GUVCVIEW_H

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

/* Must set this as global so they */
/* can be set from any callback.   */

struct GWIDGET 
{
	/* The main window*/
	GtkWidget *mainwin;
	/* A restart Dialog */
	GtkWidget *restartdialog;
	/*Paned containers*/
	GtkWidget *boxv;
	GtkWidget *boxh;

	GtkWidget *AVIComp;
	GtkWidget *SndEnable; 
	GtkWidget *SndSampleRate;
	GtkWidget *SndDevice;
	GtkWidget *SndNumChan;
	GtkWidget *SndComp;
	/*must be called from main loop if capture timer enabled*/
	GtkWidget *ImageFNameEntry;
	GtkWidget *ImgFileButt;
	GtkWidget *ImageType;
	GtkWidget *CapImageButt;
	//GtkWidget *QCapImageButt;
	GtkWidget *ImageInc;
	GtkWidget *ImageIncLabel;
	GtkWidget *CapAVIButt;
	//GtkWidget *QCapAVIButt;
	GtkWidget *AVIFNameEntry;
	GtkWidget *AVIIncLabel;
	GtkWidget *AVIInc;
	GtkWidget *Resolution;

	GtkWidget *FileDialog;
}   __attribute__ ((packed));

struct ALL_DATA 
{
	struct paRecordData *pdata;
	struct GLOBAL *global;
	struct focusData *AFdata;
	struct vdIn *videoIn;
	struct avi_t *AviOut;
	struct GWIDGET *gwidget;
	struct VidState *s;
	gchar *EXEC_CALL;
	GThread *video_thread;
}   __attribute__ ((packed));

#endif
