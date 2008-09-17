/*******************************************************************************#
#	    guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#										#
# This program is free software; you can redistribute it and/or modify         	#
# it under the terms of the GNU General Public License as published by   	#
# the Free Software Foundation; either version 2 of the License, or           	#
# (at your option) any later version.                                          	#
#                                                                              	#
# This program is distributed in the hope that it will be useful,              	#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             	#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 	#
#                                                                              	#
# You should have received a copy of the GNU General Public License           	#
# along with this program; if not, write to the Free Software                  	#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	#
#                                                                              	#
********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
#include <pthread.h>
#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>
#include <SDL/SDL_audio.h>
#include <SDL/SDL_timer.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <SDL/SDL_syswm.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include "../config.h"



#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <portaudio.h>

#include "v4l2uvc.h"
#include "avilib.h"

#include "prototype.h"
#include "autofocus.h"

/*----------------------------- globals --------------------------------------*/
struct GLOBAL *global=NULL;
struct JPEG_ENCODER_STRUCTURE *jpeg_struct=NULL;
struct focusData *AFdata=NULL;

struct vdIn *videoIn=NULL;
VidState * s;

/* The main window*/
GtkWidget *mainwin;
/* A restart Dialog */
GtkWidget *restartdialog;
/*Paned containers*/
GtkWidget *boxv;
GtkWidget *boxh;

/* Must set this as global so they */
/* can be set from any callback.   */
/* When AVI is in capture mode we  */
/* can't change settings           */
GtkWidget *AVIComp;
GtkWidget *SndEnable; 
GtkWidget *SndSampleRate;
GtkWidget *SndDevice;
GtkWidget *SndNumChan;
GtkWidget *FiltMirrorEnable;
GtkWidget *FiltUpturnEnable;
GtkWidget *FiltNegateEnable;
GtkWidget *FiltMonoEnable;
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
GtkWidget *FileDialog;

/*thread definitions*/
pthread_t mythread;
pthread_attr_t attr;

pthread_t sndthread;
pthread_attr_t sndattr;

/* parameters passed when restarting*/
char *EXEC_CALL=NULL;
/*avi structure used by libavi*/
avi_t *AviOut=NULL;


/*exposure menu for old type controls */
static const char *exp_typ[]={"Manual Mode",
	                      "Auto Mode",
	                      "Shutter Priority Mode",
	                      "Aperture Priority Mode"};

/*defined at end of file*/
/*remove build warning  */ 
static void clean_struct (void);
static void shutd (gint restart); 

/*------------------------------ get time ------------------------------------*/
static DWORD
ms_time (void)
{
   static struct timeval tod;
   gettimeofday (&tod, NULL);
   return ((DWORD) tod.tv_sec * 1000.0 + (DWORD) tod.tv_usec / 1000.0);
}
/*--------------------------- check image file extension -----------------------------*/
static int 
check_image_type (char *filename) {
	
	char str_ext[3];
	/*get the file extension*/
	sscanf(filename,"%*[^.].%3c",str_ext);
	/* change image type */
	int somExt = str_ext[0]+str_ext[1]+str_ext[2];
	switch (somExt) {
		/* there are 8 variations we will check for 3*/
		case ('j'+'p'+'g'):
		case ('J'+'P'+'G'):
		case ('J'+'p'+'g'):
			global->imgFormat=0;
			break;
		case ('b'+'m'+'p'):	
		case ('B'+'M'+'P'):
		case ('B'+'m'+'p'):
			global->imgFormat=1;
			break;
		case ('p'+'n'+'g'):			
		case ('P'+'N'+'G'):		
		case ('P'+'n'+'g'):
			global->imgFormat=2;
			break;
	      	case ('r'+'a'+'w'):
	      	case ('R'+'A'+'W'):
	      	case ('R'+'a'+'w'):
	      		global->imgFormat=3;
		 	break;
		default: /* use jpeg as default*/
			global->imgFormat=0;
	}

	return (global->imgFormat);	
}

/*---------------------------- error message dialog-----------------------------*/
static void 
ERR_DIALOG(const char *err_title, const char* err_msg) {
     
    
    GtkWidget *errdialog;
    errdialog = gtk_message_dialog_new (GTK_WINDOW(mainwin),
					GTK_DIALOG_DESTROY_WITH_PARENT,
                               		GTK_MESSAGE_ERROR,
                               		GTK_BUTTONS_CLOSE,
                               		gettext (err_title));
   
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(errdialog),gettext (err_msg));
    
    gtk_widget_show(errdialog);
    gtk_dialog_run (GTK_DIALOG (errdialog));
    gtk_widget_destroy (errdialog);
    if (global) closeGlobals (global);
    if (EXEC_CALL) free(EXEC_CALL);
    printf("Terminated.\n");;
    exit(1);

};

/*--------------------------- controls enable/disable --------------------------*/

/* sound controls*/
static void 
set_sensitive_snd_contrls (const int flag){
	gtk_widget_set_sensitive (SndSampleRate, flag);
	gtk_widget_set_sensitive (SndDevice, flag);
	gtk_widget_set_sensitive (SndNumChan, flag);
}

/*avi controls*/
static void 
set_sensitive_avi_contrls (const int flag){
	/*sound and avi compression controls*/
	gtk_widget_set_sensitive (AVIComp, flag);
	gtk_widget_set_sensitive (SndEnable, flag); 
	if(global->Sound_enable > 0) {	 
		set_sensitive_snd_contrls(flag);
	}		
}

/*image controls*/
static void 
set_sensitive_img_contrls (const int flag){
	gtk_widget_set_sensitive(ImgFileButt, flag);/*image butt File chooser*/
	gtk_widget_set_sensitive(ImageType, flag);/*file type combo*/
	gtk_widget_set_sensitive(ImageFNameEntry, flag);/*Image Entry*/
	gtk_widget_set_sensitive(ImageInc, flag);/*image inc checkbox*/
}

/*----------------------- write conf (.guvcviewrc) file ----------------------*/
static int 
writeConf(const char *confpath) {
	int ret=0;
	FILE *fp;
	if ((fp = fopen(confpath,"w"))!=NULL) {
		fprintf(fp,"# guvcview configuration file\n\n");
		fprintf(fp,"# video device\n");
		fprintf(fp,"video_device=%s\n",global->videodevice);
		fprintf(fp,"# Thread stack size: default 128 pages of 64k = 8388608 bytes\n");
		fprintf(fp,"stack_size=%d\n",global->stack_size);
		fprintf(fp,"# video loop sleep time in ms: 0,1,2,3,...\n");
		fprintf(fp,"# increased sleep time -> less cpu load, more droped frames\n");
		fprintf(fp,"vid_sleep=%i\n",global->vid_sleep);
		fprintf(fp,"# video resolution \n");
		fprintf(fp,"resolution=%ix%i\n",global->width,global->height);
		fprintf(fp,"# control window size: default %ix%i\n",WINSIZEX,WINSIZEY);
		fprintf(fp,"windowsize=%ix%i\n",global->winwidth,global->winheight);
		fprintf(fp,"#vertical pane size\n");
		fprintf(fp,"vpane=%i\n",global->boxvsize);
		fprintf(fp,"#spin button behavior: 0-non editable 1-editable\n");
		fprintf(fp,"spinbehave=%i\n", global->spinbehave);
		fprintf(fp,"# mode video format 'yuv' or 'jpg'(default)\n");
		fprintf(fp,"mode=%s\n",global->mode);
		fprintf(fp,"# frames per sec. - hardware supported - default( %i )\n",DEFAULT_FPS);
		fprintf(fp,"fps=%d/%d\n",global->fps_num,global->fps);
		fprintf(fp,"#Display Fps counter: 1- Yes 0- No\n");
		fprintf(fp,"fps_display=%i\n",global->FpsCount);
		fprintf(fp,"#auto focus (continuous): 1- Yes 0- No\n");
		fprintf(fp,"auto_focus=%i\n",global->autofocus);
		fprintf(fp,"# bytes per pixel: default (0 - current)\n");
		fprintf(fp,"bpp=%i\n",global->bpp);
		fprintf(fp,"# hardware accelaration: 0 1 (default - 1)\n");
		fprintf(fp,"hwaccel=%i\n",global->hwaccel);
		fprintf(fp,"# video grab method: 0 -read 1 -mmap (default - 1)\n");
		fprintf(fp,"grabmethod=%i\n",global->grabmethod);
		fprintf(fp,"# video compression format: 0-MJPG 1-YUY2 2-DIB (BMP 24)\n");
		fprintf(fp,"avi_format=%i\n",global->AVIFormat);
		fprintf(fp,"# avi file max size (default %d bytes)\n",AVI_MAX_SIZE);
		fprintf(fp,"avi_max_len=%li\n",global->AVI_MAX_LEN);
		fprintf(fp,"# sound 0 - disable 1 - enable\n");
		fprintf(fp,"sound=%i\n",global->Sound_enable);
		fprintf(fp,"# snd_device - sound device id as listed by portaudio\n");
		fprintf(fp,"snd_device=%i\n",global->Sound_UseDev);
		fprintf(fp,"# snd_samprate - sound sample rate\n");
		fprintf(fp,"snd_samprate=%i\n",global->Sound_SampRateInd);
		fprintf(fp,"# snd_numchan - sound number of channels 0- dev def 1 - mono 2 -stereo\n");
		fprintf(fp,"snd_numchan=%i\n",global->Sound_NumChanInd);
		fprintf(fp,"#snd_numsec - avi audio blocks size in sec: 1,2,3,.. \n");
		fprintf(fp,"# more seconds = more granularity, more memory allocation but less disc I/O\n");
		fprintf(fp,"snd_numsec=%i\n",global->Sound_NumSec);
		fprintf(fp,"# snd_buf_fact - audio buffer size = audio block frames total size x snd_buf_fact\n");
		fprintf(fp,"snd_buf_fact=%i\n",global->Sound_BuffFactor);
		fprintf(fp,"#Pan Step in degrees, Default=2\n");
		fprintf(fp,"Pan_Step=%i\n",global->PanStep);
		fprintf(fp,"#Tilt Step in degrees, Default=2\n");
		fprintf(fp,"Tilt_Step=%i\n",global->TiltStep);
		fprintf(fp,"# video filters: 0 -none 1- flip 2- upturn 4- negate 8- mono (add the ones you want)\n");
		fprintf(fp,"frame_flags=%i\n",global->Frame_Flags);
		fprintf(fp,"# Image capture Full Path: Path (Max 100 characters) Filename (Max 20 characters)\n");
		fprintf(fp,"image_path=%s/%s\n",global->imgFPath[1],global->imgFPath[0]);
		fprintf(fp,"# Auto Image naming (filename-n.ext)\n");
		fprintf(fp,"image_inc=%d\n",global->image_inc);
		fprintf(fp,"# Avi capture Full Path Path (Max 100 characters) Filename (Max 20 characters)\n");
		fprintf(fp,"avi_path=%s/%s\n",global->aviFPath[1],global->aviFPath[0]);
		fprintf(fp,"# control profiles Full Path Path (Max 10 characters) Filename (Max 20 characters)\n");
		fprintf(fp,"profile_path=%s/%s\n",global->profile_FPath[1],global->profile_FPath[0]);
		printf("write %s OK\n",confpath);
		fclose(fp);
	} else {
	printf("Could not write file %s \n Please check file permissions\n",confpath);
	ret=1;
	}
	return ret;
}
/*----------------------- read conf (.guvcviewrc) file -----------------------*/
static int 
readConf(const char *confpath) {
	int ret=1;
	char variable[16];
	char value[128];

	int i=0;

	FILE *fp;

	if((fp = fopen(confpath,"r"))!=NULL) {
		char line[144];
	while (fgets(line, 144, fp) != NULL) {
		if ((line[0]=='#') || (line[0]==' ') || (line[0]=='\n')) {
			/*skip*/
		} else if ((i=sscanf(line,"%[^#=]=%[^#\n ]",variable,value))==2){
			/* set variables */
			if (strcmp(variable,"video_device")==0) {
				snprintf(global->videodevice,15,"%s",value);
			} else if (strcmp(variable,"stack_size")==0) {
				sscanf(value,"%i",&(global->stack_size));
			} else if (strcmp(variable,"vid_sleep")==0) {
				sscanf(value,"%i",&(global->vid_sleep));
			} else if (strcmp(variable,"resolution")==0) {
				sscanf(value,"%ix%i",&(global->width),&(global->height));			
			} else if (strcmp(variable,"windowsize")==0) {
				sscanf(value,"%ix%i",&(global->winwidth),&(global->winheight));
			} else if (strcmp(variable,"vpane")==0) { 
				sscanf(value,"%i",&(global->boxvsize));
			} else if (strcmp(variable,"spinbehave")==0) { 
				sscanf(value,"%i",&(global->spinbehave));
			} else if (strcmp(variable,"mode")==0) {
				snprintf(global->mode,5,"%s",value);
			} else if (strcmp(variable,"fps")==0) {
				sscanf(value,"%i/%i",&(global->fps_num),&(global->fps));
			} else if (strcmp(variable,"fps_display")==0) { 
				sscanf(value,"%hi",&(global->FpsCount));
			} else if (strcmp(variable,"auto_focus")==0) { 
				sscanf(value,"%i",&(global->autofocus));
			} else if (strcmp(variable,"bpp")==0) {
				sscanf(value,"%i",&(global->bpp));
			} else if (strcmp(variable,"hwaccel")==0) {
				sscanf(value,"%i",&(global->hwaccel));
			} else if (strcmp(variable,"grabmethod")==0) {
				sscanf(value,"%i",&(global->grabmethod));
			} else if (strcmp(variable,"avi_format")==0) {
				sscanf(value,"%i",&(global->AVIFormat));
			} else if (strcmp(variable,"avi_max_len")==0) {
				sscanf(value,"%li",&(global->AVI_MAX_LEN));
			    	AVI_set_MAX_LEN (global->AVI_MAX_LEN);
			} else if (strcmp(variable,"sound")==0) {
				sscanf(value,"%hi",&(global->Sound_enable));
			} else if (strcmp(variable,"snd_device")==0) {
				sscanf(value,"%i",&(global->Sound_UseDev));
			} else if (strcmp(variable,"snd_samprate")==0) {
				sscanf(value,"%i",&(global->Sound_SampRateInd));
			} else if (strcmp(variable,"snd_numchan")==0) {
				sscanf(value,"%i",&(global->Sound_NumChanInd));
			} else if (strcmp(variable,"snd_numsec")==0) {
				sscanf(value,"%i",&(global->Sound_NumSec));
			} else if (strcmp(variable,"snd_buf_fact")==0) {
				sscanf(value,"%hi",&(global->Sound_BuffFactor));
			} else if (strcmp(variable,"Pan_Step")==0){ 
				sscanf(value,"%i",&(global->PanStep));
			} else if (strcmp(variable,"Tilt_Step")==0){ 
				sscanf(value,"%i",&(global->TiltStep));
			} else if (strcmp(variable,"frame_flags")==0) {
				sscanf(value,"%i",&(global->Frame_Flags));
			} else if (strcmp(variable,"image_path")==0) {
				global->imgFPath = splitPath(value,global->imgFPath);
				/*get the file type*/
				global->imgFormat = check_image_type(global->imgFPath[0]);
			} else if (strcmp(variable,"image_inc")==0) {
				sscanf(value,"%d",&(global->image_inc));
			} else if (strcmp(variable,"avi_path")==0) {
				global->aviFPath=splitPath(value,global->aviFPath);
			} else if (strcmp(variable,"profile_path")==0) {
				global->profile_FPath=splitPath(value,global->profile_FPath);
			}
		}    
		}
		fclose(fp);
	    	if (global->debug) { /*it will allways be FALSE unless DEBUG=1*/
			printf("video_device: %s\n",global->videodevice);
			printf("vid_sleep: %i\n",global->vid_sleep);
			printf("resolution: %i x %i\n",global->width,global->height);
			printf("windowsize: %i x %i\n",global->winwidth,global->winheight);
			printf("vert pane: %i\n",global->boxvsize);
			printf("spin behavior: %i\n",global->spinbehave);
			printf("mode: %s\n",global->mode);
			printf("fps: %i/%i\n",global->fps_num,global->fps);
			printf("Display Fps: %i\n",global->FpsCount);
			printf("bpp: %i\n",global->bpp);
			printf("hwaccel: %i\n",global->hwaccel);
			printf("grabmethod: %i\n",global->grabmethod);
			printf("avi_format: %i\n",global->AVIFormat);
			printf("sound: %i\n",global->Sound_enable);
			printf("sound Device: %i\n",global->Sound_UseDev);
			printf("sound samp rate: %i\n",global->Sound_SampRateInd);
			printf("sound Channels: %i\n",global->Sound_NumChanInd);
			printf("Sound Block Size: %i seconds\n",global->Sound_NumSec);
			printf("sound Buffer Factor: %i\n",global->Sound_BuffFactor);
			printf("Pan Step: %i degrees\n",global->PanStep);
			printf("Tilt Step: %i degrees\n",global->TiltStep);
			printf("Video Filter Flags: %i\n",global->Frame_Flags);
			printf("image inc: %d\n",global->image_inc);
			printf("profile(default):%s/%s\n",global->profile_FPath[1],global->profile_FPath[0]);
		}
	} else {
		printf("Could not open %s for read,\n will try to create it\n",confpath);
		ret=writeConf(confpath);
	}
	return ret;
}

