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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <string.h>
//#include <pthread.h>
#include <SDL/SDL.h>
#include <linux/videodev.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <glib.h>
#include <glib/gprintf.h>
/* support for internationalization - i18n */
#include <locale.h> //gentoo patch
#include <glib/gi18n.h>
#include "../config.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <portaudio.h>

#include "v4l2uvc.h"
#include "avilib.h"
#include "globals.h"
#include "colorspaces.h"
#include "sound.h"
#include "snd_devices.h"
#include "jpgenc.h"
#include "autofocus.h"
#include "picture.h"
#include "ms_time.h"
#include "string_utils.h"
#include "options.h"
#include "guvcview.h"
#include "video.h"
#include "mp2.h"
#include "profile.h"
#include "callbacks.h"
#include "close.h"
#include "img_controls.h"
#include "timers.h"

/*----------------------------- globals --------------------------------------*/

struct paRecordData *pdata = NULL;
struct GLOBAL *global = NULL;
struct focusData *AFdata = NULL;
struct vdIn *videoIn = NULL;
struct avi_t *AviOut = NULL;

/*controls data*/
struct VidState *s = NULL;
/*global widgets*/
struct GWIDGET *gwidget = NULL;

/*thread definitions*/
GThread *video_thread = NULL;

/* parameters passed when restarting*/
//gchar *EXEC_CALL;

/*--------------------------- file chooser dialog ----------------------------*/
static void
file_chooser (GtkButton * FileButt, const int isAVI)
{
	gwidget->FileDialog = gtk_file_chooser_dialog_new (_("Save File"),
		GTK_WINDOW (gwidget->mainwin),
		GTK_FILE_CHOOSER_ACTION_SAVE,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
		NULL);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (gwidget->FileDialog), TRUE);

	if(isAVI) 
	{ /* avi File chooser*/
	
		const gchar *basename =  gtk_entry_get_text(GTK_ENTRY(gwidget->AVIFNameEntry));
		
		global->aviFPath=splitPath((gchar *) basename, global->aviFPath);
	
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (gwidget->FileDialog), 
			global->aviFPath[1]);
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (gwidget->FileDialog),
			global->aviFPath[0]);

		if (gtk_dialog_run (GTK_DIALOG (gwidget->FileDialog)) == GTK_RESPONSE_ACCEPT)
		{
			gchar *fullname = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (gwidget->FileDialog));
			global->aviFPath=splitPath(fullname, global->aviFPath);
			gtk_entry_set_text(GTK_ENTRY(gwidget->AVIFNameEntry)," ");
			gtk_entry_set_text(GTK_ENTRY(gwidget->AVIFNameEntry),global->aviFPath[0]);
			g_free(fullname);
		}
	}
	else 
	{	/* Image File chooser*/
		const gchar *basename =  gtk_entry_get_text(GTK_ENTRY(gwidget->ImageFNameEntry));

		global->imgFPath=splitPath((gchar *)basename, global->imgFPath);
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (gwidget->FileDialog), 
			global->imgFPath[1]);
		gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (gwidget->FileDialog), 
			global->imgFPath[0]);

		if (gtk_dialog_run (GTK_DIALOG (gwidget->FileDialog)) == GTK_RESPONSE_ACCEPT)
		{
			gchar *fullname = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (gwidget->FileDialog));
			global->imgFPath=splitPath(fullname, global->imgFPath);
			g_free(fullname);
			gtk_entry_set_text(GTK_ENTRY(gwidget->ImageFNameEntry)," ");
			gtk_entry_set_text(GTK_ENTRY(gwidget->ImageFNameEntry),global->imgFPath[0]);
			/*get the file type*/
			global->imgFormat = check_image_type(global->imgFPath[0]);
			/*set the file type*/
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->ImageType),global->imgFormat);
		  
			if(global->image_inc>0)
			{ 
				global->image_inc=1; /*if auto naming restart counter*/
				g_snprintf(global->imageinc_str,24,_("File num:%d"),global->image_inc);
				gtk_label_set_text(GTK_LABEL(gwidget->ImageIncLabel), global->imageinc_str);
			}
		
		}
	}
	gtk_widget_destroy (gwidget->FileDialog);
}

