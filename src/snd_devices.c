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
#include <glib.h>
#include <glib/gprintf.h>
#include <portaudio.h>
#include "snd_devices.h"

GtkWidget * 
list_snd_devices(struct GLOBAL *global)
{
	int   it, numDevices, defaultDisplayed;
	const PaDeviceInfo *deviceInfo;

	PaError err;
	/*sound device combo box*/
	GtkWidget *SndDevice = gtk_combo_box_new_text ();
	
	numDevices = Pa_GetDeviceCount();
	if( numDevices < 0 )
	{
		g_printf( "SOUND DISABLE: Pa_CountDevices returned 0x%x\n", numDevices );
		err = numDevices;
		global->Sound_enable=0;
	} 
	else 
	{
		global->Sound_DefDev = 0;
			
		for( it=0; it<numDevices; it++ )
		{
			deviceInfo = Pa_GetDeviceInfo( it );
			if (global->debug) g_printf( "--------------------------------------- device #%d\n", it );
			// Mark global and API specific default devices
			defaultDisplayed = 0;
			
			// with pulse, ALSA is now listed first and doesn't set a API default- 11-2009
			if( it == Pa_GetDefaultInputDevice() )
			{
				if (global->debug) g_printf( "[ Default Input" );
				defaultDisplayed = 1;
				global->Sound_DefDev=global->Sound_numInputDev;/*default index in array of input devs*/
			}
			else if( it == Pa_GetHostApiInfo( deviceInfo->hostApi )->defaultInputDevice )
			{
				const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
				if (global->debug) g_printf( "[ Default %s Input", hostInfo->name );
				defaultDisplayed = 2;
				//global->Sound_DefDev=global->Sound_numInputDev;/*index in array of input devs*/
			}
			// OUTPUT device doesn't matter for capture
			if( it == Pa_GetDefaultOutputDevice() )
			{
			 	if (global->debug) 
				{
					g_printf( (defaultDisplayed ? "," : "[") );
					g_printf( " Default Output" );
				}
				defaultDisplayed = 3;
			}
			else if( it == Pa_GetHostApiInfo( deviceInfo->hostApi )->defaultOutputDevice )
			{
				const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo( deviceInfo->hostApi );
				if (global->debug)
				{
					g_printf( (defaultDisplayed ? "," : "[") );                
					g_printf( " Default %s Output", hostInfo->name );/* OSS ALSA etc*/
				}
				defaultDisplayed = 4;
			}

			if( defaultDisplayed!=0 )
				if (global->debug) g_printf( " ]\n" );

			/* print device info fields */
			if (global->debug) 
			{
				g_printf( "Name                     = %s\n", deviceInfo->name );
				g_printf( "Host API                 = %s\n",  Pa_GetHostApiInfo( deviceInfo->hostApi )->name );
				g_printf( "Max inputs = %d", deviceInfo->maxInputChannels  );
			}
			// INPUT devices (if it has input channels it's a capture device)
			if (deviceInfo->maxInputChannels >0) 
			{ 
				global->Sound_numInputDev++;
				//allocate new Sound Device Info
				global->Sound_IndexDev = g_renew(sndDev, global->Sound_IndexDev, global->Sound_numInputDev);
				//fill structure with sound data
				global->Sound_IndexDev[global->Sound_numInputDev-1].id=it; /*saves dev id*/
				global->Sound_IndexDev[global->Sound_numInputDev-1].chan=deviceInfo->maxInputChannels;
				global->Sound_IndexDev[global->Sound_numInputDev-1].samprate=deviceInfo->defaultSampleRate;
				//Sound_IndexDev[Sound_numInputDev].Hlatency=deviceInfo->defaultHighInputLatency;
				//Sound_IndexDev[Sound_numInputDev].Llatency=deviceInfo->defaultLowInputLatency;
				gtk_combo_box_append_text(GTK_COMBO_BOX(SndDevice),deviceInfo->name);
			}
			if (global->debug) 
			{
				g_printf( ", Max outputs = %d\n", deviceInfo->maxOutputChannels  );
				g_printf( "Def. low input latency   = %8.3f\n", deviceInfo->defaultLowInputLatency  );
				g_printf( "Def. low output latency  = %8.3f\n", deviceInfo->defaultLowOutputLatency  );
				g_printf( "Def. high input latency  = %8.3f\n", deviceInfo->defaultHighInputLatency  );
				g_printf( "Def. high output latency = %8.3f\n", deviceInfo->defaultHighOutputLatency  );
				g_printf( "Def. sample rate         = %8.2f\n", deviceInfo->defaultSampleRate );
			}
			
		}
		
		if (global->debug) g_printf("----------------------------------------------\n");
	}
	
	return (SndDevice);
}