/*------------------------- read command line options ------------------------*/
static void
readOpts(int argc,char *argv[]) {
	
	int i=0;
	char *separateur;
	char *sizestring = NULL;
	
	for (i = 1; i < argc; i++) {
	
		/* skip bad arguments */
		if (argv[i] == NULL || *argv[i] == 0 || *argv[i] != '-') {
			continue;
		}
	    	if (strcmp(argv[i], "--verbose") == 0) {
			global->debug=1; /*debug mode*/
		}
	    	
		if ((strcmp(argv[i], "-d") == 0) || (strcmp(argv[i], "--device") == 0)) {
			if (i + 1 >= argc || *argv[i+1] =='-') {
				printf("No parameter specified for device, using default.\n");
			} else {
				snprintf(global->videodevice,15,"%s",argv[i + 1]);
			}
		}
		if (strcmp(argv[i], "-g") == 0) {
			/* Ask for read instead default  mmap */
			global->grabmethod = 0;
		}
		if (strcmp(argv[i], "-w") == 0) {
			if ( i + 1 >= argc || *argv[i+1] =='-') {
				printf("No parameter specified with -w, using default.\n");	
			} else {
				if (strcmp(argv[i+1], "enable") == 0) global->hwaccel=1;
				else 
					if (strcmp(argv[i+1], "disable") == 0) global->hwaccel=0;
			}
		}
		if ((strcmp(argv[i], "-f") == 0) || (strcmp(argv[i], "--format") == 0)) {
			if ( i + 1 >= argc || *argv[i+1] =='-') {
				printf("No parameter specified for format, using default.\n");	
			} else {
				global->mode[0] = argv[i + 1][0];
				global->mode[1] = argv[i + 1][1];
				global->mode[2] = argv[i + 1][2];
			}
		}
		if ((strcmp(argv[i], "-s") == 0) || (strcmp(argv[i], "--size") == 0)) {
			if (i + 1 >= argc || *argv[i+1] =='-') {
			printf("No parameter specified for image size, using default.\n");
			} else {

				sizestring = strdup(argv[i + 1]);

				global->width = strtoul(sizestring, &separateur, 10);
				if (*separateur != 'x') {
					printf("Error in size usage: -s[--size] widthxheight \n");
				} else {
					++separateur;
					global->height = strtoul(separateur, &separateur, 10);
					if (*separateur != 0)
						printf("hmm.. dont like that!! trying this height \n");
				}
			}
			printf(" size width: %d height: %d \n",global->width, global->height);
		}
		if ((strcmp(argv[i], "-c") == 0) || (strcmp(argv[i], "--captime") == 0)) {
			if (i + 1 >= argc || *argv[i+1] =='-') {
				printf("No parameter specified for image capture time. Ignoring option.\n");	
			} else {
				char *timestr = strdup(argv[i + 1]);
				global->image_timer= strtoul(timestr, &separateur, 10);
				global->image_inc=1;
				printf("capturing images every %i seconds",global->image_timer);
			}
		}
		if ((strcmp(argv[i], "-m") == 0) || (strcmp(argv[i], "--npics") == 0)) {
			if (i + 1 >= argc || *argv[i+1] =='-') {
				printf("No parameter specified for number of pics. Ignoring option.\n");	
			} else {
				char *npicstr = strdup(argv[i + 1]);
				global->image_npics= strtoul(npicstr, &separateur, 10);
				printf("capturing at max %d pics",global->image_npics);
			}
		}
		if ((strcmp(argv[i], "-i") == 0) || (strcmp(argv[i], "--image") == 0)) {
			if (i + 1 >= argc || *argv[i+1] =='-') {
				printf("No parameter specified for image name. Ignoring option.\n");	
			} else {
				char *image_path = strdup(argv[i + 1]);
				global->imgFPath=splitPath(image_path,global->imgFPath);
				/*get the file type*/
				global->imgFormat = check_image_type(global->imgFPath[0]);
			}
		}
		
		if ((strcmp(argv[i], "-n") == 0) || (strcmp(argv[i], "--avi") == 0)) {
			if (i + 1 >= argc || *argv[i+1] =='-') {
				printf("No parameter specified for avi name. Ignoring option.\n");	
			} else {
				global->avifile = strdup(argv[i + 1]);
				global->aviFPath=splitPath(global->avifile,global->aviFPath);
			}
		}
		if ((strcmp(argv[i], "-t") == 0) || (strcmp(argv[i], "--avitime") == 0)) {
			if (i + 1 >= argc || *argv[i+1] =='-') {
				printf("No parameter specified for avi time. Ignoring option.\n");	
			} else {
				char *timestr = strdup(argv[i + 1]);
				global->Capture_time= strtoul(timestr, &separateur, 10);
				printf("capturing avi for %i seconds",global->Capture_time);
			}
		}
		if (strcmp(argv[i], "-p") == 0) {
			if ( i + 1 >= argc || *argv[i+1] =='-') {
				printf("No parameter specified with -p, using default.\n");	
			} else {
				if (strcmp(argv[i+1], "enable") == 0) global->FpsCount=1;
				else 
					if (strcmp(argv[i+1], "disable") == 0) global->FpsCount=0;
			}
		}
		if ((strcmp(argv[i], "-l") == 0) || (strcmp(argv[i], "--profile") == 0)) {
			if (i + 1 >= argc || *argv[i+1] =='-') {
				printf("No parameter specified for profile name. Ignoring option.\n");	
			} else {
				global->lprofile=1;
				global->profile_FPath=splitPath(argv[i + 1],global->profile_FPath);
			}
		}
		if ((strcmp(argv[i], "-h") == 0) || (strcmp(argv[i], "--help") == 0)) {
			printf("usage: guvcview [options] \n\n");
			printf("options:\n");
			printf("-h[--help]\t:print this message \n");
		    	printf("--verbose \tverbose mode, prints a lot of debug related info\n");
			printf("-d[--device] /dev/videoX\t:use videoX device\n");
			printf("-g\t:use read method for grab instead mmap\n");
			printf("-w enable|disable\t:SDL hardware accel. \n");
			printf("-f[--format] format\t:video format\n");
			printf("   default jpg  others options are yuv jpg \n");
			printf("-s[--size] widthxheight\t:use specified input size \n");
			printf("-i[--image] image_file_name\t:sets the default image name\n"); 
			printf("   available image formats: jpg png bmp\n");
			printf("-c[--captime] time_in_seconds\t:time between image captures (sec.)\n"); 
			printf("   enables auto image capture\n");
			printf("-m[--npics] num_pics\t:max number of image captures\n");
			printf("   defaults to 999 if not set\n");
			printf("-n[--avi] avi_file_name\t:if set, enable avi capture from start \n");
			printf("-t[--avitime] capture_time\t:used with -n option, avi capture time (sec.)\n");
			printf("-p enable|disable\t:fps counter in title bar\n");
			printf("-l[--profile] filename\t:loads the given control profile\n");
			closeGlobals(global);
			exit(0);
		}
	}
	
	/*if -n not set reset capture time*/
	if(global->Capture_time>0 && global->avifile==NULL) global->Capture_time=0;
	
	if (strncmp(global->mode, "yuv", 3) == 0) {
		global->format = V4L2_PIX_FMT_YUYV;
		global->formind = 1;
	} else if (strncmp(global->mode, "jpg", 3) == 0) {
		global->format = V4L2_PIX_FMT_MJPEG;
		global->formind = 0;
	} else {
		global->format = V4L2_PIX_FMT_MJPEG;
		global->formind = 0;
	}
    	if (global->debug) printf("Format is %s(%d)\n",global->mode,global->formind);
}


/*--------------------------- sound threaded loop ------------------------------*/
static void*
sound_capture(void *data)
{
	size_t sndstacksize;
	PaStreamParameters inputParameters;
	PaStream *stream;
	PaError err;
	SAMPLE *recordedSamples=NULL;
	int i;
	int totalFrames;
	int numSamples;
	
	/*gets the stack size for the thread (DEBUG)*/
	pthread_attr_getstacksize (&sndattr, &sndstacksize);
	if (global->debug) printf("Sound Thread: stack size = %d bytes \n", (int) sndstacksize);
	
	if(global->Sound_SampRateInd==0)
	   global->Sound_SampRate=global->Sound_IndexDev[global->Sound_UseDev].samprate;/*using default*/
	
	if(global->Sound_NumChanInd==0) {
	   /*using default if channels <3 or stereo(2) otherwise*/
	   global->Sound_NumChan=(global->Sound_IndexDev[global->Sound_UseDev].chan<3)?global->Sound_IndexDev[global->Sound_UseDev].chan:2;
	}
    	
    	/*set audio header for avi*/
	AVI_set_audio(AviOut, global->Sound_NumChan, global->Sound_SampRate, sizeof(SAMPLE)*8,WAVE_FORMAT_PCM);
	
    	/* setting maximum buffer size*/
	totalFrames = global->Sound_NumSec * global->Sound_SampRate;
	numSamples = totalFrames * global->Sound_NumChan;
	global->snd_numBytes = numSamples * sizeof(SAMPLE);
    
	recordedSamples = (SAMPLE *) malloc( global->snd_numBytes ); /*capture buffer*/
	global->avi_sndBuff = (SAMPLE *) malloc( global->snd_numBytes );/*secondary shared buffer*/
    
	if( recordedSamples == NULL )
	{
		printf("Could not allocate record array.\n");
		pthread_exit((void *) -2);
	}
	for( i=0; i<numSamples; i++ ) recordedSamples[i] = 0;
	
	err = Pa_Initialize();
	if( err != paNoError ) goto error;
	/* Record for a few seconds. */

	inputParameters.device = global->Sound_IndexDev[global->Sound_UseDev].id; /* input device */
	inputParameters.channelCount = global->Sound_NumChan;
	inputParameters.sampleFormat = PA_SAMPLE_TYPE;
	inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultLowInputLatency;
	inputParameters.hostApiSpecificStreamInfo = NULL; 
	
	/*---------------------------- Record some audio. ----------------------------- */
	/* Input buffer will be twice(default) the size of frames to read               */
	/* This way even in slow machines it shouldn't overflow and drop frames         */
	err = Pa_OpenStream(
			  &stream,
			  &inputParameters,
			  NULL,                  /* &outputParameters, */
			  global->Sound_SampRate,
			  (totalFrames*global->Sound_BuffFactor),/* buffer Size - totalFrames*/
			  paNoFlag,      /* PaNoFlag - clip and dhiter*/
			  NULL, /* sound callback - using blocking API*/
			  NULL ); /* callback userData -no callback no data */
	if( err != paNoError ) goto error;  
	err = Pa_StartStream( stream );
	if( err != paNoError ) goto error; /*should close the stream if error ?*/
	/*----------------------------- capture loop ----------------------------------*/
	global->snd_begintime = ms_time();

	do {
	   err = Pa_ReadStream( stream, recordedSamples, totalFrames );

	   /*copy recorded samples to shared buffer*/
	   if(global->audio_flag==0) { 
	       memcpy(global->avi_sndBuff, recordedSamples ,global->snd_numBytes); 
	       global->audio_flag =1;
	   } else {
	   	/*wait a bit and retry?*/
	       printf("sound not ready...\n");
	   }
	
	} while (videoIn->capAVI);   

	err = Pa_StopStream( stream );
	if( err != paNoError ) goto error;
	
	err = Pa_CloseStream( stream ); /*closes the stream*/
	if( err != paNoError ) goto error; 
	
	if(recordedSamples) free( recordedSamples  );
	recordedSamples=NULL;
	Pa_Terminate();
	
	pthread_exit((void *) 0);

error:
	if(recordedSamples) free( recordedSamples );
	recordedSamples=NULL;
	Pa_Terminate();
	fprintf( stderr, "An error occured while using the portaudio stream\n" );
	fprintf( stderr, "Error number: %d\n", err );
	fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
	pthread_exit((void *) -1);
	
}


/*------------------------------ Event handlers -------------------------------*/
/* window close */
static gint
delete_event (GtkWidget *widget, GdkEventConfigure *event)
{
	shutd(0);//shutDown
	
	return 0;
}

/*-------------------------------- avi close functions -----------------------*/

/* Called at avi capture stop       */
/* from avi capture button callback */
static void
aviClose (void)
{
  DWORD tottime = 0;
  int tstatus;
	
  if (AviOut)
  {
	  tottime = global->AVIstoptime - global->AVIstarttime;
	  if (global->debug) printf("stop= %d start=%d \n",global->AVIstoptime,global->AVIstarttime);
	  if (tottime > 0) {
		/*try to find the real frame rate*/
		AviOut->fps = (double) (global->framecount * 1000) / tottime;
	  }
	  else {
		/*set the hardware frame rate*/   
		AviOut->fps=videoIn->fps;
	  }
     
     	  if (global->debug) printf("AVI: %d frames in %d ms = %f fps\n",global->framecount,tottime,AviOut->fps);
	  /*---------------- write last audio data to avi if Sound Enable ------------------*/
	  if (global->Sound_enable > 0) {
		/* Free attribute and wait for the thread */
		pthread_attr_destroy(&sndattr);
	
		pthread_join(sndthread, (void *)&tstatus);
	   	
		if (tstatus!=0)
		{
			printf("ERROR: status from sound thread join is %d\n", tstatus);
			/* don't add sound*/
		} else {
			if (global->debug) printf("Capture sound thread join with status= %d\n", tstatus);
		    	if (global->audio_flag>0) {
				AVI_append_audio(AviOut,(BYTE *) global->avi_sndBuff,global->snd_numBytes);
				global->audio_flag=0;
			}
		}
	  } 
	  AVI_close (AviOut);
	  AviOut = NULL;
	  global->framecount = 0;
	  global->AVIstarttime = 0;
	  if (global->debug) printf ("close avi\n"); 	 
  }
}

/* counts chars needed for n*/
static int
num_chars (int n)
{
	int i = 0;

	if (n <= 0) {
		i++;
		n = -n;
	}

	while (n != 0) {
		n /= 10;
		i++;
	}
	return i;
}


/*--------------------------- focus control ----------------------------------*/
static int 
get_focus (){
	int ret;
	struct v4l2_control c;
	int val=0;
    
	c.id  = V4L2_CID_FOCUS_LOGITECH;
	ret = ioctl (videoIn->fd, VIDIOC_G_CTRL, &c);
	if (ret == 0)
		val = c.value;
	else
		val = -1;
	
	return val;

}

static int 
set_focus (int val) {
	int ret;
	struct v4l2_control c;

	c.id  = V4L2_CID_FOCUS_LOGITECH;
	c.value = val;
	ret = ioctl (videoIn->fd, VIDIOC_S_CTRL, &c);

	return ret;
}


/*----------------------------- Callbacks ------------------------------------*/
/*slider controls callback*/
static void
slider_changed (GtkRange * range, VidState * s)
{
  
	ControlInfo * ci = g_object_get_data (G_OBJECT (range), "control_info");
	InputControl * c = s->control + ci->idx;
	int val = (int) gtk_range_get_value (range);
	
	if (input_set_control (videoIn, c, val) == 0) {
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(ci->spinbutton), val);
	}
	else {
		if (global->debug) printf ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn, c, &val) == 0) {
			if (global->debug) printf ("hardware value is %d\n", val);
			gtk_range_set_value (GTK_RANGE(ci->widget),val);
		}
		else {
			printf ("hardware get failed\n");
		}
	}
}

/*spin controls callback*/
static void
spin_changed (GtkSpinButton * spin, VidState * s)
{
	ControlInfo * ci = g_object_get_data (G_OBJECT (spin), "control_info");
   	InputControl * c = s->control + ci->idx;
   	int val = gtk_spin_button_get_value_as_int (spin);
   
   	if (input_set_control (videoIn, c, val) == 0) {
		gtk_range_set_value (GTK_RANGE(ci->widget),val);
	}
   	else {
		if (global->debug) printf ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn, c, &val) == 0) {
			if (global->debug) printf ("hardware value is %d\n", val);
		   	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ci->spinbutton),val);
		}
		else {
			printf ("hardware get failed\n");
		}
	}
}

/*check box controls callback*/
static void
autofocus_changed (GtkToggleButton * toggle, VidState * s) {
    
	ControlInfo * ci = g_object_get_data (G_OBJECT (toggle), "control_info");
	int val;
	
	val = gtk_toggle_button_get_active (toggle) ? 1 : 0;
    
    	/*if autofocus disable manual focus control*/
    	gtk_widget_set_sensitive (ci->widget, !val);
	gtk_widget_set_sensitive (ci->spinbutton, !val);
    	
    	/*reset flag*/
    	AFdata->flag = 0;
	AFdata->ind = 0;
	AFdata->focus = -1; /*reset focus*/
    	AFdata->right = 255;
    	AFdata->left = 8;
	/*set focus to first value if autofocus enabled*/
	if (val>0) {
		if (set_focus (AFdata->focus) != 0) 
			printf("ERROR: couldn't set focus to %d\n", AFdata->focus);
    	}
	global->autofocus = val;
}

static void
check_changed (GtkToggleButton * toggle, VidState * s)
{
	
	ControlInfo * ci = g_object_get_data (G_OBJECT (toggle), "control_info");
	InputControl * c = s->control + ci->idx;
	int val;
	
	val = gtk_toggle_button_get_active (toggle) ? 1 : 0;
	
	if (input_set_control (videoIn, c, val) != 0) {
		printf ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn, c, &val) == 0) {
			printf ("hardware value is %d\n", val);
		}
		else {
			printf ("hardware get failed\n");
		}
	} else {
		if (global->debug) printf("changed %s to %d\n",c->name,val);
		if (input_get_control (videoIn, c, &val) == 0) {
			if (global->debug) printf ("hardware value is %d\n", val);
		}
		else {
			printf ("hardware get failed\n");
		}
		
	}
	
}

static void
bayer_changed (GtkToggleButton * toggle, VidState * s)
{	
   	ControlInfo * ci = g_object_get_data (G_OBJECT (toggle), "control_info");
	InputControl * c = s->control + ci->idx;
   	int val;
   
	val = gtk_toggle_button_get_active (toggle) ? 1 : 0;
   	if (input_set_control (videoIn, c, val) != 0) {
		printf ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn, c, &val) == 0) {
			printf ("hardware value is %d\n", val);
		}
		else {
			printf ("hardware get failed\n");
		}
	} else {
		if (global->debug) printf("changed %s to %d\n",c->name,val);
		/*stop and restart stream*/
		videoIn->setFPS=1;
		/*read value*/
		if (input_get_control (videoIn, c, &val) == 0) {
			if (val>0) {
			   videoIn->isbayer=1;
			}
			else videoIn->isbayer=0;
		}
		else {
			printf ("hardware get failed\n");
		}
		
	}
   
}

static void
pix_ord_changed (GtkComboBox * combo, VidState * s)
{
   int index = gtk_combo_box_get_active (combo);
   videoIn->pix_order=index;
   
}


