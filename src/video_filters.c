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

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "video_filters.h"
#include "v4l2uvc.h"

/* Flip YUYV frame - horizontal
 * args:
 *      frame = pointer to frame buffer (yuyv format)
 *      width = frame width
 *      height= frame height
 * returns: void
 */
void 
yuyv_mirror (BYTE *frame, int width, int height)
{
	int h=0;
	int w=0;
	int sizeline = width*2; /* 2 bytes per pixel*/ 
	BYTE *pframe;
	pframe=frame;
	BYTE line[sizeline-1];/*line buffer*/
	for (h=0; h < height; h++) 
	{	/*line iterator*/
		for(w=sizeline-1; w > 0; w = w - 4) 
		{	/* pixel iterator */
			line[w-1]=*pframe++;
			line[w-2]=*pframe++;
			line[w-3]=*pframe++;
			line[w]=*pframe++;
		}
		memcpy(frame+(h*sizeline), line, sizeline); /*copy reversed line to frame buffer*/           
	}
}

/* Invert YUV frame
 * args:
 *      frame = pointer to frame buffer (yuyv or uyvy format)
 *      width = frame width
 *      height= frame height
 * returns: void
 */
void 
yuyv_negative(BYTE* frame, int width, int height)
{
	int size=width*height*2;
	int i=0;
	for(i=0; i < size; i++)
		frame[i] = ~frame[i];     
}

/* Flip YUV frame - vertical
 * args:
 *      frame = pointer to frame buffer (yuyv or uyvy format)
 *      width = frame width
 *      height= frame height
 * returns: void
 */
void 
yuyv_upturn(BYTE* frame, int width, int height)
{   
	int h=0;
	int sizeline = width*2; /* 2 bytes per pixel*/ 
	BYTE *pframe;
	pframe=frame;
	BYTE line1[sizeline-1];/*line1 buffer*/
	BYTE line2[sizeline-1];/*line2 buffer*/
	for (h=0; h < height/2; h++) 
	{	/*line iterator*/
		memcpy(line1,frame+h*sizeline,sizeline);
		memcpy(line2,frame+(height-1-h)*sizeline,sizeline);
		
		memcpy(frame+h*sizeline, line2, sizeline);
		memcpy(frame+(height-1-h)*sizeline, line1, sizeline);
	}
}

/* monochromatic effect for YUYV frame
 * args:
 *      frame = pointer to frame buffer (yuyv format)
 *      width = frame width
 *      height= frame height
 * returns: void
 */
void
yuyv_monochrome(BYTE* frame, int width, int height) 
{
	int size=width*height*2;
	int i=0;

	for(i=0; i < size; i = i + 4)
	{	/* keep Y - luma */
		frame[i+1]=0x80;/*U - median (half the max value)=128*/
		frame[i+3]=0x80;/*V - median (half the max value)=128*/        
	}
}


/*break image in little square pieces
 * args:
 *    frame  = pointer to frame buffer (yuyv or uyvy format)
 *    width  = frame width
 *    height = frame height
 *    piece_size = multiple of 2 (we need at least 2 pixels to get the entire pixel information)
 *    format = v4l2 pixel format
 */
void
pieces(BYTE* frame, int width, int height, int piece_size )
{
	int numx = width / piece_size;
	int numy = height / piece_size;
	BYTE *piece = g_new0 (BYTE, (piece_size * piece_size * 2));
	int i = 0, j = 0, row = 0, line = 0, column = 0, linep = 0, px = 0, py = 0;
	GRand* rand_= g_rand_new_with_seed(2);
	int rot = 0;

	for(j = 0; j < numy; j++)
	{
		row = j * piece_size;
		for(i = 0; i < numx; i++)
		{
			column = i * piece_size * 2;
			//get piece
			for(py = 0; py < piece_size; py++)
			{
				linep = py * piece_size * 2;
				line = (py + row) * width * 2;
				for(px=0 ; px < piece_size * 2; px++)
				{
					piece[px + linep] = frame[(px + column) + line];
				}
			}
			/*rotate piece and copy it to frame*/
			//rotation is random
			rot = g_rand_int_range(rand_, 0, 8);
			switch(rot)
			{
				case 0: // do nothing
					break;
				case 5:
				case 1: //mirror
					yuyv_mirror(piece, piece_size, piece_size);
					break;
				case 6:
				case 2: //upturn
					yuyv_upturn(piece, piece_size, piece_size);
					break;
				case 4:
				case 3://mirror upturn
					yuyv_upturn(piece, piece_size, piece_size);
					yuyv_mirror(piece, piece_size, piece_size);
					break;
				default: //do nothing
					break;
			}
			//write piece
			for(py = 0; py < piece_size; py++)
			{
				linep = py * piece_size * 2;
				line = (py + row) * width * 2;
				for(px=0 ; px < piece_size * 2; px++)
				{
					frame[(px + column) + line] = piece[px + linep];
				}
			}
		}
	}
	
	g_free(piece);
	g_rand_free(rand_);
}
