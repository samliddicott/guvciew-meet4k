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

#include <assert.h>
#include <math.h>

#include "gview.h"
#include "gviewrender.h"

extern int verbosity;

#define REFERENCE_LEVEL 0.8
#define VU_BARS         20

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
	int bw = 2 * (width  / (VU_BARS * 8)); /*make it at least two pixels*/
	int bh = height / 24;

	int channel;
	for (channel = 0; channel < 2; ++channel)
	{
		if((render_get_osd_mask() & REND_OSD_VUMETER_MONO) != 0 && channel > 0)
			continue; /*if mono mode only render first channel*/

		/*make sure we have a positive value (required by log10)*/
		if(vu_level[channel] < 0)
			vu_level[channel] = -vu_level[channel];

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

		/*by default no bar is light */
		float dBuLevel = - 4 * (VU_BARS - 1);
		float dBuPeak = - 4 * (VU_BARS - 1);

		if(vu_level[channel] > 0)
			dBuLevel = 10 * log10(vu_level[channel] / REFERENCE_LEVEL);

		if(vu_peak[channel] > 0)
			dBuPeak  = 10 * log10(vu_peak[channel]  / REFERENCE_LEVEL);

  		/* draw the bars */
  		int peaked = 0;
  		int box = 0;
  		for (box = 0; box <= (VU_BARS - 1); ++box)
  		{
			/*
			 * The dB it takes to light the current box
			 * step of 2 db between boxes
			 */
			float db = 2 * (box - (VU_BARS - 1));

			/* start x coordinate for box */
			int bx = box * (bw + 4) + (16);
			/* Start y coordinate for box (box top)*/
			int by = channel * (bh + 4) + bh;

			uint8_t y = 127;
			uint8_t u = 127;
			uint8_t v = 127;

			/*green bar*/
			if (db < -10)
			{
				y = 154;
  				u = 72;
  				v = 57;
			}
			else if (db < -2) /*yellow bar*/
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
#ifdef USE_PLANAR_YUV
				uint8_t *py = frame;
				uint8_t *pu = frame + (width * height);
				uint8_t *pv = pu + ((width * height) / 4);

				/*y*/
				int h = 0;
				for(h = 0; h < bh; ++h)
				{
					py = frame + bx + ((by + h) * width);
					int w = 0;
					for(w = 0; w < bw; ++w)
					{
						*py++ = y;
					}
				}
				
				/*u v*/
				for(h = 0; h < bh; h += 2) /*every two lines*/
				{
					pu = frame + (width * height) + (bx/2) + (((by + h) * width) /4);
					pv = pu + ((width * height) / 4);
					
					int w = 0;
					for(w = 0; w < bw; w += 2) /*every two rows*/
					{
						*pu++ = u;
						*pv++ = v;
					}
				}
#else
  				int i = 0;
  				for (i = 0; i < bh; ++i)
  				{
					int bi = 2 * (bx + by * width); /*2 bytes per pixel*/
					by++; /*next row*/

					int j = 0;
					for (j = 0; j < bw/2; ++j) /*packed yuyv*/
					{
						/*we set two pixels in each loop*/
						frame[bi] = y;
	  					frame[bi+1] = u;
	  					frame[bi+2] = y;
	  					frame[bi+3] = v;
	 	 				bi += 4; /*next two pixels*/
					}
  				}
#endif
			}
			else if (bw > 0) /*draw single line*/
			{
#ifdef USE_PLANAR_YUV
				uint8_t *py = frame;
				uint8_t *pu = frame + (width * height);
				uint8_t *pv = pu + ((width * height) / 4);

				int w = 0;
				
				/*y*/
				py = frame + bx + ((by + bh/2) * width);
				for(w = 0; w < bw; ++w)
				{
					*py++ = y;
				}
				
				/*u v*/
				pu = frame + (width * height) + (bx/2) + (((by + bh/2) * width) /4);
				pv = pu + ((width * height) / 4);
				for(w = 0; w < bw; w += 2) /*every two rows*/
				{
					*pu++ = u;
					*pv++ = v;
				}
#else
				int bi = 2 * (bx + (by + bh/2) * width);

				int j = 0;
				for (j = 0; j < bw/2; j++)
				{
					frame[bi] = y;
	  				frame[bi+1] = u;
	  				frame[bi+2] = y;
	  				frame[bi+3] = v;
					bi += 4;
				}
#endif
			}
		}
  	}
}
