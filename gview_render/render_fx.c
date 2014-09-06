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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <inttypes.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>

#include "gviewrender.h"
#include "gview.h"
#include "../config.h"

/*random generator (HAS_GSL is set in ../config.h)*/
#ifdef HAS_GSL
	#include <gsl/gsl_rng.h>
#endif


typedef struct _particle_t
{
	int PX;
	int PY;
	uint8_t Y;
	uint8_t U;
	uint8_t V;
	int size;
	float decay;
} particle_t;

static particle_t *particles = NULL;

/*
 * Flip YUYV frame - horizontal
 * args:
 *    frame - pointer to frame buffer (yuyv format)
 *    width - frame width
 *    height- frame height
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
static void fx_yuyv_mirror (uint8_t *frame, int width, int height)
{
	/*asserts*/
	assert(frame != NULL);

	int h=0;
	int w=0;
	int sizeline = width*2;    /* 2 bytes per pixel*/
	uint8_t *pframe;
	pframe=frame;
	uint8_t line[sizeline-1];  /*line buffer*/
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

/*
 * Invert YUV frame
 * args:
 *    frame - pointer to frame buffer (yuyv format)
 *    width - frame width
 *    height- frame height
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
static void fx_yuyv_negative(uint8_t *frame, int width, int height)
{
	/*asserts*/
	assert(frame != NULL);

	int size= width * height * 2;
	int i=0;
	for(i=0; i < size; i++)
		frame[i] = ~frame[i];
}

/*
 * Flip YUV frame - vertical
 * args:
 *    frame - pointer to frame buffer (yuyv format)
 *    width - frame width
 *    height- frame height
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
static void fx_yuyv_upturn(uint8_t *frame, int width, int height)
{
	/*asserts*/
	assert(frame != NULL);

	int h = 0;
	int sizeline = width * 2;  /* 2 bytes per pixel*/
	uint8_t line1[sizeline-1]; /*line1 buffer*/
	uint8_t line2[sizeline-1]; /*line2 buffer*/
	for ( h = 0; h < height/2; ++h)
	{	/*line iterator*/
		memcpy(line1, frame + h * sizeline, sizeline);
		memcpy(line2, frame + (height - 1 - h) * sizeline, sizeline);

		memcpy(frame + h * sizeline, line2, sizeline);
		memcpy(frame + (height - 1 - h) * sizeline, line1, sizeline);
	}
}

/*
 * Monochromatic effect for YUYV frame
 * args:
 *     frame - pointer to frame buffer (yuyv format)
 *     width - frame width
 *     height- frame height
 *
 * asserts:
 *     frame is not null
 *
 * returns: void
 */
static void fx_yuyv_monochrome(uint8_t* frame, int width, int height)
{
	int size = width * height * 2;
	int i = 0;

	for(i=0; i < size; i = i + 4)
	{	/* keep Y - luma */
		frame[i+1]=0x80;/*U - median (half the max value)=128*/
		frame[i+3]=0x80;/*V - median (half the max value)=128*/
	}
}


#ifdef HAS_GSL
/*
 * Break image in little square pieces
 * args:
 *    frame  - pointer to frame buffer (yuyv format)
 *    width  - frame width
 *    height - frame height
 *    piece_size - multiple of 2 (we need at least 2 pixels to get the entire pixel information)
 *
 * asserts:
 *    frame is not null
 */
