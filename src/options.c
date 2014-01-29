/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
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
#include <unistd.h>
#include <fcntl.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>
/*needs locale.h when debug is enabled*/
#include <locale.h>

#include "defs.h"
#include "globals.h"
#include "vcodecs.h"
#include "acodecs.h"
#include "string_utils.h"
#include "avilib.h"
#include "v4l2uvc.h"
#include "../config.h"
/*----------------------- write conf (.guvcviewrc) file ----------------------*/
int
writeConf(struct GLOBAL *global, char *videodevice)
{
	int ret=0;
	FILE *fp;
    //use c locale - make sure floats are writen with a "." and not a ","
    setlocale(LC_NUMERIC, "C");
    //get pointers to codec properties
    vcodecs_data *vcodec_defaults = get_codec_defaults(global->VidCodec);
    acodecs_data *acodec_defaults = get_aud_codec_defaults(get_ind_by4cc(global->Sound_Format));
	// write to tmp file then rename after sucessfull fsync
	// using fsync avois data loss on system crash
	// see: https://bugs.launchpad.net/ubuntu/+source/linux/+bug/317781/comments/54
	char *tmpfile = g_strjoin (".", global->confPath, "tmp", NULL);
	if ((fp = g_fopen(tmpfile,"w"))!=NULL)
	{
		g_fprintf(fp,"# guvcview configuration file for %s\n\n",videodevice);
		g_fprintf(fp,"version='%s'\n",VERSION);
		g_fprintf(fp,"# video loop sleep time in ms: 0,1,2,3,...\n");
		g_fprintf(fp,"# increased sleep time -> less cpu load, more droped frames\n");
		g_fprintf(fp,"vid_sleep=%i\n",global->vid_sleep);
		g_fprintf(fp,"# capture method: 1- mmap 2- read\n");
		g_fprintf(fp,"cap_meth=%i\n",global->cap_meth);
		g_fprintf(fp,"# video resolution \n");
		g_fprintf(fp,"resolution='%ix%i'\n", global->width, global->height);
		g_fprintf(fp,"# control window size: default %ix%i\n", WINSIZEX, WINSIZEY);
		g_fprintf(fp,"windowsize='%ix%i'\n",global->winwidth, global->winheight);
		g_fprintf(fp,"#Default action. 0 for picture, 1 for video.\n");
		g_fprintf(fp,"default_action=%i\n", global->default_action);
		g_fprintf(fp,"# mode video format 'yuvy' 'yvyu' 'uyvy' 'yyuv' 'yu12' 'yv12' 'nv12' 'nv21' 'nv16' 'nv61' 'y41p' 'grey' 'y10b' 'y16 ' 's501' 's505' 's508' 'gbrg' 'grbg' 'ba81' 'rggb' 'rgb3' 'bgr3' 'jpeg' 'mjpg'(default)\n");
		g_fprintf(fp,"mode='%s'\n",global->mode);
		g_fprintf(fp,"# frames per sec. - hardware supported - default( %i )\n",DEFAULT_FPS);
		g_fprintf(fp,"fps='%d/%d'\n",global->fps_num,global->fps);
		g_fprintf(fp,"#Display Fps counter: 1- Yes 0- No\n");
		g_fprintf(fp,"fps_display=%i\n",global->FpsCount);
		g_fprintf(fp,"#auto focus (continuous): 1- Yes 0- No\n");
		g_fprintf(fp,"auto_focus=%i\n",global->autofocus);
		g_fprintf(fp,"# bytes per pixel: default (0 - current)\n");
		g_fprintf(fp,"bpp=%i\n",global->bpp);
		g_fprintf(fp,"# hardware accelaration: 0 1 (default - 1)\n");
		g_fprintf(fp,"hwaccel=%i\n",global->hwaccel);
		g_fprintf(fp,"# video compression format: 0-MJPG 1-YUY2/UYVY 2-DIB (BMP 24) 3-MPEG1 4-FLV1 5-MPEG2 6-MS MPEG4 V3(DIV3) 7-MPEG4 (DIV5)\n");
		g_fprintf(fp,"vid_codec=%i\n",global->VidCodec);
		g_fprintf(fp,"# video muxer format: 0-AVI 1-MKV 2-WebM\n");
		g_fprintf(fp,"vid_format=%i\n",global->VidFormat);
		g_fprintf(fp,"# Auto Video naming (ex: filename-n.avi)\n");
		g_fprintf(fp,"vid_inc=%i\n",(global->vid_inc > 0 ? 1: 0));
		g_fprintf(fp,"# sound 0 - disable 1 - enable\n");
		g_fprintf(fp,"sound=%i\n",global->Sound_enable);
		g_fprintf(fp,"# sound API: 0- Portaudio  1- Pulseaudio\n");
		g_fprintf(fp,"snd_api=%i\n", global->Sound_API);
		g_fprintf(fp,"# snd_device - sound device index/id as listed by audio API\n");
		g_fprintf(fp,"snd_device=%i\n",global->Sound_UseDev);
		g_fprintf(fp,"# snd_samprate - sound sample rate\n");
		g_fprintf(fp,"snd_samprate=%i\n",global->Sound_SampRateInd);
		g_fprintf(fp,"# snd_numchan - sound number of channels 0- dev def 1 - mono 2 -stereo\n");
		g_fprintf(fp,"snd_numchan=%i\n",global->Sound_NumChanInd);
		g_fprintf(fp,"# sound delay in nanosec - delays sound by the specified amount when capturing video\n");
		g_fprintf(fp,"snd_delay=%llu\n",(unsigned long long) global->Sound_delay);
		g_fprintf(fp,"# Audio codec (PCM=0; MPG2=1; ... )\n");
		g_fprintf(fp,"aud_codec=%i\n",global->AudCodec);
		g_fprintf(fp,"# video filters: 0 -none 1- flip 2- upturn 4- negate 8- mono ...\n");
		g_fprintf(fp,"frame_flags=%i\n",global->Frame_Flags);
		g_fprintf(fp,"#on screen display flags (VU meter)\n");
		g_fprintf(fp,"osd_flags=%i\n",global->osdFlags);
		g_fprintf(fp,"# Auto Image naming (filename-n.ext)\n");
		g_fprintf(fp,"image_inc=%i\n",(global->image_inc > 0 ? 1: 0));
		g_fprintf(fp,"# Image capture Full Path\n");
		g_fprintf(fp,"image_path='%s/%s'\n",global->imgFPath[1],global->imgFPath[0]);
		g_fprintf(fp,"# Video capture Full Path\n");
		g_fprintf(fp,"video_path='%s/%s'\n",global->vidFPath[1],global->vidFPath[0]);
		g_fprintf(fp,"# control profiles Full Path\n");
		g_fprintf(fp,"profile_path='%s/%s'\n",global->profile_FPath[1],global->profile_FPath[0]);
        g_fprintf(fp, "# audio codec properties (remove for default values\n");
        g_fprintf(fp, "acodec_bit_rate=%d\n",acodec_defaults->bit_rate);
		g_fprintf(fp, "# video codec (%s) properties (remove for default values\n", vcodec_defaults->compressor);
		g_fprintf(fp, "vcodec_bit_rate=%d\n",vcodec_defaults->bit_rate);
		g_fprintf(fp, "vcodec_fps=%d\n",vcodec_defaults->fps);
		g_fprintf(fp, "vcodec_monotonic_pts=%d\n",vcodec_defaults->monotonic_pts);
		g_fprintf(fp, "vcodec_qmax=%d\n",vcodec_defaults->qmax);
		g_fprintf(fp, "vcodec_qmin=%d\n",vcodec_defaults->qmin);
		g_fprintf(fp, "vcodec_max_qdiff=%d\n",vcodec_defaults->max_qdiff);
		g_fprintf(fp, "vcodec_dia=%d\n",vcodec_defaults->dia);
		g_fprintf(fp, "vcodec_pre_dia=%d\n",vcodec_defaults->pre_dia);
		g_fprintf(fp, "vcodec_pre_me=%d\n",vcodec_defaults->pre_me);
		g_fprintf(fp, "vcodec_me_pre_cmp=%d\n",vcodec_defaults->me_pre_cmp);
		g_fprintf(fp, "vcodec_me_cmp=%d\n",vcodec_defaults->me_cmp);
		g_fprintf(fp, "vcodec_me_sub_cmp=%d\n",vcodec_defaults->me_sub_cmp);
		g_fprintf(fp, "vcodec_last_pred=%d\n",vcodec_defaults->last_pred);
		g_fprintf(fp, "vcodec_gop_size=%d\n",vcodec_defaults->gop_size);
		g_fprintf(fp, "vcodec_qcompress=%.2f\n",vcodec_defaults->qcompress);
		g_fprintf(fp, "vcodec_qblur=%.2f\n",vcodec_defaults->qblur);
		g_fprintf(fp, "vcodec_subq=%d\n",vcodec_defaults->subq);
		g_fprintf(fp, "vcodec_framerefs=%d\n",vcodec_defaults->framerefs);
		g_fprintf(fp, "vcodec_mb_decision=%d\n",vcodec_defaults->mb_decision);
		g_fprintf(fp, "vcodec_trellis=%d\n",vcodec_defaults->trellis);
		g_fprintf(fp, "vcodec_me_method=%d\n",vcodec_defaults->me_method);
		g_fprintf(fp, "vcodec_mpeg_quant=%d\n",vcodec_defaults->mpeg_quant);
		g_fprintf(fp, "vcodec_max_b_frames=%d\n",vcodec_defaults->max_b_frames);
		g_fprintf(fp, "vcodec_num_threads=%d\n",vcodec_defaults->num_threads);
		g_fprintf(fp, "vcodec_flags=%d\n",vcodec_defaults->flags);
		printf("write %s OK\n",global->confPath);

		//flush stream buffers to filesystem
		fflush(fp);

		//close file after fsync (sync file data to disk)
		if (fsync(fileno(fp)) || fclose(fp))
		{
			perror("error writing configuration data to file");
			ret=-1;
		}
		else
		{
			//rename from tmp to real name
			g_rename (tmpfile, global->confPath);
		}
	}
	else
	{
		g_printerr("Could not write file %s \n Please check file permissions\n",tmpfile);
		ret=-2;
	}
	g_free(tmpfile);

    //return to system locale
    setlocale(LC_NUMERIC, "");

	return ret;
}

