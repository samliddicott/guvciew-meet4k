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
#include <portaudio.h>
#include "sound.h"
#include "guvcview.h"
#include "callbacks.h"
#include "snd_devices.h"

int
portaudio_list_snd_devices(struct GLOBAL *global)
{
	int   it, numDevices, defaultDisplayed;
	const PaDeviceInfo *deviceInfo;
	
	//reset device count
	global->Sound_numInputDev = 0;
	
	numDevices = Pa_GetDeviceCount();
	if( numDevices < 0 )
	{
		g_print( "SOUND DISABLE: Pa_CountDevices returned 0x%x\n", numDevices );
		//err = numDevices;
		global->Sound_enable=0;
	} 
	else 
	{
		global->Sound_DefDev = 0;
			
		for( it=0; it<numDevices; it++ )
		{
			deviceInfo = Pa_GetDeviceInfo( it );
			if (global->debug) g_print( "--------------------------------------- device #%d\n", it );
			// Mark global and API specific default devices
			defaultDisplayed = 0;
			
			// with pulse, ALSA is now listed first and doesn't set a API default- 11-2009
			if( it == Pa_GetDefaultInputDevice() )
			{
				if (global->debug) g_print( "[ Default Input" );
				defaultDisplayed = 1;
				global->Sound_DefDev=global->Sound_numInputDev;/*default index in array of input devs*/
			}
			else if( it == Pa_GetHostApiInfo( deviceInfo->hostApi )->defaultInputDevice )
			{
				const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
				if (global->debug) g_print( "[ Default %s Input", hostInfo->name );
				defaultDisplayed = 2;
				//global->Sound_DefDev=global->Sound_numInputDev;/*index in array of input devs*/
			}
			// OUTPUT device doesn't matter for capture
			if( it == Pa_GetDefaultOutputDevice() )
			{
			 	if (global->debug) 
				{
					g_print( (defaultDisplayed ? "," : "[") );
					g_print( " Default Output" );
				}
				defaultDisplayed = 3;
			}
			else if( it == Pa_GetHostApiInfo( deviceInfo->hostApi )->defaultOutputDevice )
			{
				const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
				if (global->debug)
				{
					g_print( (defaultDisplayed ? "," : "[") );                
					g_print( " Default %s Output", hostInfo->name );/* OSS ALSA etc*/
				}
				defaultDisplayed = 4;
			}

			if( defaultDisplayed!=0 )
				if (global->debug) g_print( " ]\n" );

			/* print device info fields */
			if (global->debug) 
			{
				g_print( "Name                     = %s\n", deviceInfo->name );
				g_print( "Host API                 = %s\n",  Pa_GetHostApiInfo( deviceInfo->hostApi )->name );
				g_print( "Max inputs = %d", deviceInfo->maxInputChannels  );
			}
			// INPUT devices (if it has input channels it's a capture device)
			if (deviceInfo->maxInputChannels >0) 
			{ 
				global->Sound_numInputDev++;
				//allocate new Sound Device Info
				global->Sound_IndexDev = g_renew(sndDev, global->Sound_IndexDev, global->Sound_numInputDev);
				//fill structure with sound data
				global->Sound_IndexDev[global->Sound_numInputDev-1].id=it; /*saves dev id*/
				strncpy(global->Sound_IndexDev[global->Sound_numInputDev-1].name, deviceInfo->name, 511);
				strncpy(global->Sound_IndexDev[global->Sound_numInputDev-1].description, deviceInfo->name, 255);
				global->Sound_IndexDev[global->Sound_numInputDev-1].chan=deviceInfo->maxInputChannels;
				global->Sound_IndexDev[global->Sound_numInputDev-1].samprate=deviceInfo->defaultSampleRate;
				//Sound_IndexDev[Sound_numInputDev].Hlatency=deviceInfo->defaultHighInputLatency;
				//Sound_IndexDev[Sound_numInputDev].Llatency=deviceInfo->defaultLowInputLatency;
			}
			if (global->debug) 
			{
				g_print( ", Max outputs = %d\n", deviceInfo->maxOutputChannels  );
				g_print( "Def. low input latency   = %8.3f\n", deviceInfo->defaultLowInputLatency  );
				g_print( "Def. low output latency  = %8.3f\n", deviceInfo->defaultLowOutputLatency  );
				g_print( "Def. high input latency  = %8.3f\n", deviceInfo->defaultHighInputLatency  );
				g_print( "Def. high output latency = %8.3f\n", deviceInfo->defaultHighOutputLatency  );
				g_print( "Def. sample rate         = %8.2f\n", deviceInfo->defaultSampleRate );
			}
			
		}
		
		if (global->debug) g_print("----------------------------------------------\n");
	}
	
	return 0;
}

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
	
	GtkWidget *SndDevice = gtk_combo_box_text_new ();
	
	for(i=0; i < global->Sound_numInputDev; i++)
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(SndDevice),global->Sound_IndexDev[i].description);
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
