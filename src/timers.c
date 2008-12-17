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

#include "timers.h"

/* called by capture from start timer [-t seconds] command line option*/
gboolean
timer_callback(gpointer data)
{
	struct ALL_DATA * all_data = (struct ALL_DATA *) data;
	struct GLOBAL *global = all_data->global;
	struct GWIDGET *gwidget = all_data->gwidget;
	
	/*stop avi capture*/
	capture_avi(GTK_BUTTON(gwidget->CapAVIButt), all_data);
	global->Capture_time=0; 
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
	videoIn->ImageFName = incFilename(videoIn->ImageFName, 
			global->imgFPath,
			global->image_inc);
	
	snprintf(global->imageinc_str,24,_("File num:%d"),global->image_inc);
		
	gtk_label_set_text(GTK_LABEL(gwidget->ImageIncLabel), global->imageinc_str);
	
	global->image_inc++;
	/*set image capture flag*/
	videoIn->capImage = TRUE;
	if(global->image_inc > global->image_npics) 
	{	/*destroy timer*/
		gtk_button_set_label(GTK_BUTTON(gwidget->CapImageButt),_("Cap. Image"));
		global->image_timer=0;
		set_sensitive_img_contrls(TRUE, gwidget);/*enable image controls*/
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
		snprintf(global->WVcaption,10,"GUVCVideo");
		SDL_WM_SetCaption(global->WVcaption, NULL);
		return (FALSE);/*destroys the timer*/
	}
}
