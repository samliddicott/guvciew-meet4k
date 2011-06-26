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

#include <unistd.h>
#include <SDL/SDL.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <portaudio.h>

#include "v4l2uvc.h"
#include "avilib.h"
#include "globals.h"
#include "sound.h"
#include "jpgenc.h"
#include "autofocus.h"
#include "ms_time.h"
#include "options.h"
#include "video.h"
#include "close.h"



/*-------------------------- clean up and shut down --------------------------*/

void
clean_struct (struct ALL_DATA *all_data) 
{
	struct GWIDGET *gwidget = all_data->gwidget;
	struct VidState *s = all_data->s;
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct focusData *AFdata = all_data->AFdata;
	struct vdIn *videoIn = all_data->videoIn;
	struct VideoFormatData *videoF = all_data->videoF;
	
	gboolean control_only = (global->control_only || global->add_ctrls);
	if((!control_only) && (pdata != NULL))
	{
		/*destroy mutex for sound buffers*/
		if (global->debug) g_printf("free audio mutex\n");
		g_mutex_free( pdata->mutex );

		g_free(pdata);
		pdata=NULL;
		all_data->pdata=NULL;
		g_free(videoF);
		videoF=NULL;
	}
	
	if(videoIn) close_v4l2(videoIn, control_only);
	videoIn=NULL;
	if (global->debug) g_printf("closed v4l2 strutures\n");

	

	if (s->control_list) 
	{
		free_control_list (s->control_list);
		s->control_list = NULL;
		g_printf("free controls\n");
	}
	g_free(s);
	s = NULL;
	all_data->s = NULL;

	g_free(gwidget);
	gwidget = NULL;
	all_data->gwidget = NULL;

	if (global->debug) g_printf("free controls - vidState\n");

	g_free(AFdata);
	AFdata = NULL;
	all_data->AFdata = NULL;

	if(global) closeGlobals(global);
	global=NULL;
	all_data->global=NULL;

	g_printf("cleaned allocations - 100%%\n");
}

void 
shutd (gint restart, struct ALL_DATA *all_data) 
{
	int exec_status=0;
	gchar videodevice[16];
	struct GWIDGET *gwidget = all_data->gwidget;
	//gchar *EXEC_CALL = all_data->EXEC_CALL;
	GThread *video_thread = all_data->video_thread;
	struct paRecordData *pdata = all_data->pdata;
	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	gboolean control_only = (global->control_only || global->add_ctrls);
	
	/* wait for the video thread */
	if(!(control_only))
	{ 
		if (global->debug) g_printf("Shuting Down Thread\n");
		g_mutex_lock(videoIn->mutex);
			videoIn->signalquit=TRUE;
		g_mutex_unlock(videoIn->mutex);
		g_thread_join( video_thread );
		if (global->debug) g_printf("Video Thread finished\n");
	}

	/* destroys fps timer*/
	if (global->timer_id > 0) g_source_remove(global->timer_id);
    /* destroys udev device event check timer*/
    if (global->udev_timer_id > 0) g_source_remove(global->udev_timer_id);

    if(!global->no_display)
    {
	    gtk_window_get_size(GTK_WINDOW(gwidget->mainwin),&(global->winwidth),&(global->winheight));//mainwin or widget
	    global->boxvsize=gtk_paned_get_position (GTK_PANED(gwidget->boxv));
	}
	/*save configuration*/
	writeConf(global, videoIn->videodevice);
	
	g_snprintf(videodevice, 15, "%s", global->videodevice);
	
	clean_struct(all_data);
	gwidget = NULL;
	pdata = NULL;
	global = NULL;
	videoIn = NULL;

	//end gtk main loop
	gtk_main_quit();

	if (restart==1) 
	{	
		//closing portaudio
		if(!control_only)
		{
			g_printf("Closing portaudio ...");
			if (Pa_Terminate() != paNoError) 
				g_printf("Error\n");
			else
				g_printf("OK\n");
		}
		/* replace running process with new one */
		g_printf("Restarting: guvcview -d %s\n", videodevice);
		exec_status = execlp(g_get_prgname(),
			g_get_prgname(),
			"-d", 
			videodevice,
			NULL);
		if(exec_status < 0) perror("ERROR restarting guvcview");
	}
	//if we didn't restart return to main after gtk_main() and close portaudio there
	//this reduces chances for segfault caused by Pa_Terminate() [probable race condition] 
}
