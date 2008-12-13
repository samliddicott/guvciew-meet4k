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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <getopt.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <libgen.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>

#include "defs.h"
#include "globals.h"
#include "string_utils.h"
#include "avilib.h"
#include "v4l2uvc.h"
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
		fprintf(fp,"video_device='%s'\n",global->videodevice);
		fprintf(fp,"# Thread stack size: default 128 pages of 64k = 8388608 bytes\n");
		fprintf(fp,"stack_size=%d\n",global->stack_size);
		fprintf(fp,"# video loop sleep time in ms: 0,1,2,3,...\n");
		fprintf(fp,"# increased sleep time -> less cpu load, more droped frames\n");
		fprintf(fp,"vid_sleep=%i\n",global->vid_sleep);
		fprintf(fp,"# video resolution \n");
		fprintf(fp,"resolution='%ix%i'\n",global->width,global->height);
		fprintf(fp,"# control window size: default %ix%i\n",WINSIZEX,WINSIZEY);
		fprintf(fp,"windowsize='%ix%i'\n",global->winwidth,global->winheight);
		fprintf(fp,"#vertical pane size\n");
		fprintf(fp,"vpane=%i\n",global->boxvsize);
		fprintf(fp,"#spin button behavior: 0-non editable 1-editable\n");
		fprintf(fp,"spinbehave=%i\n", global->spinbehave);
		fprintf(fp,"# mode video format 'yuv' 'uyv' 'yup' 'gbr' 'jpeg' 'mjpg'(default)\n");
		fprintf(fp,"mode='%s'\n",global->mode);
		fprintf(fp,"# frames per sec. - hardware supported - default( %i )\n",DEFAULT_FPS);
		fprintf(fp,"fps='%d/%d'\n",global->fps_num,global->fps);
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
		fprintf(fp,"image_path='%s/%s'\n",global->imgFPath[1],global->imgFPath[0]);
		fprintf(fp,"# Auto Image naming (filename-n.ext)\n");
		fprintf(fp,"image_inc=%d\n",global->image_inc);
		fprintf(fp,"# Avi capture Full Path Path (Max 100 characters) Filename (Max 20 characters)\n");
		fprintf(fp,"avi_path='%s/%s'\n",global->aviFPath[1],global->aviFPath[0]);
		fprintf(fp,"# control profiles Full Path Path (Max 10 characters) Filename (Max 20 characters)\n");
		fprintf(fp,"profile_path='%s/%s'\n",global->profile_FPath[1],global->profile_FPath[0]);
		printf("write %s OK\n",global->confPath);
		fclose(fp);
	} 
	else 
	{
		printf("Could not write file %s \n Please check file permissions\n",global->confPath);
		ret=-1;
	}
	return ret;
}