/*----------------------- read conf (.config/guvcview/videoX) file -----------------------*/
int
readConf(struct GLOBAL *global)
{
	int ret=0;
	//int signal=1; /*1=>+ or -1=>-*/
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
		TRUE,                          /* scan double quote strings */
		TRUE,                          /* numbers to int */
		FALSE,                         /* int to float */
		TRUE,                          /* identifier to string */
		TRUE,                          /* char to token */
		FALSE,                         /* symbol to token */
		FALSE,                         /* scope 0 fallback */
		TRUE                           /* store int64 */
	};

	int fd = g_open (global->confPath, O_RDONLY, 0);

	if (fd < 0 )
	{
		printf("Could not open %s for read,\n will try to create it\n",global->confPath);
		ret=writeConf(global, global->videodevice);
	}
	else
	{
		scanner = g_scanner_new (&config);
		g_scanner_input_file (scanner, fd);
		scanner->input_name = global->confPath;
		//temp codec values
		int ac_bit_rate =-1, vc_bit_rate=-1, vc_fps=-1, vc_qmax=-1, vc_qmin=-1, vc_max_qdiff=-1, vc_dia=-1;
		int vc_pre_dia=-1, vc_pre_me=-1, vc_me_pre_cmp=-1, vc_me_cmp=-1, vc_me_sub_cmp=-1, vc_last_pred=-1;
		int vc_gop_size=-1, vc_subq=-1, vc_framerefs=-1, vc_mb_decision=-1, vc_trellis=-1, vc_me_method=-1;
		int vc_mpeg_quant=-1, vc_max_b_frames=-1, vc_num_threads=-1, vc_flags=-1, vc_monotonic_pts=-1;
		float vc_qcompress=-1, vc_qblur=-1;
		int VMAJOR =-1, VMINOR=-1, VMICRO=-1;

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
					/*check for signed integers*/
					if(ttype == '-')
					{
						//signal = -1;
						ttype = g_scanner_get_next_token (scanner);
					}

					if (ttype == G_TOKEN_STRING)
					{

						if (g_strcmp0(name,"version")==0)
						{
							sscanf(scanner->value.v_string,"%i.%i.%i",
								&(VMAJOR),
								&(VMINOR),
								&(VMICRO));
						}
						else if (g_strcmp0(name,"resolution")==0)
						{
							if(global->flg_res < 1) /*must check for defaults since ReadOpts runs before ReadConf*/
								sscanf(scanner->value.v_string,"%ix%i",
									&(global->width),
									&(global->height));
						}
						else if (g_strcmp0(name,"windowsize")==0)
						{
							sscanf(scanner->value.v_string,"%ix%i",
								&(global->winwidth), &(global->winheight));
						}
						else if (g_strcmp0(name,"mode")==0)
						{
							if(global->flg_mode < 1)
							{
								/*use fourcc but keep it compatible with luvcview*/
								if(g_strcmp0(scanner->value.v_string,"yuv") == 0)
									g_snprintf(global->mode,5,"yuyv");
								else
									g_snprintf(global->mode,5,"%s",scanner->value.v_string);
							}
						}
						else if (g_strcmp0(name,"fps")==0)
						{
							sscanf(scanner->value.v_string,"%i/%i",
								&(global->fps_num), &(global->fps));
						}
						else if (g_strcmp0(name,"image_path")==0)
						{
							if(global->flg_imgFPath < 1)
							{
								global->imgFPath = splitPath(scanner->value.v_string,global->imgFPath);
								/*get the file type*/

								global->imgFormat = check_image_type(global->imgFPath[0]);
							}
							else
							{
							    /* check if new file != old file */
							    gchar * newPath = g_strjoin ("/", global->imgFPath[1], global->imgFPath[0], NULL);
							    //printf("image path: %s\n old path: %s\n", newPath, scanner->value.v_string);
							    if(g_strcmp0(scanner->value.v_string, newPath) !=0)
	                            {
	                                /* reset counter */
	                                //printf("reset counter from: %i\n", global->image_inc);
	                                if(global->image_inc > 0)
	                                {
	                                    global->image_inc = 1;
	                                    //g_snprintf(global->imageinc_str,20,_("File num:%d"),global->image_inc);
	                                }
	                            }
	                            g_free(newPath);
							}
						}
						else if ((g_strcmp0(name,"video_path")==0) || (g_strcmp0(name,"avi_path")==0))
						{
							if(global->vidfile == NULL)
							{
								global->vidFPath=splitPath(scanner->value.v_string,global->vidFPath);
							}
						}
						else if (g_strcmp0(name,"profile_path")==0)
						{
							if(global->lprofile < 1)
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
						if (g_strcmp0(name,"vid_sleep")==0)
						{
							global->vid_sleep = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"cap_meth")==0)
						{
							if(!(global->flg_cap_meth))
								global->cap_meth = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"default_action")==0)
						{
							global->default_action = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"fps")==0)
						{
							/*parse non-quoted fps values*/
							int line = g_scanner_cur_line(scanner);

							global->fps_num = scanner->value.v_int;
							ttype = g_scanner_peek_next_token (scanner);
							if(ttype=='/')
							{
								/*get '/'*/
								g_scanner_get_next_token (scanner);
								ttype = g_scanner_peek_next_token (scanner);
								if(ttype==G_TOKEN_INT)
								{
									ttype = g_scanner_get_next_token (scanner);
									global->fps = scanner->value.v_int;
								}
								else if (scanner->next_line>line)
								{
									/*start new loop*/
									break;
								}
								else
								{
									ttype = g_scanner_get_next_token (scanner);
									g_scanner_unexp_token (scanner,
										G_TOKEN_NONE,
										NULL,
										NULL,
										NULL,
										"bad value for fps",
										FALSE);
								}
							}
						}
						else if (strcmp(name,"fps_display")==0)
						{
							if(global->flg_FpsCount < 1)
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
							if(global->flg_hwaccel < 1)
								global->hwaccel = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vid_codec")==0 || (g_strcmp0(name,"avi_format")==0))
						{
							global->VidCodec = scanner->value.v_int;
							//sync with global->VidCodec_ID when creating the menu entry for the codecs
						}
						else if (g_strcmp0(name,"vid_format")==0)
						{
							global->VidFormat = scanner->value.v_int;
						}
						else if ((g_strcmp0(name,"vid_inc")==0) || (g_strcmp0(name,"avi_inc")==0))
						{
							global->vid_inc = (DWORD) scanner->value.v_int;
						}
						else if (g_strcmp0(name,"sound")==0)
						{
							global->Sound_enable = (short) scanner->value.v_int;
						}
						else if (g_strcmp0(name,"snd_api")==0)
						{
							global->Sound_API = scanner->value.v_int;
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
						else if (g_strcmp0(name,"snd_delay")==0)
						{
							global->Sound_delay = scanner->value.v_int64;
						}
						else if (g_strcmp0(name,"aud_codec")==0)
						{
							global->AudCodec = scanner->value.v_int;
							global->Sound_Format = get_aud4cc(global->AudCodec);
						}
						else if (g_strcmp0(name,"frame_flags")==0)
						{
							global->Frame_Flags = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"osd_flags")==0)
						{
							global->osdFlags = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"image_inc")==0)
						{
							global->image_inc = (DWORD) scanner->value.v_int;
						}
						else if (g_strcmp0(name,"acodec_bit_rate")==0)
						{
						    ac_bit_rate = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_bit_rate")==0)
						{
						    vc_bit_rate = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_fps")==0)
						{
						    vc_fps = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_monotonic_pts")==0)
						{
						    vc_monotonic_pts = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_qmax")==0)
						{
						    vc_qmax = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_qmin")==0)
						{
						    vc_qmin = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_max_qdiff")==0)
						{
						    vc_max_qdiff = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_dia")==0)
						{
						    vc_dia = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_pre_dia")==0)
						{
						    vc_pre_dia = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_pre_me")==0)
						{
						    vc_pre_me= scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_me_pre_cmp")==0)
						{
						    vc_me_pre_cmp = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_me_cmp")==0)
						{
						    vc_me_cmp = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_me_sub_cmp")==0)
						{
						    vc_me_sub_cmp = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_last_pred")==0)
						{
						    vc_last_pred = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_gop_size")==0)
						{
						    vc_gop_size = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_subq")==0)
						{
						    vc_subq = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_framerefs")==0)
						{
						    vc_framerefs = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_mb_decision")==0)
						{
						    vc_mb_decision = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_trellis")==0)
						{
						    vc_trellis = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_me_method")==0)
						{
						    vc_me_method = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_mpeg_quant")==0)
						{
						    vc_mpeg_quant = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_max_b_frames")==0)
						{
						    vc_max_b_frames = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_flags")==0)
						{
						    vc_flags = scanner->value.v_int;
						}
						else if (g_strcmp0(name,"vcodec_num_threads")==0)
						{
						    vc_num_threads = scanner->value.v_int;
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
					    if (g_strcmp0(name,"vcodec_qcompress")==0)
						{
						    vc_qcompress = scanner->value.v_float;
						}
						else if (g_strcmp0(name,"vcodec_qblur")==0)
						{
						    vc_qblur = scanner->value.v_float;
						}
						else
						    printf("unexpected float value (%f) for %s\n", scanner->value.v_float, name);
					}
					else if (ttype==G_TOKEN_CHAR)
					{
						printf("unexpected char value (%c) for %s\n", scanner->value.v_char, name);
					}
					else
					{
						g_scanner_unexp_token (scanner,
							G_TOKEN_NONE,
							NULL,
							NULL,
							NULL,
							"string values must be quoted - skiping",
							FALSE);
						int line = g_scanner_cur_line (scanner);
						int stp=0;

						do
						{
							ttype = g_scanner_peek_next_token (scanner);
							if(scanner->next_line > line)
							{
								//printf("next line reached\n");
								stp=1;
								break;
							}
							else
							{
								ttype = g_scanner_get_next_token (scanner);
							}
						}
						while (!stp);
					}
				}
				g_free(name);
			}
		}

		g_scanner_destroy (scanner);
		close (fd);

		//get pointers to codec properties
        vcodecs_data *vcodec_defaults = get_codec_defaults(global->VidCodec);
        acodecs_data *acodec_defaults = get_aud_codec_defaults(get_ind_by4cc(global->Sound_Format));

        if (ac_bit_rate >= 0) acodec_defaults->bit_rate = ac_bit_rate;
        if (vc_bit_rate >= 0) vcodec_defaults->bit_rate = vc_bit_rate;
        if (vc_fps >= 0) vcodec_defaults->fps = vc_fps;
        //from 1.5.3 onwards we set version on conf file and monotonic is set by default for all codecs
        if ((vc_monotonic_pts >= 0) && (VMAJOR > 0)) vcodec_defaults->monotonic_pts = vc_monotonic_pts;
		if (vc_qmax >= 0) vcodec_defaults->qmax = vc_qmax;
		if (vc_qmin >= 0) vcodec_defaults->qmin = vc_qmin;
		if (vc_max_qdiff >=0) vcodec_defaults->max_qdiff = vc_max_qdiff;
		if (vc_dia >=0) vcodec_defaults->dia = vc_dia;
		if (vc_pre_dia >=0) vcodec_defaults->pre_dia = vc_pre_dia;
		if (vc_pre_me >=0) vcodec_defaults->pre_me = vc_pre_me;
		if (vc_me_pre_cmp >=0) vcodec_defaults->me_pre_cmp = vc_me_pre_cmp;
		if (vc_me_cmp >=0) vcodec_defaults->me_cmp = vc_me_cmp;
		if (vc_me_sub_cmp >=0) vcodec_defaults->me_sub_cmp = vc_me_sub_cmp;
		if (vc_last_pred >= 0) vcodec_defaults->last_pred = vc_last_pred;
		if (vc_gop_size >= 0) vcodec_defaults->gop_size = vc_gop_size;
		if (vc_subq >=0) vcodec_defaults->subq = vc_subq;
		if (vc_framerefs >=0) vcodec_defaults->framerefs = vc_framerefs;
		if (vc_mb_decision >=0) vcodec_defaults->mb_decision = vc_mb_decision;
		if (vc_trellis >=0) vcodec_defaults->trellis = vc_trellis;
		if (vc_me_method >=0) vcodec_defaults->me_method = vc_me_method;
		if (vc_mpeg_quant >=0) vcodec_defaults->mpeg_quant = vc_mpeg_quant;
		if (vc_max_b_frames >=0) vcodec_defaults->max_b_frames = vc_max_b_frames;
		if (vc_num_threads >=0) vcodec_defaults->num_threads = vc_num_threads;
        if (vc_flags >=0) vcodec_defaults->flags = vc_flags;
        if (vc_qcompress >= 0) vcodec_defaults->qcompress = vc_qcompress;
		if (vc_qblur >=0) vcodec_defaults->qblur = vc_qblur;

        if(global->vid_inc>0)
		{
			uint64_t suffix = get_file_suffix(global->vidFPath[1], global->vidFPath[0]);
			fprintf(stderr, "Video file suffix detected: %" PRIu64 "\n", suffix);
			if(suffix >= G_MAXUINT64)
			{
				global->vidFPath[0] = add_file_suffix(global->vidFPath[0], suffix);
				suffix = 0;
			}
			if(suffix > 0)
				global->vid_inc = suffix + 1;
		}

		if(global->image_inc>0)
		{
			uint64_t suffix = get_file_suffix(global->imgFPath[1], global->imgFPath[0]);
			fprintf(stderr, "Image file suffix detected: %" PRIu64 "\n", suffix);
			if(suffix >= G_MAXUINT64)
			{
				global->imgFPath[0] = add_file_suffix(global->imgFPath[0], suffix);
				suffix = 0;
			}
			if(suffix > 0)
				global->image_inc = suffix + 1;
		}

		if (global->debug)
		{
			g_print("video_device: %s\n",global->videodevice);
			g_print("vid_sleep: %i\n",global->vid_sleep);
			g_print("cap_meth: %i\n",global->cap_meth);
			g_print("resolution: %i x %i\n",global->width,global->height);
			g_print("windowsize: %i x %i\n",global->winwidth,global->winheight);
			g_print("default action: %i\n",global->default_action);
			g_print("mode: %s\n",global->mode);
			g_print("fps: %i/%i\n",global->fps_num,global->fps);
			g_print("Display Fps: %i\n",global->FpsCount);
			g_print("bpp: %i\n",global->bpp);
			g_print("hwaccel: %i\n",global->hwaccel);
			g_print("vid_codec: %i\n",global->VidCodec);
			g_print("sound: %i\n",global->Sound_enable);
			g_print("sound Device: %i\n",global->Sound_UseDev);
			g_print("sound samp rate: %i\n",global->Sound_SampRateInd);
			g_print("sound Channels: %i\n",global->Sound_NumChanInd);
			g_print("Sound delay: %llu nanosec\n",(unsigned long long) global->Sound_delay);
			g_print("Sound Format: %i \n",global->Sound_Format);
			g_print("Pan Step: %i degrees\n",global->PanStep);
			g_print("Tilt Step: %i degrees\n",global->TiltStep);
			g_print("Video Filter Flags: %i\n",global->Frame_Flags);
			g_print("image inc: %" PRIu64 "\n",global->image_inc);
			g_print("profile(default):%s/%s\n",global->profile_FPath[1],global->profile_FPath[0]);
		}
	}

	return (ret);
}

/*------------------------- read command line options ------------------------*/
void
readOpts(int argc,char *argv[], struct GLOBAL *global)
{
	gchar *device=NULL;
	gchar *format=NULL;
	gchar *size = NULL;
	gchar *image = NULL;
	gchar *video=NULL;
	gchar *profile=NULL;
	gchar *separateur=NULL;
	gboolean help = FALSE;
	gboolean help_gtk = FALSE;
	gboolean help_all = FALSE;
	gboolean vers = FALSE;
	gchar *help_str = NULL;
	gchar *help_gtk_str = NULL;
	gchar *help_all_str = NULL;
	gchar *config = NULL;
	int hwaccel=-1;
	int FpsCount=-1;
	int cap_meth=-1;

	GOptionEntry entries[] =
	{
		{ "help-all", 'h', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &help_all, "Display all help options", NULL},
		{ "help-gtk", '!', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &help_gtk, "DISPLAY GTK+ help", NULL},
		{ "help", '?', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &help, "Display help", NULL},
		{ "version", 0, 0, G_OPTION_ARG_NONE, &vers, N_("Prints version"), NULL},
		{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &global->debug, N_("Displays debug information"), NULL },
		{ "device", 'd', 0, G_OPTION_ARG_STRING, &device, N_("Video Device to use [default: /dev/video0]"), "VIDEO_DEVICE" },
		{ "add_ctrls", 'a', 0, G_OPTION_ARG_NONE, &global->add_ctrls, N_("Exit after adding UVC extension controls (needs root/sudo)"), NULL},
		{ "control_only", 'o', 0, G_OPTION_ARG_NONE, &global->control_only, N_("Don't stream video (image controls only)"), NULL},
        { "no_display", 0,0, G_OPTION_ARG_NONE, &global->no_display, N_("Don't display a GUI"), NULL},
		{ "capture_method", 'r', 0, G_OPTION_ARG_INT, &cap_meth, N_("Capture method (1-mmap (default)  2-read)"), "[1 | 2]"},
		{ "config", 'g', 0, G_OPTION_ARG_STRING, &config, N_("Configuration file"), "FILENAME" },
		{ "hwd_acel", 'w', 0, G_OPTION_ARG_INT, &hwaccel, N_("Hardware accelaration (enable(1) | disable(0))"), "[1 | 0]" },
		{ "format", 'f', 0, G_OPTION_ARG_STRING, &format, N_("Pixel format(mjpg|jpeg|yuyv|yvyu|uyvy|yyuv|yu12|yv12|nv12|nv21|nv16|nv61|y41p|grey|y10b|y16 |s501|s505|s508|gbrg|grbg|ba81|rggb|bgr3|rgb3)"), "FORMAT" },
		{ "size", 's', 0, G_OPTION_ARG_STRING, &size, N_("Frame size, default: 640x480"), "WIDTHxHEIGHT"},
		{ "image", 'i', 0, G_OPTION_ARG_STRING, &image, N_("Image File name"), "FILENAME"},
		{ "cap_time", 'c', 0, G_OPTION_ARG_INT, &global->image_timer, N_("Image capture interval in seconds"), "TIME"},
		{ "npics", 'm', 0, G_OPTION_ARG_INT, &global->image_npics, N_("Number of Pictures to capture"), "NUMPIC"},
		{ "video", 'n', 0, G_OPTION_ARG_STRING, &video, N_("Video File name"), "FILENAME"},
		{ "vid_time", 't', 0, G_OPTION_ARG_INT, &global->Capture_time,N_("Video capture time (seconds) - capture from start"), "TIME"},
		{ "exit_on_close", 0, 0, G_OPTION_ARG_NONE, &global->exit_on_close, N_("Exits guvcview after closing video"), NULL},
		{ "skip", 'j', 0, G_OPTION_ARG_INT, &global->skip_n, N_("Number of initial frames to skip"), "N_FRAMES"},
		{ "show_fps", 'p', 0, G_OPTION_ARG_INT, &FpsCount, N_("Show FPS value (enable(1) | disable (0))"), "[1 | 0]"},
		{ "profile", 'l', 0, G_OPTION_ARG_STRING, &profile, N_("Load Profile at start"), "FILENAME"},
		{ "lctl_method", 'k', 0, G_OPTION_ARG_INT, &global->lctl_method, N_("List controls method (0:loop, 1:next_ctrl flag [def])"), "[0 |1]"},
		{ NULL }
	};

	GError *error = NULL;
	GOptionContext *context;
	context = g_option_context_new (N_("- local options"));
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gtk_get_option_group (FALSE));
	g_set_prgname (PACKAGE);
	help_str = g_option_context_get_help (context, TRUE, NULL);
	help_gtk_str = g_option_context_get_help (context, FALSE, gtk_get_option_group (TRUE));
	help_all_str = g_option_context_get_help (context, FALSE, NULL);
	/*disable automatic help parsing - must clean global before exit*/
	g_option_context_set_help_enabled (context, FALSE);
	if (!g_option_context_parse (context, &argc, &argv, &error))
	{
		g_printerr ("option parsing failed: %s\n", error->message);
		g_error_free ( error );
		closeGlobals(global);
		global=NULL;
		g_print("%s",help_all_str);
		g_free(help_all_str);
		g_free(help_str);
		g_free(help_gtk_str);
		g_option_context_free (context);
		exit (1);
	}

	if(vers)
	{
		//print version and exit
		//version already printed in guvcview.c
		closeGlobals(global);
		global=NULL;
		g_free(help_all_str);
		g_free(help_str);
		g_free(help_gtk_str);
		g_option_context_free (context);
		exit(0);
	}
	/*Display help message and exit*/
	if(help_all)
	{
		closeGlobals(global);
		global=NULL;
		g_print("%s",help_all_str);
		g_free(help_all_str);
		g_free(help_str);
		g_free(help_gtk_str);
		g_option_context_free (context);
		exit(0);
	}
	else if(help)
	{
		closeGlobals(global);
		global=NULL;
		g_print("%s",help_str);
		g_free(help_str);
		g_free(help_gtk_str);
		g_free(help_all_str);
		g_option_context_free (context);
		exit(0);
	} else if(help_gtk)
	{
		closeGlobals(global);
		global=NULL;
		g_print("%s",help_gtk_str);
		g_free(help_str);
		g_free(help_gtk_str);
		g_free(help_all_str);
		g_option_context_free (context);
		exit(0);
	}

	/*regular options*/
	if(device)
	{
		gchar *basename = NULL;
		gchar *dirname = NULL;
		basename = g_path_get_basename(device);
		if(!(g_str_has_prefix(basename,"video")))
		{
			g_printerr("%s not a valid video device name\n",
				basename);
		}
		else
		{
			g_free(global->videodevice);
			global->videodevice=NULL;
			dirname = g_path_get_dirname(device);
			if(g_strcmp0(".",dirname)==0)
			{
				g_free(dirname);
				dirname=g_strdup("/dev");
			}

			global->videodevice = g_strjoin("/",
				dirname,
				basename,
				NULL);
			if(global->flg_config < 1)
			{
				if(g_strcmp0("video0",basename) !=0 )
				{
					g_free(global->confPath);
					global->confPath=NULL;
					global->confPath = g_strjoin("/",
						g_get_home_dir(),
						".config",
						"guvcview",
						basename,
						NULL);
				}
			}
		}
		g_free(dirname);
		g_free(basename);
	}
	if(config)
	{
		g_free(global->confPath);
		global->confPath=NULL;
		global->confPath = g_strdup(config);
		global->flg_config = 1;
	}
	if(format)
	{
		/*use fourcc but keep compatability with luvcview*/
		if(g_strcmp0("yuv",format)==0)
			g_snprintf(global->mode,5,"yuyv");
		else if (g_strcmp0("bggr",format)==0) // be compatible with guvcview < 1.1.4
			g_snprintf(global->mode,5,"ba81");
		else
			g_snprintf(global->mode,5,"%s ",format);

	    printf("requested format \"%s\" from command line\n", global->mode);

		global->flg_mode = TRUE;
	}
	if(size)
	{
		global->width = (int) g_ascii_strtoull(size, &separateur, 10);
		if (*separateur != 'x')
		{
			g_printerr("Error in size usage: -s[--size] WIDTHxHEIGHT \n");
		}
		else
		{
			++separateur;
			global->height = (int) g_ascii_strtoull(separateur, &separateur, 10);
			if (*separateur != 0)
				g_printerr("hmm.. don't like that!! trying this height \n");
		}

		global->flg_res = 1;
	}
	if(image)
	{
		global->imgFPath=splitPath(image,global->imgFPath);
		/*get the file type*/
		global->imgFormat = check_image_type(global->imgFPath[0]);
		global->flg_imgFPath = TRUE;

		if(global->image_inc>0)
		{
			uint64_t suffix = get_file_suffix(global->imgFPath[1], global->imgFPath[0]);
			fprintf(stderr, "Image file suffix detected: %" PRIu64 "\n", suffix);
			if(suffix >= G_MAXUINT64)
			{
				global->imgFPath[0] = add_file_suffix(global->imgFPath[0], suffix);
				suffix = 0;
			}
			if(suffix > 0)
				global->image_inc = suffix + 1;
		}
	}
	if(global->image_timer > 0 )
	{
		g_print("capturing images every %i seconds\n",global->image_timer);
	}
	if(video)
	{
		global->vidFPath=splitPath(video, global->vidFPath);
		if(global->vid_inc>0)
		{
			uint64_t suffix = get_file_suffix(global->vidFPath[1], global->vidFPath[0]);
			fprintf(stderr, "Video file suffix detected: %" PRIu64 "\n", suffix);
			if(suffix >= G_MAXUINT64)
			{
				global->vidFPath[0] = add_file_suffix(global->vidFPath[0], suffix);
				suffix = 0;
			}
			if(suffix > 0)
				global->vid_inc = suffix + 1;
		}

		global->vidfile = joinPath(global->vidfile, global->vidFPath);

		g_print("set video file: %s \n",global->vidfile);
		/*get the file type*/
		global->VidFormat = check_video_type(global->vidFPath[0]);
	}
	if(profile)
	{
		global->lprofile=1;
		global->profile_FPath=splitPath(profile,global->profile_FPath);
	}
	if(hwaccel != -1 )
	{
		global->hwaccel = hwaccel;
		global->flg_hwaccel = 1;
	}
	if(FpsCount != -1)
	{
		global->FpsCount = FpsCount;
		global->flg_FpsCount = 1;
	}
	if(cap_meth != -1)
	{
		global->flg_cap_meth = TRUE;
		global->cap_meth = cap_meth;
	}

	//g_print("option capture meth is %i\n", global->cap_meth);
	g_free(help_str);
	g_free(help_gtk_str);
	g_free(help_all_str);
	g_free(device);
	g_free(config);
	g_free(format);
	g_free(size);
	g_free(image);
	g_free(video);
	g_free(profile);
	g_option_context_free (context);
}
