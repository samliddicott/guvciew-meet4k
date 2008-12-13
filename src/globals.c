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
	global->debug = DEBUG;
	
	gchar *home = g_strdup(g_get_home_dir());
	
	if((global->videodevice = (char *) calloc(16, sizeof(char)))==NULL)
	{
		printf("couldn't calloc memory for:global->videodevice\n");
		goto error;
	}
	
	g_snprintf(global->videodevice, 15, "/dev/video0");
	
	global->confPath = g_strjoin("/", home, ".guvcviewrc", NULL);
	
	if((global->aviFPath = (pchar *) calloc(2, sizeof(pchar)))==NULL)
	{
		printf("couldn't calloc memory for:global->aviFPath\n");
		goto error;
	}
	if((global->imgFPath = (pchar *) calloc(2, sizeof(pchar)))==NULL)
	{
		printf("couldn't calloc memory for:global->imgFPath\n");
		goto error;
	}
	if((global->profile_FPath = (pchar *) calloc(2, sizeof(pchar)))==NULL)
	{
		printf("couldn't calloc memory for:global->profile_FPath\n");
		goto error;
	}
	
	global->aviFPath[1] = g_strdup(home);
	
	global->imgFPath[1] = g_strdup(home);
	
	global->profile_FPath[1] = g_strdup(home);
	
	global->aviFPath[0] = g_strdup(DEFAULT_AVI_FNAME);
	
	global->imgFPath[0] = g_strdup(DEFAULT_IMAGE_FNAME);
	
	global->profile_FPath[0] = g_strdup("default.gpfl");
	
	
	if((global->WVcaption= (char *) calloc(32, sizeof(char)))==NULL)
	{
		printf("couldn't calloc memory for:global->WVcaption\n");
		goto error;
	}
	g_snprintf(global->WVcaption,10,"GUVCVIdeo");
	
	global->stack_size=TSTACK;
	
	global->image_inc=0;

	if((global->imageinc_str= (char *) calloc(25, sizeof(char)))==NULL)
	{
		printf("couldn't calloc memory for:global->imageinc_str\n");
		goto error;
	}
	g_snprintf(global->imageinc_str,20,_("File num:%d"),global->image_inc);
	
	if((global->aviinc_str= (char *) calloc(25, sizeof(char)))==NULL)
	{
		printf("couldn't calloc memory for:global->aviinc_str\n");
		goto error;
	}
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
	global->Sound_Format=WAVE_FORMAT_PCM;
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
	if((global->mode = (char *) calloc(6, sizeof(char)))==NULL)
	{
		printf("couldn't calloc memory for:global->mode\n");
		goto error;
	}
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
	
	if (home!= NULL) free(home);
	return (0);
	
error:
	return(-1); /*no mem should exit*/

}

int closeGlobals(struct GLOBAL *global)
{
	if (global->videodevice) free(global->videodevice);
	if (global->confPath) free(global->confPath);
	if (global->aviFPath[1]) free(global->aviFPath[1]);
	if (global->imgFPath[1]) free(global->imgFPath[1]);
	if (global->imgFPath[0]) free(global->imgFPath[0]);
	if (global->aviFPath[0]) free(global->aviFPath[0]);
	if (global->profile_FPath[1]) free(global->profile_FPath[1]);
	if (global->profile_FPath[0]) free(global->profile_FPath[0]);
	if (global->aviFPath) free(global->aviFPath);
	if (global->imgFPath) free(global->imgFPath);
	if (global->profile_FPath) free(global->profile_FPath);
	if (global->WVcaption) free (global->WVcaption);
	if (global->imageinc_str) free(global->imageinc_str);
	if (global->aviinc_str) free(global->aviinc_str);
	if (global->avifile) free(global->avifile);
	if (global->mode) free(global->mode);
	global->videodevice=NULL;
	global->confPath=NULL;
	global->avifile=NULL;
	global->mode=NULL;
	if(global->jpeg) free (global->jpeg);
	global->jpeg=NULL;
	free(global);
	global=NULL;
	return (0);
}