/*----------------------- read conf (.guvcviewrc) file -----------------------*/
int
readConf(struct GLOBAL *global)
{
	int ret=0;
	int signal=1; /*1=>+ or -1=>-*/
	GScanner  *scanner;
	GTokenType ttype;
	GScannerConfig config = 
	{
		" \t\r\n",                     /* characters to skip */
		G_CSET_a_2_z "_" G_CSET_A_2_Z, /* identifier start */
		G_CSET_a_2_z "_." G_CSET_A_2_Z G_CSET_DIGITS,/* identifier cont. */
		"#\n",                         /* single line comment */
		FALSE,                         /* case_sensitive */
		TRUE,                          /* skip multi-line comments */
		TRUE,                          /* skip single line comments */
		FALSE,                         /* scan multi-line comments */
		TRUE,                          /* scan identifiers */
		TRUE,                          /* scan 1-char identifiers */
		FALSE,                         /* scan NULL identifiers */
		FALSE,                         /* scan symbols */
		FALSE,                         /* scan binary */
		FALSE,                         /* scan octal */
		TRUE,                          /* scan float */
		TRUE,                          /* scan hex */
		FALSE,                         /* scan hex dollar */
		TRUE,                          /* scan single quote strings */
		TRUE,                          /* scan double quite strings */
		TRUE,                          /* numbers to int */
		FALSE,                         /* int to float */
		TRUE,                          /* identifier to string */
		TRUE,                          /* char to token */
		FALSE,                          /* symbol to token */
		FALSE,                         /* scope 0 fallback */
		FALSE                          /* store int64 */
	};

	int fd = open (global->confPath, O_RDONLY);
	
	if (fd < 0 )
	{
		printf("Could not open %s for read,\n will try to create it\n",global->confPath);
		ret=writeConf(global);
	}
	else
	{
		scanner = g_scanner_new (&config);
		g_scanner_input_file (scanner, fd);
		scanner->input_name = global->confPath;
		
		for (ttype = g_scanner_get_next_token (scanner);
			ttype != G_TOKEN_EOF;
			ttype = g_scanner_get_next_token (scanner)) 
		{
			if (ttype == G_TOKEN_STRING) 
			{
				//printf("reading %s...\n",scanner->value.v_string);
				char *name = g_strdup (scanner->value.v_string);
				ttype = g_scanner_get_next_token (scanner);
				if (ttype != G_TOKEN_EQUAL_SIGN) 
				{
					g_scanner_unexp_token (scanner,
						G_TOKEN_EQUAL_SIGN,
						NULL,
						NULL,
						NULL,
						NULL,
						FALSE);
				}
				else
				{
					ttype = g_scanner_get_next_token (scanner);
					
					if (ttype == G_TOKEN_STRING)
					{
						if (g_strcmp0(name,"video_device")==0) 
						{
							g_snprintf(global->videodevice,15,"%s",scanner->value.v_string);
						}
						else if (g_strcmp0(name,"resolution")==0) 
						{
							sscanf(scanner->value.v_string,"%ix%i",
								&(global->width), &(global->height));
						}
						else if (g_strcmp0(name,"windowsize")==0) 
						{
							sscanf(scanner->value.v_string,"%ix%i",
								&(global->winwidth), &(global->winheight));
						}
						else if (g_strcmp0(name,"mode")==0)
						{
							/*mode to format conversion is done in readOpts    */
							/*so readOpts must allways be called after readConf*/
							g_snprintf(global->mode,5,"%s",scanner->value.v_string);
						}
						else if (g_strcmp0(name,"fps")==0)
						{
							sscanf(scanner->value.v_string,"%i/%i",
								&(global->fps_num), &(global->fps));
						}
						else if (g_strcmp0(name,"image_path")==0)
						{
							global->imgFPath = splitPath(scanner->value.v_string,global->imgFPath);
							/*get the file type*/
							global->imgFormat = check_image_type(global->imgFPath[0]);
						}
						else if (g_strcmp0(name,"avi_path")==0) 
						{
							global->aviFPath=splitPath(scanner->value.v_string,global->aviFPath);
						}
						else if (g_strcmp0(name,"profile_path")==0) 
						{
							global->profile_FPath=splitPath(scanner->value.v_string,
								global->profile_FPath);
						}
						else
						{
							printf("unexpected string value (%s) for %s\n", 
								scanner->value.v_string, name);
						}
					}
					else if (ttype==G_TOKEN_INT)
					{
						if (g_strcmp0(name,"stack_size")==0)
						{
							global->stack_size = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vid_sleep")==0)
						{
							global->vid_sleep = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vpane")==0) 
						{
							global->boxvsize = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"spinbehave")==0) 
						{
							global->spinbehave = scanner->value.v_int;
						}
						else if (strcmp(name,"fps_display")==0) 
						{
							global->FpsCount = (short) scanner->value.v_int;
						}
						else if (g_strcmp0(name,"auto_focus")==0) 
						{
							global->autofocus = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"bpp")==0) 
						{
							global->bpp = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"hwaccel")==0) 
						{
							global->hwaccel = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"grabmethod")==0) 
						{
							global->grabmethod = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"avi_format")==0) 
						{
							global->AVIFormat = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"avi_max_len")==0) 
						{
							global->AVI_MAX_LEN = (ULONG) scanner->value.v_int;
							global->AVI_MAX_LEN = AVI_set_MAX_LEN (global->AVI_MAX_LEN);
						}
						else if (g_strcmp0(name,"avi_inc")==0) 
						{
							global->avi_inc = (DWORD) scanner->value.v_int;
							g_snprintf(global->aviinc_str,20,_("File num:%d"),global->avi_inc);
						}
						else if (g_strcmp0(name,"sound")==0) 
						{
							global->Sound_enable = (short) scanner->value.v_int;
						}
						else if (g_strcmp0(name,"snd_device")==0) 
						{
							global->Sound_UseDev = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"snd_samprate")==0) 
						{
							global->Sound_SampRateInd = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"snd_numchan")==0) 
						{
							global->Sound_NumChanInd = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"snd_numsec")==0) 
						{
							global->Sound_NumSec = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"snd_format")==0) 
						{
							global->Sound_Format = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"snd_bitrate")==0) 
						{
							global->Sound_bitRate = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"Pan_Step")==0)
						{
							global->PanStep = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"Tilt_Step")==0)
						{
							global->TiltStep = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"frame_flags")==0) 
						{
							global->Frame_Flags = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"image_inc")==0) 
						{
							global->image_inc = (DWORD) scanner->value.v_int;
							g_snprintf(global->imageinc_str,20,_("File num:%d"),global->image_inc);
						}
						else
						{
							printf("unexpected integer value (%lu) for %s\n", 
								scanner->value.v_int, name);
							printf("Strings must be quoted\n");
						}
					}
					else if (ttype==G_TOKEN_FLOAT)
					{
						printf("unexpected float value (%f) for %s\n", scanner->value.v_float, name);
					}
					else if (ttype==G_TOKEN_CHAR)
					{
						printf("unexpected char value (%c) for %s\n", scanner->value.v_char, name);
					}
					else
					{
						if (g_strcmp0(name,"Pan_Step")==0)
						{
							if(ttype == '-')
								signal=-1;
							else
								signal=1;
							ttype = g_scanner_get_next_token (scanner);
							if (ttype==G_TOKEN_INT)
								global->PanStep = signal * scanner->value.v_int;
							else
								g_scanner_unexp_token (scanner,
									G_TOKEN_NONE,
									NULL,
									NULL,
									NULL,
									"expected a integer value",
									FALSE);
						}
						else
						{
							g_scanner_unexp_token (scanner,
								G_TOKEN_NONE,
								NULL,
								NULL,
								NULL,
								"string values must be quoted",
								FALSE);
						}
					}
				}
				if (name != NULL) free(name);
			}
		}
		
		g_scanner_destroy (scanner);
		close (fd);
		
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
	
	return (ret);
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
				g_snprintf(global->videodevice,15,"%s",optarg);
				break;

			case 'w':
				if (g_strcmp0(optarg, "enable") == 0) global->hwaccel=1;
				else if (g_strcmp0(optarg, "disable") == 0) global->hwaccel=0;
				break;

			case 'f':
				g_snprintf(global->mode,5,"%s",optarg);
				//global->mode[0] = optarg[0];
				break;

			case 's':
				tmpstr = g_strdup(optarg);
	
				global->width = strtoul(tmpstr, &separateur, 10);
				if (*separateur != 'x') 
				{
					printf("Error in size usage: -s[--size] widthxheight \n");
				} 
				else 
				{
					global->avifile = g_strdup(optarg); /*allocate avifile - must be freed*/
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
				tmpstr = g_strdup(optarg);
				global->imgFPath=splitPath(tmpstr,global->imgFPath);
				/*get the file type*/
				global->imgFormat = check_image_type(global->imgFPath[0]);
				free(tmpstr);
				tmpstr=NULL;
				break;
		
			case 'c':
				tmpstr = g_strdup(optarg);
				global->image_timer= strtoul(tmpstr, &separateur, 10);
				global->image_inc=1;
				printf("capturing images every %i seconds",global->image_timer);
				free(tmpstr);
				tmpstr=NULL;
				break;
		
			case 'm':
				tmpstr = g_strdup(optarg);
				global->image_npics= strtoul(tmpstr, &separateur, 10);
				printf("capturing at max %d pics",global->image_npics);
				free(tmpstr);
				tmpstr=NULL;
				break;
	
			case 'n':
				global->avifile = g_strdup(optarg);
				global->aviFPath=splitPath(global->avifile,global->aviFPath);
				break;
		
			case 't':
				tmpstr = g_strdup(optarg);
				global->Capture_time= strtoul(tmpstr, &separateur, 10);
				printf("capturing avi for %i seconds",global->Capture_time);
				free(tmpstr);
				tmpstr=NULL;
				break;
		
			case 'p':
				if (g_strcmp0(optarg, "enable") == 0) global->FpsCount=1;
				else if (g_strcmp0(optarg, "disable") == 0) global->FpsCount=0;
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
				printf("   default mjpg  others options are yuv uyv yup gbr mjpg jpeg\n");
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
	
	/*get format from mode*/
	global->format=get_PixFormat(global->mode);
	
	if (global->debug) printf("Format is %s\n",global->mode);
}