/*--------------------------------- MAIN -------------------------------------*/
int main(int argc, char *argv[])
{
	int i, line;
	int ret=0;
	/*print package name and version*/ 
	g_printf("%s\n", PACKAGE_STRING);
	
	/* initialize glib threads - make glib thread safe*/ 
	if( !g_thread_supported() )
	{
		g_thread_init(NULL);
		gdk_threads_init ();
		//printf("g_thread supported\n");
	}
	else
	{
		g_printerr("Fatal:g_thread NOT supported\n");
		exit(1);
	}
	
#ifdef ENABLE_NLS
	char* lc_all = setlocale (LC_ALL, "");
	char* lc_dir = bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	char* txtdom = textdomain (GETTEXT_PACKAGE);
#endif
	/*structure containing all shared data - passed in callbacks*/
	struct ALL_DATA all_data;
	memset(&all_data,0,sizeof(struct ALL_DATA));
	
	/*allocate global variables*/
	global = g_new0(struct GLOBAL, 1);
	initGlobals(global);
	
	/*------------------------ reads command line options --------------------*/
	readOpts(argc,argv,global);
	
	/*------------------------- reads configuration file ---------------------*/
	readConf(global);
	
	/*---------------------------------- Allocations -------------------------*/
	
	gwidget = g_new0(struct GWIDGET, 1);
	gwidget->avi_widget_state = TRUE;

	/* widgets */
	GtkWidget *scroll1;
	GtkWidget *scroll2;
	GtkWidget *scroll3;
	GtkWidget *buttons_table;
	GtkWidget *profile_labels;
	GtkWidget *capture_labels;
	GtkWidget *FrameRate;
	GtkWidget *ShowFPS;
	GtkWidget *ImpType;
	GtkWidget *label_ImpType;
	GtkWidget *label_FPS;
	GtkWidget *table2;
	GtkWidget *table3;
	GtkWidget *labelResol;
	//GtkWidget *AviFileButt;
	GtkWidget *label_Device;
	GtkWidget *label_ImageType;
	GtkWidget *label_AVIComp;
	GtkWidget *label_SndSampRate;
	GtkWidget *label_SndDevice;
	GtkWidget *label_SndNumChan;
	GtkWidget *label_SndComp;
	GtkWidget *label_videoFilters;
	GtkWidget *label_audioFilters;
	GtkWidget *table_filt;
	GtkWidget *table_snd_eff;
	GtkWidget *quitButton;
	GtkWidget *SProfileButton;
	GtkWidget *LProfileButton;
	GtkWidget *Tab1;
	GtkWidget *Tab1Label;
	GtkWidget *Tab1Icon;
	GtkWidget *Tab2;
	GtkWidget *Tab2Label;
	GtkWidget *Tab2Icon;
	GtkWidget *Tab3;
	GtkWidget *Tab3Label;
	GtkWidget *Tab3Icon;
	GtkWidget *label_ImgFile;
	GtkWidget *label_AVIFile;
	//GtkWidget *AVIButton_Img;
	GtkWidget *ImgButton_Img;
	GtkWidget *SButton_Img;
	GtkWidget *LButton_Img;
	GtkWidget *QButton_Img;
	GtkWidget *HButtonBox;
	GtkWidget *FiltMirrorEnable;
	GtkWidget *FiltUpturnEnable;
	GtkWidget *FiltNegateEnable;
	GtkWidget *FiltMonoEnable;
	GtkWidget *FiltPiecesEnable;
	GtkWidget *EffEchoEnable;
	GtkWidget *EffFuzzEnable;
	GtkWidget* EffRevEnable;
	GtkWidget* EffWahEnable;
	GtkWidget* EffDuckyEnable;
	GtkWidget *VidFolder_img;
	GtkWidget *ImgFolder_img;

	s = g_new0(struct VidState, 1);
	
	pdata = g_new0(struct paRecordData, 1);
	
	/*create mutex for sound buffers*/
	pdata->mutex = g_mutex_new();
	
	/* Allocate the avi_t struct */
	AviOut = g_new0(struct avi_t, 1);
   
#ifdef ENABLE_NLS
	/* if --verbose mode set do debug*/
	if (global->debug) g_printf("language catalog=> dir:%s lang:%s cat:%s.mo\n",lc_dir,lc_all,txtdom);
#endif   
	/*---------------------------- GTK init ----------------------------------*/
	
	gtk_init(&argc, &argv);
	
	/*must be set after gdk_init - called by gtk_init*/
	//gchar* prgname = g_get_prgname();
	//if (prgname == NULL) EXEC_CALL = g_strdup(argv[0]);
	//else EXEC_CALL = g_strdup(prgname);
	
	//printf("EXEC_CALL=%s\n",EXEC_CALL);
	
	/* Create a main window */
	gwidget->mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (gwidget->mainwin), _("GUVCViewer Controls"));
	//gtk_widget_set_usize(mainwin, winwidth, winheight);
	gtk_window_resize(GTK_WINDOW(gwidget->mainwin),global->winwidth,global->winheight);
	/* Add event handlers */
	gtk_signal_connect(GTK_OBJECT(gwidget->mainwin), "delete_event", GTK_SIGNAL_FUNC(delete_event), &all_data);
	
	/*----------------------- init videoIn structure --------------------------*/
	videoIn = g_new0(struct vdIn, 1);
	
	/*set structure with all global allocations*/
	all_data.pdata = pdata;
	all_data.global = global;
	all_data.AFdata = AFdata; /*not allocated yet*/
	all_data.videoIn = videoIn;
	all_data.AviOut = AviOut;
	all_data.gwidget = gwidget;
	all_data.s = s;
	//all_data.EXEC_CALL = EXEC_CALL;
	//all_data.video_thread = video_thread; /*declare after thread creation*/
	
	/*get format from selected mode*/
	global->format = get_PixFormat(global->mode);
	
	if ((ret=init_videoIn
		(videoIn, (char *) global->videodevice, global->width,global->height, 
		global->format, global->grabmethod, global->fps, global->fps_num) )< 0)
	{
		g_printerr("Init video returned %i\n",ret);
		switch (ret) 
		{
			case -1:/*can't open device*/
				ERR_DIALOG (N_("Guvcview error:\n\nUnable to open device"),
					N_("Please make sure the camera is connected\nand that the linux-UVC driver is installed."),
					&all_data);
				break;
				
			case -2:/*invalid format*/
				g_printf("trying minimum setup ...\n");
				if (videoIn->numb_formats > 0) /*check for supported formats*/
				{
					global->formind = 0; /* get the first supported format */
					global->width = videoIn->listVidFormats[global->formind].listVidCap[0].width;
					global->width = videoIn->listVidFormats[global->formind].listVidCap[0].height;
					global->fps_num = videoIn->listVidFormats[global->formind].listVidCap[0].framerate_num[0];
					global->fps = videoIn->listVidFormats[global->formind].listVidCap[0].framerate_denom[0];
				}
				else 
				{
					g_printerr("ERROR: Can't set video stream. No supported format found\nExiting...\n");
					ERR_DIALOG (N_("Guvcview error:\n\nCan't set MJPG or YUV stream for guvcview"),
						N_("Make sure you have a UVC compliant camera\nand that you have the linux UVC driver installed."),
						&all_data);
				}
				
				/*try again with new format*/
				if (init_videoIn
					(videoIn, (char *) global->videodevice, global->width,global->height, 
					global->format, global->grabmethod, global->fps, global->fps_num) < 0)
				{
					g_printerr("ERROR: Minimum Setup Failed.\n Exiting...\n");
					ERR_DIALOG (N_("Guvcview error:\n\nUnable to start with minimum setup"),
						N_("Please reconnect your camera."), 
						&all_data);
				}
				break;
				
			case -3:/*unable to allocate dequeue buffers or mem*/
			case -4:
			case -6:
			default:
				ERR_DIALOG (N_("Guvcview error:\n\nUnable to allocate Buffers"),
					N_("Please try restarting your system."),
					&all_data);
				break;
		}
	}
	/*make sure global and videoIn are in sync after v4l2 initialization*/
	global->width = videoIn->width;
	global->height = videoIn->height;
	global->fps = videoIn->fps;
	global->fps_num = videoIn->fps_num;
	global->format = videoIn->formatIn;
	global->formind = get_FormatIndex(videoIn, global->format);
	if(global->formind < 0) 
	{
		g_printerr("ERROR: Can't set video stream. No supported format found\nExiting...\n");
		ERR_DIALOG (N_("Guvcview error:\n\nCan't set MJPG or YUV stream for guvcview"),
			N_("Make sure you have a UVC compliant camera\nand that you have the linux UVC driver installed."),
			&all_data);
	}
	/* Set jpeg encoder buffer size */
	global->jpeg_bufsize=((videoIn->width)*(videoIn->height))>>1;
	/*-----------------------------GTK widgets---------------------------------*/
	/*----------------------- Image controls Tab ------------------------------*/
	s->table = gtk_table_new (1, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (s->table), 4);
	gtk_table_set_col_spacings (GTK_TABLE (s->table), 4);
	gtk_container_set_border_width (GTK_CONTAINER (s->table), 2);
	
	s->control = NULL;
	/*-- draw the controls --*/
	draw_controls(&all_data);
	
	if (global->lprofile > 0) LoadControls (s,global);
	
	gwidget->boxv = gtk_vpaned_new ();
	gwidget->boxh = gtk_notebook_new();

	gtk_widget_show (s->table);
	gtk_widget_show (gwidget->boxh);
	
	scroll1=gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll1),s->table);
	gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(scroll1), GTK_CORNER_TOP_LEFT);
	
	gtk_widget_show(scroll1);
	
	Tab1 = gtk_hbox_new(FALSE,2);
	Tab1Label = gtk_label_new(_("Image Controls"));
	gtk_widget_show (Tab1Label);
	/*check for files*/
	gchar* Tab1IconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/image_controls.png",NULL);
	/*don't test for file - use default empty image if load fails*/
	/*get icon image*/
	Tab1Icon = gtk_image_new_from_file(Tab1IconPath);
	g_free(Tab1IconPath);
	//gtk_image_set_pixel_size (GTK_IMAGE(Tab1Icon), 48);
	gtk_widget_show (Tab1Icon);
	gtk_box_pack_start (GTK_BOX(Tab1), Tab1Icon, FALSE, FALSE,1);
	gtk_box_pack_start (GTK_BOX(Tab1), Tab1Label, FALSE, FALSE,1);
	
	gtk_widget_show (Tab1);
	
	gtk_notebook_append_page(GTK_NOTEBOOK(gwidget->boxh),scroll1,Tab1);

	gtk_paned_add1(GTK_PANED(gwidget->boxv),gwidget->boxh);
	
	gtk_widget_show (gwidget->boxv);
	
	/*---------------------- Add  Buttons ---------------------------------*/
	buttons_table = gtk_table_new(1,5,FALSE);
	HButtonBox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(HButtonBox),GTK_BUTTONBOX_SPREAD);	
	gtk_box_set_homogeneous(GTK_BOX(HButtonBox),TRUE);

	gtk_table_set_row_spacings (GTK_TABLE (buttons_table), 1);
	gtk_table_set_col_spacings (GTK_TABLE (buttons_table), 4);
	gtk_container_set_border_width (GTK_CONTAINER (buttons_table), 1);
	
	gtk_widget_show (buttons_table);
	gtk_paned_add2(GTK_PANED(gwidget->boxv),buttons_table);
	
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

	if(global->image_timer)
	{	/*image auto capture*/
		gwidget->CapImageButt=gtk_button_new_with_label (_("Stop Auto"));
	}
	else 
	{
		gwidget->CapImageButt=gtk_button_new_with_label (_("Cap. Image"));
	}

	if (global->avifile) 
	{	/*avi capture enabled from start*/
		gwidget->CapAVIButt=gtk_toggle_button_new_with_label (_("Stop AVI"));
		
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->CapAVIButt), TRUE);
	} 
	else 
	{
		gwidget->CapAVIButt=gtk_toggle_button_new_with_label (_("Cap. AVI"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->CapAVIButt), FALSE);
	}

	/*add images to Buttons and top window*/
	/*check for files*/
	gchar* icon1path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/guvcview.png",NULL);
	if (g_file_test(icon1path,G_FILE_TEST_EXISTS))
	{
		gtk_window_set_icon_from_file(GTK_WINDOW (gwidget->mainwin),icon1path,NULL);
	}

	gchar* pix1path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/movie.png",NULL);
	if (g_file_test(pix1path,G_FILE_TEST_EXISTS)) 
	{
		gwidget->AVIButton_Img = gtk_image_new_from_file (pix1path);
		//gtk_image_set_pixel_size (GTK_IMAGE(AVIButton_Img), 64);
		
		gtk_button_set_image(GTK_BUTTON(gwidget->CapAVIButt),gwidget->AVIButton_Img);
		gtk_button_set_image_position(GTK_BUTTON(gwidget->CapAVIButt),GTK_POS_TOP);
	}
	gchar* pix2path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/camera.png",NULL);
	if (g_file_test(pix2path,G_FILE_TEST_EXISTS)) 
	{
		ImgButton_Img = gtk_image_new_from_file (pix2path);
		//gtk_image_set_pixel_size (GTK_IMAGE(ImgButton_Img), 64);
		
		gtk_button_set_image(GTK_BUTTON(gwidget->CapImageButt),ImgButton_Img);
		gtk_button_set_image_position(GTK_BUTTON(gwidget->CapImageButt),GTK_POS_TOP);
	}
	gchar* pix3path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/save.png",NULL);
	if (g_file_test(pix3path,G_FILE_TEST_EXISTS)) 
	{
		SButton_Img = gtk_image_new_from_file (pix3path);
		//gtk_image_set_pixel_size (GTK_IMAGE(SButton_Img), 64);
		
		gtk_button_set_image(GTK_BUTTON(SProfileButton),SButton_Img);
		gtk_button_set_image_position(GTK_BUTTON(SProfileButton),GTK_POS_TOP);
	}
	gchar* pix4path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/controls_folder.png",NULL);
	if (g_file_test(pix4path,G_FILE_TEST_EXISTS)) 
	{
		LButton_Img = gtk_image_new_from_file (pix4path);
		//gtk_image_set_pixel_size (GTK_IMAGE(LButton_Img), 64);
		
		gtk_button_set_image(GTK_BUTTON(LProfileButton),LButton_Img);
		gtk_button_set_image_position(GTK_BUTTON(LProfileButton),GTK_POS_TOP);
	}
	gchar* pix5path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/close.png",NULL);
	if (g_file_test(pix5path,G_FILE_TEST_EXISTS)) 
	{
		QButton_Img = gtk_image_new_from_file (pix5path);
		//gtk_image_set_pixel_size (GTK_IMAGE(QButton_Img), 64);
		
		gtk_button_set_image(GTK_BUTTON(quitButton),QButton_Img);
		gtk_button_set_image_position(GTK_BUTTON(quitButton),GTK_POS_TOP);
	}

	/*must free path strings*/
	g_free(icon1path);
	g_free(pix1path);
	g_free(pix2path);
	g_free(pix3path);
	g_free(pix4path);
	g_free(pix5path);

	gtk_box_pack_start(GTK_BOX(HButtonBox),gwidget->CapImageButt,TRUE,TRUE,2);
	gtk_box_pack_start(GTK_BOX(HButtonBox),gwidget->CapAVIButt,TRUE,TRUE,2);
	gtk_box_pack_start(GTK_BOX(HButtonBox),SProfileButton,TRUE,TRUE,2);
	gtk_box_pack_start(GTK_BOX(HButtonBox),LProfileButton,TRUE,TRUE,2);
	gtk_box_pack_start(GTK_BOX(HButtonBox),quitButton,TRUE,TRUE,2);

	gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (gwidget->CapAVIButt), FALSE);

	gtk_widget_show (gwidget->CapImageButt);
	gtk_widget_show (gwidget->CapAVIButt);
	gtk_widget_show (LProfileButton);
	gtk_widget_show (SProfileButton);
	gtk_widget_show (quitButton);

	g_signal_connect (GTK_BUTTON(gwidget->CapImageButt), "clicked",
		G_CALLBACK (capture_image), &all_data);
	g_signal_connect (GTK_TOGGLE_BUTTON(gwidget->CapAVIButt), "toggled",
		G_CALLBACK (capture_avi), &all_data);

	g_signal_connect (GTK_BUTTON(quitButton), "clicked",
		G_CALLBACK (quitButton_clicked), &all_data);
	
	gboolean SProfile = TRUE;
	g_object_set_data (G_OBJECT (SProfileButton), "profile_save", &(SProfile));
	g_signal_connect (GTK_BUTTON(SProfileButton), "clicked",
		G_CALLBACK (ProfileButton_clicked), &all_data);
	gboolean LProfile = FALSE;
	g_object_set_data (G_OBJECT (LProfileButton), "profile_save", &(LProfile));
	g_signal_connect (GTK_BUTTON(LProfileButton), "clicked",
		G_CALLBACK (ProfileButton_clicked), &all_data);
	
	/*------------------------- Video Tab ---------------------------------*/
	line=0;
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
	
	Tab2 = gtk_hbox_new(FALSE,2);
	Tab2Label = gtk_label_new(_("Video & Files"));
	gtk_widget_show (Tab2Label);
	
	gchar* Tab2IconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/video_controls.png",NULL);
	/*don't test for file - use default empty image if load fails*/
	/*get icon image*/
	Tab2Icon = gtk_image_new_from_file(Tab2IconPath);
	g_free(Tab2IconPath);
	//gtk_image_set_pixel_size (GTK_IMAGE(Tab2Icon), 48);
	gtk_widget_show (Tab2Icon);
	gtk_box_pack_start (GTK_BOX(Tab2), Tab2Icon, FALSE, FALSE,1);
	gtk_box_pack_start (GTK_BOX(Tab2), Tab2Label, FALSE, FALSE,1);
	
	gtk_widget_show (Tab2);
	gtk_notebook_append_page(GTK_NOTEBOOK(gwidget->boxh),scroll2,Tab2);
	
	/*sets the pan position*/
	if(global->boxvsize==0) 
	{
		global->boxvsize=global->winheight-122;
	}
	gtk_paned_set_position (GTK_PANED(gwidget->boxv),global->boxvsize);
	
	/*Devices*/
	label_Device = gtk_label_new(_("Device:"));
	gtk_misc_set_alignment (GTK_MISC (label_Device), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_Device, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_Device);
	
	
	gwidget->Devices = gtk_combo_box_new_text ();
	for(i=0;i<(videoIn->num_devices);i++)
	{
		gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->Devices),
			videoIn->listVidDevices[i].name);
		if(videoIn->listVidDevices[i].current)
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->Devices),i);
	}
	gtk_table_attach(GTK_TABLE(table2), gwidget->Devices, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->Devices);
	g_signal_connect (GTK_COMBO_BOX(gwidget->Devices), "changed",
		G_CALLBACK (Devices_changed), &all_data);
	
	/* Resolution*/
	gwidget->Resolution = gtk_combo_box_new_text ();
	char temp_str[20];
	int defres=0;
	if (global->debug) 
		g_printf("resolutions of %dº format=%d \n",
			global->formind+1,
			videoIn->listVidFormats[global->formind].numb_res);
	for(i=0;i<videoIn->listVidFormats[global->formind].numb_res;i++) 
	{
		if (videoIn->listVidFormats[global->formind].listVidCap[i].width>0)
		{
			g_snprintf(temp_str,18,"%ix%i",videoIn->listVidFormats[global->formind].listVidCap[i].width,
							 videoIn->listVidFormats[global->formind].listVidCap[i].height);
			gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->Resolution),temp_str);
			
			if ((global->width==videoIn->listVidFormats[global->formind].listVidCap[i].width) && 
				(global->height==videoIn->listVidFormats[global->formind].listVidCap[i].height))
					defres=i;/*set selected resolution index*/
		}
	}
	
	/* Frame Rate */
	line++;
	videoIn->fps_num=global->fps_num;
	videoIn->fps=global->fps;
	input_set_framerate (videoIn);
				  
	FrameRate = gtk_combo_box_new_text ();
	int deffps=0;
	if (global->debug) 
		g_printf("frame rates of %dº resolution=%d \n",
			defres+1,
			videoIn->listVidFormats[global->formind].listVidCap[defres].numb_frates);
	for (i=0;i<videoIn->listVidFormats[global->formind].listVidCap[defres].numb_frates;i++) 
	{
		g_snprintf(temp_str,18,"%i/%i fps",videoIn->listVidFormats[global->formind].listVidCap[defres].framerate_num[i],
			videoIn->listVidFormats[global->formind].listVidCap[defres].framerate_denom[i]);
		gtk_combo_box_append_text(GTK_COMBO_BOX(FrameRate),temp_str);
		
		if ((videoIn->fps_num==videoIn->listVidFormats[global->formind].listVidCap[defres].framerate_num[i]) && 
			(videoIn->fps==videoIn->listVidFormats[global->formind].listVidCap[defres].framerate_denom[i]))
				deffps=i;/*set selected*/
	}
	
	gtk_table_attach(GTK_TABLE(table2), FrameRate, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (FrameRate);
	
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(FrameRate),deffps);
	if (deffps==0) 
	{
		global->fps=videoIn->listVidFormats[global->formind].listVidCap[0].framerate_denom[0];
		global->fps_num=videoIn->listVidFormats[global->formind].listVidCap[0].framerate_num[0];
		videoIn->fps=global->fps;
		videoIn->fps_num=global->fps_num;
	}
	gtk_widget_set_sensitive (FrameRate, TRUE);
	g_signal_connect (GTK_COMBO_BOX(FrameRate), "changed",
		G_CALLBACK (FrameRate_changed), &all_data);
	
	label_FPS = gtk_label_new(_("Frame Rate:"));
	gtk_misc_set_alignment (GTK_MISC (label_FPS), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_FPS, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_FPS);
	
	ShowFPS=gtk_check_button_new_with_label (_(" Show"));
	gtk_table_attach(GTK_TABLE(table2), ShowFPS, 2, 3, line, line+1,
		GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ShowFPS),(global->FpsCount > 0));
	gtk_widget_show (ShowFPS);
	g_signal_connect (GTK_CHECK_BUTTON(ShowFPS), "toggled",
		G_CALLBACK (ShowFPS_changed), &all_data);
	
	
	/* add resolution combo box*/
	line++;
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->Resolution),defres);
	
	if(global->debug) 
		g_printf("Def. Res: %i  numb. fps:%i\n",defres,videoIn->listVidFormats[global->formind].listVidCap[defres].numb_frates);
	
	gtk_table_attach(GTK_TABLE(table2), gwidget->Resolution, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->Resolution);
	
	gtk_widget_set_sensitive (gwidget->Resolution, TRUE);
	g_signal_connect (gwidget->Resolution, "changed",
		G_CALLBACK (resolution_changed), &all_data);
	
	labelResol = gtk_label_new(_("Resolution:"));
	gtk_misc_set_alignment (GTK_MISC (labelResol), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), labelResol, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (labelResol);
	
	/* Input Format*/
	line++;
	ImpType= gtk_combo_box_new_text ();
	
	int fmtind=0;
	for (fmtind=0;fmtind<videoIn->numb_formats;fmtind++)
	{
		gtk_combo_box_append_text(GTK_COMBO_BOX(ImpType),videoIn->listVidFormats[fmtind].fourcc);
		if(global->format == videoIn->listVidFormats[fmtind].format)
			gtk_combo_box_set_active(GTK_COMBO_BOX(ImpType),fmtind); /*set active*/
	}
	
	gtk_table_attach(GTK_TABLE(table2), ImpType, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_widget_set_sensitive (ImpType, TRUE);
	g_signal_connect (GTK_COMBO_BOX(ImpType), "changed",
		G_CALLBACK (ImpType_changed), &all_data);
	gtk_widget_show (ImpType);
	
	label_ImpType = gtk_label_new(_("Camera Output:"));
	gtk_misc_set_alignment (GTK_MISC (label_ImpType), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_ImpType, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_ImpType);
	
	/* Image Capture*/
	line++;
	label_ImgFile= gtk_label_new(_("Image File:"));
	gtk_misc_set_alignment (GTK_MISC (label_ImgFile), 1, 0.5);
    
	gwidget->ImageFNameEntry = gtk_entry_new();
	
	gtk_entry_set_text(GTK_ENTRY(gwidget->ImageFNameEntry),global->imgFPath[0]);
	
	gtk_table_attach(GTK_TABLE(table2), label_ImgFile, 0, 1, line, line+1,
		GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_table_attach(GTK_TABLE(table2), gwidget->ImageFNameEntry, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gwidget->ImgFileButt=gtk_button_new_from_stock(GTK_STOCK_OPEN); 
	gchar* OImgIconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/images_folder.png",NULL);
	/*don't test for file - use default empty image if load fails*/
	/*get icon image*/
	ImgFolder_img = gtk_image_new_from_file(OImgIconPath);
	g_free(OImgIconPath);
	//gtk_image_set_pixel_size (GTK_IMAGE(ImgFolder_img), 32);
	gtk_button_set_image (GTK_BUTTON(gwidget->ImgFileButt), ImgFolder_img);
	
	gtk_table_attach(GTK_TABLE(table2), gwidget->ImgFileButt, 2, 3, line, line+1,
		GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->ImgFileButt);
	
	/*incremental capture*/
	line++;
	gwidget->ImageIncLabel=gtk_label_new(global->imageinc_str);
	gtk_misc_set_alignment (GTK_MISC (gwidget->ImageIncLabel), 0, 0.5);

	gtk_table_attach (GTK_TABLE(table2), gwidget->ImageIncLabel, 1, 2, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (gwidget->ImageIncLabel);
	
	gwidget->ImageInc=gtk_check_button_new_with_label (_("File,Auto"));
	gtk_table_attach(GTK_TABLE(table2), gwidget->ImageInc, 2, 3, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->ImageInc),(global->image_inc > 0));
	gtk_widget_show (ShowFPS);
	g_signal_connect (GTK_CHECK_BUTTON(gwidget->ImageInc), "toggled",
		G_CALLBACK (ImageInc_changed), &all_data);
	gtk_widget_show (gwidget->ImageInc);
	
	/*image format*/
	line++;
	label_ImageType=gtk_label_new(_("Image Format:"));
	gtk_misc_set_alignment (GTK_MISC (label_ImageType), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_ImageType, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_ImageType);
	
	gwidget->ImageType=gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->ImageType),"JPG");
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->ImageType),"BMP");
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->ImageType),"PNG");
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->ImageType),"RAW");
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->ImageType),global->imgFormat);
	gtk_table_attach(GTK_TABLE(table2), gwidget->ImageType, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->ImageType);
	
	gtk_widget_show (label_ImgFile);
	gtk_widget_show (gwidget->ImageFNameEntry);
	gtk_widget_show (gwidget->ImageType);
	g_signal_connect (GTK_COMBO_BOX(gwidget->ImageType), "changed",
		G_CALLBACK (ImageType_changed), &all_data);
	
	g_signal_connect (GTK_BUTTON(gwidget->ImgFileButt), "clicked",
		 G_CALLBACK (file_chooser), GINT_TO_POINTER (0));
	
	
	/*AVI Capture*/
	line++;
	label_AVIFile= gtk_label_new(_("AVI File:"));
	gtk_misc_set_alignment (GTK_MISC (label_AVIFile), 1, 0.5);
	gwidget->AVIFNameEntry = gtk_entry_new();
	
	gtk_table_attach(GTK_TABLE(table2), label_AVIFile, 0, 1, line, line+1,
		GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_table_attach(GTK_TABLE(table2), gwidget->AVIFNameEntry, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gwidget->AviFileButt=gtk_button_new_from_stock(GTK_STOCK_OPEN);
	gchar* OVidIconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/videos_folder.png",NULL);
	/*don't test for file - use default empty image if load fails*/
	/*get icon image*/
	VidFolder_img = gtk_image_new_from_file(OVidIconPath);
	g_free(OVidIconPath);
	//gtk_image_set_pixel_size (GTK_IMAGE(VidFolder_img), 32);
	gtk_button_set_image (GTK_BUTTON(gwidget->AviFileButt), VidFolder_img);
	
	gtk_table_attach(GTK_TABLE(table2), gwidget->AviFileButt, 2, 3, line, line+1,
		GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_widget_show (gwidget->AviFileButt);
	gtk_widget_show (label_AVIFile);
	gtk_widget_show (gwidget->AVIFNameEntry);
	
	g_signal_connect (GTK_BUTTON(gwidget->AviFileButt), "clicked",
		G_CALLBACK (file_chooser), GINT_TO_POINTER (1));
	
	if (global->avifile) 
	{	/*avi capture enabled from start*/
		gtk_entry_set_text(GTK_ENTRY(gwidget->AVIFNameEntry),global->avifile);
	} 
	else 
	{
		videoIn->capAVI = FALSE;
		pdata->capAVI = videoIn->capAVI;
		gtk_entry_set_text(GTK_ENTRY(gwidget->AVIFNameEntry),global->aviFPath[0]);
	}
	
	/*incremental capture*/
	line++;
	gwidget->AVIIncLabel=gtk_label_new(global->aviinc_str);
	gtk_misc_set_alignment (GTK_MISC (gwidget->AVIIncLabel), 0, 0.5);

	gtk_table_attach (GTK_TABLE(table2), gwidget->AVIIncLabel, 1, 2, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (gwidget->AVIIncLabel);
	
	gwidget->AVIInc=gtk_check_button_new_with_label (_("File,Auto"));
	gtk_table_attach(GTK_TABLE(table2), gwidget->AVIInc, 2, 3, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->AVIInc),(global->avi_inc > 0));
	
	g_signal_connect (GTK_CHECK_BUTTON(gwidget->AVIInc), "toggled",
		G_CALLBACK (AVIInc_changed), &all_data);
	gtk_widget_show (gwidget->AVIInc);
	
	
	/* AVI Compressor */
	line++;
	gwidget->AVIComp = gtk_combo_box_new_text ();
	
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->AVIComp),_("MJPG - compressed"));
	
	if(videoIn->formatIn == V4L2_PIX_FMT_UYVY)
		gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->AVIComp),_("UYVY - uncomp YUV"));
	else
		gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->AVIComp),_("YUY2 - uncomp YUV"));
	
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->AVIComp),_("RGB - uncomp BMP"));
	
	gtk_table_attach(GTK_TABLE(table2), gwidget->AVIComp, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->AVIComp);
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->AVIComp),global->AVIFormat);
	
	gtk_widget_set_sensitive (gwidget->AVIComp, TRUE);
	g_signal_connect (GTK_COMBO_BOX(gwidget->AVIComp), "changed",
		G_CALLBACK (AVIComp_changed), &all_data);
	
	label_AVIComp = gtk_label_new(_("AVI Format:"));
	gtk_misc_set_alignment (GTK_MISC (label_AVIComp), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_AVIComp, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_AVIComp);
	
	/*----- Filter controls ----*/
	line++;
	label_videoFilters = gtk_label_new(_("---- Video Filters ----"));
	gtk_misc_set_alignment (GTK_MISC (label_videoFilters), 0.5, 0.5);

	gtk_table_attach (GTK_TABLE(table2), label_videoFilters, 0, 3, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL , 0, 0, 0);

	gtk_widget_show (label_videoFilters);
	
	line++;
	table_filt = gtk_table_new(1,4,FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table_filt), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table_filt), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table_filt), 4);
	gtk_widget_set_size_request (table_filt, -1, -1);
	
	/* Mirror */
	int MirrorFlag = YUV_MIRROR;
	FiltMirrorEnable=gtk_check_button_new_with_label (_(" Mirror"));
	g_object_set_data (G_OBJECT (FiltMirrorEnable), "filt_info", &MirrorFlag);
	gtk_table_attach(GTK_TABLE(table_filt), FiltMirrorEnable, 0, 1, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltMirrorEnable),(global->Frame_Flags & YUV_MIRROR)>0);
	gtk_widget_show (FiltMirrorEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltMirrorEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), &all_data);
	/*Upturn*/
	int UpturnFlag = YUV_UPTURN;
	FiltUpturnEnable=gtk_check_button_new_with_label (_(" Invert"));
	g_object_set_data (G_OBJECT (FiltUpturnEnable), "filt_info", &UpturnFlag);
	gtk_table_attach(GTK_TABLE(table_filt), FiltUpturnEnable, 1, 2, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltUpturnEnable),(global->Frame_Flags & YUV_UPTURN)>0);
	gtk_widget_show (FiltUpturnEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltUpturnEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), &all_data);
	/*Negate*/
	int NegateFlag = YUV_NEGATE;
	FiltNegateEnable=gtk_check_button_new_with_label (_(" Negative"));
	g_object_set_data (G_OBJECT (FiltNegateEnable), "filt_info", &NegateFlag);
	gtk_table_attach(GTK_TABLE(table_filt), FiltNegateEnable, 2, 3, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltNegateEnable),(global->Frame_Flags & YUV_NEGATE)>0);
	gtk_widget_show (FiltNegateEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltNegateEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), &all_data);
	/*Mono*/
	int MonoFlag = YUV_MONOCR;
	FiltMonoEnable=gtk_check_button_new_with_label (_(" Mono"));
	g_object_set_data (G_OBJECT (FiltMonoEnable), "filt_info", &MonoFlag);
	gtk_table_attach(GTK_TABLE(table_filt), FiltMonoEnable, 3, 4, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltMonoEnable),(global->Frame_Flags & YUV_MONOCR)>0);
	gtk_widget_show (FiltMonoEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltMonoEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), &all_data);
    
	/*Pieces*/
	int PiecesFlag = YUV_PIECES;
	FiltPiecesEnable=gtk_check_button_new_with_label (_(" Pieces"));
	g_object_set_data (G_OBJECT (FiltPiecesEnable), "filt_info", &PiecesFlag);
	gtk_table_attach(GTK_TABLE(table_filt), FiltPiecesEnable, 0, 1, 1, 2,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(FiltPiecesEnable),(global->Frame_Flags & YUV_PIECES)>0);
	gtk_widget_show (FiltPiecesEnable);
	g_signal_connect (GTK_CHECK_BUTTON(FiltPiecesEnable), "toggled",
		G_CALLBACK (FiltEnable_changed), &all_data);
	
	gtk_table_attach (GTK_TABLE(table2), table_filt, 0, 3, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (table_filt);

	/*-------------------------- sound Tab --------------------------------*/
	line=0;
	table3 = gtk_table_new(1,3,FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table3), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table3), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table3), 2);
	gtk_widget_show (table3);
	
	scroll3=gtk_scrolled_window_new(NULL,NULL);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scroll3),table3);
	gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(scroll3),
		GTK_CORNER_TOP_LEFT);
	gtk_widget_show(scroll3);
	
	Tab3 = gtk_hbox_new(FALSE,2);
	Tab3Label = gtk_label_new(_("Audio"));
	gtk_widget_show (Tab3Label);
	/*check for files*/
	gchar* Tab3IconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/audio_controls.png",NULL);
	/*don't test for file - use default empty image if load fails*/
	/*get icon image*/
	Tab3Icon = gtk_image_new_from_file(Tab3IconPath);
	g_free(Tab3IconPath);
	//gtk_image_set_pixel_size (GTK_IMAGE(Tab3Icon), 48);
	gtk_widget_show (Tab3Icon);
	gtk_box_pack_start (GTK_BOX(Tab3), Tab3Icon, FALSE, FALSE,1);
	gtk_box_pack_start (GTK_BOX(Tab3), Tab3Label, FALSE, FALSE,1);
	
	gtk_widget_show (Tab3);
	gtk_notebook_append_page(GTK_NOTEBOOK(gwidget->boxh),scroll3,Tab3);
	
	
	/* get sound device list and info */
	gwidget->SndDevice = list_snd_devices (global);
	
	/*--------------------- sound controls --------------------------------*/
	/*enable sound*/
	line++;
	gwidget->SndEnable=gtk_check_button_new_with_label (_(" Sound"));
	gtk_table_attach(GTK_TABLE(table3), gwidget->SndEnable, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->SndEnable),
		(global->Sound_enable > 0));
	gtk_widget_show (gwidget->SndEnable);
	g_signal_connect (GTK_CHECK_BUTTON(gwidget->SndEnable), "toggled",
		G_CALLBACK (SndEnable_changed), &all_data);
		
	/*sound device*/
	line++;	
	gtk_table_attach(GTK_TABLE(table3), gwidget->SndDevice, 1, 3, line, line+1,
		GTK_SHRINK | GTK_FILL , 0, 0, 0);
	gtk_widget_show (gwidget->SndDevice);
	/* using default device*/
	if(global->Sound_UseDev==0) global->Sound_UseDev=global->Sound_DefDev;
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndDevice),global->Sound_UseDev);
	
	if (global->Sound_enable) gtk_widget_set_sensitive (gwidget->SndDevice, TRUE);
	else  gtk_widget_set_sensitive (gwidget->SndDevice, FALSE);
	g_signal_connect (GTK_COMBO_BOX(gwidget->SndDevice), "changed",
		G_CALLBACK (SndDevice_changed), &all_data);
	
	label_SndDevice = gtk_label_new(_("Input Device:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndDevice), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table3), label_SndDevice, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_SndDevice);
	
	/*sample rate*/
	line++;
	gwidget->SndSampleRate= gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndSampleRate),_("Dev. Default"));
	for( i=1; stdSampleRates[i] > 0; i++ )
	{
		char dst[8];
		sprintf(dst,"%d",stdSampleRates[i]);
		gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndSampleRate),dst);
	}
	if (global->Sound_SampRateInd>(i-1)) global->Sound_SampRateInd=0; /*out of range*/
	
	gtk_table_attach(GTK_TABLE(table3), gwidget->SndSampleRate, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->SndSampleRate);
	
	global->Sound_SampRate=stdSampleRates[global->Sound_SampRateInd];
	gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndSampleRate),global->Sound_SampRateInd); /*device default*/
	
	
	if (global->Sound_enable) gtk_widget_set_sensitive (gwidget->SndSampleRate, TRUE);
	else  gtk_widget_set_sensitive (gwidget->SndSampleRate, FALSE);
	g_signal_connect (GTK_COMBO_BOX(gwidget->SndSampleRate), "changed",
		G_CALLBACK (SndSampleRate_changed), &all_data);
	
	label_SndSampRate = gtk_label_new(_("Sample Rate:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndSampRate), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table3), label_SndSampRate, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_SndSampRate);
	
	/*channels*/
	line++;
	gwidget->SndNumChan= gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndNumChan),_("Dev. Default"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndNumChan),_("1 - mono"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndNumChan),_("2 - stereo"));
	
	gtk_table_attach(GTK_TABLE(table3), gwidget->SndNumChan, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	gtk_widget_show (gwidget->SndNumChan);
	switch (global->Sound_NumChanInd) 
	{
		case 0:/*device default*/
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndNumChan),0);
			break;
			
		case 1:/*mono*/	
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndNumChan),1);
			global->Sound_NumChan=1;
			break;
			
		case 2:/*stereo*/
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndNumChan),2);
			global->Sound_NumChan=2;
			break;
			
		default:
			/*set Default to NUM_CHANNELS*/
			global->Sound_NumChan=NUM_CHANNELS;
			break;
	}
	if (global->Sound_enable) gtk_widget_set_sensitive (gwidget->SndNumChan, TRUE);
	else gtk_widget_set_sensitive (gwidget->SndNumChan, FALSE);
	g_signal_connect (GTK_COMBO_BOX(gwidget->SndNumChan), "changed",
		G_CALLBACK (SndNumChan_changed), &all_data);
	
	label_SndNumChan = gtk_label_new(_("Channels:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndNumChan), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table3), label_SndNumChan, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_SndNumChan);
	if (global->debug) g_printf("SampleRate:%d Channels:%d\n",global->Sound_SampRate,global->Sound_NumChan);
	
	/*sound format*/
	line++;
	gwidget->SndComp = gtk_combo_box_new_text ();
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndComp),_("PCM"));
	gtk_combo_box_append_text(GTK_COMBO_BOX(gwidget->SndComp),_("MP2"));
	
	switch (global->Sound_Format) 
	{
		case PA_FOURCC:/*PCM - INT16 or FLOAT32*/
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndComp),0);
			break;
			
		case ISO_FORMAT_MPEG12:/*MP2*/	
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndComp),1);
			break;
			
		default:
			/*set Default to MP2*/
			gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndComp),1);
			global->Sound_Format = ISO_FORMAT_MPEG12;
	}
	if (global->Sound_enable) gtk_widget_set_sensitive (gwidget->SndComp, TRUE);
	
	g_signal_connect (GTK_COMBO_BOX(gwidget->SndComp), "changed",
		G_CALLBACK (SndComp_changed), &all_data);
	
	gtk_table_attach(GTK_TABLE(table3), gwidget->SndComp, 1, 2, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);
	
	gtk_widget_show (gwidget->SndComp);

	label_SndComp = gtk_label_new(_("Audio Format:"));
	gtk_misc_set_alignment (GTK_MISC (label_SndComp), 1, 0.5);

	gtk_table_attach (GTK_TABLE(table3), label_SndComp, 0, 1, line, line+1,
		GTK_FILL, 0, 0, 0);

	gtk_widget_show (label_SndComp);
	
	/*----- Audio effects ----*/
	line++;
	label_audioFilters = gtk_label_new(_("---- Audio Effects ----"));
	gtk_misc_set_alignment (GTK_MISC (label_videoFilters), 0.5, 0.5);

	gtk_table_attach (GTK_TABLE(table3), label_audioFilters, 0, 3, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL , 0, 0, 0);

	gtk_widget_show (label_audioFilters);
	
	line++;
	table_snd_eff = gtk_table_new(1,4,FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (table_snd_eff), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table_snd_eff), 4);
	gtk_container_set_border_width (GTK_CONTAINER (table_snd_eff), 4);
	gtk_widget_set_size_request (table_snd_eff, -1, -1);
	
	gtk_table_attach (GTK_TABLE(table3), table_snd_eff, 0, 3, line, line+1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL , 0, 0, 0);
	gtk_widget_show (table_snd_eff);
	
	/* Echo */
	int EchoEffect = SND_ECHO;
	EffEchoEnable=gtk_check_button_new_with_label (_(" Echo"));
	g_object_set_data (G_OBJECT (EffEchoEnable), "effect_info", &EchoEffect);
	gtk_table_attach(GTK_TABLE(table_snd_eff), EffEchoEnable, 0, 1, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffEchoEnable),(pdata->snd_Flags & SND_ECHO)>0);
	gtk_widget_show (EffEchoEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffEchoEnable), "toggled",
		G_CALLBACK (EffEnable_changed), &all_data);

	/* FUZZ */
	int FuzzEffect = SND_FUZZ;
	EffFuzzEnable=gtk_check_button_new_with_label (_(" Fuzz"));
	g_object_set_data (G_OBJECT (EffFuzzEnable), "effect_info", &FuzzEffect);
	gtk_table_attach(GTK_TABLE(table_snd_eff), EffFuzzEnable, 1, 2, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffFuzzEnable),(pdata->snd_Flags & SND_FUZZ)>0);
	gtk_widget_show (EffFuzzEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffFuzzEnable), "toggled",
		G_CALLBACK (EffEnable_changed), &all_data);
	
	/* Reverb */
	int ReverbEffect = SND_REVERB;
	EffRevEnable=gtk_check_button_new_with_label (_(" Reverb"));
	g_object_set_data (G_OBJECT (EffRevEnable), "effect_info", &ReverbEffect);
	gtk_table_attach(GTK_TABLE(table_snd_eff), EffRevEnable, 2, 3, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffRevEnable),(pdata->snd_Flags & SND_REVERB)>0);
	gtk_widget_show (EffRevEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffRevEnable), "toggled",
		G_CALLBACK (EffEnable_changed), &all_data);
	
	/* WahWah */
	int WahEffect = SND_WAHWAH;
	EffWahEnable=gtk_check_button_new_with_label (_(" WahWah"));
	g_object_set_data (G_OBJECT (EffWahEnable), "effect_info", &WahEffect);
	gtk_table_attach(GTK_TABLE(table_snd_eff), EffWahEnable, 3, 4, 0, 1,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffWahEnable),(pdata->snd_Flags & SND_WAHWAH)>0);
	gtk_widget_show (EffWahEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffWahEnable), "toggled",
		G_CALLBACK (EffEnable_changed), &all_data);
	
	/* Ducky */
	int DuckyEffect = SND_DUCKY;
	EffDuckyEnable=gtk_check_button_new_with_label (_(" Ducky"));
	g_object_set_data (G_OBJECT (EffDuckyEnable), "effect_info", &DuckyEffect);
	gtk_table_attach(GTK_TABLE(table_snd_eff), EffDuckyEnable, 0, 1, 1, 2,
		GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(EffDuckyEnable),(pdata->snd_Flags & SND_DUCKY)>0);
	gtk_widget_show (EffDuckyEnable);
	g_signal_connect (GTK_CHECK_BUTTON(EffDuckyEnable), "toggled",
		G_CALLBACK (EffEnable_changed), &all_data);
	
	/* main container */
	gtk_container_add (GTK_CONTAINER (gwidget->mainwin), gwidget->boxv);
	
	gtk_widget_show (gwidget->mainwin);
	
	/* if autofocus exists allocate data*/
	if(global->AFcontrol) 
	{
		AFdata = g_new0(struct focusData, 1);
		initFocusData(AFdata);
	}

	all_data.AFdata = AFdata;
	
	/*------------------ Creating the main loop (video) thread ---------------*/
	GError *err1 = NULL;

	if( (video_thread = g_thread_create_full((GThreadFunc) main_loop, 
		(void *) &all_data,       //data - argument supplied to thread
		global->stack_size,       //stack size
		TRUE,                     //joinable
		FALSE,                    //bound
		G_THREAD_PRIORITY_NORMAL, //priority - no priority for threads in GNU-Linux
		&err1)                    //error
	) == NULL)
	{
		g_printerr("Thread create failed: %s!!\n", err1->message );
		g_error_free ( err1 ) ;

		ERR_DIALOG (N_("Guvcview error:\n\nUnable to create Video Thread"),
			N_("Please report it to http://developer.berlios.de/bugs/?group_id=8179"),
			&all_data);      
	}
	all_data.video_thread = video_thread;
	
	/*---------------------- image timed capture -----------------------------*/
	if(global->image_timer)
	{
		global->image_timer_id=g_timeout_add(global->image_timer*1000,
			Image_capture_timer,&all_data);
		set_sensitive_img_contrls(FALSE, gwidget);/*disable image controls*/
	}
	/*--------------------- avi capture from start ---------------------------*/
	if(global->avifile) 
	{
		videoIn->AVIFName = joinPath(videoIn->AVIFName, global->aviFPath);
		
		if(AVI_open_output_file(AviOut, videoIn->AVIFName)<0) 
		{
			g_printerr("Error: Couldn't create Avi: %s\n",
				videoIn->AVIFName);
			videoIn->capAVI = FALSE;
			pdata->capAVI = videoIn->capAVI;
		}
		else 
		{
			/*4CC compression "YUY2"/"UYVY" (YUV) or "DIB " (RGB24)  or  "MJPG"*/
			char *compression="MJPG";

			switch (global->AVIFormat) 
			{
				case 0:
					compression="MJPG";
					break;
					
				case 1:
					if(videoIn->formatIn == V4L2_PIX_FMT_UYVY) compression="UYVY";
					else compression="YUY2";
					break;
					
				case 2:
					compression="DIB ";
					break;
					
				default:
					compression="MJPG";
			}
			AVI_set_video(AviOut, videoIn->width, videoIn->height, videoIn->fps,compression);
			/* audio will be set in aviClose - if enabled*/

			/*disabling sound and avi compression controls*/
			set_sensitive_avi_contrls (FALSE, global->Sound_enable, gwidget);

			if(global->Sound_enable > 0) 
			{
				/*get channels and sample rate*/
				set_sound(global,pdata);
				/*set audio header for avi*/
				AVI_set_audio(AviOut, global->Sound_NumChan, 
					global->Sound_SampRate,
					global->Sound_bitRate, 
					sizeof(SAMPLE)*8, 
					global->Sound_Format);
				/* Initialize sound (open stream)*/
				if(init_sound (pdata)) g_printerr("error opening portaudio\n");
				if (global->Sound_Format == ISO_FORMAT_MPEG12) 
				{
					init_MP2_encoder(pdata, global->Sound_bitRate);
				}
				/* start video capture - with sound*/
				global->AVIstarttime = ms_time();
				videoIn->capAVI = TRUE; /* start video capture */
				pdata->capAVI = videoIn->capAVI;
			} 
			else
			{
				/* start video capture - no sound*/
				global->AVIstarttime = ms_time();
				videoIn->capAVI = TRUE;
				pdata->capAVI = videoIn->capAVI;
			}
	
			if (global->Capture_time) 
			{
				/*sets the timer function*/
				g_timeout_add(global->Capture_time*1000,timer_callback,&all_data);
			}
		}
	}
	
	if (global->FpsCount>0) 
	{
		/*sets the Fps counter timer function every 2 sec*/
		global->timer_id = g_timeout_add(2*1000,FpsCount_callback,&all_data);
	}
	/* The last thing to get called */
	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();

	return 0;
}


