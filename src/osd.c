/*******************************************************************************#
#           guvcview              http://guvcview.sourceforge.net               #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            #
#                             Add UYVY color support(Macbook iSight)            #
#           Flemming Frandsen <dren.dk@gmail.com>                               #
#                             Add VU meter OSD                                  #  
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

#include <math.h>
#include <glib.h>
#include "defs.h"
#include "guvcview.h"

#define __AMUTEX &pdata->mutex

#define AUDIO_REFERENCE_LEVEL 0.2

void draw_vu_meter(int width, int height, SAMPLE vuPeak[2], int vuPeakFreeze[2], void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;
    
    struct paRecordData *pdata = all_data->pdata;
    struct vdIn *videoIn = all_data->videoIn;
    
	int i,j; // Fuck pre-C99 sucks.
	__LOCK_MUTEX( __AMUTEX );
	SAMPLE vuLevel[2]; // The maximum sample for this frame.
	for (i=0;i<2;i++) 
	{
		vuLevel[i] = 0;
		// Run through all available samples and find the current level.
		SAMPLE *samples = pdata->audio_buff[pdata->blast_ind][pdata->last_ind].frame;
		for (j=0;j<pdata->aud_numSamples;j++) 
		{
			int channel = j % pdata->channels;
			if (channel < 2) 
			{
		  		SAMPLE s = samples[j];
		  		if (s > vuLevel[channel]) 
		  		{
					vuLevel[channel] = s;
		  		}
			}
  		}
	}
	__UNLOCK_MUTEX( __AMUTEX );

	BYTE *vuFrame = videoIn->framebuffer;
	
	int bh = height / 20;
	int bw = width  / 150;
	int channel;
	for (channel=0;channel<2;channel++) 
	{
		// Handle peak calculation and falloff.
		if (vuPeak[channel] < vuLevel[channel]) 
		{
			vuPeak[channel] = vuLevel[channel];
			vuPeakFreeze[channel] = 30;
		} 
		else if (vuPeakFreeze[channel] > 0) 
		{
			vuPeakFreeze[channel]--;
  		}
  		else if (vuPeak[channel] > vuLevel[channel]) 
  		{
			vuPeak[channel] -= (vuPeak[channel]-vuLevel[channel]) / 10;
  		}
  		float dBuLevel = 10*log10(vuLevel[channel]/AUDIO_REFERENCE_LEVEL);
  		float dBuPeak  = 10*log10(vuPeak[channel] /AUDIO_REFERENCE_LEVEL);

  		// Draw the pretty bars, but only if there actually is an audio channel with samples present
  		if (vuLevel[channel] == 0) 
			continue;

  		int peaked = 0;
  		int box;
  		for (box=0;box<=26;box++) 
  		{
			float db = box-20; // The dB it takes to light the current box.    
			int bx = (5+box)*bw*2*4; // Start byte for box on each line
			int by = (channel*(bh+5) + bh*2)* 2*width; // Start byte for box top.
			  
			BYTE u = 0;
			BYTE v = 0;
			if (db > 4) 
			{
  				u=128;
  				v=255;
			} 
			else if (db > 0) 
			{
  				u=0;
  				v=200;	
			}

			int light = dBuLevel > db;
			if (dBuPeak < db+1 && !peaked) 
			{
  				peaked = 1;
  				light = 1;
			}

			if (light) 
			{
  				int yc;
  				for (yc=0;yc<bh;yc++) 
  				{
					int bi = bx + by;
					by += width*2;
		  
					for (j=0;j<bw;j++) 
					{
	  					vuFrame[bi+1] = u;
	  					vuFrame[bi+3] = v;
	 	 				bi += 4;
					}
  				}

			} 
			else if (bw > 0) 
			{
  				int bi = bx + by + width*2*bh/2;
  				for (j=0;j<bw;j++) 
  				{
					vuFrame[bi+1] = u;
					vuFrame[bi+3] = v;
					bi += 4;
  				}
			}
  		}    	    
	}

}