/*combobox controls callback*/
static void
combo_changed (GtkComboBox * combo, VidState * s)
{
	
	ControlInfo * ci = g_object_get_data (G_OBJECT (combo), "control_info");
	InputControl * c = s->control + ci->idx;
	int index = gtk_combo_box_get_active (combo);
	int val=0;
		
	if (c->id == V4L2_CID_EXPOSURE_AUTO_OLD) {
		val=exp_vals[videoIn->available_exp[index]];	
	} else {	
		val=index;
	}

	if (input_set_control (videoIn, c, val) != 0) {
		printf ("%s change to %d failed\n",c->name, val);
		if (input_get_control (videoIn, c, &val) == 0) {
			printf ("hardware value is %d\n", val);
		}
		else {
			printf ("hardware get failed\n");
		}
	}
	
}

/* Pan left (for motor cameras ex: Logitech Orbit/Sphere) */
static void
setfocus_clicked (GtkButton * FocusButton, VidState * s)
{	
	AFdata->setFocus = 1;
    	AFdata->ind = 0;
    	AFdata->flag = 0;
    	AFdata->right = 255;
    	AFdata->left = 8;
    	AFdata->focus = -1; /*reset focus*/
	if (set_focus (AFdata->focus) != 0) 
		printf("ERROR: couldn't set focus to %d\n", AFdata->focus);
}

/* Pan left (for motor cameras ex: Logitech Orbit/Sphere) */
static void
PanLeft_clicked (GtkButton * PanLeft, VidState * s)
{	
	if(uvcPanTilt(videoIn, -INCPANTILT*(global->PanStep), 0, 0)<0) {
		printf("Pan Left Error");
	}
}
/* Pan Right (for motor cameras ex: Logitech Orbit/Sphere) */
static void
PanRight_clicked (GtkButton * PanRight, VidState * s)
{	
	if(uvcPanTilt(videoIn, INCPANTILT*(global->PanStep), 0, 0)<0) {
		printf("Pan Right Error");
	}
}
/* Tilt Up (for motor cameras ex: Logitech Orbit/Sphere)   */
static void
TiltUp_clicked (GtkButton * TiltUp, VidState * s)
{	
	if(uvcPanTilt(videoIn, 0, -INCPANTILT*(global->TiltStep), 0)<0) {
		printf("Tilt UP Error");
	}
}
/* Tilt Down (for motor cameras ex: Logitech Orbit/Sphere) */
static void
TiltDown_clicked (GtkButton * TiltDown, VidState * s)
{	
	if(uvcPanTilt(videoIn, 0, INCPANTILT*(global->TiltStep), 0)<0) {
		printf("Tilt Down Error");
	}
}
/* Pan Reset (for motor cameras ex: Logitech Orbit/Sphere)*/
static void
PReset_clicked (GtkButton * PReset, VidState * s)
{	
	if(uvcPanTilt(videoIn, 0, 0, 1)<0) {
		printf("Pan Reset Error");
	}
}
/* Tilt Reset (for motor cameras ex: Logitech Orbit/Sphere)*/
static void
TReset_clicked (GtkButton * PTReset, VidState * s)
{	
	if(uvcPanTilt(videoIn, 0, 0, 2)<0) {
		printf("Pan Reset Error");
	}
}
/* Pan Tilt Reset (for motor cameras ex: Logitech Orbit/Sphere)*/
static void
PTReset_clicked (GtkButton * PTReset, VidState * s)
{	
	if(uvcPanTilt(videoIn, 0, 0, 3)<0) {
		printf("Pan Tilt Reset Error");
	}
}

/*resolution control callback*/
static void
resolution_changed (GtkComboBox * Resolution, void *data)
{
	/* The new resolution is writen to conf file at exit             */
	/* then is read back at start. This means that for changing */
	/* resolution we must restart the application                    */
	
	int index = gtk_combo_box_get_active(Resolution);
	global->width=videoIn->listVidCap[global->formind][index].width;
	global->height=videoIn->listVidCap[global->formind][index].height;
	
	/*check if frame rate is available at the new resolution*/
	int i=0;
	int deffps=0;
	for(i=0;i<videoIn->listVidCap[global->formind][index].numb_frates;i++) {
		if ((videoIn->listVidCap[global->formind][index].framerate_num[i]==global->fps_num) && 
			   (videoIn->listVidCap[global->formind][index].framerate_denom[i]==global->fps)) deffps=i;	
	}
	
	global->fps_num=videoIn->listVidCap[global->formind][index].framerate_num[deffps];
	global->fps=videoIn->listVidCap[global->formind][index].framerate_denom[deffps];		
	
	
	restartdialog = gtk_dialog_new_with_buttons (_("Program Restart"),
						    GTK_WINDOW(mainwin),
						    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						    _("now"),
						    GTK_RESPONSE_ACCEPT,
						    _("Later"),
						    GTK_RESPONSE_REJECT,
						    NULL);
	
	GtkWidget *message = gtk_label_new (_("Changes will only take effect after guvcview restart.\n\n\nRestart now?\n"));
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(restartdialog)->vbox), message);
	gtk_widget_show_all(GTK_WIDGET(GTK_CONTAINER (GTK_DIALOG(restartdialog)->vbox)));
	
	gint result = gtk_dialog_run (GTK_DIALOG (restartdialog));
	switch (result) {
		case GTK_RESPONSE_ACCEPT:
			/*restart app*/
			shutd(1);
			break;
		default:
			/* do nothing since Restart rejected*/		
			break;
	}
  
	gtk_widget_destroy (restartdialog);
		
}


/* Input Type control (YUV MJPG)*/
static void
ImpType_changed(GtkComboBox * ImpType, void * Data) 
{
	int index = gtk_combo_box_get_active(ImpType);
	
	if ((videoIn->SupMjpg >0) && (videoIn->SupYuv >0)) {
		global->formind = index;
	} else {
		/* if only one format available the callback shouldn't get called*/
		/* in any case ...                                               */
		if (videoIn->SupMjpg >0) global->formind = 0;
		else global->formind = 1;
	}
	
	/*check if frame rate and resolution are available */
	/*if not use minimum values                        */
	int i=0;
	int j=0;
	int defres=0;
	int deffps=0;
	int SupRes=0;
	
	if (global->formind > 0) { /* is Yuv*/
		snprintf(global->mode, 4, "yuv");
		SupRes=videoIn->SupYuv;
		
	} else {  /* is Mjpg */
		snprintf(global->mode, 4, "jpg");
		SupRes=videoIn->SupMjpg;
	}
	
	for (i=0;i<SupRes;i++) {
			if((videoIn->listVidCap[global->formind][i].height==global->height) &&
				(videoIn->listVidCap[global->formind][i].width==global->width) ) {
				/* resolution ok check fps*/
				defres=i;
				for (j=0;j<videoIn->listVidCap[global->formind][i].numb_frates;j++) {
					if ((videoIn->listVidCap[global->formind][i].framerate_num[j]==global->fps_num) && 
					    (videoIn->listVidCap[global->formind][i].framerate_denom[j]==global->fps))
						deffps=j;
				}
			}
		}
	
	global->height=videoIn->listVidCap[global->formind][defres].height;
	global->width=videoIn->listVidCap[global->formind][defres].width;
	global->fps_num=videoIn->listVidCap[global->formind][defres].framerate_num[deffps];
	global->fps=videoIn->listVidCap[global->formind][defres].framerate_denom[deffps];
	
	restartdialog = gtk_dialog_new_with_buttons (_("Program Restart"),
						     GTK_WINDOW(mainwin),
						     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						     _("now"),
						     GTK_RESPONSE_ACCEPT,
						     _("Later"),
						     GTK_RESPONSE_REJECT,
						     NULL);
	
	GtkWidget *message = gtk_label_new (_("Changes will only take effect after guvcview restart.\n\n\nRestart now?\n"));
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG(restartdialog)->vbox), message);
	gtk_widget_show_all(GTK_WIDGET(GTK_CONTAINER (GTK_DIALOG(restartdialog)->vbox)));
	
	gint result = gtk_dialog_run (GTK_DIALOG (restartdialog));
	switch (result) {
		case GTK_RESPONSE_ACCEPT:
			/*restart app*/
			shutd(1);
			break;
		default:
			/* do nothing since Restart rejected*/		
			break;
	}
  
	gtk_widget_destroy (restartdialog);

}

/*frame rate control callback*/
static void
FrameRate_changed (GtkComboBox * FrameRate,GtkComboBox * Resolution)
{
	int resind = gtk_combo_box_get_active(Resolution);
	
	int index = gtk_combo_box_get_active (FrameRate);
		
	videoIn->fps=videoIn->listVidCap[global->formind][resind].framerate_denom[index];
	videoIn->fps_num=videoIn->listVidCap[global->formind][resind].framerate_num[index];
 
	videoIn->setFPS=1;

	global->fps=videoIn->fps;
	global->fps_num=videoIn->fps_num;
	
}

/*sound sample rate control callback*/
static void
SndSampleRate_changed (GtkComboBox * SampleRate, void *data)
{
	global->Sound_SampRateInd = gtk_combo_box_get_active (SampleRate);
	global->Sound_SampRate=stdSampleRates[global->Sound_SampRateInd];
	
	
}

/*image type control callback*/
static void
ImageType_changed (GtkComboBox * ImageType,GtkEntry *ImageFNameEntry) 
{
	const char *filename;
	global->imgFormat=gtk_combo_box_get_active (ImageType);	
	filename=gtk_entry_get_text(ImageFNameEntry);
	
	if(strcmp(filename,global->imgFPath[0])!=0) {
		global->imgFPath=splitPath((char *)filename, global->imgFPath);
	}
	
	int sname = strlen(global->imgFPath[0]);
	char basename[sname];
	sscanf(global->imgFPath[0],"%[^.]",basename);
	switch(global->imgFormat){
		case 0:
			sprintf(global->imgFPath[0],"%s.jpg",basename);
			break;
		case 1:
			sprintf(global->imgFPath[0],"%s.bmp",basename);
			break;
		case 2:
			sprintf(global->imgFPath[0],"%s.png",basename);
			break;
	      	case 3:
	      		sprintf(global->imgFPath[0],"%s.raw",basename);
			break;
		default:
			sprintf(global->imgFPath[0],"%s",DEFAULT_IMAGE_FNAME);
	}
	gtk_entry_set_text(ImageFNameEntry," ");
	gtk_entry_set_text(ImageFNameEntry,global->imgFPath[0]);
	
	if(global->image_inc>0) {
		global->image_inc=1; /*if auto naming restart counter*/
	}
	snprintf(global->imageinc_str,24,_("File num:%d"),global->image_inc);
	gtk_label_set_text(GTK_LABEL(ImageIncLabel), global->imageinc_str);
}

/*sound device control callback*/
static void
SndDevice_changed (GtkComboBox * SoundDevice, void *data)
{
 
	global->Sound_UseDev=gtk_combo_box_get_active (SoundDevice);
	
	printf("using device id:%d\n",global->Sound_IndexDev[global->Sound_UseDev].id);
	
}

/*sound channels control callback*/
static void
SndNumChan_changed (GtkComboBox * SoundChan, void *data)
{
	/*0-device default 1-mono 2-stereo*/
	global->Sound_NumChanInd = gtk_combo_box_get_active (SoundChan);
	global->Sound_NumChan=global->Sound_NumChanInd;
}

/*avi compression control callback*/
static void
AVIComp_changed (GtkComboBox * AVIComp, void *data)
{
	int index = gtk_combo_box_get_active (AVIComp);
		
	global->AVIFormat=index;
}

/* sound enable check box callback */
static void
SndEnable_changed (GtkToggleButton * toggle, VidState * s)
{
	global->Sound_enable = gtk_toggle_button_get_active (toggle) ? 1 : 0;
	if (!global->Sound_enable) {
		set_sensitive_snd_contrls(FALSE);
	} else { 
		set_sensitive_snd_contrls(TRUE);
	}
}

/* Mirror check box callback */
static void
FiltMirrorEnable_changed(GtkToggleButton * toggle, void *data)
{
	global->Frame_Flags = gtk_toggle_button_get_active (toggle) ? 
				(global->Frame_Flags | YUV_MIRROR) : 
					(global->Frame_Flags & ~YUV_MIRROR);
}

/* Upturn check box callback */
static void
FiltUpturnEnable_changed(GtkToggleButton * toggle, void *data)
{
	global->Frame_Flags = gtk_toggle_button_get_active (toggle) ? 
				(global->Frame_Flags | YUV_UPTURN) : 
					(global->Frame_Flags & ~YUV_UPTURN);
}

/* Negate check box callback */
static void
FiltNegateEnable_changed(GtkToggleButton * toggle, void *data)
{
	global->Frame_Flags = gtk_toggle_button_get_active (toggle) ? 
				(global->Frame_Flags | YUV_NEGATE) : 
					(global->Frame_Flags & ~YUV_NEGATE);
}

/* Upturn check box callback */
static void
FiltMonoEnable_changed(GtkToggleButton * toggle, void *data)
{
	global->Frame_Flags = gtk_toggle_button_get_active (toggle) ? 
				(global->Frame_Flags | YUV_MONOCR) : 
					(global->Frame_Flags & ~YUV_MONOCR);
}
/*--------------------------- file chooser dialog ----------------------------*/
static void
file_chooser (GtkButton * FileButt, const int isAVI)
{	
  const char *basename;
  char *fullname;
	
  FileDialog = gtk_file_chooser_dialog_new (_("Save File"),
					  GTK_WINDOW (mainwin),
					  GTK_FILE_CHOOSER_ACTION_SAVE,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					  NULL);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (FileDialog), TRUE);

  if(isAVI) { /* avi File chooser*/
	
	basename =  gtk_entry_get_text(GTK_ENTRY(AVIFNameEntry));
	
	global->aviFPath=splitPath((char *)basename, global->aviFPath);
	
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (FileDialog), 
								global->aviFPath[1]);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (FileDialog),
								global->aviFPath[0]);
      
	if (gtk_dialog_run (GTK_DIALOG (FileDialog)) == GTK_RESPONSE_ACCEPT)
	{
		fullname = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (FileDialog));
		global->aviFPath=splitPath(fullname, global->aviFPath);
		gtk_entry_set_text(GTK_ENTRY(AVIFNameEntry)," ");
		gtk_entry_set_text(GTK_ENTRY(AVIFNameEntry),global->aviFPath[0]);
	}
	  
  } else {/* Image File chooser*/
	
	basename =  gtk_entry_get_text(GTK_ENTRY(ImageFNameEntry));
	
	global->imgFPath=splitPath((char *)basename, global->imgFPath);
	
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (FileDialog), 
							global->imgFPath[1]);
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (FileDialog), 
							global->imgFPath[0]);

	if (gtk_dialog_run (GTK_DIALOG (FileDialog)) == GTK_RESPONSE_ACCEPT)
	{
		fullname = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (FileDialog));
		global->imgFPath=splitPath(fullname, global->imgFPath);
		
		gtk_entry_set_text(GTK_ENTRY(ImageFNameEntry)," ");
		gtk_entry_set_text(GTK_ENTRY(ImageFNameEntry),global->imgFPath[0]);
		/*get the file type*/
		global->imgFormat = check_image_type(global->imgFPath[0]);
		/*set the file type*/
		gtk_combo_box_set_active(GTK_COMBO_BOX(ImageType),global->imgFormat);
		
		if(global->image_inc>0){ 
			global->image_inc=1; /*if auto naming restart counter*/
			snprintf(global->imageinc_str,24,_("File num:%d"),global->image_inc);
			gtk_label_set_text(GTK_LABEL(ImageIncLabel), global->imageinc_str);
		}
			
	}
	  
  }
  gtk_widget_destroy (FileDialog);
	
}

/*----------------------------- Capture Image --------------------------------*/
/*image capture button callback*/
static void
capture_image (GtkButton *ImageButt, void *data)
{
	int fsize=20;
	int sfname=120;
	
	const char *fileEntr=gtk_entry_get_text(GTK_ENTRY(ImageFNameEntry));
	if(strcmp(fileEntr,global->imgFPath[0])!=0) {
		/*reset if entry change from last capture*/
		if(global->image_inc) global->image_inc=1;
		global->imgFPath=splitPath((char *)fileEntr, global->imgFPath);
		gtk_entry_set_text(GTK_ENTRY(ImageFNameEntry),"");
		gtk_entry_set_text(GTK_ENTRY(ImageFNameEntry),global->imgFPath[0]);
		/*get the file type*/
		global->imgFormat = check_image_type(global->imgFPath[0]);
		/*set the file type*/
		gtk_combo_box_set_active(GTK_COMBO_BOX(ImageType),global->imgFormat);
	}
	fsize=strlen(global->imgFPath[0]);
	sfname=strlen(global->imgFPath[1])+fsize+10;
	char filename[sfname]; /*10 - digits for auto increment*/
	snprintf(global->imageinc_str,24,_("File num:%d"),global->image_inc);
	gtk_label_set_text(GTK_LABEL(ImageIncLabel), global->imageinc_str);
	
	if ((global->image_timer == 0) && (global->image_inc>0)) {
		char basename[fsize];
		char extension[4];
		sscanf(global->imgFPath[0],"%[^.].%3c",basename,extension);
		extension[3]='\0';
		snprintf(filename,sfname,"%s/%s-%d.%s",global->imgFPath[1],basename,
				            global->image_inc,extension);
		
		global->image_inc++;
	} else {
		//printf("fsize=%d bytes fname=%d bytes\n",fsize,sfname);
		snprintf(filename,sfname,"%s/%s", global->imgFPath[1],global->imgFPath[0]);
	}
	if ((sfname>120) && (sfname>strlen(videoIn->ImageFName))) {
		printf("realloc image file name by %d bytes.\n",sfname);
		videoIn->ImageFName=realloc(videoIn->ImageFName,sfname);
		if (videoIn->ImageFName==NULL) exit(-1);
	}
	//videoIn->ImageFName=strncpy(videoIn->ImageFName,filename,sfname);
	snprintf(videoIn->ImageFName,sfname,"%s",filename);
	if(global->image_timer > 0) { 
		/*auto capture on -> stop it*/
		if (global->image_timer_id > 0) g_source_remove(global->image_timer_id);
	    	gtk_button_set_label(GTK_BUTTON(CapImageButt),_("Cap. Image"));
		global->image_timer=0;
		set_sensitive_img_contrls(TRUE);/*enable image controls*/
	} else {
	   	if(global->imgFormat == 3) { /*raw frame*/
			videoIn->cap_raw = 1;
		} else {
			videoIn->capImage = TRUE;
		}
	}
}

