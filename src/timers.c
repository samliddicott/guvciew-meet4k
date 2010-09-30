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

#include <SDL/SDL.h>
/* support for internationalization - i18n */
#include <glib/gi18n.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/statfs.h>

#include "string_utils.h"
#include "v4l2uvc.h"
#include "globals.h"
#include "guvcview.h"
#include "callbacks.h"
#include "close.h"

/* called for timed shutdown (from video thread)*/
gboolean
shutd_timer(gpointer data)
{
    /*stop video capture*/
    shutd (0, data);
    
    return (FALSE);/*destroys the timer*/
}

/* called by video capture from start timer */
gboolean
timer_callback(gpointer data)
{
    struct ALL_DATA * all_data = (struct ALL_DATA *) data;
    struct GLOBAL *global = all_data->global;
    struct GWIDGET *gwidget = all_data->gwidget;
    
    /*stop video capture*/
    if(global->debug) g_printf("setting video toggle to FALSE\n");
    gdk_threads_enter();
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
    gdk_flush();
    gdk_threads_leave();
    
    global->Capture_time=0;
    //if exit_on_close then shutdown
    if(global->exit_on_close)
        shutd (0, data);
    
    return (FALSE);/*destroys the timer*/
}

/*called by timed capture [-c seconds] command line option*/
gboolean
Image_capture_timer(gpointer data)
{
    struct ALL_DATA * all_data = (struct ALL_DATA *) data;
    struct GLOBAL *global = all_data->global;
    struct GWIDGET *gwidget = all_data->gwidget;
    struct vdIn *videoIn = all_data->videoIn; 
    
    /*increment image name */
    //g_mutex_lock(videoIn->mutex);
    videoIn->ImageFName = incFilename(videoIn->ImageFName, 
        global->imgFPath,
        global->image_inc);
    //g_mutex_unlock(videoIn->mutex);

    //g_mutex_lock(global->mutex);
    g_snprintf(global->imageinc_str,24,_("File num:%d"),global->image_inc);
    
    gdk_threads_enter();
        gtk_label_set_text(GTK_LABEL(gwidget->ImageIncLabel), global->imageinc_str);
        gdk_flush();
    gdk_threads_leave();
    
    global->image_inc++;
    videoIn->capImage = TRUE;

    if(global->image_inc > global->image_npics) 
    {   /*destroy timer*/
        gdk_threads_enter();
        gtk_button_set_label(GTK_BUTTON(gwidget->CapImageButt),_("Cap. Image"));
        global->image_timer=0;
        set_sensitive_img_contrls(TRUE, gwidget);/*enable image controls*/
        gdk_flush();
        gdk_threads_leave();
        
        //if exit_on_close then shutdown
        if(global->exit_on_close)
            shutd (0, data);
        
        return (FALSE);
    }
    else return (TRUE);/*keep the timer*/
}

/* called by fps counter every 2 sec */
gboolean 
FpsCount_callback(gpointer data)
{
    struct ALL_DATA * all_data = (struct ALL_DATA *) data;
    struct GLOBAL *global = all_data->global;
    
    global->DispFps = (double) global->frmCount / 2;

    if (global->FpsCount>0) 
        return(TRUE); /*keeps the timer*/
    else 
    {
        g_snprintf(global->WVcaption,10,"GUVCVideo");
        SDL_WM_SetCaption(global->WVcaption, NULL);
        return (FALSE);/*destroys the timer*/
    }
}

/*
 * Not a timer callback
 * Regular function to determine if enought free space is available
 * returns TRUE if still enough free space left on disk
 * FALSE otherwise
 */
gboolean
DiskSupervisor(gpointer data)
{
    struct ALL_DATA * all_data = (struct ALL_DATA *) data;
    struct GLOBAL *global = all_data->global;
    struct vdIn *videoIn = all_data->videoIn;

    int free_tresh = 51200; //50Mb - default for compressed data
    int percent = 0;
    QWORD free_kbytes=0;
    QWORD total_kbytes=0;
    struct statfs buf;
    
    switch(global->VidCodec)
    {
        case 0: //MJPEG
            free_tresh = 102400; // 100Mb
            break;

        case 1: //yuyv
            free_tresh = 358400; // 300Mb
            break;

        case 2: //rgb
            free_tresh = 512000; // 500Mb
            break;

        default: //lavc
            free_tresh = 51200; //50Mb 
            break;
    }

    statfs(global->vidFPath[1], &buf);

    total_kbytes= buf.f_blocks * (buf.f_bsize/1024);
    free_kbytes= buf.f_bavail * (buf.f_bsize/1024);

    if(total_kbytes > 0)
        percent = (int) ((1.0f-((float)free_kbytes/(float)total_kbytes))*100.0f);
    else
    {
        g_printerr("couldn't get disk stats for %s\n", videoIn->VidFName);
        return (TRUE); /* don't invalidate video capture*/
    }

    if(global->debug) 
        g_printf("(%s) %lluK bytes free on a total of %lluK (used: %d %%) treshold=%iK\n", 
            global->vidFPath[1], (unsigned long long) free_kbytes, 
            (unsigned long long) total_kbytes, percent, free_tresh);

    if(free_kbytes < free_tresh)
    {
        g_printerr("Not enough free disk space (%lluKb) left on disk, need > %ik \n", 
            (unsigned long long) free_kbytes, free_tresh);
        WARN_DIALOG(N_("Guvcview Warning:"), N_("Not enough free space left on disk"), data);
        return(FALSE); /* not enough free space left on disk   */
    }
    else
        return (TRUE); /* still have enough free space on disk */
}

/* called by video capture every 10 sec for checking disk free space*/
gboolean 
FreeDiskCheck_timer(gpointer data)
{
    struct ALL_DATA * all_data = (struct ALL_DATA *) data;
    struct vdIn *videoIn = all_data->videoIn;
    struct GLOBAL *global = all_data->global;
    struct GWIDGET *gwidget = all_data->gwidget;

    g_mutex_lock(videoIn->mutex);
        gboolean capVid = videoIn->capVid;
    g_mutex_unlock(videoIn->mutex);

    if (capVid) 
    {
        if(!DiskSupervisor(data))
        {
            g_printerr("Stopping video Capture\n");
            /*stop video capture*/
            if(global->debug) g_printf("setting video toggle to FALSE\n");
            gdk_threads_enter();
            gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
            gdk_flush();
            gdk_threads_leave();
        }
        else
            return(TRUE); /*keeps the timer*/
    }

    return (FALSE);/*destroys the timer*/
}

