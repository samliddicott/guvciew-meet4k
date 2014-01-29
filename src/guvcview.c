/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#           Dr. Alexander K. Seewald <alex@seewald.at>                          #
#                             Autofocus algorithm                               #
#           Flemming Frandsen <dren.dk@gmail.com>                               #
#           George Sedov <radist.morse@gmail.com>                               #
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
#include <signal.h>
#include <fcntl.h>		/* for fcntl, O_NONBLOCK */
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
#include "lavc_common.h"
#include "vcodecs.h"
#include "acodecs.h"
#include "create_video.h"
#include "profile.h"
#include "callbacks.h"
#include "close.h"
#include "img_controls.h"
#include "menubar.h"
#include "video_tab.h"
#include "audio_tab.h"
#include "uvc_h264.h"
#include "timers.h"

#define __AMUTEX &pdata->mutex


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
//__THREAD_TYPE video_thread;

/*
 * Unix signals that are cought are written to a pipe. The pipe connects
 * the unix signal handler with GTK's event loop. The array signal_pipe will
 * hold the file descriptors for the two ends of the pipe (index 0 for
 * reading, 1 for writing).
 */
int signal_pipe[2];

/*
 * The unix signal handler.
 * Write any unix signal into the pipe. The writing end of the pipe is in
 * non-blocking mode. If it is full (which can only happen when the
 * event loop stops working) signals will be dropped.
 */
void pipe_signals(int signal)
{
  if(write(signal_pipe[1], &signal, sizeof(int)) != sizeof(int))
    {
      fprintf(stderr, "unix signal %d lost\n", signal);
    }
}

/*
 * The event loop callback that handles the unix signals. Must be a GIOFunc.
 * The source is the reading end of our pipe, cond is one of
 *   G_IO_IN or G_IO_PRI (I don't know what could lead to G_IO_PRI)
 * the pointer d is always NULL
 */
gboolean deliver_signal(GIOChannel *source, GIOCondition cond, gpointer data)
{
  GError *error = NULL;		/* for error handling */

  /*
   * There is no g_io_channel_read or g_io_channel_read_int, so we read
   * char's and use a union to recover the unix signal number.
   */
  union {
    gchar chars[sizeof(int)];
    int signal;
  } buf;
  GIOStatus status;		/* save the reading status */
  gsize bytes_read;		/* save the number of chars read */

  /*
   * Read from the pipe as long as data is available. The reading end is
   * also in non-blocking mode, so if we have consumed all unix signals,
   * the read returns G_IO_STATUS_AGAIN.
   */
  while((status = g_io_channel_read_chars(source, buf.chars,
		     sizeof(int), &bytes_read, &error)) == G_IO_STATUS_NORMAL)
    {
      g_assert(error == NULL);	/* no error if reading returns normal */

      /*
       * There might be some problem resulting in too few char's read.
       * Check it.
       */
      if(bytes_read != sizeof(int)){
	fprintf(stderr, "lost data in signal pipe (expected %lu, received %lu)\n",
		(long unsigned int) sizeof(int), (long unsigned int) bytes_read);
	continue;	      /* discard the garbage and keep fingers crossed */
      }

      /* Ok, we read a unix signal number, so let the label reflect it! */
     switch (buf.signal)
     {
     	case SIGINT:
     		shutd(0, data);//shutDown
     		break;
     	case SIGUSR1:
     		capture_vid (NULL, data); /* start/stop video capture */
     		break;
     	case SIGUSR2:
     		capture_image (NULL, data); /* capture a Image */
     		break;
    	default:
    		printf("guvcview signal %d caught\n", buf.signal);
    		break;
     }
    }

  /*
   * Reading from the pipe has not returned with normal status. Check for
   * potential errors and return from the callback.
   */
  if(error != NULL){
    fprintf(stderr, "reading signal pipe failed: %s\n", error->message);
    exit(1);
  }
  if(status == G_IO_STATUS_EOF){
    fprintf(stderr, "signal pipe has been closed\n");
    exit(1);
  }

  g_assert(status == G_IO_STATUS_AGAIN);
  return (TRUE);		/* keep the event source */
}