/*--------------------------- Capture AVI ------------------------------------*/
/*avi capture button callback*/
static void
capture_avi (GtkButton *AVIButt, void *data)
{
	const char *fileEntr = gtk_entry_get_text(GTK_ENTRY(AVIFNameEntry));
	if(strcmp(fileEntr,global->aviFPath[0])!=0) {
		/*reset if entry change from last capture*/
		//if(global->avi_inc) global->avi_inc=1;
		global->aviFPath=splitPath((char *)fileEntr, global->aviFPath);
		gtk_entry_set_text(GTK_ENTRY(AVIFNameEntry),"");
		gtk_entry_set_text(GTK_ENTRY(AVIFNameEntry),global->aviFPath[0]);
	}
	
	char *compression="MJPG";

	switch (global->AVIFormat) {
		case 0:
			compression="MJPG";
			break;
		case 1:
			compression="YUY2";
			break;
		case 2:
			compression="DIB ";
			break;
		default:
			compression="MJPG";
	}	
	if(videoIn->capAVI) {  /************* Stop AVI ************/
		gtk_button_set_label(GTK_BUTTON(CapAVIButt),_("Cap. AVI"));
		global->AVIstoptime = ms_time();	
		videoIn->capAVI = FALSE;
		aviClose();
		/*enabling sound and avi compression controls*/
		set_sensitive_avi_contrls(TRUE);
	} 
	else { /******************** Start AVI *********************/
	       	global->aviFPath=splitPath((char *)fileEntr, global->aviFPath);
	
		int sfname=strlen(global->aviFPath[1])+strlen(global->aviFPath[0])+2;
		char filename[sfname];
		
	   	sprintf(filename,"%s/%s", global->aviFPath[1],global->aviFPath[0]);
		if ((sfname>120) && (sfname>strlen(videoIn->AVIFName))) {
			printf("realloc avi file name by %d.\n",sfname+1);
			videoIn->AVIFName=realloc(videoIn->AVIFName,sfname+1);
		}	
						
		videoIn->AVIFName=strncpy(videoIn->AVIFName,filename,sfname);
		
		//printf("opening avi file: %s\n",videoIn->AVIFName);
	    	gtk_button_set_label(GTK_BUTTON(CapAVIButt),_("Stop AVI"));
		AviOut = AVI_open_output_file(videoIn->AVIFName);
		/*4CC compression "YUY2" (YUV) or "DIB " (RGB24)  or  "MJPG"*/	
	        
		AVI_set_video(AviOut, videoIn->width, videoIn->height, videoIn->fps,compression);		
		/* audio will be set in aviClose - if enabled*/
		global->AVIstarttime = ms_time();

	    	videoIn->capAVI = TRUE; /* start video capture */
		/*disabling sound and avi compression controls*/
		set_sensitive_avi_contrls(FALSE);
	    
		/* Creating the sound capture loop thread if Sound Enable*/ 
		if(global->Sound_enable > 0) { 
			/* Initialize and set snd thread detached attribute */
			size_t stacksize;
			stacksize = sizeof(char) * global->stack_size;
		   	pthread_attr_init(&sndattr);
		   	pthread_attr_setstacksize (&sndattr, stacksize);
			pthread_attr_setdetachstate(&sndattr, PTHREAD_CREATE_JOINABLE);
		  
			int rsnd = pthread_create(&sndthread, &sndattr, sound_capture, NULL); 
			if (rsnd)
			{
				printf("ERROR; return code from snd pthread_create() is %d\n", rsnd);
			}
		}
	}	
}


/* called by capture from start timer [-t seconds] command line option*/
static int
timer_callback(){
	/*stop avi capture*/
	capture_avi(GTK_BUTTON(CapAVIButt),AVIFNameEntry);
	global->Capture_time=0; 
	return (FALSE);/*destroys the timer*/
}


/* called by fps counter every 2 sec */
static int 
FpsCount_callback(){
	global->DispFps = (double) global->frmCount / 2;
	if (global->FpsCount>0) return(TRUE); /*keeps the timer*/
	else {
		snprintf(global->WVcaption,10,"GUVCVideo");
		SDL_WM_SetCaption(global->WVcaption, NULL);
		return (FALSE);/*destroys the timer*/
	}
}
/*called by timed capture [-c seconds] command line option*/
static int
Image_capture_timer(){
	/*increment image name (max 1-99999)*/
	int sfname=strlen(global->imgFPath[0]);
	char basename[sfname];
	char extension[4];
	sscanf(global->imgFPath[0],"%[^.].%3c",basename,extension);
	int namesize=strlen(global->imgFPath[1])+strlen(basename)+5;
	
	extension[3] = '\0';
	
	if(namesize>110) {
		videoIn->ImageFName=realloc(videoIn->ImageFName,namesize+11);
	}
	
	sprintf(videoIn->ImageFName,"%s/%s-%i.%s",global->imgFPath[1],
			                        basename,global->image_inc,extension );
	snprintf(global->imageinc_str,24,_("File num:%d"),global->image_inc);
		
	gtk_label_set_text(GTK_LABEL(ImageIncLabel), global->imageinc_str);
	
	global->image_inc++;
	/*set image capture flag*/
	videoIn->capImage = TRUE;
	if(global->image_inc > global->image_npics) {/*destroy timer*/
		gtk_button_set_label(GTK_BUTTON(CapImageButt),_("Cap. Image"));
		global->image_timer=0;
		set_sensitive_img_contrls(TRUE);/*enable image controls*/
		return (FALSE);
	}
	else return (TRUE);/*keep the timer*/
}

static void 
ShowFPS_changed(GtkToggleButton * toggle, void *data)
{
	global->FpsCount = gtk_toggle_button_get_active (toggle) ? 1 : 0;
	
	if(global->FpsCount > 0) {
		/*sets the Fps counter timer function every 2 sec*/
		global->timer_id = g_timeout_add(2*1000,FpsCount_callback,NULL);
	} else {
		if (global->timer_id > 0) g_source_remove(global->timer_id);
		snprintf(global->WVcaption,10,"GUVCVideo");
		SDL_WM_SetCaption(global->WVcaption, NULL);
	}

}

static void 
ImageInc_changed(GtkToggleButton * toggle, void *data)
{
	global->image_inc = gtk_toggle_button_get_active (toggle) ? 1 : 0;
	
	snprintf(global->imageinc_str,24,_("File num:%d"),global->image_inc);
	
	gtk_label_set_text(GTK_LABEL(ImageIncLabel), global->imageinc_str);

}

static int
SaveControls(VidState *s)
{
	
	FILE *fp;
	int i=0;
	int val=0;
	int sfname=strlen(global->profile_FPath[1])+strlen(global->profile_FPath[0]);
	char filename[sfname+2];
	
	sprintf(filename,"%s/%s", global->profile_FPath[1],global->profile_FPath[0]);
	
	fp=fopen(filename,"w");
	if( fp == NULL )
	{
		printf("Could not open profile data file: %s.\n",filename);
		return (-1);
	} else {
		if (s->control) {
			fprintf(fp,"#guvcview control profile\n");
			fprintf(fp,"version=%s\n",VERSION);
			fprintf(fp,"# control name +\n");
			fprintf(fp,"#control[num]:id:type=val\n");
			/*save controls by type order*/
			/* 1- Boolean controls       */
			/* 2- Menu controls          */
			/* 3- Integer controls       */
			fprintf(fp,"# 1-BOOLEAN CONTROLS\n");
			for (i = 0; i < s->num_controls; i++) {
				/*Boolean*/
				InputControl * c = s->control + i;
				if(c->type == INPUT_CONTROL_TYPE_BOOLEAN) {
					if (input_get_control (videoIn, c, &val) != 0) {
						val=c->default_val;
					}
					val = val & 0x0001;
					fprintf(fp,"# %s +\n",c->name);
					fprintf(fp,"control[%d]:0x%x:%d=%d\n",c->i,c->id,c->type, val);
				}
			}
			fprintf(fp,"# 2-MENU CONTROLS\n");
			for (i = 0; i < s->num_controls; i++) {
				/*Menu*/
				InputControl * c = s->control + i;
				if(c->type == INPUT_CONTROL_TYPE_MENU) {
					if (input_get_control (videoIn, c, &val) != 0) {
						val=c->default_val;
					}
					fprintf(fp,"# %s +\n",c->name);
					fprintf(fp,"control[%d]:0x%x:%d=%d\n",c->i,c->id,c->type, val);
				}
			}
			fprintf(fp,"# 3-INTEGER CONTROLS\n");
			for (i = 0; i < s->num_controls; i++) {
				/*Integer*/
				InputControl * c = s->control + i;
				if(c->type == INPUT_CONTROL_TYPE_INTEGER) {
					if (input_get_control (videoIn, c, &val) != 0) {
						val=c->default_val;
					}
					fprintf(fp,"# %s +\n",c->name);
					fprintf(fp,"control[%d]:0x%x:%d=%d\n",c->i,c->id,c->type, val);
				}
			}
			
		}
	}
	fclose(fp);
	return (0);
	
}

static int
LoadControls(VidState *s)
{
	
	FILE *fp;
	int i=0;
	unsigned int id=0;
	int type=0;
	int val=0;
	//char contr_inf[100];
	int sfname=strlen(global->profile_FPath[1])+strlen(global->profile_FPath[0]);
	char filename[sfname+2];
	ControlInfo *base_ci = s->control_info;
	InputControl *base_c = s->control;
	ControlInfo *ci;
	InputControl *c;
	
	sprintf(filename,"%s/%s", global->profile_FPath[1],global->profile_FPath[0]);
	
	if((fp = fopen(filename,"r"))!=NULL) {
		char line[144];

		while (fgets(line, 144, fp) != NULL) {
			
			if ((line[0]=='#') || (line[0]==' ') || (line[0]=='\n')) {
				/*skip*/
			} else if ((sscanf(line,"control[%i]:0x%x:%i=%i",&i,&id,&type,&val))==4){
				/*set control*/
				if (i < s->num_controls) {
					ci=base_ci+i;
					c=base_c+i;
					printf("control[%i]:0x%x:%i=%d\n",i,id,type,val);
					if((c->id==id) && (c->type==type)) {
						if(type == INPUT_CONTROL_TYPE_INTEGER) {					
							//input_set_control (videoIn, c, val);
							gtk_range_set_value (GTK_RANGE (ci->widget), val);
						} else if (type == INPUT_CONTROL_TYPE_BOOLEAN) {
							val = val & 0x0001;
							//input_set_control (videoIn, c, val);
							gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
												val ? TRUE : FALSE);
						} else if (type == INPUT_CONTROL_TYPE_MENU) {
							//input_set_control (videoIn, c, val);
							gtk_combo_box_set_active (GTK_COMBO_BOX (ci->widget),
				        							     val);
						}
					}
					else {
						printf("wrong control id(0x%x:0x%x) or type(%i:%i) for %s\n",
							   id,c->id,type,c->type,c->name);
					}
				} else {
					printf("wrong control index: %d\n",i);
				}
			}
		}	
	} else {
		printf("Could not open profile data file: %s.\n",filename);
		return (-1);
	} 
	

	fclose(fp);
	return (0);
	
}

/*--------------------- buttons callbacks ------------------*/
static void
SProfileButton_clicked (GtkButton * SProfileButton,VidState *vst)
{
	char *filename;
	
	FileDialog = gtk_file_chooser_dialog_new (_("Save File"),
					  GTK_WINDOW(mainwin),
					  GTK_FILE_CHOOSER_ACTION_SAVE,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
					  NULL);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (FileDialog), TRUE);
	//printf("profile(default):%s/%s\n",global->profile_FPath[1],global->profile_FPath[0]);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (FileDialog),
                                                                global->profile_FPath[1]);
	
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (FileDialog),
                                                                global->profile_FPath[0]);
	
	if (gtk_dialog_run (GTK_DIALOG (FileDialog)) == GTK_RESPONSE_ACCEPT)
	{
		/*Save Controls Data*/
		filename= gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (FileDialog));
		global->profile_FPath=splitPath(filename,global->profile_FPath);
		SaveControls(vst);
	}
	gtk_widget_destroy (FileDialog);
	
}

static void
LProfileButton_clicked (GtkButton * LProfileButton, VidState *vst)
{
	char *filename;
	
	FileDialog = gtk_file_chooser_dialog_new (_("Load File"),
					  GTK_WINDOW(mainwin),
					  GTK_FILE_CHOOSER_ACTION_OPEN,
					  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
					  NULL);
	
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (FileDialog),
                                                                  global->profile_FPath[1]);
	
	
	if (gtk_dialog_run (GTK_DIALOG (FileDialog)) == GTK_RESPONSE_ACCEPT)
	{
		/*Load Controls Data*/
		filename= gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (FileDialog));
		global->profile_FPath=splitPath(filename,global->profile_FPath);
		LoadControls(vst);
	}
	gtk_widget_destroy (FileDialog);
}

static void
quitButton_clicked (GtkButton * quitButton, void *data)
{
	shutd(0);//shutDown
}



