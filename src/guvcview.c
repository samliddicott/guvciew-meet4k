/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#           Dr. Alexander K. Seewald <alex@seewald.at>                          #
#                             Autofocus algorithm                               #
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


#include <SDL/SDL.h>
#include <glib.h>
#include <glib/gprintf.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>
/* locale.h is needed if -O0 used (no optimiztions)  */
/* otherwise included from libintl.h on glib/gi18n.h */
#include <locale.h> 
#include <gtk/gtk.h>
#include <portaudio.h>

#include "../config.h"
#include "avilib.h"
#include "colorspaces.h"
#include "sound.h"
#include "snd_devices.h"
#include "jpgenc.h"
#include "autofocus.h"
#include "picture.h"
#include "ms_time.h"
#include "string_utils.h"
#include "options.h"
#include "video.h"
#include "vcodecs.h"
#include "create_video.h"
#include "mp2.h"
#include "profile.h"
#include "callbacks.h"
#include "close.h"
#include "img_controls.h"
#include "video_tab.h"
#include "audio_tab.h"
#include "timers.h"

/*----------------------------- globals --------------------------------------*/

struct paRecordData *pdata = NULL;
struct GLOBAL *global = NULL;
struct focusData *AFdata = NULL;
struct vdIn *videoIn = NULL;
struct VideoFormatData *videoF = NULL;

/*controls data*/
struct VidState *s = NULL;
/*global widgets*/
struct GWIDGET *gwidget = NULL;

/*thread definitions*/
GThread *video_thread = NULL;

