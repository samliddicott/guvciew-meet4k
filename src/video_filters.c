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
 *      frame = pointer to frame buffer (yuyv format)
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
 *      frame = pointer to frame buffer (yuyv format)
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
 *    frame  = pointer to frame buffer (yuyv format)
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

/*sets a trail of particles obtained from the image
 * args:
 *    frame  = pointer to frame buffer (yuyv format)
 *    width  = frame width
 *    height = frame height
 *    trail_size  = trail size (in frames)
 *    particles = pointer to particles array (struct particle)
 */
struct particle*
particles_effect(BYTE* frame, int width, int height, int trail_size, struct particle* particles)
{
	int i,j,w,h = 0;
	int part_w = width>>4;
	int part_h = height>>3;
	int y_pos = 0; //luma position in the frame
	GRand* rand_= g_rand_new_with_seed(2);
	//allocation
	if (particles == NULL)
	{
		particles = g_new0(struct particle, trail_size * part_w * part_h);
	}
	
	struct particle* part = particles;
	struct particle* part1 = part;
	
	//move particles in trail
	for (i=trail_size; i > 1; i--)
	{
		part  += (i - 1) * part_w * part_h;
		part1 += (i - 2) * part_w * part_h;
		
		for (j= 0; j < part_w * part_h; j++)
		{
			part->PX = part1->PX + g_rand_int_range(rand_, 1, 2);
			part->PY = part1->PY + g_rand_int_range(rand_, -1, 1);
			part->Y = part1->Y;
			part->U = part1->U;
			part->V = part1->V;
			if((part->PX > width) || (part->PY > height))
			{
				part->PX = 0;
				part->PY = 0;
				part->decay = 0.0;
			}
			else
			{
				part->decay = part1->decay - i;
			}
			
			part++;
			part1++;
		}
		part = particles; //reset
		part1 = part;
	}
			
	//get particles from frame
	for(w=1; w <= part_w; w++)
	{
		for(h=1; h <= part_h; h++)
		{
			part->PX = (g_rand_int_range(rand_, 1, 1<<4) * w) -1;
			part->PY = (g_rand_int_range(rand_, 1, 1<<3 ) * h) -1;
			y_pos = part->PX * 2 + (part->PY * width * 2);
			part->Y = frame[y_pos];
			if(ODD(y_pos))
			{
				part->U = frame[y_pos -1];
				part->V = frame[y_pos +1];
			}
			else
			{
				part->U = frame[y_pos +1];
				part->V = frame[y_pos +3];
			}
			part->decay = (float) trail_size;
			
			part++; //next particle
		}
	}
	
	part = particles; //reset
	//render particles to frame
	for (i = 0; i < trail_size * part_w * part_h; i++)
	{	
		y_pos = part->PX * 2 + (part->PY * width * 2);
		frame[y_pos] = frame[y_pos] * (1-(part->decay/trail_size)) + part->Y * (part->decay/trail_size);
		if(ODD(y_pos))
		{
			frame[y_pos -1] = frame[y_pos -1] * (1-(part->decay/trail_size)) + part->U * (part->decay/trail_size);
			frame[y_pos +1] = frame[y_pos +1] * (1-(part->decay/trail_size)) + part->V * (part->decay/trail_size);
		}
		else
		{
			frame[y_pos +1] = frame[y_pos +1] * (1-(part->decay/trail_size)) + part->U * (part->decay/trail_size);
			frame[y_pos +3] = frame[y_pos +3] * (1-(part->decay/trail_size)) + part->V * (part->decay/trail_size);
		}
	}
	
	g_rand_free(rand_);
	return(particles);
}
