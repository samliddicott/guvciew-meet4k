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

#ifndef GLOBALS_H
#define GLOBALS_H

#include "defs.h"
#include "utils.h"

typedef struct _sndDev 
{
	int id;
	int chan;
	int samprate;
	//PaTime Hlatency;
	//PaTime Llatency;
} sndDev;

/*global variables used in guvcview*/
struct GLOBAL 
{
	int debug;
	char *videodevice;
	int stack_size;
	char *confPath;
	short flg_config; /*flag confPath if set in args*/
	pchar* aviFPath;
	pchar* imgFPath;
	short  flg_imgFPath; /*flag imgFPath if set in args*/
	char *avifile; /*avi filename passed through argument options with -n */
	pchar* profile_FPath;
	char *WVcaption; /*video preview title bar caption*/
	char *imageinc_str; /*label for File inc*/
	char *aviinc_str; /*label for File inc*/
	int vid_sleep;
	int Capture_time; /*avi capture time passed through argument options with -t */
	short lprofile; /* flag for command line -l option */ 
	int imgFormat;
	int AVIFormat; /*0-"MJPG"  1-"YUY2" 2-"DIB "(rgb32)*/
	ULONG AVI_MAX_LEN;
	DWORD snd_begintime;/*begin time for audio capture*/
	DWORD currtime;
	DWORD lasttime;
	DWORD AVIstarttime;
	DWORD AVIstoptime;
	DWORD avi_inc;/*avi name increment*/
	DWORD framecount;
	unsigned char frmrate;
	short Sound_enable; /*Enable Sound by Default*/
	int Sound_SampRate;
	int Sound_SampRateInd;
	int Sound_numInputDev;
	sndDev Sound_IndexDev[20]; /*up to 20 input devices (should be alocated dinamicly like controls)*/
	int Sound_DefDev; 
	int Sound_UseDev;
	int Sound_NumChan;
	int Sound_NumChanInd;
	int Sound_NumSec;
	int Sound_Format;
	int Sound_bitRate; /* bit rate for mpeg compression */
     
	int PanStep;/*step angle for Pan*/
	int TiltStep;/*step angle for Tilt*/
	float DispFps;
	DWORD frmCount;
	short FpsCount;
	short flg_FpsCount;
	int timer_id;/*fps count timer*/
	int image_timer_id;/*auto image capture timer*/
	int image_timer;/*auto image capture time*/
	DWORD image_inc;/*image name increment*/
	int image_npics;/*number of captures*/
	short flg_npics; /*flag npics if set in args*/
	int fps;
	int fps_num;
	int bpp; //current bytes per pixel
	int hwaccel; //use hardware acceleration
	short flg_hwaccel; /*flag hwaccel if set in args*/
	int grabmethod;//default mmap(1) or read(0)
	int width;
	int height;
	short flg_res; /*flag resol if set in args*/
	int winwidth;
	int winheight;
	int boxvsize;
	int spinbehave; /*spin: 0-non editable 1-editable*/
	char *mode; /*jpg (default) or yuv*/
	short flg_mode; /*flag mode if set in args*/
	int format;
	int formind; /*0-MJPG 1-YUYV*/
	int Frame_Flags;
	BYTE *jpeg;
	int   jpeg_size;
	unsigned int   jpeg_bufsize; /* width*height/2 */
	int autofocus;/*some autofocus flags*/
	int AFcontrol;
}   __attribute__ ((packed));

/*----------------------------- prototypes ------------------------------------*/
int initGlobals(struct GLOBAL *global);

int closeGlobals(struct GLOBAL *global);


#endif