/*--------------------------------- MAIN -------------------------------------*/
int main(int argc, char *argv[])
{
	int ret=0;
	int n=0; //button box labels column
	gboolean control_only = FALSE;
	/*print package name and version*/ 
	g_printf("%s\n", PACKAGE_STRING);
	
	/* initialize glib threads - make glib thread safe*/ 
	if( !g_thread_supported() )
	{
		g_thread_init(NULL);
		gdk_threads_init ();
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
	const gchar * const * langs = g_get_language_names (); //get ordered list of defined languages
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
	
	control_only = global->control_only; //sets local control_only flag
	/*---------------------------------- Allocations -------------------------*/
	
	gwidget = g_new0(struct GWIDGET, 1);
	gwidget->vid_widget_state = TRUE;

	/* widgets */
	GtkWidget *scroll1;
	GtkWidget *buttons_table;
	GtkWidget *profile_labels;
	GtkWidget *capture_labels;
	GtkWidget *quitButton;
	GtkWidget *SProfileButton;
	GtkWidget *LProfileButton;
	GtkWidget *Tab1;
	GtkWidget *Tab1Label;
	GtkWidget *Tab1Icon;
	GtkWidget *ImgButton_Img;
	GtkWidget *SButton_Img;
	GtkWidget *LButton_Img;
	GtkWidget *QButton_Img;
	GtkWidget *HButtonBox;
	
	
	s = g_new0(struct VidState, 1);

	if(!control_only) /*control_only exclusion (video and Audio) */
	{
		pdata = g_new0(struct paRecordData, 1);

		/*create mutex for sound buffers*/
		pdata->mutex = g_mutex_new();

		/* Allocate the video Format struct */
		videoF = g_new0(struct VideoFormatData, 1);

		// must be called before using avcodec lib
		avcodec_init();

		// register all the codecs (you can also register only the codec
		//you wish to have smaller code
		avcodec_register_all();

		/*---------------------------- Start PortAudio API -----------------------*/
		if(global->debug) g_printf("starting portaudio...\n");
		Pa_Initialize(); 
	}

#ifdef ENABLE_NLS
	/* if --verbose mode set do debug*/
	if (global->debug) g_printf("language catalog=> dir:%s type:%s lang:%s cat:%s.mo\n",
		lc_dir, lc_all, langs[0], txtdom);
#endif
	/*---------------------------- GTK init ----------------------------------*/

	gtk_init(&argc, &argv);
	
	/* make sure the type is realized so that we can change the properties*/
	g_type_class_unref (g_type_class_ref (GTK_TYPE_BUTTON));
	/* make sure gtk-button-images property is set to true (defaults to false in karmic)*/
	g_object_set (gtk_settings_get_default (), "gtk-button-images", TRUE, NULL);

	/* Create a main window */
	gwidget->mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (gwidget->mainwin), _("GUVCViewer Controls"));
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
	all_data.videoF = videoF;
	all_data.gwidget = gwidget;
	all_data.s = s;

	/*get format from selected mode*/
	global->format = get_PixFormat(global->mode);
	if(global->debug) g_printf("%s: setting format to %i\n", global->mode, global->format);

	if ( ( ret=init_videoIn (videoIn, global) ) != 0)
	{
		g_printerr("Init video returned %i\n",ret);
		switch (ret) 
		{
			case VDIN_DEVICE_ERR://can't open device
				ERR_DIALOG (N_("Guvcview error:\n\nUnable to open device"),
					N_("Please make sure the camera is connected\nand that the correct driver is installed."),
					&all_data);
				break;
			
			case VDIN_DYNCTRL_OK: //uvc extension controls OK
				ERR_DIALOG (N_("Guvcview:\n\nUVC Extension controls"),
					N_("Extension controls were added to the UVC driver"),
					&all_data);
				break;				

			case VDIN_DYNCTRL_ERR: //uvc extension controls error - EACCES (needs root user)
				ERR_DIALOG (N_("Guvcview error:\n\nUVC Extension controls"),
					N_("An error ocurred while adding extension\ncontrols to the UVC driver\nMake sure you run guvcview as root (or sudo)."),
					&all_data);
				break;
				
			case VDIN_UNKNOWN_ERR: //unknown error (treat as invalid format)
			case VDIN_FORMAT_ERR://invalid format
			case VDIN_RESOL_ERR: //invalid resolution
				g_printf("trying minimum setup ...\n");
				if (videoIn->listFormats->numb_formats > 0) //check for supported formats
				{
					VidFormats *listVidFormats;
					videoIn->listFormats->current_format = 0; //get the first supported format 
					global->format = videoIn->listFormats->listVidFormats[0].format;
					if(get_PixMode(global->format, global->mode) < 0)
						g_printerr("IMPOSSIBLE: format has no supported mode !?\n");
					listVidFormats = &videoIn->listFormats->listVidFormats[0];
					global->width = listVidFormats->listVidCap[0].width;
					global->width = listVidFormats->listVidCap[0].height;
					if (listVidFormats->listVidCap[0].framerate_num != NULL)
						global->fps_num = listVidFormats->listVidCap[0].framerate_num[0];
					if (listVidFormats->listVidCap[0].framerate_denom != NULL)
						global->fps = listVidFormats->listVidCap[0].framerate_denom[0];
				}
				else 
				{
					g_printerr("ERROR: Can't set video stream. No supported format found\nExiting...\n");
					ERR_DIALOG (N_("Guvcview error:\n\nCan't set a valid video stream for guvcview"),
						N_("Make sure your device driver is v4l2 compliant\nand that it is properly installed."),
						&all_data);
				}
				
				//try again with new format
				ret = init_videoIn (videoIn, global);
				
				if ((ret == VDIN_QUERYBUF_ERR) && (global->cap_meth != videoIn->cap_meth))
				{
					//mmap not supported ? try again with read method
					g_printerr("mmap failed trying read method...");
					global->cap_meth = videoIn->cap_meth;
					ret = init_videoIn (videoIn, global);
					if (ret == VDIN_OK) 
						g_printerr("OK\n");
					else
						g_printerr("FAILED\n");
				} 
				
				if (ret < 0)
				{
					g_printerr("ERROR: Minimum Setup Failed.\n Exiting...\n");
					ERR_DIALOG (N_("Guvcview error:\n\nUnable to start with minimum setup"),
						N_("Please reconnect your camera."), 
						&all_data);
				}
				
				break;
				
			case VDIN_QUERYBUF_ERR:
				if (global->cap_meth != videoIn->cap_meth)
				{
					//mmap not supported ? try again with read method
					g_printerr("mmap failed trying read method...");
					global->cap_meth = videoIn->cap_meth;
					ret = init_videoIn (videoIn, global);
					if (ret == VDIN_OK) 
						g_printerr("OK\n");
					else
					{
						g_printerr("FAILED\n");
						//return to default method(mmap)
						global->cap_meth = IO_MMAP;
						g_printerr("ERROR: Minimum Setup Failed.\n Exiting...\n");
						ERR_DIALOG (N_("Guvcview error:\n\nUnable to start with minimum setup"),
							N_("Please reconnect your camera."), 
							&all_data);
					}
				}
				break;
				
			case VDIN_QUERYCAP_ERR:
				ERR_DIALOG (N_("Guvcview error:\n\nCouldn't query device capabilities"),
					N_("Make sure the device driver suports v4l2."),
					&all_data);
				break;
			case VDIN_READ_ERR:
				ERR_DIALOG (N_("Guvcview error:\n\nRead method error"),
					N_("Please try mmap instead (--capture_method=1)."),
					&all_data);
				break;
			
			case VDIN_REQBUFS_ERR:/*unable to allocate dequeue buffers or mem*/
			case VDIN_ALLOC_ERR:
			case VDIN_FBALLOC_ERR:
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
	videoIn->listFormats->current_format = get_FormatIndex(videoIn->listFormats, global->format);
	if(videoIn->listFormats->current_format < 0) 
	{
		g_printerr("ERROR: Can't set video stream. No supported format found\nExiting...\n");
		ERR_DIALOG (N_("Guvcview error:\n\nCan't set a valid video stream for guvcview"),
			N_("Make sure your device driver is v4l2 compliant\nand that it is properly installed."),
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
	
	if(!control_only) /*control_only exclusion (video and Audio) */
	{
		capture_labels=gtk_label_new(_("Capture:"));
		gtk_misc_set_alignment (GTK_MISC (capture_labels), 0.5, 0.5);
		gtk_table_attach (GTK_TABLE(buttons_table), capture_labels, n, n+2, 0, 1,
			GTK_SHRINK | GTK_FILL | GTK_EXPAND, 0, 0, 0);
		gtk_widget_show (capture_labels);
		n+=2; //increment column for labels
	}//end of control only exclusion
	
	profile_labels=gtk_label_new(_("Control Profiles:"));
	gtk_misc_set_alignment (GTK_MISC (profile_labels), 0.5, 0.5);

	gtk_table_attach (GTK_TABLE(buttons_table), profile_labels, n, n+2, 0, 1,
		GTK_SHRINK | GTK_FILL | GTK_EXPAND , 0, 0, 0);
	gtk_widget_show (profile_labels);
	
	gtk_table_attach(GTK_TABLE(buttons_table), HButtonBox, 0, 5, 1, 2,
		GTK_SHRINK | GTK_FILL | GTK_EXPAND, 0, 0, 0);
		
	gtk_widget_show(HButtonBox);
	
	quitButton=gtk_button_new_from_stock(GTK_STOCK_QUIT);
	SProfileButton=gtk_button_new_from_stock(GTK_STOCK_SAVE);
	LProfileButton=gtk_button_new_from_stock(GTK_STOCK_OPEN);

	gchar* icon1path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/guvcview.png",NULL);
	if (g_file_test(icon1path,G_FILE_TEST_EXISTS))
	{
		gtk_window_set_icon_from_file(GTK_WINDOW (gwidget->mainwin),icon1path,NULL);
	}
	g_free(icon1path);
	
	if(!control_only)/*control_only exclusion Image and video buttons*/
	{
		if(global->image_timer)
		{	/*image auto capture*/
			gwidget->CapImageButt=gtk_button_new_with_label (_("Stop Auto"));
		}
		else 
		{
			gwidget->CapImageButt=gtk_button_new_with_label (_("Cap. Image"));
		}

		if (global->vidfile) 
		{	/*vid capture enabled from start*/
			gwidget->CapVidButt=gtk_toggle_button_new_with_label (_("Stop Video"));
		
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->CapVidButt), TRUE);
		} 
		else 
		{
			gwidget->CapVidButt=gtk_toggle_button_new_with_label (_("Cap. Video"));
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
		}

		/*add images to Buttons and top window*/
		/*check for files*/

		gchar* pix1path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/movie.png",NULL);
		if (g_file_test(pix1path,G_FILE_TEST_EXISTS)) 
		{
			gwidget->VidButton_Img = gtk_image_new_from_file (pix1path);
		
			gtk_button_set_image(GTK_BUTTON(gwidget->CapVidButt),gwidget->VidButton_Img);
			gtk_button_set_image_position(GTK_BUTTON(gwidget->CapVidButt),GTK_POS_TOP);
			//gtk_widget_show (gwidget->VidButton_Img);
		}
		//else g_printf("couldn't load %s\n", pix1path);
		gchar* pix2path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/camera.png",NULL);
		if (g_file_test(pix2path,G_FILE_TEST_EXISTS)) 
		{
			ImgButton_Img = gtk_image_new_from_file (pix2path);
		
			gtk_button_set_image(GTK_BUTTON(gwidget->CapImageButt),ImgButton_Img);
			gtk_button_set_image_position(GTK_BUTTON(gwidget->CapImageButt),GTK_POS_TOP);
			//gtk_widget_show (ImgButton_Img);
		}
		g_free(pix1path);
		g_free(pix2path);
		gtk_box_pack_start(GTK_BOX(HButtonBox),gwidget->CapImageButt,TRUE,TRUE,2);
		gtk_box_pack_start(GTK_BOX(HButtonBox),gwidget->CapVidButt,TRUE,TRUE,2);
		gtk_toggle_button_set_mode (GTK_TOGGLE_BUTTON (gwidget->CapVidButt), FALSE);
		gtk_widget_show (gwidget->CapImageButt);
		gtk_widget_show (gwidget->CapVidButt);
		
		g_signal_connect (GTK_BUTTON(gwidget->CapImageButt), "clicked",
			G_CALLBACK (capture_image), &all_data);
		g_signal_connect (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), "toggled",
			G_CALLBACK (capture_vid), &all_data);
	}/*end of control_only exclusion*/
	
	gchar* pix3path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/save.png",NULL);
	if (g_file_test(pix3path,G_FILE_TEST_EXISTS)) 
	{
		SButton_Img = gtk_image_new_from_file (pix3path);
		
		gtk_button_set_image(GTK_BUTTON(SProfileButton),SButton_Img);
		gtk_button_set_image_position(GTK_BUTTON(SProfileButton),GTK_POS_TOP);
		//gtk_widget_show (SButton_Img);
	}
	gchar* pix4path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/controls_folder.png",NULL);
	if (g_file_test(pix4path,G_FILE_TEST_EXISTS)) 
	{
		LButton_Img = gtk_image_new_from_file (pix4path);
		
		gtk_button_set_image(GTK_BUTTON(LProfileButton),LButton_Img);
		gtk_button_set_image_position(GTK_BUTTON(LProfileButton),GTK_POS_TOP);
		//gtk_widget_show (LButton_Img);
	}
	gchar* pix5path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/close.png",NULL);
	if (g_file_test(pix5path,G_FILE_TEST_EXISTS)) 
	{
		QButton_Img = gtk_image_new_from_file (pix5path);
		
		gtk_button_set_image(GTK_BUTTON(quitButton),QButton_Img);
		gtk_button_set_image_position(GTK_BUTTON(quitButton),GTK_POS_TOP);
		//gtk_widget_show (QButton_Img);
	}

	/*must free path strings*/
	g_free(pix3path);
	g_free(pix4path);
	g_free(pix5path);

	gtk_box_pack_start(GTK_BOX(HButtonBox),SProfileButton,TRUE,TRUE,2);
	gtk_box_pack_start(GTK_BOX(HButtonBox),LProfileButton,TRUE,TRUE,2);
	gtk_box_pack_start(GTK_BOX(HButtonBox),quitButton,TRUE,TRUE,2);

	gtk_widget_show_all (LProfileButton);
	gtk_widget_show_all (SProfileButton);
	gtk_widget_show_all (quitButton);

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
	
	/*sets the pan position*/
	if(global->boxvsize==0) 
	{
		global->boxvsize=global->winheight-122;
	}
	gtk_paned_set_position (GTK_PANED(gwidget->boxv),global->boxvsize);
	
	if(!control_only) /*control_only exclusion (video and Audio) */
	{
		/*------------------------- Video Tab ---------------------------------*/
		video_tab (&all_data);
		
		/*-------------------------- Audio Tab --------------------------------*/
		audio_tab (&all_data);
	} /*end of control_only exclusion*/
	
	/* main container */
	gtk_container_add (GTK_CONTAINER (gwidget->mainwin), gwidget->boxv);
	
	gtk_widget_show (gwidget->mainwin);
	
	if (!control_only) /*control_only exclusion*/
	{
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
		/*--------------------- video capture from start ---------------------------*/
		if(global->vidfile) 
		{
			videoIn->VidFName = joinPath(videoIn->VidFName, global->vidFPath);
			
			/*start disk check timed callback (every 10 sec)*/
			if (!global->disk_timer_id)
				global->disk_timer_id=g_timeout_add(10*1000, FreeDiskCheck_timer, &all_data);
				
			initVideoFile(&all_data);
			if (global->Capture_time) 
			{
				/*sets the timer function*/
				g_timeout_add(global->Capture_time*1000,timer_callback,&all_data);
			}
			
		}
	
		if (global->FpsCount>0)
		{
			/*sets the Fps counter timer function every 2 sec*/
			global->timer_id = g_timeout_add(2*1000,FpsCount_callback,&all_data);
		}
	
	}/*end of control_only exclusion*/ 
	
	/* The last thing to get called */
	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
	
	//closing portaudio
	if(!control_only)
	{
		g_printf("Closing portaudio ...");
		if (Pa_Terminate() != paNoError) 
			g_printf("Error\n");
		else
			g_printf("OK\n");
	}
	
	//g_printf("Exit: OK\n");
	return 0;
}