static void fx_pieces(uint8_t* frame, int width, int height, int piece_size )
{
	int numx = width / piece_size;
	int numy = height / piece_size;
	uint8_t *piece = calloc (piece_size * piece_size * 2, sizeof(uint8_t));
	if(piece == NULL)
	{
		fprintf(stderr,"RENDER: FATAL memory allocation failure (fx_pieces): %s\n", strerror(errno));
		exit(-1);
	}
	int i = 0, j = 0, line = 0, column = 0, linep = 0, px = 0, py = 0;

	/*random generator setup*/
	gsl_rng_env_setup();
	const gsl_rng_type *T = gsl_rng_default;
	gsl_rng *r = gsl_rng_alloc (T);

	int rot = 0;

	for(j = 0; j < numy; j++)
	{
		int row = j * piece_size;
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
			rot = (int) lround(8 * gsl_rng_uniform (r)); /*0 to 8*/

			switch(rot)
			{
				case 0: // do nothing
					break;
				case 5:
				case 1: //mirror
					fx_yuyv_mirror(piece, piece_size, piece_size);
					break;
				case 6:
				case 2: //upturn
					fx_yuyv_upturn(piece, piece_size, piece_size);
					break;
				case 4:
				case 3://mirror upturn
					fx_yuyv_upturn(piece, piece_size, piece_size);
					fx_yuyv_mirror(piece, piece_size, piece_size);
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

	/*free the random seed generator*/
	gsl_rng_free (r);
	/*free the piece buffer*/
	free(piece);
}

/*
 * Trail of particles obtained from the image frame
 * args:
 *    frame  - pointer to frame buffer (yuyv format)
 *    width  - frame width
 *    height - frame height
 *    trail_size  - trail size (in frames)
 *    particle_size - maximum size in pixels - should be even (square - size x size)
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
static void fx_particles(uint8_t* frame, int width, int height, int trail_size, int particle_size)
{
	/*asserts*/
	assert(frame != NULL);

	int i,j,w,h = 0;
	int part_w = width>>7;
	int part_h = height>>6;
	int y_pos = 0; /*luma position in the frame*/

	/*random generator setup*/
	gsl_rng_env_setup();
	const gsl_rng_type *T = gsl_rng_default;
	gsl_rng *r = gsl_rng_alloc (T);

	/*allocation*/
	if (particles == NULL)
	{
		particles = calloc(trail_size * part_w * part_h, sizeof(particle_t));
		if(particles == NULL)
		{
			fprintf(stderr,"RENDER: FATAL memory allocation failure (fx_particles): %s\n", strerror(errno));
			exit(-1);
		}
	}

	particle_t *part = particles;
	particle_t *part1 = part;

	/*move particles in trail*/
	for (i = trail_size; i > 1; --i)
	{
		part  += (i - 1) * part_w * part_h;
		part1 += (i - 2) * part_w * part_h;

		for (j= 0; j < part_w * part_h; ++j)
		{
			if(part1->decay > 0)
			{
				part->PX = part1->PX + (int) lround(3 * gsl_rng_uniform (r)); /*0  to 3*/
				part->PY = part1->PY -4 + (int) lround(5 * gsl_rng_uniform (r));/*-4 to 1*/

				if(ODD(part->PX)) part->PX++; /*make sure PX is allways even*/

				if((part->PX > (width-particle_size)) || (part->PY > (height-particle_size)) || (part->PX < 0) || (part->PY < 0))
				{
					part->PX = 0;
					part->PY = 0;
					part->decay = 0;
				}
				else
				{
					part->decay = part1->decay - 1;
				}

				part->Y = part1->Y;
				part->U = part1->U;
				part->V = part1->V;
				part->size = part1->size;
			}
			else
			{
				part->decay = 0;
			}
			part++;
			part1++;
		}
		part = particles; /*reset*/
		part1 = part;
	}

	part = particles; /*reset*/
	/*get particles from frame (one pixel per particle - make PX allways even)*/
	for(i =0; i < part_w * part_h; i++)
	{
		/* (2 * particle_size) to (width - 4 * particle_size)*/
		part->PX = 2 * particle_size + (int) lround( (width - 6 * particle_size) * gsl_rng_uniform (r));
		/* (2 * particle_size) to (height - 4 * particle_size)*/
		part->PY = 2 * particle_size + (int) lround( (height - 6 * particle_size) * gsl_rng_uniform (r));

		if(ODD(part->PX)) part->PX++;

		y_pos = part->PX * 2 + (part->PY * width * 2);

		part->Y = frame[y_pos];
		part->U = frame[y_pos +1];
		part->V = frame[y_pos +3];

		part->size = 1 + (int) lround((particle_size -1) * gsl_rng_uniform (r));
		if(ODD(part->size)) part->size++;

		part->decay = (float) trail_size;

		part++; /*next particle*/
	}

	part = particles; /*reset*/
	int line = 0;
	float blend =0;
	float blend1 =0;
	/*render particles to frame (expand pixel to particle size)*/
	for (i = 0; i < trail_size * part_w * part_h; i++)
	{
		if(part->decay > 0)
		{
			y_pos = part->PX * 2 + (part->PY * width * 2);
			blend = part->decay/trail_size;
			blend1= 1 -blend;
			for(h=0; h<(part->size); h++)
			{
				line = h * width * 2;
				for (w=0; w<(part->size)*2; w+=4)
				{
					frame[y_pos + w + line] = CLIP(part->Y*blend + frame[y_pos + w + line]*blend1);
					frame[(y_pos + w + 1) + line] = CLIP(part->U*blend + frame[(y_pos + w + 1) + line]*blend1);
					frame[(y_pos + w + 2) + line] = CLIP(part->Y*blend + frame[(y_pos + w + 2) + line]*blend1);
					frame[(y_pos + w + 3) + line] = CLIP(part->V*blend + frame[(y_pos + w + 3) + line]*blend1);
				}
			}
		}
		part++;
	}

	/*free the random seed generator*/
	gsl_rng_free (r);
}

#endif

/*
 * Apply fx filters
 * args:
 *    frame - pointer to frame buffer (yuyv format)
 *    width - frame width
 *    height - frame height
 *    mask  - or'ed filter mask
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
void render_fx_apply(uint8_t *frame, int width, int height, uint32_t mask)
{
	if(mask != REND_FX_YUV_NOFILT)
    {
		#ifdef HAS_GSL
		if(mask & REND_FX_YUV_PARTICLES)
			fx_particles (frame, width, height, 20, 4);
		#endif

		if(mask & REND_FX_YUV_MIRROR)
			fx_yuyv_mirror(frame, width, height);

		if(mask & REND_FX_YUV_UPTURN)
			fx_yuyv_upturn(frame, width, height);

		if(mask & REND_FX_YUV_NEGATE)
			fx_yuyv_negative (frame, width, height);

		if(mask & REND_FX_YUV_MONOCR)
			fx_yuyv_monochrome (frame, width, height);

		#ifdef HAS_GSL
		if(mask & REND_FX_YUV_PIECES)
			fx_pieces (frame, width, height, 16 );
		#endif
	}
	else
		render_clean_fx();
}

/*
 * clean fx filters
 * args:
 *    none
 *
 * asserts:
 *    none
 *
 * returns: void
 */
void render_clean_fx()
{
	if(particles != NULL)
	{
		free(particles);
		particles = NULL;
	}
}