/*--------------------------- draw camera controls ---------------------------*/
static void
draw_controls (VidState *s)
{
	int i;
	
	
	if (s->control) {
		for (i = 0; i < s->num_controls; i++) {
			ControlInfo * ci = s->control_info + i;
			if (ci->widget)
				gtk_widget_destroy (ci->widget);
			if (ci->label)
				gtk_widget_destroy (ci->label);
			if (ci->spinbutton)
				gtk_widget_destroy (ci->spinbutton);
		}
		free (s->control_info);
		s->control_info = NULL;
		input_free_controls (s->control, s->num_controls);
		s->control = NULL;
	}
	
	s->control = input_enum_controls (videoIn, &s->num_controls);

	if (global->debug) {
		printf("Controls:\n");
		for (i = 0; i < s->num_controls; i++) {
			printf("control[%d]: 0x%x",i,s->control[i].id);
			printf ("  %s, %d:%d:%d, default %d\n", s->control[i].name,
					s->control[i].min, s->control[i].step, s->control[i].max,
					s->control[i].default_val);
		}
	}

   if((s->control_info = malloc (s->num_controls * sizeof (ControlInfo)))==NULL){
			printf("couldn't allocate memory for: s->control_info\n");
			ERR_DIALOG (N_("Guvcview error:\n\nUnable to allocate Buffers"),
				N_("Please try restarting your system.")); 
   }
    int row=0;

	for (i = 0; i < s->num_controls; i++) {
		ControlInfo * ci = s->control_info + i;
		InputControl * c = s->control + i;

		ci->idx = i;
		ci->widget = NULL;
		ci->label = NULL;
		ci->spinbutton = NULL;
		
		if (c->id == V4L2_CID_EXPOSURE_AUTO_OLD) {
			
			int j=0;
			int val=0;
			/* test available modes */
			int def=0;
			input_get_control (videoIn, c, &def);/*get stored value*/

			for (j=0;j<4;j++) {
				if (input_set_control (videoIn, c, exp_vals[j]) == 0) {
					videoIn->available_exp[val]=j;/*store index to values*/
					val++;
				}
			}
			input_set_control (videoIn, c, def);/*set back to stored*/
			
			ci->widget = gtk_combo_box_new_text ();
			for (j = 0; j <val; j++) {
				gtk_combo_box_append_text (GTK_COMBO_BOX (ci->widget), 
								gettext(exp_typ[videoIn->available_exp[j]]));
				if (def==exp_vals[videoIn->available_exp[j]]){
					gtk_combo_box_set_active (GTK_COMBO_BOX (ci->widget), j);
				}
			}

			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
			gtk_widget_show (ci->widget);

			if (!c->enabled) {
				gtk_widget_set_sensitive (ci->widget, FALSE);
			}
			
			g_signal_connect (G_OBJECT (ci->widget), "changed",
					G_CALLBACK (combo_changed), s);

			ci->label = gtk_label_new (_("Exposure:"));	
			
		} else if ((c->id == V4L2_CID_PAN_RELATIVE_NEW) ||
				   (c->id == V4L2_CID_PAN_RELATIVE_OLD)) {
			videoIn->PanTilt=1;
			ci->widget = gtk_hbox_new (FALSE, 0);
			GtkWidget *PanLeft = gtk_button_new_with_label(_("Left"));
			GtkWidget *PanRight = gtk_button_new_with_label(_("Right"));
			gtk_box_pack_start (GTK_BOX (ci->widget), PanLeft, TRUE, TRUE, 0);
			gtk_box_pack_start (GTK_BOX (ci->widget), PanRight, TRUE, TRUE, 0);
			gtk_widget_show (PanLeft);
			gtk_widget_show (PanRight);
			g_signal_connect (GTK_BUTTON (PanLeft), "clicked",
					G_CALLBACK (PanLeft_clicked), s);
			g_signal_connect (GTK_BUTTON (PanRight), "clicked",
					G_CALLBACK (PanRight_clicked), s);
			
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
			ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
			gtk_widget_show (ci->widget);

			ci->label = gtk_label_new (g_strdup_printf ("%s:", gettext(c->name)));
			
		} else if ((c->id == V4L2_CID_TILT_RELATIVE_NEW) ||
				   (c->id == V4L2_CID_TILT_RELATIVE_OLD)) {
			videoIn->PanTilt=1;
			ci->widget = gtk_hbox_new (FALSE, 0);
			GtkWidget *TiltUp = gtk_button_new_with_label(_("Up"));
			GtkWidget *TiltDown = gtk_button_new_with_label(_("Down"));
			gtk_box_pack_start (GTK_BOX (ci->widget), TiltUp, TRUE, TRUE, 0);
			gtk_box_pack_start (GTK_BOX (ci->widget), TiltDown, TRUE, TRUE, 0);
			gtk_widget_show (TiltUp);
			gtk_widget_show (TiltDown);
			g_signal_connect (GTK_BUTTON (TiltUp), "clicked",
					G_CALLBACK (TiltUp_clicked), s);
			g_signal_connect (GTK_BUTTON (TiltDown), "clicked",
					G_CALLBACK (TiltDown_clicked), s);
			
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
			ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
			gtk_widget_show (ci->widget);

			ci->label = gtk_label_new (g_strdup_printf ("%s:", gettext(c->name)));
			
		} else if (c->id == V4L2_CID_PAN_RESET_NEW) {
			ci->widget = gtk_button_new_with_label(_("Reset"));
			g_signal_connect (GTK_BUTTON (ci->widget), "clicked",
					G_CALLBACK (PReset_clicked), s);
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
			ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
			gtk_widget_show (ci->widget);

			ci->label = gtk_label_new (g_strdup_printf ("%s:", gettext(c->name)));
		
		} else if (c->id == V4L2_CID_TILT_RESET_NEW) {
			ci->widget = gtk_button_new_with_label(_("Reset"));
			g_signal_connect (GTK_BUTTON (ci->widget), "clicked",
					G_CALLBACK (TReset_clicked), s);
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
			ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
			gtk_widget_show (ci->widget);

			ci->label = gtk_label_new (g_strdup_printf ("%s:", gettext(c->name)));
		
		}else if ((c->id == V4L2_CID_PANTILT_RESET_LOGITECH) ||
				   (c->id == V4L2_CID_PANTILT_RESET_OLD)) {
			ci->widget = gtk_hbox_new (FALSE, 0);
			GtkWidget *PTReset = gtk_button_new_with_label(_("Reset"));
			gtk_box_pack_start (GTK_BOX (ci->widget), PTReset, TRUE, TRUE, 0);
			gtk_widget_show (PTReset);
			g_signal_connect (GTK_BUTTON (PTReset), "clicked",
					G_CALLBACK (PTReset_clicked), s);
			
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
			ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
			gtk_widget_show (ci->widget);

			ci->label = gtk_label_new (g_strdup_printf ("%s:", gettext(c->name)));
			
		} else if (c->type == INPUT_CONTROL_TYPE_INTEGER) {
			int val;

			if (c->step == 0)
				c->step = 1;
			ci->widget = gtk_hscale_new_with_range (c->min, c->max, c->step);
			gtk_scale_set_draw_value (GTK_SCALE (ci->widget), FALSE);

			/* This is a hack to use always round the HScale to integer
			 * values.  Strangely, this functionality is normally only
			 * available when draw_value is TRUE. */
			GTK_RANGE (ci->widget)->round_digits = 0;

			g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
			ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
			gtk_widget_show (ci->widget);
			
			
			ci->spinbutton = gtk_spin_button_new_with_range(c->min,c->max,c->step);
		   	g_object_set_data (G_OBJECT (ci->spinbutton), "control_info", ci);
			
		   	/*can't edit the spin value by hand*/
		   	gtk_editable_set_editable(GTK_EDITABLE(ci->spinbutton),FALSE);

			if (input_get_control (videoIn, c, &val) == 0) {
				gtk_range_set_value (GTK_RANGE (ci->widget), val);
			   	gtk_spin_button_set_value (GTK_SPIN_BUTTON(ci->spinbutton), val);
			}
			else {
				/*couldn't get control value -> set to default*/
				input_set_control (videoIn, c, c->default_val);
				gtk_range_set_value (GTK_RANGE (ci->widget), c->default_val);
				gtk_spin_button_set_value (GTK_SPIN_BUTTON(ci->spinbutton), c->default_val);
			   	gtk_widget_set_sensitive (ci->widget, TRUE);
				gtk_widget_set_sensitive (ci->spinbutton, TRUE);
			}

			if (!c->enabled) {
				gtk_widget_set_sensitive (ci->widget, FALSE);
				gtk_widget_set_sensitive (ci->spinbutton, FALSE);
			}
			
			//set_spin_value (GTK_RANGE (ci->widget));
			g_signal_connect (G_OBJECT (ci->widget), "value-changed",
					G_CALLBACK (slider_changed), s);
		   	g_signal_connect (G_OBJECT (ci->spinbutton),"value-changed",
					G_CALLBACK (spin_changed), s);

			gtk_widget_show (ci->spinbutton);

			ci->label = gtk_label_new (g_strdup_printf ("%s:", gettext(c->name)));
		    	/* ---- Add auto-focus checkbox and focus button ----- */
		    	if (c->id== V4L2_CID_FOCUS_LOGITECH) {
				global->AFcontrol=1;
				GtkWidget *Focus_box = gtk_hbox_new (FALSE, 0);
				GtkWidget *AutoFocus = gtk_check_button_new_with_label (_("Auto Focus (continuous)"));
				GtkWidget *FocusButton = gtk_button_new_with_label (_("set Focus"));
				gtk_box_pack_start (GTK_BOX (Focus_box), AutoFocus, TRUE, TRUE, 0);
				gtk_box_pack_start (GTK_BOX (Focus_box), FocusButton, TRUE, TRUE, 0);
				gtk_widget_show (Focus_box);
				gtk_widget_show (AutoFocus);
				gtk_widget_show (FocusButton);
				gtk_table_attach (GTK_TABLE (s->table), Focus_box, 1, 2, 3+row, 4+row,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			
				
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (AutoFocus),
							      global->autofocus ? TRUE: FALSE);

				g_object_set_data (G_OBJECT (AutoFocus), "control_info", ci);
				g_object_set_data (G_OBJECT (FocusButton), "control_info", ci);
				
				g_signal_connect (G_OBJECT (AutoFocus), "toggled",
					G_CALLBACK (autofocus_changed), s);
				g_signal_connect (G_OBJECT (FocusButton), "clicked",
					G_CALLBACK (setfocus_clicked), s);
				row++; /*increment control row*/
			
			}
		    	gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
		    
		    	gtk_table_attach (GTK_TABLE (s->table), ci->spinbutton, 2, 3,
					3+row, 4+row, GTK_SHRINK | GTK_FILL, 0, 0, 0);
		    	
		} else if(c->id ==V4L2_CID_DISABLE_PROCESSING_LOGITECH) {
		      	int val;
		      	ci->widget = gtk_vbox_new (FALSE, 0);
		      	GtkWidget *check_bayer = gtk_check_button_new_with_label (gettext(c->name));
		      	g_object_set_data (G_OBJECT (check_bayer), "control_info", ci);
		      	GtkWidget *pix_ord = gtk_combo_box_new_text ();
		      	gtk_combo_box_append_text(GTK_COMBO_BOX(pix_ord),"GBGB... | RGRG...");
		        gtk_combo_box_append_text(GTK_COMBO_BOX(pix_ord),"GRGR... | BGBG...");
		        gtk_combo_box_append_text(GTK_COMBO_BOX(pix_ord),"BGBG... | GRGR...");
		        gtk_combo_box_append_text(GTK_COMBO_BOX(pix_ord),"RGRG... | GBGB...");
		      	
		      	
		      	/* auto set pix order for 2Mp logitech cameras */
		      	if((videoIn->width == 160) || (videoIn->width == 176) || (videoIn->width == 352) || (videoIn->width == 960)) 
			      videoIn->pix_order=3; /* rg */
			else videoIn->pix_order=0; /* gb */
		      
		      	gtk_combo_box_set_active(GTK_COMBO_BOX(pix_ord),videoIn->pix_order);
		      	
		        gtk_box_pack_start (GTK_BOX (ci->widget), check_bayer, TRUE, TRUE, 0);
			gtk_box_pack_start (GTK_BOX (ci->widget), pix_ord, TRUE, TRUE, 0);
			gtk_widget_show (check_bayer);
			gtk_widget_show (pix_ord);
			g_signal_connect (GTK_TOGGLE_BUTTON (check_bayer), "toggled",
					G_CALLBACK (bayer_changed), s);
			g_signal_connect (GTK_COMBO_BOX (pix_ord), "changed",
					G_CALLBACK (pix_ord_changed), s);
			
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			ci->maxchars = MAX (num_chars (c->min), num_chars (c->max));
			gtk_widget_show (ci->widget);

		      	ci->label = gtk_label_new (g_strdup_printf (_("raw pixel order:")));
		      	
		        if (input_get_control (videoIn, c, &val) == 0) {
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_bayer),
						val ? TRUE : FALSE);
			   	if(val>0) {
			   		if (global->debug) {
					      printf("bayer mode set\n");
					}
					videoIn->isbayer=1;
				}
			}

			if (!c->enabled) {
				gtk_widget_set_sensitive (ci->widget, FALSE);
			}
		
		}
		else if (c->type == INPUT_CONTROL_TYPE_BOOLEAN) {
			int val;
			ci->widget = gtk_check_button_new_with_label (gettext(c->name));
			g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
			gtk_widget_show (ci->widget);
			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 3, 3+row, 4+row,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

			if (input_get_control (videoIn, c, &val) == 0) {
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
						val ? TRUE : FALSE);
			}
			else {
				/*couldn't get control value -> set to default*/
				input_set_control (videoIn, c, c->default_val);
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ci->widget),
						c->default_val ? TRUE : FALSE);
				gtk_widget_set_sensitive (ci->widget, TRUE);
			}

			if (!c->enabled) {
				gtk_widget_set_sensitive (ci->widget, FALSE);
			}
			
			g_signal_connect (G_OBJECT (ci->widget), "toggled",
					G_CALLBACK (check_changed), s);
		}
		else if (c->type == INPUT_CONTROL_TYPE_MENU) {
			int val, j;

			ci->widget = gtk_combo_box_new_text ();
			for (j = 0; j <= c->max; j++) {
				gtk_combo_box_append_text (GTK_COMBO_BOX (ci->widget), gettext(c->entries[j]));
			}

			gtk_table_attach (GTK_TABLE (s->table), ci->widget, 1, 2, 3+row, 4+row,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
			g_object_set_data (G_OBJECT (ci->widget), "control_info", ci);
			gtk_widget_show (ci->widget);

			if (input_get_control (videoIn, c, &val) == 0) {
				gtk_combo_box_set_active (GTK_COMBO_BOX (ci->widget), val);
			}
			else {
				/*couldn't get control value -> set to default*/
				input_set_control (videoIn, c, c->default_val);
				gtk_combo_box_set_active (GTK_COMBO_BOX (ci->widget), c->default_val);
				gtk_widget_set_sensitive (ci->widget, TRUE);
			}

			if (!c->enabled) {
				gtk_widget_set_sensitive (ci->widget, FALSE);
			}
			
			g_signal_connect (G_OBJECT (ci->widget), "changed",
					G_CALLBACK (combo_changed), s);

			ci->label = gtk_label_new (g_strdup_printf ("%s:", gettext(c->name)));
		}
		else {
			printf ("TODO: implement button\n");
			continue;
		}

		if (ci->label) {
			gtk_misc_set_alignment (GTK_MISC (ci->label), 1, 0.5);

			gtk_table_attach (GTK_TABLE (s->table), ci->label, 0, 1, 3+row, 4+row,
					GTK_FILL, 0, 0, 0);

			gtk_widget_show (ci->label);
		}
	    	row++; /*increment control row*/
	}

}


