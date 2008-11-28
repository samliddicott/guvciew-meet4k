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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <linux/videodev.h>
#include <sys/file.h>
#include <unistd.h>
#include <getopt.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>

#include "globals.h"
#include "string_utils.h"
#include "avilib.h"
/*----------------------- write conf (.guvcviewrc) file ----------------------*/
int 
writeConf(struct GLOBAL *global) 
{
	int ret=0;
	FILE *fp;
	
	if ((fp = fopen(global->confPath,"w"))!=NULL) 
	{
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
		fprintf(fp,"# mode video format 'yuv' 'uyv' 'yup' 'gbr' or 'jpg'(default)\n");
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
		fprintf(fp,"# video compression format: 0-MJPG 1-YUY2/UYVY 2-DIB (BMP 24)\n");
		fprintf(fp,"avi_format=%i\n",global->AVIFormat);
		fprintf(fp,"# avi file max size (MAX: %d bytes)\n",AVI_MAX_SIZE);
		fprintf(fp,"avi_max_len=%li\n",global->AVI_MAX_LEN);
	    	fprintf(fp,"# Auto AVI naming (filename-n.avi)\n");
		fprintf(fp,"avi_inc=%d\n",global->avi_inc);
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
		fprintf(fp,"# Sound Format (PCM=1 (0x0001) MP2=80 (0x0050)\n");
		fprintf(fp,"snd_format=%i\n",global->Sound_Format);
		fprintf(fp,"# Sound bit Rate used by mpeg audio default=160 Kbps\n");
		fprintf(fp,"#other values: 48 56 64 80 96 112 128 160 192 224 384\n");
		fprintf(fp,"snd_bitrate=%i\n",global->Sound_bitRate);
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
		printf("write %s OK\n",global->confPath);
		fclose(fp);
	} 
	else 
	{
		printf("Could not write file %s \n Please check file permissions\n",global->confPath);
		ret=1;
	}
	return ret;
}

