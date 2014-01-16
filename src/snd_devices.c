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
#include <glib.h>
#include <glib/gprintf.h>
#include <string.h>
#include "port_audio.h"
#include "pulse_audio.h"
#include "sound.h"
#include "guvcview.h"
#include "callbacks.h"
#include "snd_devices.h"

GtkWidget * 
list_snd_devices(struct GLOBAL *global)
{
	int i = 0;
	
	switch(global->Sound_API)
	{
#ifdef PULSEAUDIO
		case PULSE:
			pulse_list_snd_devices(global);
			break;
#endif
		default:
		case PORT:
			portaudio_list_snd_devices(global);
			break;
	}
	/*sound device combo box*/
	
	GtkWidget *SndDevice = NULL;
	
	if(!global->no_display)
	{
		SndDevice = gtk_combo_box_text_new ();
	
		for(i=0; i < global->Sound_numInputDev; i++)
		{
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(SndDevice),global->Sound_IndexDev[i].description);
		}
	}
	
	return (SndDevice);
}

void
update_snd_devices(struct ALL_DATA *all_data)
{
	struct GLOBAL *global = all_data->global;
	struct GWIDGET *gwidget = all_data->gwidget;
	
	int i = 0;
	
	switch(global->Sound_API)
	{
#ifdef PULSEAUDIO
		case PULSE:
			pulse_list_snd_devices(global);
			break;
#endif
		default:
		case PORT:
			portaudio_list_snd_devices(global);
			break;
	}
	
	if(!global->no_display)
	{
		/*disable fps combobox signals*/
		g_signal_handlers_block_by_func(GTK_COMBO_BOX_TEXT(gwidget->SndDevice), G_CALLBACK (SndDevice_changed), all_data);
		/* clear out the old device list... */
		GtkListStore *store = GTK_LIST_STORE(gtk_combo_box_get_model (GTK_COMBO_BOX(gwidget->SndDevice)));
		gtk_list_store_clear(store);
	
		for(i=0; i < global->Sound_numInputDev; i++)
		{
			gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(gwidget->SndDevice), global->Sound_IndexDev[i].description);
		}
	
		/*set default device in combo*/
		global->Sound_UseDev = global->Sound_DefDev;
		gtk_combo_box_set_active(GTK_COMBO_BOX(gwidget->SndDevice), global->Sound_UseDev);
	
		/*enable fps combobox signals*/ 
		g_signal_handlers_unblock_by_func(GTK_COMBO_BOX_TEXT(gwidget->SndDevice), G_CALLBACK (SndDevice_changed), all_data);
	}
	
}