/*--------------------------------- MAIN -------------------------------------*/
int main(int argc, char *argv[])
{
	int ret=0;

	/*
   	* In order to register the reading end of the pipe with the event loop
   	* we must convert it into a GIOChannel.
   	*/
  	GIOChannel *g_signal_in;
  	long fd_flags; 	    /* used to change the pipe into non-blocking mode */
  	GError *error = NULL;	/* handle errors */

	/*print package name and version*/
	g_print("%s\n", PACKAGE_STRING);

	//g_type_init ();
	//gdk_threads_init();

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

	/** initGlobals needs codecs registered
	 * so do it here
	 */
#if !LIBAVCODEC_VER_AT_LEAST(53,34)
	avcodec_init();
#endif
	// register all the codecs (you can also register only the codec
	//you wish to have smaller code)
	avcodec_register_all();

	/*allocate global variables*/
	global = g_new0(struct GLOBAL, 1);
	initGlobals(global);

	/*------------------------ reads command line options --------------------*/
	readOpts(argc,argv,global);

	/*------------------------- reads configuration file ---------------------*/
	readConf(global);

	//sets local control_only flag - prevents several initializations/allocations
	gboolean control_only = (global->control_only || global->add_ctrls) ;

    if(global->no_display && global->control_only )
    {
	g_printerr("incompatible options (control_only and no_display): enabling display");
	global->no_display = FALSE;
    }

	/*---------------------------------- Allocations -------------------------*/

	gwidget = g_new0(struct GWIDGET, 1);
	gwidget->vid_widget_state = TRUE;

	/* widgets */
	GtkWidget *scroll1;
	GtkWidget *Tab1;
	GtkWidget *Tab1Label;
	GtkWidget *Tab1Icon;
	GtkWidget *ImgButton_Img;
	GtkWidget *VidButton_Img;
	GtkWidget *QButton_Img;
	GtkWidget *HButtonBox;


	s = g_new0(struct VidState, 1);

	if(!control_only) /*control_only exclusion (video and Audio) */
	{
		pdata = g_new0(struct paRecordData, 1);
		//pdata->maincontext = g_main_context_default();
		/*create mutex for sound buffers*/
		__INIT_MUTEX(__AMUTEX);

		/* Allocate the video Format struct */
		videoF = g_new0(struct VideoFormatData, 1);

		/*---------------------------- Start PortAudio API -----------------------*/
		if(global->debug) g_print("starting portaudio...\n");
		Pa_Initialize();
	}

#ifdef ENABLE_NLS
	/* if --verbose mode set do debug*/
	if (global->debug) g_print("language catalog=> dir:%s type:%s lang:%s cat:%s.mo\n",
		lc_dir, lc_all, langs[0], txtdom);
#endif
	/*---------------------------- GTK init ----------------------------------*/
	if(!global->no_display)
	{
		if(!gtk_init_check(&argc, &argv))
		{
			g_printerr("GUVCVIEW: can't open display: changing to no_display mode\n");
			global->no_display = TRUE; /*if we can't open the display fallback to no_display mode*/
		}
	}

    if(!global->no_display)
    {
		g_set_application_name(_("Guvcview Video Capture"));
		g_setenv("PULSE_PROP_media.role", "video", TRUE); //needed for Pulse Audio

        /* make sure the type is realized so that we can change the properties*/
        g_type_class_unref (g_type_class_ref (GTK_TYPE_BUTTON));
        /* make sure gtk-button-images property is set to true (defaults to false in karmic)*/
        g_object_set (gtk_settings_get_default (), "gtk-button-images", TRUE, NULL);

        /* Create a main window */
        gwidget->mainwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title (GTK_WINDOW (gwidget->mainwin), _("GUVCViewer Controls"));
        //get screen resolution
        if((!global->desktop_w) || (!global->desktop_h))
        {
            GdkScreen* screen = NULL;
            screen = gtk_window_get_screen(GTK_WINDOW(gwidget->mainwin));
            global->desktop_w = gdk_screen_get_width(screen);
            global->desktop_h = gdk_screen_get_height(screen);
        }
        if(global->debug)
            g_print("Screen resolution is (%d x %d)\n", global->desktop_w, global->desktop_h);

        if((global->winwidth > global->desktop_w) && (global->desktop_w > 0))
            global->winwidth = global->desktop_w;
        if((global->winheight > global->desktop_h) && (global->desktop_h > 0))
            global->winheight = global->desktop_h;

        gtk_window_resize(GTK_WINDOW(gwidget->mainwin),global->winwidth,global->winheight);

        /* Add event handlers */
        g_signal_connect(GTK_WINDOW(gwidget->mainwin), "delete_event", G_CALLBACK(delete_event), &all_data);
    }

	/*----------------------- init videoIn structure --------------------------*/
	videoIn = g_new0(struct vdIn, 1);

	/*set structure with all global allocations*/
	all_data.pdata = pdata;
	all_data.global = global;
	all_data.AFdata = AFdata; /*not allocated yet*/
	all_data.videoIn = videoIn;
	all_data.videoF = videoF;
	all_data.gwidget = gwidget;
	all_data.h264_controls = NULL; /*filled by add_uvc_h264_controls_tab */
	all_data.s = s;

	/*get format from selected mode*/
	global->format = get_PixFormat(global->mode);
	if(global->debug)
		g_print("%s: setting format to %c%c%c%c\n",
			global->mode,
			global->format & 0xFF, (global->format >> 8) & 0xFF,
			(global->format >> 16) & 0xFF, (global->format >> 24) & 0xFF);

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

			case VDIN_DYNCTRL_OK: //uvc extension controls OK, give warning and shutdown (called with --add_ctrls)
				WARN_DIALOG (N_("Guvcview:\n\nUVC Extension controls"),
					N_("Extension controls were added to the UVC driver"),
					&all_data);
				clean_struct(&all_data);
				exit(0);
				break;

			case VDIN_DYNCTRL_ERR: //uvc extension controls error - EACCES (needs root user)
				ERR_DIALOG (N_("Guvcview error:\n\nUVC Extension controls"),
					N_("An error occurred while adding extension\ncontrols to the UVC driver\nMake sure you run guvcview as root (or sudo)."),
					&all_data);
				break;

			case VDIN_UNKNOWN_ERR: //unknown error (treat as invalid format)
			case VDIN_FORMAT_ERR://invalid format
			case VDIN_RESOL_ERR: //invalid resolution
				g_print("trying minimum setup ...\n");
				if (videoIn->listFormats->numb_formats > 0 && videoIn->listFormats->listVidFormats != NULL) //check for supported formats
				{
					VidFormats *listVidFormats;
					videoIn->listFormats->current_format = 0; //get the first supported format
					global->format = videoIn->listFormats->listVidFormats[0].format;
					g_print("\tformat: %c%c%c%c\n",
						global->format & 0xFF, (global->format >> 8) & 0xFF,
						(global->format >> 16) & 0xFF, (global->format >> 24) & 0xFF);

					if(get_PixMode(global->format, global->mode) < 0)
						g_printerr("IMPOSSIBLE: format has no supported mode !?\n");
					listVidFormats = &videoIn->listFormats->listVidFormats[0];
					if(listVidFormats->numb_res > 0 && listVidFormats->listVidCap != NULL)
					{
						global->width = listVidFormats->listVidCap[0].width;
						global->height = listVidFormats->listVidCap[0].height;
						g_print("\tresolution: %i x %i\n", global->width, global->height);
						if (listVidFormats->listVidCap[0].framerate_num != NULL)
							global->fps_num = listVidFormats->listVidCap[0].framerate_num[0];
						if (listVidFormats->listVidCap[0].framerate_denom != NULL)
							global->fps = listVidFormats->listVidCap[0].framerate_denom[0];
						g_print("\tframerate: %i/%i\n", global->fps_num, global->fps);
					}
					else
					{
						g_printerr("ERROR: Can't set video stream. No supported resolution found for specified format\nExiting...\n");
						ERR_DIALOG (N_("Guvcview error:\n\nCan't set a valid video stream for guvcview"),
							N_("Make sure your device driver is v4l2 compliant\nand that it is properly installed."),
							&all_data);
					}
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
					N_("Make sure the device driver supports v4l2."),
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

	videoIn->listFormats->current_format = get_FormatIndex(videoIn->listFormats, global->format);
	if(videoIn->listFormats->current_format < 0)
	{
		g_printerr("ERROR: Can't set video stream. No supported format found\nExiting...\n");
		ERR_DIALOG (N_("Guvcview error:\n\nCan't set a valid video stream for guvcview"),
			N_("Make sure your device driver is v4l2 compliant\nand that it is properly installed."),
			&all_data);
	}
	/*-----------------------------GTK widgets---------------------------------*/
	/*----------------------- Image controls Tab ------------------------------*/

    if(!(global->no_display))
    {
		gwidget->maintable = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

		gtk_widget_show (gwidget->maintable);

		/** Attach the menu */
		gtk_box_pack_start(GTK_BOX(gwidget->maintable), create_menu(&all_data, control_only), FALSE, TRUE, 2);

        s->control_list = NULL;
        /** draw the controls */
        printf("drawing controls\n\n");
        draw_controls(&all_data);

        if (global->lprofile > 0) LoadControls (&all_data);

        gwidget->boxh = gtk_notebook_new();

        gtk_widget_show (s->table);
        gtk_widget_show (gwidget->boxh);

        scroll1=gtk_scrolled_window_new(NULL,NULL);
        gtk_scrolled_window_set_placement(GTK_SCROLLED_WINDOW(scroll1), GTK_CORNER_TOP_LEFT);

        //viewport is only needed for gtk < 3.8
        //for 3.8 and above s->table can be directly added to scroll1
        GtkWidget* viewport = gtk_viewport_new(NULL,NULL);
        gtk_container_add(GTK_CONTAINER(viewport), s->table);
        gtk_widget_show(viewport);

        gtk_container_add(GTK_CONTAINER(scroll1), viewport);
        gtk_widget_show(scroll1);

        Tab1 = gtk_grid_new();
        Tab1Label = gtk_label_new(_("Image Controls"));
        gtk_widget_show (Tab1Label);
        /** check for files */
        gchar* Tab1IconPath = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/image_controls.png",NULL);
        /** don't test for file - use default empty image if load fails */
        /** get icon image*/
        Tab1Icon = gtk_image_new_from_file(Tab1IconPath);
        g_free(Tab1IconPath);
        gtk_widget_show (Tab1Icon);
        gtk_grid_attach (GTK_GRID(Tab1), Tab1Icon, 0, 0, 1, 1);
		gtk_grid_attach (GTK_GRID(Tab1), Tab1Label, 1, 0, 1, 1);

        gtk_widget_show (Tab1);

        gtk_notebook_append_page(GTK_NOTEBOOK(gwidget->boxh),scroll1,Tab1);

        /*---------------------- Add  Buttons ---------------------------------*/
        HButtonBox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_halign (HButtonBox, GTK_ALIGN_FILL);
		gtk_widget_set_hexpand (HButtonBox, TRUE);
        gtk_button_box_set_layout(GTK_BUTTON_BOX(HButtonBox),GTK_BUTTONBOX_SPREAD);
        gtk_box_set_homogeneous(GTK_BOX(HButtonBox),TRUE);

		gtk_widget_show(HButtonBox);

		/** Attach the buttons */
		gtk_box_pack_start(GTK_BOX(gwidget->maintable), HButtonBox, FALSE, TRUE, 2);
		/** Attach the notebook (tabs) */
		gtk_box_pack_start(GTK_BOX(gwidget->maintable), gwidget->boxh, TRUE, TRUE, 2);

        gwidget->quitButton=gtk_button_new_from_stock(GTK_STOCK_QUIT);

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
                gwidget->CapImageButt=gtk_button_new_with_label (_("Stop Auto (I)"));
            }
            else
            {
                gwidget->CapImageButt=gtk_button_new_with_label (_("Cap. Image (I)"));
            }

            if (global->Capture_time > 0)
            {	/*vid capture enabled from start*/
                gwidget->CapVidButt=gtk_toggle_button_new_with_label (_("Stop Video (V)"));
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->CapVidButt), TRUE);
            }
            else
            {
                gwidget->CapVidButt=gtk_toggle_button_new_with_label (_("Cap. Video (V)"));
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
            }

            /*add images to Buttons and top window*/
            /*check for files*/

            gchar* pix1path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/movie.png",NULL);
            if (g_file_test(pix1path,G_FILE_TEST_EXISTS))
            {
                VidButton_Img = gtk_image_new_from_file (pix1path);

                gtk_button_set_image(GTK_BUTTON(gwidget->CapVidButt),VidButton_Img);
                gtk_button_set_image_position(GTK_BUTTON(gwidget->CapVidButt),GTK_POS_TOP);
                //gtk_widget_show (gwidget->VidButton_Img);
            }
            //else g_print("couldn't load %s\n", pix1path);
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
            /*key events*/
            gtk_widget_add_events (GTK_WIDGET (gwidget->mainwin), GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
            g_signal_connect (GTK_WINDOW(gwidget->mainwin), "key_press_event", G_CALLBACK(key_pressed), &all_data);
        }/*end of control_only exclusion*/

        gchar* pix3path = g_strconcat (PACKAGE_DATA_DIR,"/pixmaps/guvcview/close.png",NULL);
        if (g_file_test(pix3path,G_FILE_TEST_EXISTS))
        {
            QButton_Img = gtk_image_new_from_file (pix3path);

            gtk_button_set_image(GTK_BUTTON(gwidget->quitButton),QButton_Img);
            gtk_button_set_image_position(GTK_BUTTON(gwidget->quitButton),GTK_POS_TOP);
            //gtk_widget_show (QButton_Img);
        }

        /*must free path strings*/
        g_free(pix3path);

        gtk_box_pack_start(GTK_BOX(HButtonBox), gwidget->quitButton,TRUE,TRUE,2);

        gtk_widget_show_all (gwidget->quitButton);

        g_signal_connect (GTK_BUTTON(gwidget->quitButton), "clicked",
            G_CALLBACK (quitButton_clicked), &all_data);

		/*----------------------------H264 Controls Tab --------------------------*/
		if(global->uvc_h264_unit > 0)
			add_uvc_h264_controls_tab(&all_data);

        if(!control_only) /*control_only exclusion (video and Audio) */
        {
            /*------------------------- Video Tab ---------------------------------*/
            video_tab (&all_data);

            /*-------------------------- Audio Tab --------------------------------*/
            audio_tab (&all_data);
        } /*end of control_only exclusion*/

		gwidget->status_bar = gtk_statusbar_new();
		gwidget->status_warning_id = gtk_statusbar_get_context_id (GTK_STATUSBAR(gwidget->status_bar), "warning");

        gtk_widget_show(gwidget->status_bar);
       /** add the status bar*/
        gtk_box_pack_start(GTK_BOX(gwidget->maintable), gwidget->status_bar, FALSE, FALSE, 2);
        /* main container */
        gtk_container_add (GTK_CONTAINER (gwidget->mainwin), gwidget->maintable);

        gtk_widget_show (gwidget->mainwin);

         /*Add udev device monitoring timer*/
        global->udev_timer_id=g_timeout_add( 500, check_v4l2_udev_events, &all_data);
    }
    else
        list_snd_devices (global);


	if (!control_only) /*control_only exclusion*/
	{
		/*------------------ Creating the video thread ---------------*/
		if( __THREAD_CREATE(&all_data.video_thread, main_loop, (void *) &all_data))
		{
			g_printerr("Video thread creation failed\n");

			ERR_DIALOG (N_("Guvcview error:\n\nUnable to create Video Thread"),
				N_("Please report it to http://developer.berlios.de/bugs/?group_id=8179"),
				&all_data);
		}
		//all_data.video_thread = video_thread;

		/*---------------------- image timed capture -----------------------------*/
		if(global->image_timer)
		{
			global->image_timer_id=g_timeout_add(global->image_timer*1000,
				Image_capture_timer, &all_data);
            if(!global->no_display)
            {
                set_sensitive_img_contrls(FALSE, gwidget);/*disable image controls*/
                char *message = g_strjoin(" ", _("capturing photo to"), videoIn->ImageFName, NULL);
				gtk_statusbar_pop (GTK_STATUSBAR(gwidget->status_bar), gwidget->status_warning_id);
				gtk_statusbar_push (GTK_STATUSBAR(gwidget->status_bar), gwidget->status_warning_id, message);
				g_free(message);
            }
		}
		/*--------------------- video capture from start ---------------------------*/
		if(global->Capture_time > 0)
		{

			if (global->vid_inc>0)
			{
				videoIn->VidFName = incFilename(videoIn->VidFName,
					global->vidFPath,
					global->vid_inc);

				global->vid_inc++;
			}
			else
			{
				videoIn->VidFName = joinPath(videoIn->VidFName, global->vidFPath);
			}

			if(!global->no_display)
			{
				char * message = g_strjoin(" ", _("capturing video to"), videoIn->VidFName, NULL);
				gtk_statusbar_pop (GTK_STATUSBAR(gwidget->status_bar), gwidget->status_warning_id);
				gtk_statusbar_push (GTK_STATUSBAR(gwidget->status_bar), gwidget->status_warning_id, message);
				g_free(message);
			}

			gboolean cap_ok = TRUE;
			/* check if enough free space is available on disk*/
			if(!DiskSupervisor(&all_data))
			{
				cap_ok = FALSE;
			}
			else
			{
				/*start disk check timed callback (every 10 sec)*/
				if (!global->disk_timer_id)
					global->disk_timer_id=g_timeout_add(10*1000, FreeDiskCheck_timer, &all_data);

				//request a IDR frame with SPS and PPS data
				uvcx_request_frame_type(videoIn->fd, global->uvc_h264_unit, PICTURE_TYPE_IDR_FULL);

				/*start IO thread*/
				if( __THREAD_CREATE(&all_data.IO_thread, IO_loop, (void *) &all_data))
				{
					g_printerr("IO thread creation failed\n");
					cap_ok = FALSE;
				}
				else
				{
					/*sets the timer function*/
					g_timeout_add(global->Capture_time*1000,timer_callback,&all_data);
				}
			}

			if(!cap_ok)
			{
				g_printerr("ERROR: couldn't start video capture\n");
                if(!global->no_display)
                {
                    //g_signal_handlers_block_by_func(GTK_TOGGLE_BUTTON(gwidget->CapVidButt), G_CALLBACK (capture_vid), all_data);
                    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
                    gtk_button_set_label(GTK_BUTTON(gwidget->CapVidButt),_("Cap. Video"));
                    //g_signal_handlers_unblock_by_func(GTK_TOGGLE_BUTTON(gwidget->CapVidButt), G_CALLBACK (capture_vid), all_data);
                }
			}
			else if(!global->no_display)
			{
				/*disabling sound and video compression controls*/
				set_sensitive_vid_contrls(FALSE, global->Sound_enable, gwidget);
				char *message = g_strjoin(" ", _("capturing video to"), videoIn->VidFName, NULL);
				gtk_statusbar_pop (GTK_STATUSBAR(gwidget->status_bar), gwidget->status_warning_id);
				gtk_statusbar_push (GTK_STATUSBAR(gwidget->status_bar), gwidget->status_warning_id, message);
				g_free(message);
			}
		}

		if (global->FpsCount>0)
		{
			/*sets the Fps counter timer function every 2 sec*/
			global->timer_id = g_timeout_add(2*1000,FpsCount_callback,&all_data);
		}

	}/*end of control_only exclusion*/

	/*
   	* Set the unix signal handling up.
   	* First create a pipe.
   	*/
  	if(pipe(signal_pipe))
  	{
    	perror("pipe");
   	 	exit(1);
  	}

  	/*
   	* put the write end of the pipe into nonblocking mode,
   	* need to read the flags first, otherwise we would clear other flags too.
   	*/
  	fd_flags = fcntl(signal_pipe[1], F_GETFL);
  	if(fd_flags == -1)
    {
      	perror("read descriptor flags");
    }
  	if(fcntl(signal_pipe[1], F_SETFL, fd_flags | O_NONBLOCK) == -1)
    {
      	perror("write descriptor flags");
    }

  	/* Install the unix signal handler pipe_signals for the signals of interest */
  	signal(SIGINT, pipe_signals);
  	signal(SIGUSR1, pipe_signals);
  	signal(SIGUSR2, pipe_signals);

  	/* convert the reading end of the pipe into a GIOChannel */
  	g_signal_in = g_io_channel_unix_new(signal_pipe[0]);

  	/*
   	* we only read raw binary data from the pipe,
   	* therefore clear any encoding on the channel
   	*/
  	g_io_channel_set_encoding(g_signal_in, NULL, &error);
  	if(error != NULL)
  	{
  		/* handle potential errors */
    	fprintf(stderr, "g_io_channel_set_encoding failed %s\n",
	    	error->message);
  	}

  	/* put the reading end also into non-blocking mode */
  	g_io_channel_set_flags(g_signal_in,
    	g_io_channel_get_flags(g_signal_in) | G_IO_FLAG_NONBLOCK, &error);

  	if(error != NULL)
  	{		/* tread errors */
    	fprintf(stderr, "g_io_set_flags failed %s\n",
	    	error->message);
  	}

  	/* register the reading end with the event loop */
  	g_io_add_watch(g_signal_in, G_IO_IN | G_IO_PRI, deliver_signal, &all_data);

	g_print("\nGUVCVIEW Signals:\n");
	g_print("  SIGUSR1: Video stop/start capture\n");
	g_print("  SIGUSR2: Image capture\n");
	g_print("  SIGINT (ctrl+c): Exit\n");
	g_print("examples:\n");
	g_print("   kill -s SIGUSR1 'pid'\n");
	g_print("   killall -s USR2 guvcview\n\n");


	/* The last thing to get called (gtk or glib main loop)*/
	if(global->debug)
		g_print("Starting main loop \n");

	if(!global->no_display)
		gtk_main();
	else
	{
		gwidget->main_loop = g_main_loop_new(NULL, TRUE);
		g_main_loop_run(gwidget->main_loop);
	}

	//free all_data allocations
	free(all_data.gwidget);
	if(all_data.h264_controls != NULL)
		free(all_data.h264_controls);

	//closing portaudio
	if(!control_only)
	{
		g_print("Closing portaudio ...");
		if (Pa_Terminate() != paNoError)
			g_print("Error\n");
		else
			g_print("OK\n");
	}

	g_print("Closing GTK... OK\n");
	return 0;
}