/*-------------------------------- Main Video Loop ---------------------------*/ 
/* run in a thread (SDL overlay)*/
static void *main_loop(void *data)
{
	size_t videostacksize;
	SDL_Event event;
	/*the main SDL surface*/
	SDL_Surface *pscreen = NULL;
	SDL_Overlay *overlay=NULL;
	SDL_Rect drect;
	const SDL_VideoInfo *info;
	char driver[128];
        
	BYTE *p = NULL;
	BYTE *pim= NULL;
	BYTE *pavi=NULL;
	
	
    	int keyframe = 1;
    	
    	int last_focus = 0;
    	if (global->AFcontrol) {
    		last_focus = get_focus();
    		if (last_focus < 0) last_focus=255; /*make sure we wait for focus to settle on first check*/
		//printf("last_focus is %d and focus is %d\n",last_focus, AFdata->focus);
	}
    
	/*gets the stack size for the thread (DEBUG)*/ 
	pthread_attr_getstacksize (&attr, &videostacksize);
	if (global->debug) printf("Video Thread: stack size = %d bytes \n", (int) videostacksize);
	
	static Uint32 SDL_VIDEO_Flags =
		SDL_ANYFORMAT | SDL_DOUBLEBUF | SDL_RESIZABLE;
 
	/*----------------------------- Test SDL capabilities ---------------------*/
	if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
		printf("Couldn't initialize SDL: %s\n", SDL_GetError());
		exit(1);
	}
	
	/* For this version, we will use hardware acceleration as default*/
	if(global->hwaccel) {
		if ( ! getenv("SDL_VIDEO_YUV_HWACCEL") ) {
			putenv("SDL_VIDEO_YUV_HWACCEL=1");
		}
		if ( ! getenv("SDL_VIDEO_YUV_DIRECT") ) {
			putenv("SDL_VIDEO_YUV_DIRECT=1"); 
		}
	 } else {
		if ( ! getenv("SDL_VIDEO_YUV_HWACCEL") ) {
			putenv("SDL_VIDEO_YUV_HWACCEL=0");
		}
		if ( ! getenv("SDL_VIDEO_YUV_DIRECT") ) {
			putenv("SDL_VIDEO_YUV_DIRECT=0"); 
		}
	 }
	 
	if (SDL_VideoDriverName(driver, sizeof(driver))) {
		if (global->debug) printf("Video driver: %s\n", driver);
	}
	info = SDL_GetVideoInfo();

	if (info->wm_available) {
		if (global->debug) printf("A window manager is available\n");
	}
	if (info->hw_available) {
		if (global->debug) printf("Hardware surfaces are available (%dK video memory)\n",
		   info->video_mem);
		SDL_VIDEO_Flags |= SDL_HWSURFACE;
	}
	if (info->blit_hw) {
		if (global->debug) printf("Copy blits between hardware surfaces are accelerated\n");
		SDL_VIDEO_Flags |= SDL_ASYNCBLIT;
	}
    	if (global->debug) {
		if (info->blit_hw_CC) {
			printf ("Colorkey blits between hardware surfaces are accelerated\n");
		}
		if (info->blit_hw_A) {
			printf("Alpha blits between hardware surfaces are accelerated\n");
		}
		if (info->blit_sw) {
			printf ("Copy blits from software surfaces to hardware surfaces are accelerated\n");
		}
		if (info->blit_sw_CC) {
			printf ("Colorkey blits from software surfaces to hardware surfaces are accelerated\n");
		}
		if (info->blit_sw_A) {
			printf("Alpha blits from software surfaces to hardware surfaces are accelerated\n");
		}
		if (info->blit_fill) {
			printf("Color fills on hardware surfaces are accelerated\n");
		}
	}

	if (!(SDL_VIDEO_Flags & SDL_HWSURFACE)){
		SDL_VIDEO_Flags |= SDL_SWSURFACE;
	}
        
        SDL_WM_SetCaption(global->WVcaption, NULL); 
   
   	/* enable key repeat */
   	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,SDL_DEFAULT_REPEAT_INTERVAL);
	 
	/*------------------------------ SDL init video ---------------------*/
	pscreen =
	SDL_SetVideoMode(videoIn->width, videoIn->height, global->bpp,
			 SDL_VIDEO_Flags);
	overlay =
	SDL_CreateYUVOverlay(videoIn->width, videoIn->height,
				 SDL_YUY2_OVERLAY, pscreen);
	
	p = (unsigned char *) overlay->pixels[0];
	
	drect.x = 0;
	drect.y = 0;
	drect.w = pscreen->w;
	drect.h = pscreen->h;
	 
	while (videoIn->signalquit) {
	 /*-------------------------- Grab Frame ----------------------------------*/
	 if (uvcGrab(videoIn) < 0) {
		printf("Error grabbing image \n");
		videoIn->signalquit=0;
		snprintf(global->WVcaption,20,"GUVCVideo - CRASHED");
		SDL_WM_SetCaption(global->WVcaption, NULL);
		pthread_exit((void *) 2);
	 } else {
		if (global->FpsCount) {/* sets fps count in window title bar */
			global->frmCount++;
			if (global->DispFps>0) { /*set every 2 sec*/
				snprintf(global->WVcaption,24,"GUVCVideo - %3.2f fps",global->DispFps);
				SDL_WM_SetCaption(global->WVcaption, NULL);
				global->frmCount=0;/*resets*/
				global->DispFps=0;
			}				
		}
	     	/*---------------- autofocus control ------------------*/
		
		if (global->AFcontrol && (global->autofocus || AFdata->setFocus)) { /*AFdata = NULL if no focus control*/
			if (AFdata->focus < 0) {
			    /*starting autofocus*/
			    AFdata->focus = AFdata->left; /*start left*/
			    if (set_focus (AFdata->focus) != 0) printf("ERROR: couldn't set focus to %d\n", AFdata->focus);
			    /*number of frames until focus is stable*/
			    AFdata->focus_wait = (int) abs(AFdata->focus-last_focus)*1.4/(1000/videoIn->fps)+1; /*1.4 ms focus time - every 1 step*/
			    last_focus = AFdata->focus;
			} else {
		    		if (AFdata->focus_wait == 0) {
		    			AFdata->sharpness=getSharpMeasure (videoIn->framebuffer, videoIn->width, videoIn->height, 6);
		    			if (global->debug) printf("sharp=%d focus_sharp=%d foc=%d right=%d left=%d ind=%d flag=%d\n",AFdata->sharpness,AFdata->focus_sharpness,AFdata->focus, AFdata->right, AFdata->left, AFdata->ind, AFdata->flag);
		    			AFdata->focus=getFocusVal (AFdata);
					if ((AFdata->focus != last_focus)) {
			    			if (set_focus (AFdata->focus) != 0) printf("ERROR: couldn't set focus to %d\n", AFdata->focus);
			    			/*number of frames until focus is stable*/
			    			AFdata->focus_wait = (int) abs(AFdata->focus-last_focus)*1.4/(1000/videoIn->fps)+1; /*1.4 ms focus time - every 1 step*/
					}
		    			last_focus = AFdata->focus;
				} else {
					AFdata->focus_wait--;
			    		if (global->debug) printf("Wait Frame: %d\n",AFdata->focus_wait);
				}
			}
		    
		}
	 }
	
	 /*------------------------- Filter Frame ---------------------------------*/
	 if(global->Frame_Flags>0){
		if((global->Frame_Flags & YUV_MIRROR)==YUV_MIRROR)
			yuyv_mirror(videoIn->framebuffer,videoIn->width,videoIn->height);
		if((global->Frame_Flags & YUV_UPTURN)==YUV_UPTURN)
			yuyv_upturn(videoIn->framebuffer,videoIn->width,videoIn->height);
		if((global->Frame_Flags & YUV_NEGATE)==YUV_NEGATE)
			yuyv_negative (videoIn->framebuffer,videoIn->width,videoIn->height);
		if((global->Frame_Flags & YUV_MONOCR)==YUV_MONOCR)
			 yuyv_monochrome (videoIn->framebuffer,videoIn->width,videoIn->height);
	 }
	
	 /*-------------------------capture Image----------------------------------*/
	 //char fbasename[20];
	 if (videoIn->capImage){
		 switch(global->imgFormat) {
		 case 0:/*jpg*/
			/* Save directly from MJPG frame */	 
			if((global->Frame_Flags==0) && (videoIn->formatIn==V4L2_PIX_FMT_MJPEG)) {
				if(SaveJPG(videoIn->ImageFName,videoIn->buf.bytesused,videoIn->tmpbuffer)) {
					fprintf (stderr,"Error: Couldn't capture Image to %s \n",
						videoIn->ImageFName);		
				}
				
			} else { /* use built in encoder */
				if (!global->jpeg){ 
					if((global->jpeg = (BYTE*)malloc(global->jpeg_bufsize))==NULL) {
						printf("couldn't allocate memory for: jpeg buffer\n");
						exit(1);
					}				
				}
				if(!jpeg_struct) {
					if((jpeg_struct =(struct JPEG_ENCODER_STRUCTURE *) calloc(1, sizeof(struct JPEG_ENCODER_STRUCTURE)))==NULL){
						printf("couldn't allocate memory for: jpeg encoder struct\n");
						exit(1); 
					} else {
						/* Initialization of JPEG control structure */
						initialization (jpeg_struct,videoIn->width,videoIn->height);
	
						/* Initialization of Quantization Tables  */
						initialize_quantization_tables (jpeg_struct);
					}
				} 
				global->jpeg_size = encode_image(videoIn->framebuffer, global->jpeg, 
								jpeg_struct,1, videoIn->width, videoIn->height);
			 
				if(SaveBuff(videoIn->ImageFName,global->jpeg_size,global->jpeg)) { 
					fprintf (stderr,"Error: Couldn't capture Image to %s \n",
					videoIn->ImageFName);		
				}
			}
			break;
		 case 1:/*bmp*/
			if(pim==NULL) {  
				 /*24 bits -> 3bytes     32 bits ->4 bytes*/
				if((pim= malloc((pscreen->w)*(pscreen->h)*3))==NULL){
					printf("Couldn't allocate memory for: pim\n");
					videoIn->signalquit=0;
					pthread_exit((void *) 3);		
				}
			}
			yuyv2bgr(videoIn->framebuffer,pim,videoIn->width,videoIn->height);

			if(SaveBPM(videoIn->ImageFName, videoIn->width, videoIn->height, 24, pim)) {
				  fprintf (stderr,"Error: Couldn't capture Image to %s \n",
				  videoIn->ImageFName);
			} 
			else {	  
				//printf ("Capture BMP Image to %s \n",videoIn->ImageFName);
			}
			break;
		 case 2:/*png*/
			if(pim==NULL) {  
				 /*24 bits -> 3bytes     32 bits ->4 bytes*/
				if((pim= malloc((pscreen->w)*(pscreen->h)*3))==NULL){
					printf("Couldn't allocate memory for: pim\n");
					videoIn->signalquit=0;
					pthread_exit((void *) 3);		
				}
			}
			 yuyv2rgb(videoIn->framebuffer,pim,videoIn->width,videoIn->height);
			 write_png(videoIn->ImageFName, videoIn->width, videoIn->height,pim);
		 }
		 videoIn->capImage=FALSE;
		 if (global->debug) printf("saved image to:%s\n",videoIn->ImageFName);
	  }
	  
	  /*---------------------------capture AVI---------------------------------*/
	  if (videoIn->capAVI && videoIn->signalquit){
	   long framesize;  
	   switch (global->AVIFormat) {
		   
		case 0: /*MJPG*/
			/* save MJPG frame */   
			if((global->Frame_Flags==0) && (videoIn->formatIn==V4L2_PIX_FMT_MJPEG)) {
				//printf("avi write frame\n");
				if (AVI_write_frame (AviOut, videoIn->tmpbuffer, videoIn->buf.bytesused, keyframe) < 0) {
				 	if (AVI_getErrno () == AVI_ERR_SIZELIM) {
						/*avi file limit reached - must end capture and close file*/
					     	capture_avi(GTK_BUTTON(CapAVIButt), NULL); /*avi capture callback*/
					     	printf("AVI file size limit reached - avi capture stoped\n");
					} else {
						printf ("write error on avi out \n");
					}
				}
			} else {  /* use built in encoder */ 
				if (!global->jpeg){ 
					if((global->jpeg = (BYTE*)malloc(global->jpeg_bufsize))==NULL) {
						printf("couldn't allocate memory for: jpeg buffer\n");
						exit(1);
					}				
				}
				if(!jpeg_struct) {
					if((jpeg_struct =(struct JPEG_ENCODER_STRUCTURE *) calloc(1, sizeof(struct JPEG_ENCODER_STRUCTURE)))==NULL){
						printf("couldn't allocate memory for: jpeg encoder struct\n");
						exit(1); 
					} else {
						/* Initialization of JPEG control structure */
						initialization (jpeg_struct,videoIn->width,videoIn->height);
	
						/* Initialization of Quantization Tables  */
						initialize_quantization_tables (jpeg_struct);
					}
				} 
				global->jpeg_size = encode_image(videoIn->framebuffer, global->jpeg, 
								jpeg_struct,1, videoIn->width, videoIn->height);
			
				if (AVI_write_frame (AviOut, global->jpeg, global->jpeg_size, keyframe) < 0) {
					if (AVI_getErrno () == AVI_ERR_SIZELIM) {
						/*avi file limit reached - must end capture and close file*/
					     	capture_avi(GTK_BUTTON(CapAVIButt), NULL); /*avi capture callback*/
					     	printf("AVI file size limit reached - avi capture stoped\n");
					} else {
						printf ("write error on avi out \n");
					}
					
				}
			}
			break;
		case 1:
		   framesize=(pscreen->w)*(pscreen->h)*2; /*YUY2 -> 2 bytes per pixel */
		   if (AVI_write_frame (AviOut, p, framesize, keyframe) < 0) {
		   	if (AVI_getErrno () == AVI_ERR_SIZELIM) {
				/*avi file limit reached - must end capture and close file*/
				capture_avi(GTK_BUTTON(CapAVIButt), NULL); /*avi capture callback*/
				printf("AVI file size limit reached - avi capture stoped\n");
			} else {
				printf ("write error on avi out \n");
			}
					
		   }
		   break;
		case 2:
			framesize=(pscreen->w)*(pscreen->h)*3; /*DIB 24/32 -> 3/4 bytes per pixel*/ 
			if(pavi==NULL){
			  if((pavi= malloc(framesize))==NULL){
				printf("Couldn't allocate memory for: pim\n");
				videoIn->signalquit=0;
				pthread_exit((void *) 3);
			  }
			}
			yuyv2bgr(videoIn->framebuffer,pavi,videoIn->width,videoIn->height); 
			if (AVI_write_frame (AviOut,pavi, framesize, keyframe) < 0) {
				if (AVI_getErrno () == AVI_ERR_SIZELIM) {
					/*avi file limit reached - must end capture and close file*/
					capture_avi(GTK_BUTTON(CapAVIButt), NULL); /*avi capture callback*/
					printf("AVI file size limit reached - avi capture stoped\n");
				} else {
					printf ("write error on avi out \n");
				}
					
			}
			break;

		} 
	   global->framecount++;
	   if (keyframe) keyframe=0; /*resets key frame*/   
	   /*add audio*/
	   if ((global->Sound_enable) && (global->audio_flag>0)) {
	       if (!(AviOut->audio_bytes)) { /*first audio data - sync with video (audio thread allawys starts after video)*/
	       		int synctime= global->snd_begintime - global->AVIstarttime; /*time diff for audio-video*/
			if(synctime>0 && synctime<5000) { /*only sync up to 5 seconds*/
			/*shift sound by synctime*/
			Uint32 shiftFrames=abs(synctime*global->Sound_SampRate/1000);
			Uint32 shiftSamples=shiftFrames*global->Sound_NumChan;
			if (global->debug) printf("shift sound forward by %d ms = %d frames\n",synctime,shiftSamples);
			SAMPLE EmptySamp[shiftSamples];
			int i;
			for(i=0; i<shiftSamples; i++) EmptySamp[i]=0;/*init to zero - silence*/
				AVI_write_audio(AviOut,(BYTE *) &EmptySamp,shiftSamples*sizeof(SAMPLE));
	       		}
	       }
	       if(AVI_write_audio(AviOut,(BYTE *) global->avi_sndBuff,global->snd_numBytes) < 0) {
	       		if (AVI_getErrno () == AVI_ERR_SIZELIM) {
				/*avi file limit reached - must end capture and close file*/
				capture_avi(GTK_BUTTON(CapAVIButt), NULL); /*avi capture callback*/
				printf("AVI file size limit reached - avi capture stoped\n");
			} else {
				printf ("write error on avi out \n");
			}
					
	       }
	       global->audio_flag=0;
	       keyframe = 1; /*marks next frmae as key frame*/
	   }   
	      
	  } 
	/*------------------------- Display Frame --------------------------------*/
	 SDL_LockYUVOverlay(overlay);
	 memcpy(p, videoIn->framebuffer,
		   videoIn->width * (videoIn->height) * 2);
	 SDL_UnlockYUVOverlay(overlay);
	 SDL_DisplayYUVOverlay(overlay, &drect);
	 
	/*sleep for a while*/
	if(global->vid_sleep)
		SDL_Delay(global->vid_sleep);
		
	/*------------------------- Read Key events ------------------------------*/
	if (videoIn->PanTilt) {
		/* Poll for events */
    		while( SDL_PollEvent(&event) ){
			if(event.type==SDL_KEYDOWN) {   
                		switch( event.key.keysym.sym ){
                    		/* Keyboard event */
                    		/* Pass the event data onto PrintKeyInfo() */
                    		case SDLK_DOWN:
					/*Tilt Down*/
					uvcPanTilt (videoIn,0,INCPANTILT*(global->TiltStep),0);
				break;
                    		case SDLK_UP:
                        		/*Tilt UP*/
					uvcPanTilt (videoIn,0,-INCPANTILT*(global->TiltStep),0);
                        		break;
		    		case SDLK_LEFT:
					/*Pan Left*/
					uvcPanTilt (videoIn,-INCPANTILT*(global->PanStep),0,0);
					break;
		    		case SDLK_RIGHT:
					/*Pan Right*/
					uvcPanTilt (videoIn,INCPANTILT*(global->PanStep),0,0);
					break;
                    		default:
                        		break;
                		}
			}

        	}
	}

	
  }/*loop end*/
    
  /*check if thread exited while AVI in capture mode*/
  if (videoIn->capAVI) {
	global->AVIstoptime = ms_time();
	videoIn->capAVI = FALSE;   
  }	   
  if (global->debug) printf("Thread terminated...\n");
  
  if(pim!=NULL) free(pim);
  pim=NULL;
  if(pavi!=NULL) free(pavi);
  pavi=NULL;
  if (global->debug) printf("cleaning Thread allocations: 100%%\n");
  fflush(NULL);//flush all output buffers  
  SDL_Quit();   
  if (global->debug) printf("SDL Quit\n");	
  pthread_exit((void *) 0);
}

