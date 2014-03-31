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

#include <SDL/SDL.h>
#include <assert.h>
#include <math.h>

#include "gview.h"
#include "gviewrender.h"

extern int verbosity;

#define REFERENCE_LEVEL 0.2
#define VU_BARS         16

static float vu_peak[2] = {0.0, 0.0};
static float vu_peak_freeze[2]= {0.0 ,0.0};

/*
 * render a vu meter
 * args:
 *   frame - pointer to yuyv frame data
 *   width - frame width
 *   height - frame height
 *   vu_level - vu level values (array with 2 channels)
 *
 * asserts:
 *   none
 *
 * returns: none
 */
void render_osd_vu_meter(uint8_t *frame, int width, int height, float vu_level[2])
{

	int bh = height / 24;
	int bw = width  / (VU_BARS * 4);
	int channel;
	for (channel = 0; channel < 2; ++channel)
	{
		if(vu_level[channel] == 0)
			continue;
		if(vu_level[channel] < 0)
			vu_level[channel] = -vu_level[channel];
			
		printf("AUDIO: vu_level %f\n", vu_level[0]);
		/* Handle peak calculation and falloff */
		if (vu_peak[channel] < vu_level[channel])
		{
			vu_peak[channel] = vu_level[channel];
			vu_peak_freeze[channel] = 30;
		}
		else if (vu_peak_freeze[channel] > 0)
		{
			vu_peak_freeze[channel]--;
  		}
  		else if (vu_peak[channel] > vu_level[channel])
  		{
			vu_peak[channel] -= (vu_peak[channel] - vu_level[channel]) / 10;
  		}

  		float dBuLevel = 10 * log10(vu_level[channel] / REFERENCE_LEVEL);
  		float dBuPeak  = 10 * log10(vu_peak[channel]  / REFERENCE_LEVEL);

  		/* draw the bars */
  		int peaked = 0;
  		int box = 0;
  		for (box = 0; box <= (VU_BARS-1); ++box)
  		{
			/* The dB it takes to light the current box */
			float db = 2 * box - 32;

			/* start x coordinate for box */
			int bx = box * (bw + bw/2) + (width / 8);
			/* Start y coordinate for box (box top)*/
			int by = channel * (bh + 5) + bh;

			uint8_t y = 127;
			uint8_t u = 0;
			uint8_t v = 0;

			/*green bar*/
			if (db < -8)
			{
				y = 154;
  				u = 72;
  				v = 57;
			}
			else if (db < -4) /*yellow bar*/
			{
				y = 203;
  				u = 44;
  				v = 142;
			}
			else /*red bar*/
			{
				y = 107;
				u = 100;
				v = 212;
			}

			int light = dBuLevel > db;
			if (dBuPeak < db+1 && !peaked)
			{
  				peaked = 1;
  				light = 1;
			}

			if (light)
			{
  				int yc = 0;
  				for (yc = 0; yc < bh; ++yc)
  				{
					int bi = bx + by * width * 2; /*2 bytes per pixel*/
					by++; /*next row*/

					int j = 0;
					for (j = 0; j < bw; j++) /*packed yuyv*/
					{
						frame[bi] = y;
	  					frame[bi+1] = u;
	  					frame[bi+2] = y;
	  					frame[bi+3] = v;
	 	 				bi += 4;
					}
  				}

			}
			/*
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
			*/
		}
  	}
}
