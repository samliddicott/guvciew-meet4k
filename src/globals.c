/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
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

#include "globals.h"
#include "avilib.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/types.h>
#include <linux/videodev.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>


int initGlobals (struct GLOBAL *global) 
{
	global->mutex = g_mutex_new();

	global->debug = DEBUG;
	
	const gchar *home = g_get_home_dir();

	global->videodevice = g_strdup("/dev/video0");
	
	global->confPath = g_strjoin("/", home, ".guvcviewrc", NULL);
	
	global->aviFPath = g_new(pchar, 2);
	
	global->imgFPath = g_new(pchar, 2);
	
	global->profile_FPath = g_new(pchar, 2);
	
	global->aviFPath[1] = g_strdup(home);
	
	global->imgFPath[1] = g_strdup(home);
	
	global->profile_FPath[1] = g_strdup(home);
	
	global->aviFPath[0] = g_strdup(DEFAULT_AVI_FNAME);
	
	global->imgFPath[0] = g_strdup(DEFAULT_IMAGE_FNAME);
	
	global->profile_FPath[0] = g_strdup("default.gpfl");
	
	global->WVcaption = g_new(char, 32);
	
	g_snprintf(global->WVcaption,10,"GUVCVIdeo");
	
	global->stack_size=TSTACK;
	
	global->image_inc=0;

	global->imageinc_str = g_new(char, 25);
	g_snprintf(global->imageinc_str,20,_("File num:%d"),global->image_inc);
	
	global->aviinc_str = g_new(char, 25);
	g_snprintf(global->aviinc_str,20,_("File num:%d"),global->avi_inc);
	
	global->vid_sleep=0;
	global->avifile=NULL; /*avi filename passed through argument options with -n */
	global->Capture_time=0; /*avi capture time passed through argument options with -t */
	global->lprofile=0; /* flag for -l command line option*/
	global->imgFormat=0; /* 0 -JPG 1-BMP 2-PNG*/
	global->AVIFormat=0; /*0-"MJPG"  1-"YUY2" 2-"DIB "(rgb32)*/ 
	global->AVI_MAX_LEN=AVI_MAX_SIZE; /* ~2 Gb*/    
	global->snd_begintime=0;/*begin time for audio capture*/
	global->currtime=0;
	global->lasttime=0;
	global->AVIstarttime=0;
	global->AVIstoptime=0;
	global->framecount=0;
	global->frmrate=5;
	global->Sound_enable=1; /*Enable Sound by Default*/
	global->Sound_SampRate=SAMPLE_RATE;
	global->Sound_SampRateInd=0;
	global->Sound_numInputDev=0;
	global->Sound_Format=PA_FOURCC;
	global->Sound_DefDev=0; 
	global->Sound_UseDev=0;
	global->Sound_NumChan=NUM_CHANNELS;
	global->Sound_NumChanInd=0;
	global->Sound_NumSec=NUM_SECONDS;
	global->Sound_bitRate=160; /*160 Kbps = 20000 Bps*/
	
	global->FpsCount=0;

	global->timer_id=0;
	global->image_timer_id=0;
	global->image_timer=0;
	global->image_npics=999;/*default max number of captures*/
	global->frmCount=0;
	global->PanStep=2;/*2 degree step for Pan*/
	global->TiltStep=2;/*2 degree step for Tilt*/
	global->DispFps=0;
	global->fps = DEFAULT_FPS;
	global->fps_num = DEFAULT_FPS_NUM;
	global->bpp = 0; //current bytes per pixel
	global->hwaccel = 1; //use hardware acceleration
	global->grabmethod = 1;//default mmap(1) or read(0)
	global->width = DEFAULT_WIDTH;
	global->height = DEFAULT_HEIGHT;
	global->winwidth=WINSIZEX;
	global->winheight=WINSIZEY;
	global->spinbehave=0;
	global->boxvsize=0;
	
	global->mode = g_new(char, 6);
	g_snprintf(global->mode, 5, "mjpg");
	
	global->format = V4L2_PIX_FMT_MJPEG;
	global->formind = 0;
	global->Frame_Flags = YUV_NOFILT;
	global->jpeg=NULL;
	global->jpeg_size = 0;
	/* reset with videoIn parameters */
	global->jpeg_bufsize = 0;
	global->autofocus = 0;
	global->AFcontrol = 0;
	
	return (0);
}

int closeGlobals(struct GLOBAL *global)
{
	g_free(global->videodevice);
	g_free(global->confPath);
	g_free(global->aviFPath[1]);
	g_free(global->imgFPath[1]);
	g_free(global->imgFPath[0]);
	g_free(global->aviFPath[0]);
	g_free(global->profile_FPath[1]);
	g_free(global->profile_FPath[0]);
	g_free(global->aviFPath);
	g_free(global->imgFPath);
	g_free(global->profile_FPath);
	g_free (global->WVcaption);
	g_free(global->imageinc_str);
	g_free(global->aviinc_str);
	g_free(global->avifile);
	g_free(global->mode);
	g_mutex_free( global->mutex );
	
	global->videodevice=NULL;
	global->confPath=NULL;
	global->avifile=NULL;
	global->mode=NULL;
	g_free(global->jpeg);
	global->jpeg=NULL;
	g_free(global);
	global=NULL;
	return (0);
}