/*----------------------- read conf (.guvcviewrc) file -----------------------*/
int 
readConf(struct GLOBAL *global) 
{
	int ret=1;
	char variable[16];
	char value[128];

	int i=0;

	FILE *fp;
	
	if((fp = fopen(global->confPath,"r"))!=NULL) 
	{
		char line[144];
		while (fgets(line, 144, fp) != NULL) 
		{
			if ((line[0]=='#') || (line[0]==' ') || (line[0]=='\n')) 
			{
			/*skip*/
			} 
			else if ((i=sscanf(line,"%[^#=]=%[^#\n ]",variable,value))==2)
			{
				/* set variables */
				if (strcmp(variable,"video_device")==0) 
				{
					snprintf(global->videodevice,15,"%s",value);
				} 
				else if (strcmp(variable,"stack_size")==0) 
				{
					sscanf(value,"%i",&(global->stack_size));
				} 
				else if (strcmp(variable,"vid_sleep")==0) 
				{
					sscanf(value,"%i",&(global->vid_sleep));
				} 
				else if (strcmp(variable,"resolution")==0) 
				{
					sscanf(value,"%ix%i",&(global->width),&(global->height));
				} 
				else if (strcmp(variable,"windowsize")==0) 
				{
					sscanf(value,"%ix%i",&(global->winwidth),&(global->winheight));
				} 
				else if (strcmp(variable,"vpane")==0) 
				{ 
					sscanf(value,"%i",&(global->boxvsize));
				} 
				else if (strcmp(variable,"spinbehave")==0) 
				{ 
					sscanf(value,"%i",&(global->spinbehave));
				} 
				else if (strcmp(variable,"mode")==0) 
				{
					snprintf(global->mode,5,"%s",value);
				} 
				else if (strcmp(variable,"fps")==0) 
				{
					sscanf(value,"%i/%i",&(global->fps_num),&(global->fps));
				} 
				else if (strcmp(variable,"fps_display")==0) 
				{ 
					sscanf(value,"%hi",&(global->FpsCount));
				}
				else if (strcmp(variable,"auto_focus")==0) 
				{ 
					sscanf(value,"%i",&(global->autofocus));
				} 
				else if (strcmp(variable,"bpp")==0) 
				{
					sscanf(value,"%i",&(global->bpp));
				} 
				else if (strcmp(variable,"hwaccel")==0) 
				{
					sscanf(value,"%i",&(global->hwaccel));
				} 
				else if (strcmp(variable,"grabmethod")==0) 
				{
					sscanf(value,"%i",&(global->grabmethod));
				} 
				else if (strcmp(variable,"avi_format")==0) 
				{
					sscanf(value,"%i",&(global->AVIFormat));
				} 
				else if (strcmp(variable,"avi_max_len")==0) 
				{
					sscanf(value,"%li",&(global->AVI_MAX_LEN));
					global->AVI_MAX_LEN = AVI_set_MAX_LEN (global->AVI_MAX_LEN);
				} 
				else if (strcmp(variable,"avi_inc")==0) 
				{
					sscanf(value,"%d",&(global->avi_inc));
					snprintf(global->aviinc_str,20,_("File num:%d"),global->avi_inc);
				} 
				else if (strcmp(variable,"sound")==0) 
				{
					sscanf(value,"%hi",&(global->Sound_enable));
				} 
				else if (strcmp(variable,"snd_device")==0) 
				{
					sscanf(value,"%i",&(global->Sound_UseDev));
				} 
				else if (strcmp(variable,"snd_samprate")==0) 
				{
					sscanf(value,"%i",&(global->Sound_SampRateInd));
				} 
				else if (strcmp(variable,"snd_numchan")==0) 
				{
					sscanf(value,"%i",&(global->Sound_NumChanInd));
				} 
				else if (strcmp(variable,"snd_numsec")==0) 
				{
					sscanf(value,"%i",&(global->Sound_NumSec));
				} 
				else if (strcmp(variable,"snd_format")==0) 
				{
					sscanf(value,"%i",&(global->Sound_Format));
				} 
				else if (strcmp(variable,"snd_bitrate")==0) 
				{
					sscanf(value,"%i",&(global->Sound_bitRate));
				}
				else if (strcmp(variable,"Pan_Step")==0)
				{ 
					sscanf(value,"%i",&(global->PanStep));
				}
				else if (strcmp(variable,"Tilt_Step")==0)
				{ 
					sscanf(value,"%i",&(global->TiltStep));
				}
				else if (strcmp(variable,"frame_flags")==0) 
				{
					sscanf(value,"%i",&(global->Frame_Flags));
				}
				else if (strcmp(variable,"image_path")==0) 
				{
					global->imgFPath = splitPath(value,global->imgFPath);
					/*get the file type*/
					global->imgFormat = check_image_type(global->imgFPath[0]);
				}
				else if (strcmp(variable,"image_inc")==0) 
				{
					sscanf(value,"%d",&(global->image_inc));
					snprintf(global->imageinc_str,20,_("File num:%d"),global->image_inc);
				}
				else if (strcmp(variable,"avi_path")==0) 
				{
					global->aviFPath=splitPath(value,global->aviFPath);
				}
				else if (strcmp(variable,"profile_path")==0) 
				{
					global->profile_FPath=splitPath(value,global->profile_FPath);
				}
			}
		}
		fclose(fp);
	    	if (global->debug) 
		{ /*it will allways be FALSE unless DEBUG=1*/
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
			printf("Sound Format: %i \n",global->Sound_Format);
			printf("Sound bit Rate: %i Kbps\n",global->Sound_bitRate);
			printf("Pan Step: %i degrees\n",global->PanStep);
			printf("Tilt Step: %i degrees\n",global->TiltStep);
			printf("Video Filter Flags: %i\n",global->Frame_Flags);
			printf("image inc: %d\n",global->image_inc);
			printf("profile(default):%s/%s\n",global->profile_FPath[1],global->profile_FPath[0]);
		}
	} 
	else 
	{
		printf("Could not open %s for read,\n will try to create it\n",global->confPath);
		ret=writeConf(global);
	}
	return ret;
}