/*--------------------------------- MAIN -------------------------------------*/
int main(int argc, char *argv[])
{  
	int i;
    	int ret=0;
	printf("guvcview version %s \n", VERSION);
   
#ifdef ENABLE_NLS
	char* lc_all = setlocale (LC_ALL, "");
	char* lc_dir = bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	char* txtdom = textdomain (GETTEXT_PACKAGE);
#endif
	
	/*stores argv[0] - program call string - for restart*/
	int exec_size=strlen(argv[0])*sizeof(char)+1;
	if((EXEC_CALL=malloc(exec_size))==NULL) {
		printf("couldn't allocate memory for: EXEC_CALL)\n");
		exit(1);
	}
	snprintf(EXEC_CALL,exec_size,argv[0]);
	
	/*set global variables*/
	if((global=(struct GLOBAL *) calloc(1, sizeof(struct GLOBAL)))==NULL){
		printf("couldn't allocate memory for: global\n");
		exit(1); 
	}
   
	initGlobals(global);
	
   	/* widgets */
	GtkWidget *scroll1;
	GtkWidget *scroll2;
	GtkWidget *buttons_table;
	GtkWidget *profile_labels;
	GtkWidget *capture_labels;
	GtkWidget *Resolution;
	GtkWidget *FrameRate;
	GtkWidget *ShowFPS;
	GtkWidget *ImpType;
	GtkWidget *label_ImpType;
	GtkWidget *label_FPS;
	GtkWidget *table2;
	GtkWidget *labelResol;
	GtkWidget *AviFileButt;
	GtkWidget *label_ImageType;
	GtkWidget *label_AVIComp;
	GtkWidget *label_SndSampRate;
	GtkWidget *label_SndDevice;
	GtkWidget *label_SndNumChan;
	GtkWidget *label_videoFilters;
	GtkWidget *table3;
	GtkWidget *quitButton;
	GtkWidget *SProfileButton;
	GtkWidget *LProfileButton;
	GtkWidget *Tab1Label;
	GtkWidget *Tab2Label;
	GtkWidget *label_ImgFile;
	GtkWidget *label_AVIFile;
	GtkWidget *AVIButton_Img;
	GtkWidget *ImgButton_Img;
   	GtkWidget *SButton_Img;
   	GtkWidget *LButton_Img;
   	GtkWidget *QButton_Img;
   	GtkWidget *HButtonBox;
	
	size_t stacksize;
	
   
	if ((s = malloc (sizeof (VidState)))==NULL){
		printf("couldn't allocate memory for: s\n");
		exit(1); 
	}
	   
	char *home;
	char *pwd=NULL;
	
	home = getenv("HOME");
	//pwd = getenv("PWD");
	pwd=getcwd(pwd,0);
	
	sprintf(global->confPath,"%s%s", home,"/.guvcviewrc");
	sprintf(global->aviFPath[1],"%s", pwd);
	sprintf(global->imgFPath[1],"%s", pwd);
	  
	readConf(global->confPath);
    
	/*------------------------ reads command line options --------------------*/
	readOpts(argc,argv);
   
#ifdef ENABLE_NLS
   	/* if --verbose mode set do debug*/
	if (global->debug) printf("language catalog=> dir:%s lang:%s cat:%s.mo\n",lc_dir,lc_all,txtdom);
#endif   
	/*---------------------------- GTK init ----------------------------------*/
	
	gtk_init(&argc, &argv);
	

	/* Create a main window */
	mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (mainwin), _("GUVCViewer Controls"));
	//gtk_widget_set_usize(mainwin, winwidth, winheight);
	gtk_window_resize(GTK_WINDOW(mainwin),global->winwidth,global->winheight);
	/* Add event handlers */
	gtk_signal_connect(GTK_OBJECT(mainwin), "delete_event", GTK_SIGNAL_FUNC(delete_event), 0);
	
	//gtk_widget_show (mainwin);
	/*----------------------- init videoIn structure --------------------------*/	
	if((videoIn = (struct vdIn *) calloc(1, sizeof(struct vdIn)))==NULL){
		printf("couldn't allocate memory for: videoIn\n");
		exit(1); 
	}
	if ((ret=init_videoIn
		(videoIn, (char *) global->videodevice, global->width,global->height, 
	 	global->format, global->grabmethod, global->fps, global->fps_num) )< 0)
	{
	    switch (ret) {
	    	case -1:/*can't open device*/
		  	ERR_DIALOG (N_("Guvcview error:\n\nUnable to open device"),
				N_("Please make sure the camera is connected\nand that the linux-UVC driver is installed."));
			printf("Shouldn't get to here\n");
			break;
		case -2:/*invalid format*/
			printf("trying minimum setup...\n");
			if ((global->formind==0) && (videoIn->SupMjpg>0)) { /*use jpg mode*/
				global->formind=0;
				global->format=V4L2_PIX_FMT_MJPEG;
				snprintf(global->mode, 4, "jpg");
			} else {
				if ((global->formind==1) && (videoIn->SupYuv>0)) { /*use yuv mode*/
					global->formind=1;
					global->format=V4L2_PIX_FMT_YUYV;
					snprintf(global->mode, 4, "yuv");
				} else { /*selected mode isn't available*/
					/*check available modes*/
					if(videoIn->SupMjpg>0){
						global->formind=0;
						global->format=V4L2_PIX_FMT_MJPEG;
						snprintf(global->mode, 4, "jpg");
					} else { 
						if (videoIn->SupYuv>0) {
							global->formind=1;
							global->format=V4L2_PIX_FMT_YUYV;
							snprintf(global->mode, 4, "yuv");
						} else {
							printf("ERROR: Can't set MJPG or YUV stream.\nExiting...\n");
						   	ERR_DIALOG (N_("Guvcview error:\n\nCan't set MJPG or YUV stream for guvcview"),
								N_("Make sure you have a UVC compliant camera\nand that you have the linux UVC driver installed."));
						}
					}
				}
			}
			global->width=videoIn->listVidCap[global->formind][0].width;
			global->height=videoIn->listVidCap[global->formind][0].height;
			global->fps_num=videoIn->listVidCap[global->formind][0].framerate_num[0];
			global->fps=videoIn->listVidCap[global->formind][0].framerate_denom[0];
			if (init_videoIn
				(videoIn, (char *) global->videodevice, global->width,global->height, 
	 			global->format, global->grabmethod, global->fps, global->fps_num) < 0)
			{
				printf("ERROR: Minimum Setup Failed.\n Exiting...\n");
			   	ERR_DIALOG (N_("Guvcview error:\n\nUnable to start with minimum setup"),
					N_("Please reconnect your camera."));
			}
		    	break;
		case -3:/*unable to allocate dequeue buffers or mem*/
		case -4:
		case -6:
		default:
		     	ERR_DIALOG (N_("Guvcview error:\n\nUnable to allocate Buffers"),
				N_("Please try restarting your system."));
			break;
		}
	}
			

	/* Set jpeg encoder buffer size */
	global->jpeg_bufsize=((videoIn->width)*(videoIn->height))>>1;

	
	/*-----------------------------GTK widgets---------------------------------*/
	/*----- Left Table -----*/
	s->table = gtk_table_new (1, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (s->table), 4);
	gtk_table_set_col_spacings (GTK_TABLE (s->table), 4);
	gtk_container_set_border_width (GTK_CONTAINER (s->table), 2);
	
	s->control = NULL;
	draw_controls(s);
	if (global->lprofile > 0) LoadControls (s);
	
	boxv = gtk_vpaned_new ();
	
	boxh = gtk_notebook_new();	
   
	gtk_widget_show (s->table);
	gtk_widget_show (boxh);
	
	scroll1=gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll1),s->table);
	gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(scroll1),
                                                                GTK_CORNER_TOP_LEFT);
	
	gtk_widget_show(scroll1);
	
   	Tab1Label = gtk_label_new(_("Image Controls"));
	gtk_notebook_append_page(GTK_NOTEBOOK(boxh),scroll1,Tab1Label);
   
   	gtk_paned_add1(GTK_PANED(boxv),boxh);
	
	gtk_widget_show (boxv);
	
	/*----- Add  Buttons -----*/
	buttons_table = gtk_table_new(1,5,FALSE);
	HButtonBox = gtk_hbutton_box_new();
   	gtk_button_box_set_layout(GTK_BUTTON_BOX(HButtonBox),GTK_BUTTONBOX_SPREAD);	
   	gtk_box_set_homogeneous(GTK_BOX(HButtonBox),TRUE);
      
	gtk_table_set_row_spacings (GTK_TABLE (buttons_table), 1);
	gtk_table_set_col_spacings (GTK_TABLE (buttons_table), 4);
	gtk_container_set_border_width (GTK_CONTAINER (buttons_table), 1);
	
	gtk_widget_show (buttons_table);
	gtk_paned_add2(GTK_PANED(boxv),buttons_table);
	
	profile_labels=gtk_label_new(_("Control Profiles:"));
	gtk_misc_set_alignment (GTK_MISC (profile_labels), 0.5, 0.5);

	gtk_table_attach (GTK_TABLE(buttons_table), profile_labels, 2, 4, 0, 1,
				GTK_SHRINK | GTK_FILL | GTK_EXPAND , 0, 0, 0);
    
    	capture_labels=gtk_label_new(_("Capture:"));
	gtk_misc_set_alignment (GTK_MISC (capture_labels), 0.5, 0.5);
    	gtk_table_attach (GTK_TABLE(buttons_table), capture_labels, 0, 2, 0, 1,
				GTK_SHRINK | GTK_FILL | GTK_EXPAND, 0, 0, 0);
   
   	gtk_table_attach(GTK_TABLE(buttons_table), HButtonBox, 0, 5, 1, 2,
				GTK_SHRINK | GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_widget_show(HButtonBox);
   
	gtk_widget_show (capture_labels);
    	gtk_widget_show (profile_labels);
	
	quitButton=gtk_button_new_from_stock(GTK_STOCK_QUIT);
	SProfileButton=gtk_button_new_from_stock(GTK_STOCK_SAVE);
	LProfileButton=gtk_button_new_from_stock(GTK_STOCK_OPEN);
    
    	if(global->image_timer){ /*image auto capture*/
    		CapImageButt=gtk_button_new_with_label (_("Stop Auto"));
	} else {
		CapImageButt=gtk_button_new_with_label (_("Cap. Image"));
	}
    	
    	if (global->avifile) {	/*avi capture enabled from start*/
		CapAVIButt=gtk_button_new_with_label (_("Stop AVI"));
	} else {
		CapAVIButt=gtk_button_new_with_label (_("Cap. AVI"));
	}
    
	/*add images to Buttons and top window*/
	/*check for files*/
        gchar* icon1path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/guvcview.xpm",NULL);
        if (g_file_test(icon1path,G_FILE_TEST_EXISTS)) {
             gtk_window_set_icon_from_file(GTK_WINDOW (mainwin),icon1path,NULL);   
        }

    	gchar* pix1path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/movie.xpm",NULL);
	if (g_file_test(pix1path,G_FILE_TEST_EXISTS)) {
		AVIButton_Img = gtk_image_new_from_file (pix1path);
		gtk_button_set_image(GTK_BUTTON(CapAVIButt),AVIButton_Img);
	   	gtk_button_set_image_position(GTK_BUTTON(CapAVIButt),GTK_POS_TOP);
	} 
	gchar* pix2path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/camera.xpm",NULL);
	if (g_file_test(pix2path,G_FILE_TEST_EXISTS)) {
		ImgButton_Img = gtk_image_new_from_file (pix2path);
		gtk_button_set_image(GTK_BUTTON(CapImageButt),ImgButton_Img);
	   	gtk_button_set_image_position(GTK_BUTTON(CapImageButt),GTK_POS_TOP);
	}
   	gchar* pix3path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/save.xpm",NULL);
	if (g_file_test(pix3path,G_FILE_TEST_EXISTS)) {
		SButton_Img = gtk_image_new_from_file (pix3path);
		gtk_button_set_image(GTK_BUTTON(SProfileButton),SButton_Img);
	   	gtk_button_set_image_position(GTK_BUTTON(SProfileButton),GTK_POS_TOP);
	}
   	gchar* pix4path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/folder.xpm",NULL);
	if (g_file_test(pix4path,G_FILE_TEST_EXISTS)) {
		LButton_Img = gtk_image_new_from_file (pix4path);
		gtk_button_set_image(GTK_BUTTON(LProfileButton),LButton_Img);
	   	gtk_button_set_image_position(GTK_BUTTON(LProfileButton),GTK_POS_TOP);
	}
   	gchar* pix5path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/close.xpm",NULL);
	if (g_file_test(pix5path,G_FILE_TEST_EXISTS)) {
		QButton_Img = gtk_image_new_from_file (pix5path);
		gtk_button_set_image(GTK_BUTTON(quitButton),QButton_Img);
	   	gtk_button_set_image_position(GTK_BUTTON(quitButton),GTK_POS_TOP);
	}
   
    	/*must free path strings*/
        if(icon1path) free(icon1path);
	if(pix1path) free(pix1path);
   	if(pix2path) free(pix2path);
   	if(pix3path) free(pix3path);
   	if(pix4path) free(pix4path);
   	if(pix5path) free(pix5path);
   
   	gtk_box_pack_start(GTK_BOX(HButtonBox),CapImageButt,TRUE,TRUE,2);
   	gtk_box_pack_start(GTK_BOX(HButtonBox),CapAVIButt,TRUE,TRUE,2);
   	gtk_box_pack_start(GTK_BOX(HButtonBox),SProfileButton,TRUE,TRUE,2);
   	gtk_box_pack_start(GTK_BOX(HButtonBox),LProfileButton,TRUE,TRUE,2);
   	gtk_box_pack_start(GTK_BOX(HButtonBox),quitButton,TRUE,TRUE,2);
   
    	gtk_widget_show (CapImageButt);
    	gtk_widget_show (CapAVIButt);
   	gtk_widget_show (LProfileButton);
   	gtk_widget_show (SProfileButton);
   	gtk_widget_show (quitButton);
   
    	g_signal_connect (GTK_BUTTON(CapImageButt), "clicked",
		 G_CALLBACK (capture_image), NULL);
    	g_signal_connect (GTK_BUTTON(CapAVIButt), "clicked",
		 G_CALLBACK (capture_avi), NULL);
    
	g_signal_connect (GTK_BUTTON(quitButton), "clicked",
		 G_CALLBACK (quitButton_clicked), NULL);
	
	g_signal_connect (GTK_BUTTON(SProfileButton), "clicked",
		 G_CALLBACK (SProfileButton_clicked), s);
	
	g_signal_connect (GTK_BUTTON(LProfileButton), "clicked",
		 G_CALLBACK (LProfileButton_clicked), s);
	
	/*---- Right Table ----*/
	table2 = gtk_table_new(1,3,FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table2), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table2), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table2), 2);
	gtk_widget_show (table2);
	
	scroll2=gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll2),table2);
	gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(scroll2),
                                                              GTK_CORNER_TOP_LEFT);
	gtk_widget_show(scroll2);
	
	Tab2Label = gtk_label_new(_("Video & Files"));
	gtk_notebook_append_page(GTK_NOTEBOOK(boxh),scroll2,Tab2Label);
	
	/*sets the pan position*/
	if(global->boxvsize==0) {
		global->boxvsize=global->winheight-90;
	}
	gtk_paned_set_position (GTK_PANED(boxv),global->boxvsize);
	
	/* Resolution*/
	Resolution = gtk_combo_box_new_text ();
	char temp_str[20];
	int defres=0;
	for(i=0;i<videoIn->numb_resol;i++) {
		if (videoIn->listVidCap[global->formind][i].width>0){
			snprintf(temp_str,18,"%ix%i",videoIn->listVidCap[global->formind][i].width,
							 videoIn->listVidCap[global->formind][i].height);
			gtk_combo_box_append_text(GTK_COMBO_BOX(Resolution),temp_str);
			if ((global->width==videoIn->listVidCap[global->formind][i].width) && 
				(global->height==videoIn->listVidCap[global->formind][i].height)){
				defres=i;/*set selected*/
			}
		}
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(Resolution),defres);
	
    	if(global->debug) printf("Def. Res: %i  numb. fps:%i\n",defres,videoIn->listVidCap[global->formind][defres].numb_frates);
	
    	gtk_table_attach(GTK_TABLE(table2), Resolution, 1, 2, 3, 4,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (Resolution);
	
	gtk_widget_set_sensitive (Resolution, TRUE);
	g_signal_connect (Resolution, "changed",
			G_CALLBACK (resolution_changed), NULL);
	
	labelResol = gtk_label_new(_("Resolution:"));
	gtk_misc_set_alignment (GTK_MISC (labelResol), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), labelResol, 0, 1, 3, 4,
					GTK_FILL, 0, 0, 0);

	gtk_widget_show (labelResol);
	
	/* Frame Rate */
	videoIn->fps_num=global->fps_num;
	videoIn->fps=global->fps;
	input_set_framerate (videoIn);
				  
	FrameRate = gtk_combo_box_new_text ();
	int deffps=0;
	for (i=0;i<videoIn->listVidCap[global->formind][defres].numb_frates;i++) {
	    	snprintf(temp_str,18,"%i/%i fps",videoIn->listVidCap[global->formind][defres].framerate_num[i],
							 videoIn->listVidCap[global->formind][defres].framerate_denom[i]);
		gtk_combo_box_append_text(GTK_COMBO_BOX(FrameRate),temp_str);
		if ((videoIn->fps_num==videoIn->listVidCap[global->formind][defres].framerate_num[i]) && 
				  (videoIn->fps==videoIn->listVidCap[global->formind][defres].framerate_denom[i])){
				deffps=i;/*set selected*/
		}
	}
	
	gtk_table_attach(GTK_TABLE(table2), FrameRate, 1, 2, 2, 3,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (FrameRate);
	
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(FrameRate),deffps);
	if (deffps==0) {
		global->fps=videoIn->listVidCap[global->formind][defres].framerate_denom[0];
		global->fps_num=videoIn->listVidCap[global->formind][0].framerate_num[0];
		videoIn->fps=global->fps;
		videoIn->fps_num=global->fps_num;
	}
		
	gtk_widget_set_sensitive (FrameRate, TRUE);
	g_signal_connect (GTK_COMBO_BOX(FrameRate), "changed",
		G_CALLBACK (FrameRate_changed), Resolution);
	
	label_FPS = gtk_label_new(_("Frame Rate:"));
	gtk_misc_set_alignment (GTK_MISC (label_FPS), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_FPS, 0, 1, 2, 3,
					GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_FPS);
	
	ShowFPS=gtk_check_button_new_with_label (_(" Show"));
	gtk_table_attach(GTK_TABLE(table2), ShowFPS, 2, 3, 2, 3,
					 GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ShowFPS),(global->FpsCount > 0));
	gtk_widget_show (ShowFPS);
	g_signal_connect (GTK_CHECK_BUTTON(ShowFPS), "toggled",
		G_CALLBACK (ShowFPS_changed), NULL);
	
	
	/* Input method jpg  or yuv */
	ImpType= gtk_combo_box_new_text ();
	if (videoIn->SupMjpg>0) {/*Jpeg Input Available*/
		gtk_combo_box_append_text(GTK_COMBO_BOX(ImpType),"MJPG");
	}
	if (videoIn->SupYuv>0) {/*yuv Input Available*/
		gtk_combo_box_append_text(GTK_COMBO_BOX(ImpType),"YUV");
	}
	gtk_table_attach(GTK_TABLE(table2), ImpType, 1, 2, 4, 5,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	if ((videoIn->SupMjpg >0) && (videoIn->SupYuv >0)) {
		gtk_combo_box_set_active(GTK_COMBO_BOX(ImpType),global->formind);
	} else {
		gtk_combo_box_set_active(GTK_COMBO_BOX(ImpType),0); /*only one available*/
	}
	gtk_widget_set_sensitive (ImpType, TRUE);
	g_signal_connect (GTK_COMBO_BOX(ImpType), "changed",
		G_CALLBACK (ImpType_changed), NULL);
	gtk_widget_show (ImpType);
	
	label_ImpType = gtk_label_new(_("Camera Output:"));
	gtk_misc_set_alignment (GTK_MISC (label_ImpType), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_ImpType, 0, 1, 4, 5,
					GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_ImpType);
	
	/* Image Capture*/
    	label_ImgFile= gtk_label_new(_("Image File:"));
	gtk_misc_set_alignment (GTK_MISC (label_ImgFile), 1, 0.5);
    
    	ImageFNameEntry = gtk_entry_new();
	
	gtk_entry_set_text(GTK_ENTRY(ImageFNameEntry),global->imgFPath[0]);
	
	gtk_table_attach(GTK_TABLE(table2), label_ImgFile, 0, 1, 5, 6,
					GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_table_attach(GTK_TABLE(table2), ImageFNameEntry, 1, 2, 5, 6,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	ImgFileButt=gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_table_attach(GTK_TABLE(table2), ImgFileButt, 2, 3, 5, 6,
					 GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (ImgFileButt);
	
	ImageIncLabel=gtk_label_new(global->imageinc_str);
	gtk_misc_set_alignment (GTK_MISC (ImageIncLabel), 0, 0.5);

	gtk_table_attach (GTK_TABLE(table2), ImageIncLabel, 1, 2, 6, 7,
					GTK_FILL, 0, 0, 0);

	gtk_widget_show (ImageIncLabel);
	
	/*incremental capture*/
	ImageInc=gtk_check_button_new_with_label (_("File,Auto"));
	gtk_table_attach(GTK_TABLE(table2), ImageInc, 2, 3, 6, 7,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ImageInc),(global->image_inc > 0));
	gtk_widget_show (ShowFPS);
	g_signal_connect (GTK_CHECK_BUTTON(ImageInc), "toggled",
		G_CALLBACK (ImageInc_changed), NULL);
	gtk_widget_show (ImageInc);
	
	
	label_ImageType=gtk_label_new(_("Image Format:"));
	gtk_misc_set_alignment (GTK_MISC (label_ImageType), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_ImageType, 0, 1, 7, 8,
					GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_ImageType);
	
	ImageType=gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(ImageType),"JPG");
	gtk_combo_box_append_text(GTK_COMBO_BOX(ImageType),"BMP");
	gtk_combo_box_append_text(GTK_COMBO_BOX(ImageType),"PNG");
   	gtk_combo_box_append_text(GTK_COMBO_BOX(ImageType),"RAW");
	gtk_combo_box_set_active(GTK_COMBO_BOX(ImageType),global->imgFormat);
	gtk_table_attach(GTK_TABLE(table2), ImageType, 1, 2, 7, 8,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (ImageType);
	
	gtk_widget_show (label_ImgFile);
	gtk_widget_show (ImageFNameEntry);
	gtk_widget_show (ImageType);
	g_signal_connect (GTK_COMBO_BOX(ImageType), "changed",
		G_CALLBACK (ImageType_changed), ImageFNameEntry);
	
	g_signal_connect (GTK_BUTTON(ImgFileButt), "clicked",
		 G_CALLBACK (file_chooser), GINT_TO_POINTER (0));
	
	
	/*AVI Capture*/
    	label_AVIFile= gtk_label_new(_("AVI File:"));
    	gtk_misc_set_alignment (GTK_MISC (label_AVIFile), 1, 0.5);
	AVIFNameEntry = gtk_entry_new();
	
	if (global->avifile) {	/*avi capture enabled from start*/
		gtk_entry_set_text(GTK_ENTRY(AVIFNameEntry),global->avifile);
	} else {
		videoIn->capAVI = FALSE;
		gtk_entry_set_text(GTK_ENTRY(AVIFNameEntry),global->aviFPath[0]);
	}
	
	gtk_table_attach(GTK_TABLE(table2), label_AVIFile, 0, 1, 8, 9,
					 GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table2), AVIFNameEntry, 1, 2, 8, 9,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	AviFileButt=gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gtk_table_attach(GTK_TABLE(table2), AviFileButt, 2, 3, 8, 9,
					GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_widget_show (AviFileButt);
    	gtk_widget_show (label_AVIFile);
	gtk_widget_show (AVIFNameEntry);
	
	g_signal_connect (GTK_BUTTON(AviFileButt), "clicked",
		 G_CALLBACK (file_chooser), GINT_TO_POINTER (1));
	
	/*table 10-11: inc avi file name */
	
	/* AVI Compressor */
	AVIComp = gtk_combo_box_new_text ();
	
	gtk_combo_box_append_text(GTK_COMBO_BOX(AVIComp),_("MJPG - compressed"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(AVIComp),_("YUY2 - uncomp YUV"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(AVIComp),_("RGB - uncomp BMP"));
	
	gtk_table_attach(GTK_TABLE(table2), AVIComp, 1, 2, 10, 11,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (AVIComp);
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(AVIComp),global->AVIFormat);
	
	gtk_widget_set_sensitive (AVIComp, TRUE);
	g_signal_connect (GTK_COMBO_BOX(AVIComp), "changed",
		G_CALLBACK (AVIComp_changed), NULL);
	
	label_AVIComp = gtk_label_new(_("AVI Format:"));
	gtk_misc_set_alignment (GTK_MISC (label_AVIComp), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_AVIComp, 0, 1, 10, 11,
					GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_AVIComp);

	/*----------------------- sound interface --------------------------------*/
	
	/* get sound device list and info */
	
	SndDevice = gtk_combo_box_new_text ();
		
	int     it, numDevices, defaultDisplayed;
	const   PaDeviceInfo *deviceInfo;
	//PaStreamParameters inputParameters, outputParameters;
	PaError err;
	
	Pa_Initialize();
	
	numDevices = Pa_GetDeviceCount();
	if( numDevices < 0 )
	{
		printf( "SOUND DISABLE: Pa_CountDevices returned 0x%x\n", numDevices );
		err = numDevices;
		Pa_Terminate();
		global->Sound_enable=0;
	} else {
	
		for( it=0; it<numDevices; it++ )
		{
			deviceInfo = Pa_GetDeviceInfo( it );
			if (global->debug) printf( "--------------------------------------- device #%d\n", it );
				
			/* Mark global and API specific default devices */
			defaultDisplayed = 0;
			/* Default Input will save the ALSA default device index*/
			/* since ALSA lists after OSS							*/
			if( it == Pa_GetDefaultInputDevice() )
			{
				if (global->debug) printf( "[ Default Input" );
				defaultDisplayed = 1;
				global->Sound_DefDev=global->Sound_numInputDev;/*default index in array of input devs*/
			}
			else if( it == Pa_GetHostApiInfo( deviceInfo->hostApi )->defaultInputDevice )
			{
				const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
				if (global->debug) printf( "[ Default %s Input", hostInfo->name );
				defaultDisplayed = 2;
				global->Sound_DefDev=global->Sound_numInputDev;/*index in array of input devs*/
			}
			/* Output device doesn't matter for capture*/
			if( it == Pa_GetDefaultOutputDevice() )
			{
			 	if (global->debug) {
					printf( (defaultDisplayed ? "," : "[") );
					printf( " Default Output" );
			    	}
				defaultDisplayed = 3;
			}
			else if( it == Pa_GetHostApiInfo( deviceInfo->hostApi )->defaultOutputDevice )
			{
				const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
				if (global->debug) {
			    		printf( (defaultDisplayed ? "," : "[") );                
					printf( " Default %s Output", hostInfo->name );/* OSS ALSA etc*/
				}
				defaultDisplayed = 4;
			}

			if( defaultDisplayed!=0 )
				if (global->debug) printf( " ]\n" );

			/* print device info fields */
			if (global->debug)printf( "Name                        = %s\n", deviceInfo->name );
			if (global->debug) printf( "Host API                    = %s\n",  Pa_GetHostApiInfo( deviceInfo->hostApi )->name );
			
			if (global->debug) printf( "Max inputs = %d", deviceInfo->maxInputChannels  );
			/* if it as input channels it's a capture device*/
			if (deviceInfo->maxInputChannels >0) { 
				global->Sound_IndexDev[global->Sound_numInputDev].id=it; /*saves dev id*/
				global->Sound_IndexDev[global->Sound_numInputDev].chan=deviceInfo->maxInputChannels;
				global->Sound_IndexDev[global->Sound_numInputDev].samprate=deviceInfo->defaultSampleRate;
				//Sound_IndexDev[Sound_numInputDev].Hlatency=deviceInfo->defaultHighInputLatency;
				//Sound_IndexDev[Sound_numInputDev].Llatency=deviceInfo->defaultLowInputLatency;
				global->Sound_numInputDev++;
				gtk_combo_box_append_text(GTK_COMBO_BOX(SndDevice),deviceInfo->name);		
			}
			if (global->debug) {
				printf( ", Max outputs = %d\n", deviceInfo->maxOutputChannels  );
				printf( "Default low input latency   = %8.3f\n", deviceInfo->defaultLowInputLatency  );
				printf( "Default low output latency  = %8.3f\n", deviceInfo->defaultLowOutputLatency  );
				printf( "Default high input latency  = %8.3f\n", deviceInfo->defaultHighInputLatency  );
				printf( "Default high output latency = %8.3f\n", deviceInfo->defaultHighOutputLatency  );
				printf( "Default sample rate         = %8.2f\n", deviceInfo->defaultSampleRate );
			}
			
		}
		Pa_Terminate();
		
		if (global->debug) printf("----------------------------------------------\n");
	
	}
	
	/*--------------------- sound controls -----------------------------------*/
	gtk_table_attach(GTK_TABLE(table2), SndDevice, 1, 3, 12, 13,
					 GTK_SHRINK | GTK_FILL , 0, 0, 0);
	gtk_widget_show (SndDevice);
	/* using default device*/
	if(global->Sound_UseDev==0) global->Sound_UseDev=global->Sound_DefDev;
	gtk_combo_box_set_active(GTK_COMBO_BOX(SndDevice),global->Sound_UseDev);
	
	if (global->Sound_enable) gtk_widget_set_sensitive (SndDevice, TRUE);
	else  gtk_widget_set_sensitive (SndDevice, FALSE);
	g_signal_connect (GTK_COMBO_BOX(SndDevice), "changed",
		G_CALLBACK (SndDevice_changed), NULL);
	
	label_SndDevice = gtk_label_new(_("Input Device:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndDevice), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_SndDevice, 0, 1, 12, 13,
					GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_SndDevice);
	
	
	//~ if (Sound_numInputDev == 0) Sound_enable=0;
	//~ printf("SOUND DISABLE: no input devices detected\n");
	
	SndEnable=gtk_check_button_new_with_label (_(" Sound"));
	gtk_table_attach(GTK_TABLE(table2), SndEnable, 1, 2, 11, 12,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(SndEnable),(global->Sound_enable > 0));
	gtk_widget_show (SndEnable);
	g_signal_connect (GTK_CHECK_BUTTON(SndEnable), "toggled",
		G_CALLBACK (SndEnable_changed), NULL);
	
	SndSampleRate= gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(SndSampleRate),_("Dev. Default"));
	for( i=1; stdSampleRates[i] > 0; i++ )
	{
		char dst[8];
		sprintf(dst,"%d",stdSampleRates[i]);
		gtk_combo_box_append_text(GTK_COMBO_BOX(SndSampleRate),dst);
	}
	if (global->Sound_SampRateInd>(i-1)) global->Sound_SampRateInd=0; /*out of range*/
	
	gtk_table_attach(GTK_TABLE(table2), SndSampleRate, 1, 2, 13, 14,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (SndSampleRate);
	
	global->Sound_SampRate=stdSampleRates[global->Sound_SampRateInd];
	gtk_combo_box_set_active(GTK_COMBO_BOX(SndSampleRate),global->Sound_SampRateInd); /*device default*/
	
	
	if (global->Sound_enable) gtk_widget_set_sensitive (SndSampleRate, TRUE);
	else  gtk_widget_set_sensitive (SndSampleRate, FALSE);
	g_signal_connect (GTK_COMBO_BOX(SndSampleRate), "changed",
		G_CALLBACK (SndSampleRate_changed), NULL);
	
	label_SndSampRate = gtk_label_new(_("Sample Rate:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndSampRate), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_SndSampRate, 0, 1, 13, 14,
					GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_SndSampRate);
	
	SndNumChan= gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(SndNumChan),_("Dev. Default"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(SndNumChan),_("1 - mono"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(SndNumChan),_("2 - stereo"));
	
	gtk_table_attach(GTK_TABLE(table2), SndNumChan, 1, 2, 14, 15,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (SndNumChan);
	switch (global->Sound_NumChanInd) {
	   case 0:/*device default*/
			gtk_combo_box_set_active(GTK_COMBO_BOX(SndNumChan),0);
		break;
	   case 1:/*mono*/	
			gtk_combo_box_set_active(GTK_COMBO_BOX(SndNumChan),1);
			global->Sound_NumChan=1;
		break;
		case 2:/*stereo*/
			gtk_combo_box_set_active(GTK_COMBO_BOX(SndNumChan),2);
			global->Sound_NumChan=2;
	   default:
		/*set Default to NUM_CHANNELS*/
			global->Sound_NumChan=NUM_CHANNELS;	
	}
	if (global->Sound_enable) gtk_widget_set_sensitive (SndNumChan, TRUE);
	else gtk_widget_set_sensitive (SndNumChan, FALSE);
	g_signal_connect (GTK_COMBO_BOX(SndNumChan), "changed",
		G_CALLBACK (SndNumChan_changed), NULL);
	
	label_SndNumChan = gtk_label_new(_("Channels:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndNumChan), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_SndNumChan, 0, 1, 14, 15,
					GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_SndNumChan);
	if (global->debug) printf("SampleRate:%d Channels:%d\n",global->Sound_SampRate,global->Sound_NumChan);
	
	/*----- Filter controls ----*/
	
	label_videoFilters = gtk_label_new(_("---- Video Filters ----"));
	gtk_misc_set_alignment (GTK_MISC (label_videoFilters), 0.5, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_videoFilters, 0, 3, 15, 16,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL , 0, 0, 0);

	gtk_widget_show (label_videoFilters);
	
	table3 = gtk_table_new(1,4,FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table3), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table3), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table3), 4);
	gtk_widget_set_size_request (table3, -1, -1);
	
	
	
	/* Mirror */
	FiltMirrorEnable=gtk_check_button_new_with_label (_(" Mirror"));
	gtk_table_attach(GTK_TABLE(table3), FiltMirrorEnable, 0, 1, 0, 1,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltMirrorEnable),(global->Frame_Flags & YUV_MIRROR)>0);
	gtk_widget_show (FiltMirrorEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltMirrorEnable), "toggled",
		G_CALLBACK (FiltMirrorEnable_changed), NULL);
	/*Upturn*/
	FiltUpturnEnable=gtk_check_button_new_with_label (_(" Invert"));
	gtk_table_attach(GTK_TABLE(table3), FiltUpturnEnable, 1, 2, 0, 1,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltUpturnEnable),(global->Frame_Flags & YUV_UPTURN)>0);
	gtk_widget_show (FiltUpturnEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltUpturnEnable), "toggled",
		G_CALLBACK (FiltUpturnEnable_changed), NULL);
	/*Negate*/
	FiltNegateEnable=gtk_check_button_new_with_label (_(" Negative"));
	gtk_table_attach(GTK_TABLE(table3), FiltNegateEnable, 2, 3, 0, 1,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltNegateEnable),(global->Frame_Flags & YUV_NEGATE)>0);
	gtk_widget_show (FiltNegateEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltNegateEnable), "toggled",
		G_CALLBACK (FiltNegateEnable_changed), NULL);
	/*Mono*/
	FiltMonoEnable=gtk_check_button_new_with_label (_(" Mono"));
	gtk_table_attach(GTK_TABLE(table3), FiltMonoEnable, 3, 4, 0, 1,
					GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltMonoEnable),(global->Frame_Flags & YUV_MONOCR)>0);
	gtk_widget_show (FiltMonoEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltMonoEnable), "toggled",
		G_CALLBACK (FiltMonoEnable_changed), NULL);
	
	gtk_table_attach (GTK_TABLE(table2), table3, 0, 3, 16, 17,
					GTK_FILL, 0, 0, 0);

	gtk_widget_show (table3);
	
	/* main container */
	gtk_container_add (GTK_CONTAINER (mainwin), boxv);
	
	gtk_widget_show (mainwin);
   	
    	/* if autofocus exists allocate data*/
 	if(global->AFcontrol) {   
    		AFdata = malloc(sizeof(struct focusData));
    		initFocusData(AFdata);
	 }
    
   	/*------------------ Creating the main loop (video) thread ---------------*/
	/* Initialize and set thread detached attribute */
	stacksize = sizeof(char) * global->stack_size;
	pthread_attr_init(&attr);
   	pthread_attr_setstacksize (&attr, stacksize);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
	
	int rc = pthread_create(&mythread, &attr, main_loop, NULL); 
	if (rc) {
			printf("ERROR; return code from pthread_create() is %d\n", rc);
	   		ERR_DIALOG (N_("Guvcview error:\n\nUnable to create Video Thread"),
				N_("Please report it to http://developer.berlios.de/bugs/?group_id=8179"));	       
		}
	
	/*---------------------- image timed capture -----------------------------*/
	if(global->image_timer){
		global->image_timer_id=g_timeout_add(global->image_timer*1000,
                                                          Image_capture_timer,NULL);
		set_sensitive_img_contrls(FALSE);/*disable image controls*/
	}
	/*--------------------- avi capture from start ---------------------------*/
	if(global->avifile) {
		AviOut = AVI_open_output_file(global->avifile);
		/*4CC compression "YUY2" (YUV) or "DIB " (RGB24)  or  "MJPG"*/
		char *compression="MJPG";

		switch (global->AVIFormat) {
			case 0:
				compression="MJPG";
				break;
			case 1:
				compression="YUY2";
				break;
			case 2:
				compression="DIB ";
				break;
			default:
				compression="MJPG";
		}
	   AVI_set_video(AviOut, videoIn->width, videoIn->height, videoIn->fps,compression);		
	   /* audio will be set in aviClose - if enabled*/
	   sprintf(videoIn->AVIFName,"%s/%s",global->aviFPath[1],global->aviFPath[0]);		
	   videoIn->capAVI = TRUE;
	   global->AVIstarttime = ms_time();
	   /*disabling sound and avi compression controls*/
	   set_sensitive_avi_contrls (FALSE);
	   /* Creating the sound capture loop thread if Sound Enable*/ 
	   if(global->Sound_enable > 0) { 
		  /* Initialize and set snd thread detached attribute */
		  pthread_attr_init(&sndattr);
	          pthread_attr_setstacksize (&sndattr, stacksize);
		  pthread_attr_setdetachstate(&sndattr, PTHREAD_CREATE_JOINABLE);
		   
		  int rsnd = pthread_create(&sndthread, &sndattr, sound_capture, NULL); 
		  if (rsnd)
		  {
			 printf("ERROR; return code from snd pthread_create() is %d\n", rsnd);
		  }
		}
	   if (global->Capture_time) {
		/*sets the timer function*/
		g_timeout_add(global->Capture_time*1000,timer_callback,NULL);
           }
		
	}
	
	if (global->FpsCount>0) {
		/*sets the Fps counter timer function every 2 sec*/
		global->timer_id = g_timeout_add(2*1000,FpsCount_callback,NULL);
	}
	/* The last thing to get called */
	gtk_main();

	return 0;
}


