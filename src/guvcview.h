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
	/*the main loop : only needed for no_display option*/
	GMainLoop *main_loop;

	/* The main window*/
	GtkWidget *mainwin;
	/* A restart Dialog */
	GtkWidget *restartdialog;
	/*Paned containers*/
	GtkWidget *maintable;
	GtkWidget *boxh;

	//group list for menu video codecs
	GSList *vgroup;
	//group list for menu audio codecs
	GSList *agroup;

	//menu top widgets
	GtkWidget *menu_photo_top;
	GtkWidget *menu_video_top;

	GtkWidget *status_bar;

	GtkWidget *label_SndAPI;
	GtkWidget *SndAPI;
	GtkWidget *SndEnable;
	GtkWidget *SndSampleRate;
	GtkWidget *SndDevice;
	GtkWidget *SndNumChan;
	GtkWidget *SndComp;
	/*must be called from main loop if capture timer enabled*/
	GtkWidget *ImageType;
	GtkWidget *CapImageButt;
	GtkWidget *CapVidButt;
	GtkWidget *Resolution;
	GtkWidget *InpType;
	GtkWidget *FrameRate;
	GtkWidget *Devices;
	GtkWidget *jpeg_comp;
	GtkWidget *quitButton;

	gboolean vid_widget_state;
	int status_warning_id;
};

/* uvc H264 control widgets */
struct uvc_h264_gtkcontrols
{
	GtkWidget* FrameInterval;
	GtkWidget* BitRate;
	GtkWidget* Hints_res;
	GtkWidget* Hints_prof;
	GtkWidget* Hints_ratecontrol;
	GtkWidget* Hints_usage;
	GtkWidget* Hints_slicemode;
	GtkWidget* Hints_sliceunit;
	GtkWidget* Hints_view;
	GtkWidget* Hints_temporal;
	GtkWidget* Hints_snr;
	GtkWidget* Hints_spatial;
	GtkWidget* Hints_spatiallayer;
	GtkWidget* Hints_frameinterval;
	GtkWidget* Hints_leakybucket;
	GtkWidget* Hints_bitrate;
	GtkWidget* Hints_cabac;
	GtkWidget* Hints_iframe;
	GtkWidget* Resolution;
	GtkWidget* SliceUnits;
	GtkWidget* SliceMode;
	GtkWidget* Profile;
	GtkWidget* Profile_flags;
	GtkWidget* IFramePeriod;
	GtkWidget* EstimatedVideoDelay;
	GtkWidget* EstimatedMaxConfigDelay;
	GtkWidget* UsageType;
	//GtkWidget* UCConfig;
	GtkWidget* RateControlMode;
	GtkWidget* RateControlMode_cbr_flag;
	GtkWidget* TemporalScaleMode;
	GtkWidget* SpatialScaleMode;
	GtkWidget* SNRScaleMode;
	GtkWidget* StreamMuxOption;
	GtkWidget* StreamMuxOption_aux;
	GtkWidget* StreamMuxOption_mjpgcontainer;
	GtkWidget* StreamFormat;
	GtkWidget* EntropyCABAC;
	GtkWidget* Timestamp;
	GtkWidget* NumOfReorderFrames;
	GtkWidget* PreviewFlipped;
	GtkWidget* View;
	GtkWidget* StreamID;
	GtkWidget* SpatialLayerRatio;
	GtkWidget* LeakyBucketSize;
	GtkWidget* probe_button;
	GtkWidget* commit_button;
};

struct ALL_DATA
{
	struct paRecordData *pdata;
	struct GLOBAL *global;
	struct focusData *AFdata;
	struct vdIn *videoIn;
	struct VideoFormatData *videoF;
	struct GWIDGET *gwidget;
	struct uvc_h264_gtkcontrols  *h264_controls;
	struct VidState *s;
	__THREAD_TYPE video_thread;
	__THREAD_TYPE audio_thread;
	__THREAD_TYPE IO_thread;
};

#endif