/*------------------------- read command line options ------------------------*/
void
readOpts(int argc,char *argv[], struct GLOBAL *global) 
{
	
	int c = 0;
	static int debug_flag = 0;
	char *tmpstr = NULL;
	char *separateur;

	static struct option long_options[] = {
		/* These options set a flag. */
		{"verbose",	no_argument,	&debug_flag,	1},
		{"brief",	no_argument,	&debug_flag,	0},
		/* These options don't set a flag.
		We distinguish them by their indices. */
		{"help",	no_argument,		0,	'h'},
		{"device",	required_argument,	0,	'd'},
		{"hwd_acel",	required_argument,	0,	'w'},
		{"format",	required_argument,	0,	'f'},
		{"size",	required_argument,	0,	's'},
		{"image",	required_argument,	0,	'i'},
		{"cap_time",	required_argument,	0,	'c'},
		{"npics",	required_argument,	0,	'm'},
		{"avi",		required_argument,	0,	'n'},
		{"avi_time",	required_argument,	0,	't'},
		{"show_fps",	required_argument,	0,	'p'},
		{"profile",	required_argument,	0,	'l'},
		{0, 0, 0, 0}
	};
	/* getopt_long stores the option index here. */
	int option_index = 0;

	while (c >= 0)
	{
		c = getopt_long (argc, argv, "hd:w:f:s:i:c:m:n:t:p:l:",
			long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1)
		break;

		switch (c)
		{
			case 0:
				/* If this option set a flag, do nothing else now. */
				if (long_options[option_index].flag != 0)
					break;
				printf ("option %s", long_options[option_index].name);
				if (optarg) printf (" with arg %s", optarg);
				printf ("\n");
			break;

			case 'd':
				snprintf(global->videodevice,15,"%s",optarg);
				break;

			case 'w':
				if (strcmp(optarg, "enable") == 0) global->hwaccel=1;
				else if (strcmp(optarg, "disable") == 0) global->hwaccel=0;
				break;

			case 'f':
				snprintf(global->mode,4,"%s",optarg);
				//global->mode[0] = optarg[0];
				break;

			case 's':
				tmpstr = strdup(optarg);
	
				global->width = strtoul(tmpstr, &separateur, 10);
				if (*separateur != 'x') 
				{
					printf("Error in size usage: -s[--size] widthxheight \n");
				} 
				else 
				{
					global->avifile = strdup(optarg);
					global->aviFPath=splitPath(global->avifile,global->aviFPath);
					++separateur;
					global->height = strtoul(separateur, &separateur, 10);
					if (*separateur != 0)
						printf("hmm.. dont like that!! trying this height \n");
				}
				free(tmpstr);
				tmpstr=NULL;
				break;
		
			case 'i':
				tmpstr = strdup(optarg);
				global->imgFPath=splitPath(tmpstr,global->imgFPath);
				/*get the file type*/
				global->imgFormat = check_image_type(global->imgFPath[0]);
				free(tmpstr);
				tmpstr=NULL;
				break;
		
			case 'c':
				tmpstr = strdup(optarg);
				global->image_timer= strtoul(tmpstr, &separateur, 10);
				global->image_inc=1;
				printf("capturing images every %i seconds",global->image_timer);
				free(tmpstr);
				tmpstr=NULL;
				break;
		
			case 'm':
				tmpstr = strdup(optarg);
				global->image_npics= strtoul(tmpstr, &separateur, 10);
				printf("capturing at max %d pics",global->image_npics);
				free(tmpstr);
				tmpstr=NULL;
				break;
	
			case 'n':
				global->avifile = strdup(optarg);
				global->aviFPath=splitPath(global->avifile,global->aviFPath);
				break;
		
			case 't':
				tmpstr = strdup(optarg);
				global->Capture_time= strtoul(tmpstr, &separateur, 10);
				printf("capturing avi for %i seconds",global->Capture_time);
				free(tmpstr);
				tmpstr=NULL;
				break;
		
			case 'p':
				if (strcmp(optarg, "enable") == 0) global->FpsCount=1;
				else if (strcmp(optarg, "disable") == 0) global->FpsCount=0;
				break;
		
			case 'l':
				global->lprofile=1;
				global->profile_FPath=splitPath(optarg,global->profile_FPath);
				break;
		
			case '?':
				/* getopt_long already printed an error message. */
				break;

			case 'h':
			default:
				printf("usage: guvcview [options] \n\n");
				printf("options:\n");
				printf("-h[--help]\t:print this message \n");
				printf("--verbose \tverbose mode, prints a lot of debug related info\n");
				printf("-d[--device] /dev/videoX\t:use videoX device\n");
				//printf("-g\t:use read method for grab instead mmap\n");
				printf("-w enable|disable\t:SDL hardware accel. \n");
				printf("-f[--format] format\t:video format\n");
				printf("   default jpg  others options are yuv uyv yup gbr jpg \n");
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
				exit (0);
		}
	}
	global->debug=debug_flag;
	
	/*if -n not set reset capture time*/
	if(global->Capture_time>0 && global->avifile==NULL) global->Capture_time=0;
	
	if (strncmp(global->mode, "uyv", 3) == 0) 
	{
		global->format = V4L2_PIX_FMT_UYVY;
		global->formind = 1;
	} 
	else if (strncmp(global->mode, "yuv", 3) == 0) 
	{
		global->format = V4L2_PIX_FMT_YUYV;
		global->formind = 1;
	}
	else if (strncmp(global->mode, "yup", 3) == 0) 
	{
		global->format = V4L2_PIX_FMT_YUV420;
		global->formind = 1;
	}
	else if (strncmp(global->mode, "gbr", 3) == 0) 
	{
		global->format = V4L2_PIX_FMT_SGBRG8;
		global->formind = 1;
	}
	else if (strncmp(global->mode, "jpg", 3) == 0) 
	{
		global->format = V4L2_PIX_FMT_MJPEG;
		global->formind = 0;
	}
	else 
	{
		global->format = V4L2_PIX_FMT_MJPEG;
		global->formind = 0;
	}
    	if (global->debug) printf("Format is %s(%d)\n",global->mode,global->formind);
}
