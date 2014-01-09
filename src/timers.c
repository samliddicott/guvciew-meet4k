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

#define __VMUTEX &videoIn->mutex

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
    struct vdIn *videoIn = all_data->videoIn;
    struct GLOBAL *global = all_data->global;
    struct GWIDGET *gwidget = all_data->gwidget;
    
    __LOCK_MUTEX(__VMUTEX);
		gboolean capVid = videoIn->capVid;
	__UNLOCK_MUTEX(__VMUTEX);
	
    if(!capVid)
		return (FALSE);/*destroys the timer*/
    
    /*stop video capture*/
    if(global->debug) g_print("setting video toggle to FALSE\n");
    
    if(!global->no_display)
    {
        //gdk_threads_enter();
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
        gdk_flush();
        //gdk_threads_leave();
    }
    else
    {
        capture_vid(NULL, all_data);
    }
    
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
    
    global->image_picn++;   

	if(global->image_inc > 0)
	{
		/*increment image name */
	    videoIn->ImageFName = incFilename(videoIn->ImageFName, 
    	    global->imgFPath,
    	    global->image_inc);
        
    	if(!global->no_display)
    	{
        	char *message = g_strjoin(" ", _("capturing photo to"), videoIn->ImageFName, NULL);
			gtk_statusbar_pop (GTK_STATUSBAR(gwidget->status_bar), gwidget->status_warning_id);
			gtk_statusbar_push (GTK_STATUSBAR(gwidget->status_bar), gwidget->status_warning_id, message);
			g_free(message);
    	}
    
    	global->image_inc++;
    }
    else
    	videoIn->ImageFName = joinPath(videoIn->ImageFName, global->imgFPath);
    
    videoIn->capImage = TRUE;

    if(global->image_picn >= global->image_npics) 
    {   /*destroy timer*/
        if(!global->no_display)
        {
            //gdk_threads_enter();
            gtk_button_set_label(GTK_BUTTON(gwidget->CapImageButt),_("Cap. Image"));
            set_sensitive_img_contrls(TRUE, gwidget);/*enable image controls*/
            gdk_flush();
            //gdk_threads_leave();
        }
        global->image_timer=0;
        global->image_picn=0;
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
        if(!global->no_display)
        {
            g_snprintf(global->WVcaption,10,"GUVCVideo");
            SDL_WM_SetCaption(global->WVcaption, NULL);
        }
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
        g_print("(%s) %lluK bytes free on a total of %lluK (used: %d %%) treshold=%iK\n", 
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

    __LOCK_MUTEX(__VMUTEX);
        gboolean capVid = videoIn->capVid;
    __UNLOCK_MUTEX(__VMUTEX);

    if (capVid) 
    {
        if(!DiskSupervisor(data))
        {
            g_printerr("Stopping video Capture\n");
            /*stop video capture*/
            if(global->debug) g_print("setting video toggle to FALSE\n");
            if(!global->no_display)
            {
                //gdk_threads_enter();
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(gwidget->CapVidButt), FALSE);
                gdk_flush();
                //gdk_threads_leave();
            }
            else
                capture_vid(NULL, all_data);
        }
        else
            return(TRUE); /*keeps the timer*/
    }

    return (FALSE);/*destroys the timer*/
}

/* check for udev events for v4l2 devices*/
gboolean 
check_v4l2_udev_events(gpointer data)
{
    struct ALL_DATA * all_data = (struct ALL_DATA *) data;
    struct vdIn *videoIn = all_data->videoIn;
    struct GLOBAL *global = all_data->global;
    struct GWIDGET *gwidget = all_data->gwidget;
    
    fd_set fds;
    struct timeval tv;
    int ret;
    
    FD_ZERO(&fds);
    FD_SET(videoIn->udev_fd, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    
    ret = select(videoIn->udev_fd+1, &fds, NULL, NULL, &tv);
    
    /* Check if our file descriptor has received data. */
    if (ret > 0 && FD_ISSET(videoIn->udev_fd, &fds)) 
    {
        /* Make the call to receive the device.
            select() ensured that this will not block. */
        struct udev_device *dev = udev_monitor_receive_device(videoIn->udev_mon);
        if (dev) 
        {
            if (global->debug)
            {
                g_print("Got Device event\n");
                g_print("   Node: %s\n", udev_device_get_devnode(dev));
                g_print("   Subsystem: %s\n", udev_device_get_subsystem(dev));
                g_print("   Devtype: %s\n", udev_device_get_devtype(dev));

                g_print("   Action: %s\n",udev_device_get_action(dev));
            }
            
            /*update device list*/
            g_signal_handlers_block_by_func(GTK_COMBO_BOX_TEXT(gwidget->Devices), 
                G_CALLBACK (Devices_changed), all_data);
                
            /* clear out the old device list... */
            if(videoIn->listDevices != NULL) freeDevices(videoIn->listDevices);
            
            GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model (GTK_COMBO_BOX(gwidget->Devices)));
            gtk_list_store_clear(store);
            
            /*create new device list*/
            videoIn->listDevices = enum_devices( videoIn->videodevice, videoIn->udev, global->debug );
            
            if (videoIn->listDevices->num_devices < 1)
            {
                //use current
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->Devices),
                    videoIn->videodevice);
                gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->Devices),0);
            }
            else
            {
                int i=0;
                for(i=0;i<(videoIn->listDevices->num_devices);i++)
                {
                    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->Devices),
                        videoIn->listDevices->listVidDevices[i].name);
                    if(videoIn->listDevices->listVidDevices[i].current)
                        gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->Devices),i);
                }
            }
            g_signal_handlers_unblock_by_func(GTK_COMBO_BOX_TEXT(gwidget->Devices), 
                G_CALLBACK (Devices_changed), all_data);
            
            udev_device_unref(dev);
        }
        else 
            g_printerr("No Device from receive_device(). An error occured.\n");

    }

    return(TRUE);
}