/*-------------------------- clean up and shut down --------------------------*/

static void
clean_struct (void) {

    //int i=0;
   
    if(videoIn) close_v4l2(videoIn);	

    if (global->debug) printf("closed v4l2 strutures\n");
    
    if (s->control) {
	//~ for (i = 0; i < s->num_controls; i++) {
		//~ ControlInfo * ci = s->control_info + i;
		//~ if (ci->widget)
			//~ gtk_widget_destroy (ci->widget);
		//~ if (ci->label)
			//~ gtk_widget_destroy (ci->label);
		//~ if (ci->spinbutton)
			//~ gtk_widget_destroy (ci->spinbutton);
	//~ }
	free (s->control_info);
	s->control_info = NULL;
	input_free_controls (s->control, s->num_controls);
	printf("free controls\n");
	s->control = NULL;
    }
    if (s) free(s);	   
    if (global->debug) printf("free controls - vidState\n");
   
    if (AFdata) free(AFdata);
    if(global) closeGlobals(global);
    if (jpeg_struct != NULL) free(jpeg_struct);
    printf("cleaned allocations - 100%%\n");
   
}


static void 
shutd (gint restart) 
{
	int exec_status=0;
	
	int tstatus;
	
    	if (global->debug) printf("Shuting Down Thread\n");
	if(videoIn->signalquit > 0) videoIn->signalquit=0;
	if (global->debug) printf("waiting for thread to finish\n");
	/*shuting down while in avi capture mode*/
	/*must close avi			*/
	if(videoIn->capAVI) {
		if (global->debug) printf("stoping AVI capture\n");
		global->AVIstoptime = ms_time();
		//printf("AVI stop time:%d\n",AVIstoptime);	
		videoIn->capAVI = FALSE;
		aviClose();
	}
	/* Free attribute and wait for the main loop (video) thread */
	pthread_attr_destroy(&attr);
	int rc = pthread_join(mythread, (void *)&tstatus);
	if (rc)
	  {
		 printf("ERROR; return code from pthread_join() is %d\n", rc);
		 exit(-1);
	  }
	if (global->debug) printf("Completed join with thread status= %d\n", tstatus);
	/* destroys fps timer*/
   	if (global->timer_id > 0) g_source_remove(global->timer_id);
   
	gtk_window_get_size(GTK_WINDOW(mainwin),&(global->winwidth),&(global->winheight));//mainwin or widget
	global->boxvsize=gtk_paned_get_position (GTK_PANED(boxv));
    	
   	if (global->debug) printf("GTK quit\n");
    	gtk_main_quit();
	
   	/*save configuration*/
   	writeConf(global->confPath);
   
    	clean_struct();   
   
	if (restart==1) { /* replace running process with new one */
		 printf("Restarting guvcview with command: %s\n",EXEC_CALL);
		 exec_status = execlp(EXEC_CALL,EXEC_CALL,NULL);/*No parameters passed*/
	}
	
	if (EXEC_CALL) free(EXEC_CALL);
	printf("Terminated.\n");
}

