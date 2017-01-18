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
#include <time.h>

#include "gviewrender.h"
#include "gview.h"
#include "../config.h"

/*random generator (HAS_GSL is set in ../config.h)*/
#ifdef HAS_GSL
	#include <gsl/gsl_rng.h>
#endif


double *Gkernel = NULL; //gaussian kernel matrix
int* bSizes = NULL;
uint8_t *tmpbuffer = NULL;
uint32_t *TB_Sqrt_ind = NULL; //look up table for sqrt lens distort indexes
uint32_t *TB_Pow_ind = NULL; //look up table for pow lens distort indexes
uint32_t *TB_Pow2_ind = NULL; //look up table for pow2 lens distort indexes

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
 * Flip yu12 frame - horizontal
 * args:
 *    frame - pointer to frame buffer (yu12=iyuv format)
 *    width - frame width
 *    height- frame height
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
static void fx_yu12_mirror (uint8_t *frame, int width, int height)
{
	/*asserts*/
	assert(frame != NULL);

	int h=0;
	int w=0;
	int y_sizeline = width;
	int c_sizeline = width/2;

	uint8_t *end = NULL;
	uint8_t *end2 = NULL;

	uint8_t *py = frame;
	uint8_t *pu = frame + (width * height);
	uint8_t *pv = pu + ((width * height) / 4);

	uint8_t pixel =0;
	uint8_t pixel2=0;

	/*mirror y*/
	for(h = 0; h < height; h++)
	{
		py = frame + (h * width);
		end = py + width - 1;
		for(w = 0; w < width/2; w++)
		{
			pixel = *py;
			*py++ = *end;
			*end-- = pixel;
		}
	}

	/*mirror u v*/
	for(h = 0; h < height; h+=2)
	{
		pu = frame + (width * height) + ((h * width) / 4);
		pv = pu + ((width * height) / 4);
		end  = pu + (width / 2) - 1;
		end2 = pv + (width / 2) -1;
		for(w = 0; w < width/2; w+=2)
		{
			pixel  = *pu;
			pixel2 = *pv;
			*pu++ = *end;
			*pv++ = *end2;
			*end-- = pixel;
			*end2-- = pixel2;
		}
	}
}

/*
 * Flip half yu12 frame - horizontal
 * args:
 *    frame - pointer to frame buffer (yu12=iyuv format)
 *    width - frame width
 *    height- frame height
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
static void fx_yu12_half_mirror (uint8_t *frame, int width, int height)
{
	/*asserts*/
	assert(frame != NULL);

	int h=0;
	int w=0;

	uint8_t *end = NULL;
	uint8_t *end2 = NULL;

	uint8_t *py = frame;
	uint8_t *pu = frame + (width * height);
	uint8_t *pv = pu + ((width * height) / 4);

	uint8_t pixel =0;
	uint8_t pixel2=0;

	/*mirror y*/
	for(h = 0; h < height; h++)
	{
		py = frame + (h * width);
		end = py + width - 1;
		for(w = 0; w < width/2; w++)
		{
			*end-- = *py++;
		}
	}

	/*mirror u v*/
	for(h = 0; h < height; h+=2)
	{
		pu = frame + (width * height) + ((h * width) / 4);
		pv = pu + ((width * height) / 4);
		end  = pu + (width / 2) - 1;
		end2 = pv + (width / 2) -1;
		for(w = 0; w < width/2; w+=2)
		{
			*end-- = *pu++;
			*end2-- = *pv++;
		}
	}
}

/*
 * Invert YUV frame
 * args:
 *    frame - pointer to frame buffer (any yuv format)
 *    width - frame width
 *    height- frame height
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
static void fx_yuv_negative(uint8_t *frame, int width, int height)
{
	/*asserts*/
	assert(frame != NULL);

	int size = (width * height * 5) / 4;

	int i=0;
	for(i=0; i < size; i++)
		frame[i] = ~frame[i];
}

/*
 * Flip yu12 frame - vertical
 * args:
 *    frame - pointer to frame buffer (yu12 format)
 *    width - frame width
 *    height- frame height
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
static void fx_yu12_upturn(uint8_t *frame, int width, int height)
{
	/*asserts*/
	assert(frame != NULL);

	int h = 0;

	uint8_t line[width]; /*line1 buffer*/

	uint8_t *pi = frame; //begin of first y line
	uint8_t *pf = pi + (width * (height - 1)); //begin of last y line

	/*upturn y*/
	for ( h = 0; h < height / 2; ++h)
	{	/*line iterator*/
		memcpy(line, pi, width);
		memcpy(pi, pf, width);
		memcpy(pf, line, width);

		pi+=width;
		pf-=width;

	}

	/*upturn u*/
	pi = frame + (width * height); //begin of first u line
	pf = pi + ((width * height) / 4) - (width / 2); //begin of last u line
	for ( h = 0; h < height / 2; h += 2) //every two lines = height / 4
	{	/*line iterator*/
		memcpy(line, pi, width / 2);
		memcpy(pi, pf, width / 2);
		memcpy(pf, line, width / 2);

		pi+=width/2;
		pf-=width/2;
	}

	/*upturn v*/
	pi = frame + ((width * height * 5) / 4); //begin of first v line
	pf = pi + ((width * height) / 4) - (width / 2); //begin of last v line
	for ( h = 0; h < height / 2; h += 2) //every two lines = height / 4
	{	/*line iterator*/
		memcpy(line, pi, width / 2);
		memcpy(pi, pf, width / 2);
		memcpy(pf, line, width / 2);

		pi+=width/2;
		pf-=width/2;
	}

}

/*
 * Flip half yu12 frame - vertical
 * args:
 *    frame - pointer to frame buffer (yu12 format)
 *    width - frame width
 *    height- frame height
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
static void fx_yu12_half_upturn(uint8_t *frame, int width, int height)
{
	/*asserts*/
	assert(frame != NULL);

	int h = 0;

	uint8_t line[width]; /*line1 buffer*/

	uint8_t *pi = frame; //begin of first y line
	uint8_t *pf = pi + (width * (height - 1)); //begin of last y line

	/*upturn y*/
	for ( h = 0; h < height / 2; ++h)
	{	/*line iterator*/
		memcpy(line, pi, width);
		memcpy(pf, line, width);

		pi+=width;
		pf-=width;

	}

	/*upturn u*/
	pi = frame + (width * height); //begin of first u line
	pf = pi + ((width * height) / 4) - (width / 2); //begin of last u line
	for ( h = 0; h < height / 2; h += 2) //every two lines = height / 4
	{	/*line iterator*/
		memcpy(line, pi, width / 2);
		memcpy(pf, line, width / 2);

		pi+=width/2;
		pf-=width/2;
	}

	/*upturn v*/
	pi = frame + ((width * height * 5) / 4); //begin of first v line
	pf = pi + ((width * height) / 4) - (width / 2); //begin of last v line
	for ( h = 0; h < height / 2; h += 2) //every two lines = height / 4
	{	/*line iterator*/
		memcpy(line, pi, width / 2);
		memcpy(pf, line, width / 2);

		pi+=width/2;
		pf-=width/2;
	}

}

/*
 * Monochromatic effect for yu12 frame
 * args:
 *     frame - pointer to frame buffer (yu12 format)
 *     width - frame width
 *     height- frame height
 *
 * asserts:
 *     frame is not null
 *
 * returns: void
 */
static void fx_yu12_monochrome(uint8_t* frame, int width, int height)
{

	uint8_t *puv = frame + (width * height); //skip luma

	int i = 0;

	for(i=0; i < (width * height) / 2; ++i)
	{	/* keep Y - luma */
		*puv++=0x80;/*median (half the max value)=128*/
	}
}


#ifdef HAS_GSL

/*
 * Break yu12 image in little square pieces
 * args:
 *    frame  - pointer to frame buffer (yu12 format)
 *    width  - frame width
 *    height - frame height
 *    piece_size - multiple of 2 (we need at least 2 pixels to get the entire pixel information)
 *
 * asserts:
 *    frame is not null
 */
static void fx_yu12_pieces(uint8_t* frame, int width, int height, int piece_size )
{
	int numx = width / piece_size; //number of pieces in x axis
	int numy = height / piece_size; //number of pieces in y axis

	uint8_t piece[(piece_size * piece_size * 3) / 2];
	uint8_t *ppiece = piece;

	int i = 0, j = 0, w = 0, h = 0;

	/*random generator setup*/
	gsl_rng_env_setup();
	const gsl_rng_type *T = gsl_rng_default;
	gsl_rng *r = gsl_rng_alloc (T);

	int rot = 0;

	uint8_t *py = NULL;
	uint8_t *pu = NULL;
	uint8_t *pv = NULL;

	for(h = 0; h < height; h += piece_size)
	{
		for(w = 0; w < width; w += piece_size)
		{
			uint8_t *ppy = piece;
			uint8_t *ppu = piece + (piece_size * piece_size);
			uint8_t *ppv = ppu + ((piece_size * piece_size) / 4);

			for(i = 0; i < piece_size; ++i)
			{
				py = frame + ((h + i) * width) + w;
				for (j=0; j < piece_size; ++j)
				{
					*ppy++ = *py++;
				}
			}

			for(i = 0; i < piece_size; i += 2)
			{
				uint8_t *pu = frame + (width * height) + (((h + i) * width) / 4) + (w / 2);
				uint8_t *pv = pu + ((width * height) / 4);

				for(j = 0; j < piece_size; j += 2)
				{
					*ppu++ = *pu++;
					*ppv++ = *pv++;
				}
			}

			ppy = piece;
			ppu = piece + (piece_size * piece_size);
			ppv = ppu + ((piece_size * piece_size) / 4);

			/*rotate piece and copy it to frame*/
			//rotation is random
			rot = (int) lround(8 * gsl_rng_uniform (r)); /*0 to 8*/

			switch(rot)
			{
				case 0: // do nothing
					break;
				case 5:
				case 1: //mirror
					fx_yu12_mirror(piece, piece_size, piece_size);
					break;
				case 6:
				case 2: //upturn
					fx_yu12_upturn(piece, piece_size, piece_size);
					break;
				case 4:
				case 3://mirror upturn
					fx_yu12_upturn(piece, piece_size, piece_size);
					fx_yu12_mirror(piece, piece_size, piece_size);
					break;
				default: //do nothing
					break;
			}

			ppy = piece;
			ppu = piece + (piece_size * piece_size);
			ppv = ppu + ((piece_size * piece_size) / 4);

			for(i = 0; i < piece_size; ++i)
			{
				py = frame + ((h + i) * width) + w;
				for (j=0; j < piece_size; ++j)
				{
					*py++ = *ppy++;
				}
			}

			for(i = 0; i < piece_size; i += 2)
			{
				uint8_t *pu = frame + (width * height) + (((h + i) * width) / 4) + (w / 2);
				uint8_t *pv = pu + ((width * height) / 4);

				for(j = 0; j < piece_size; j += 2)
				{
					*pu++ = *ppu++;
					*pv++ = *ppv++;
				}
			}
		}
	}

	/*free the random seed generator*/
	gsl_rng_free (r);
}

/*
 * Trail of particles obtained from the image frame
 * args:
 *    frame  - pointer to frame buffer (yu12 format)
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

		int y_pos = part->PX + (part->PY * width);
		int u_pos = (part->PX + (part->PY * width / 2)) / 2;
		int v_pos = u_pos + ((width * height) / 4);

		part->Y = frame[y_pos];
		part->U = frame[u_pos];
		part->V = frame[v_pos];

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
			int y_pos = part->PX + (part->PY * width);
			int u_pos = (part->PX + (part->PY * width / 2)) / 2;
			int v_pos = u_pos + ((width * height) / 4);

			blend = part->decay/trail_size;
			blend1= 1 - blend;

			//y
			for(h = 0; h <(part->size); h++)
			{
				line = h * width;
				for (w = 0; w <(part->size); w++)
				{
					frame[y_pos + line + w] = CLIP((part->Y * blend) + (frame[y_pos + line + w] * blend1));
				}
			}

			//u v
			for(h = 0; h <(part->size); h+=2)
			{
				line = (h * width) / 4;
				for (w = 0; w <(part->size); w+=2)
				{
					frame[u_pos + line + (w / 2)] = CLIP((part->U * blend) + (frame[u_pos + line + (w / 2)] * blend1));
					frame[v_pos + line + (w / 2)] = CLIP((part->V * blend) + (frame[v_pos + line + (w / 2)] * blend1));
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
 * Normalize X coordinate
 * args:
 *      i - pixel position from 0 to width-1
 *      width - frame width
 *
 * returns:
 *         normalized x coordinate (-1 to 1))
 */
double normX(int i, int width)
{
    if(i<0 )
        return -1.0;
    if(i>= width)
        return 1.0;

    double x = (double) ((2 * (double)(i)) / (double)(width)) -1;

    if(x < -1)
        return -1;
    if(x > 1)
        return 1;

    return x;
}

/*
 * Normalize Y coordinate
 * args:
 *      j - pixel position from 0 to height-1
 *      height - frame height
 *
 * returns:
 *         normalized y coordinate (-1 to 1))
 */
double normY(int j, int height)
{
    if(j<0 )
        return -1.0;
    if(j>= height)
        return 1.0;

    double y = (double) ((2 * (double)(j)) / (double)(height)) -1;

    if(y < -1)
        return -1;
    if(y > 1)
        return 1;

    return y;
}

/*
 * Denormalize X coordinate
 * args:
 *      x - normalized pixel position from -1 to 1
 *      width - frame width
 *
 * returns:
 *         x coordinate (0 to width -1)
 */
int denormX(double x, int width)
{
    int i = (int) lround(0.5 * width * (x + 1) -1);

    if(i < 0)
        return 0;
    if(i >= width)
        return (width -1);

    return i;
}

/*
 * denormalize Y coordinate
 * args:
 *      y - normalized pixel position from -1 to 1
 *      height - frame height
 *
 * returns:
 *         y coordinate (0 to height -1)
 */
int denormY(double y, int height)
{

    int j = (int) lround(0.5 * height * (y + 1) -1);

    if(j < 0)
        return 0;
    if(j >= height)
        return (height -1);

    return j;
}

#define PI      3.14159265
#define DPI     6.28318531
#define PI2     1.57079632

/*
 * fast sin replacement
 */
double fast_sin(double x)
{
    if(x < -PI)
        x += DPI;
    else if (x > PI)
        x -= DPI;

    if(x < 0)
        return ((1.27323954 * x) + (.405284735 * x * x));
    else
        return ((1.27323954 * x) - (.405284735 * x * x));
}


/*
 * fast cos replacement
 */
double fast_cos(double x)
{
    x += PI2;
    if(x > PI)
        x -= DPI;

    if(x < 0)
        return ((1.27323954 * x) + (.405284735 * x * x));
    else
        return ((1.27323954 * x) - (.405284735 * x * x));
}

/*
 * fast atan2 replacement
 */
double fast_atan2( double y, double x )
{
	if ( x == 0.0f )
	{
		if ( y > 0.0f ) return PI2;
		if ( y == 0.0f ) return 0.0f;
		return -PI2;
	}
	double atan;
	double z = y/x;
	if ( fabs( z ) < 1.0f )
	{
		atan = z/(1.0f + 0.28f*z*z);
		if ( x < 0.0f )
		{
			if ( y < 0.0f ) return atan - PI;
			return atan + PI;
		}
	}
	else
	{
		atan = PI2- z/(z*z + 0.28f);
		if ( y < 0.0f ) return atan - PI;
	}
	return atan;
}

/*
 * calculate coordinate in input frame from point in ouptut
 * (all coordinates are normalized)
 * args:
 *      x,y - output cordinates
 *      xnew, ynew - pointers to input coordinates
 *      type - type of distortion
 */
void eval_coordinates (double x, double y, double *xnew, double *ynew, int type)
{
    double phi, radius, radius2;

    switch (type)
    {
        case REND_FX_YUV_POW_DISTORT:
            radius2 = x*x + y*y;
            radius = radius2; // pow(radius,2)
            phi = fast_atan2(y,x);
						//phi = atan2(y,x);

            *xnew = radius * fast_cos(phi);
            *ynew = radius * fast_sin(phi);
						//*xnew = radius * cos(phi);
						//*ynew = radius * sin(phi);
            break;

        case REND_FX_YUV_POW2_DISTORT:
            *xnew = x * x * SIGN(x);
            *ynew = y * y * SIGN(y);
            break;

        case REND_FX_YUV_SQRT_DISTORT:
        default:
            /* square root radial funtion */
            radius2 = x*x + y*y;
            radius = sqrt(radius2);
            radius = sqrt(radius);
            phi = fast_atan2(y,x);
						//phi = atan2(y,x);

            *xnew = radius * fast_cos(phi);
            *ynew = radius * fast_sin(phi);
						//*xnew = radius * cos(phi);
						//*ynew = radius * sin(phi);
            break;
    }
}

/*
 * generate gaussian kernel
 * args:
 *    radius  - radius for gaussian blur (e.g r=2 -> kernel 5x5)
 *
 * asserts:
 *    none
 *
 * returns: void
 */
static void generate_gauss_kernel(int radius)
{
	if(Gkernel != NULL)
		return; //kernel already generated

	int msize = 2 * radius + 1; //matrix size (msize * msize)
	Gkernel = malloc(msize*msize*sizeof(double));

	double r = radius;
	double s = radius;
	double sum = 0.0;

	int x = 0;
	int y = 0;

	for (y = -radius; y <= radius; ++y)
	{
		for(x = -radius; x <= radius; ++x)
		{
			r = sqrt(x*x + y*y);
			Gkernel[(y+radius)*msize + (x+radius)] = (exp(-(r*r)/s))/(PI * s);
			sum += Gkernel[(y+radius)*msize + (x+radius)];
		}
	}

	//normalize it
	for( y = 0; y <= 2 * radius; ++y)
		for( x = 0; x <= 2 * radius; ++x)
			Gkernel[(y * msize) + x] /= sum;
}

/*
 * gaussian blur
 * args:
 *    frame  - pointer to frame buffer (yu12 format)
 *    width  - frame width
 *    height - frame height
 *    radius - blur radius (even value)
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
void fx_yu12_gauss_blur(uint8_t* frame, int width, int height, int radius)
{
	assert(frame != NULL);

	//make sure we have a even radius (matrix size must be odd = radius + 1 )
	if(radius%2!=0)
		radius--;

	int msize = 2 * radius + 1;
	generate_gauss_kernel(radius);

	if(tmpbuffer == NULL)
		tmpbuffer = malloc(width * height * 3 / 2);

  memcpy(tmpbuffer, frame, width * height * 3 / 2);

	int i = 0;
	int j = 0;
	int x = 0;
	int y = 0;

	for(i = radius; i < height - radius; ++i )
	{
		for(j = radius; j < width - radius; ++j)
		{
			double sum = 0;
			for(y = -radius; y <= radius; ++y )
			{
				for(x = - radius; x <= radius; ++x)
				{
					sum += tmpbuffer[(i + y) * width + (j + x)] * Gkernel[(y + radius) * msize + (x+radius)];
				}
			}

			frame[i * width + j] = (uint8_t) lround(sum) & 0xff;
		}
	}
}

/*
 * generate box sizes for box blur
 * args:
 *    sigma - standard deviation
 *    n - number of boxes
 *
 * asserts:
 *    none
 *
 * returns: void
 */
static void boxes4gauss(int sigma, int n)
{
	if(bSizes != NULL)
		return; //already calculated

	bSizes = calloc(n, sizeof(int));

	double ideal_width = sqrt((12*sigma*sigma/n) + 1);

	int wl = lround(floor(ideal_width));
	if(wl%2==0)
		wl--;
	int wu = wl+2;

	double ideal_m = (n*wl*wl + 4*n*wl + 3*n - 12*sigma*sigma)/(4*wl + 4);

	int m = lround(ideal_m);

	int i = 0;
	for(i = 0; i < n; ++i)
		bSizes[i] = (i < m) ? wl : wu;
}

/*
 * box blur horizontal
 */
void boxBlurH(uint8_t* scl, uint8_t* tcl, int w, int h, int r)
{
	int iarr = r + r + 1;

	int i = 0;
	int j = 0;

	for(i = 0; i < h; ++i)
	{
		int ti = i*w;
		int li = ti;
		int ri = ti+r;

    int fv = scl[ti];
		int lv = scl[ti+w-1];
		int val = (r+1)*fv;

		for(j = 0; j < r; ++j)
			val += scl[ti+j];

    for(j = 0; j <= r; ++j)
		{
			val += scl[ri++] - fv;
			tcl[ti++] = (uint8_t) lround(val/iarr) & 0xff;
		}

		for(j = r+1; j < w-r; ++j)
		{
			val += scl[ri++] - scl[li++];
			tcl[ti++] = (uint8_t) lround(val/iarr) & 0xff;
		}

		for( j =w-r; j < w; ++j)
		{
			val += lv - scl[li++];
			tcl[ti++] = (uint8_t) lround(val/iarr) & 0xff;
		}
  }
}

/*
 * box blur total
 */
void boxBlurT(uint8_t* scl, uint8_t* tcl, int w, int h, int r)
{
	int iarr = r + r + 1;

	int i = 0;
	int j = 0;

	for(i = 0; i < w; ++i)
	{
  	int ti = i;
		int li = ti;
		int ri = ti+r*w;

    int fv = scl[ti];
		int lv = scl[ti+w*(h-1)];
		int val = (r+1)*fv;

    for(j = 0; j < r; ++j)
			val += scl[ti+j*w];

    for(j = 0; j <= r; ++j)
		{
			val += scl[ri] - fv;
			tcl[ti] = (uint8_t) lround(val/iarr) & 0xff;
			ri += w;
			ti += w;
		}

		for(j = r+1; j < h-r; ++j)
		{
			val += scl[ri] - scl[li];
			tcl[ti] = (uint8_t) lround(val/iarr) & 0xff;
			li += w;
			ri += w;
			ti += w;
		}

		for(j = h-r; j < h; ++j)
		{
			val += lv - scl[li];
			tcl[ti] = (uint8_t) lround(val/iarr) & 0xff;
			li += w;
			ti += w;
		}
  }
}

/*
 * box blur
 */
void boxBlur(uint8_t* scl, uint8_t* tcl, int width, int height, int r)
{

	int i = 0;
	for(i = 0; i < width * height; ++i) //memcpy
		tcl[i] = scl[i];

	boxBlurH(tcl, scl, width, height, r);
	boxBlurT(scl, tcl, width, height, r);
}

/*
 * gaussian blur aprox with 3 box blur iterations
 * args:
 *    frame  - pointer to frame buffer (yu12 format)
 *    width  - frame width
 *    height - frame height
 *    sigma - deviation
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
void fx_yu12_gauss_blur2(uint8_t* frame, int width, int height, int sigma)
{
	assert(frame != NULL);

	if(tmpbuffer == NULL)
		tmpbuffer = malloc(width * height * 3 / 2);

	//iterate 3 times
	boxes4gauss(sigma, 3);

	boxBlur(frame, tmpbuffer, width, height, (bSizes[0] - 1)/2);
	boxBlur(tmpbuffer, frame, width, height, (bSizes[1] - 1)/2);
	boxBlur(frame, tmpbuffer, width, height, (bSizes[2] - 1)/2);

}

/*
 * anti-aliasing
 * args:
 *    frame  - pointer to frame buffer (yu12 format)
 *    width  - frame width
 *    height - frame height
 *    scale - 2 (Scale2x) or 3 (Scale3x)
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
void fx_yu12_antialiasing(uint8_t* frame, int width, int height, int scale)
{
	assert(frame != NULL);

	uint8_t *pu = frame + (width*height);
	uint8_t *pv = pu + (width*height)/4;

	int luma = 0;
	int chroma = 0;

	uint8_t E = 0;
	uint8_t A = 0;
	uint8_t B = 0;
	uint8_t C = 0;
	uint8_t D = 0;
	uint8_t F = 0;
	uint8_t G = 0;
	uint8_t H = 0;
	uint8_t I = 0;

	uint8_t E0 = 0;
	uint8_t E1 = 0;
	uint8_t E2 = 0;
	uint8_t E3 = 0;
	uint8_t E4 = 0;
	uint8_t E5 = 0;
	uint8_t E6 = 0;
	uint8_t E7 = 0;
	uint8_t E8 = 0;

	int ind = 0;

	int i = 0;
	int j = 0;

	int hwidth = width >> 1;   //div by 2
	int hheight = height >> 1; //div by 2

	/*
	 *    A...B...C
	 *    D...E...F
	 *    G...H...I
	 */
	for(j = 0; j < height; ++j)
	{
		for(i = 0; i < width; ++i)
		{
			ind = i + (j * width);

			E = frame[ind];

			if(j > 0)
				B = frame[i + ((j-1) * width)];
			else
				B = E;

			if(i > 0)
				D = frame[(i-1) + (j * width)];
			else
				D = E;

			if(i < width - 1)
				F = frame[(i+1) + (j * width)];
			else
				F = E;

			if(j < (height - 1))
				H = frame[i + ((j+1) * width)];
			else
				H = E;

			switch(scale)
			{
				case 3:
				{
					if(j > 0 && i > 0)
						A = frame[(i-1) + ((j-1) * width)];
					else
						A = E;

					if(j > 0 && i < (width -1))
						C = frame[(i+1) + ((j-1) * width)];
					else
						C = E;

					if(j < (height - 1) && i > 0)
						G = frame[(i-1) + ((j+1) * width)];
					else
						G = E;

					if(j < (height - 1) && i < (width -1))
						I = frame[(i+1) + ((j+1) * width)];
					else
						I = E;

					if (B != H && D != F)
					{
						E0 = (D == B) ? D : E;
						E1 = (D == B && E != C) || (B == F && E != A) ? B : E;
						E2 = (B == F) ? F : E;
						E3 = (D == B && E != G) || (D == H && E != A) ? D : E;
						E4 = E;
						E5 = (B == F && E != I) || (H == F && E != C) ? F : E;
						E6 = (D == H) ? D : E;
						E7 = (D == H && E != I) || (H == F && E != G) ? H : E;
						E8 = (H == F) ? F : E;
					}
					else
					{
						E0 = E;
						E1 = E;
						E2 = E;
						E3 = E;
						E4 = E;
						E5 = E;
						E6 = E;
						E7 = E;
						E8 = E;
					}

					luma = (E0 + E1 + E2 + E3 + E4 + E5 + E6 + E7 + E8)/9;
					frame[ind] = luma;
				}
				break;

				case 2:
				default:
				{
					if( B != H && D != F)
					{
						E0 = (D == B) ? D : E;
						E1 = (B == F) ? F : E;
						E2 = (D == H) ? D : E;
						E3 = (H == F) ? F : E;
					}
					else
					{
						E0 = E;
						E1 = E;
						E2 = E;
						E3 = E;
					}
					luma = (E0 + E1 + E2 + E3);
					frame[ind] = luma >> 2; //div by 4
				}
				break;
			}

			if(j % 2 == 0 && i % 2 == 0)
			{
				int bi = i >> 1;
				int bj = j >> 1;
				//chroma U
				E = pu[bi + (bj * hwidth)];

				if(j > 0)
					B = pu[bi + ((bj-1) * hwidth)];
				else
					B = E;

				if(bi > 0)
					D = pu[(bi-1) + (bj * hwidth)];
				else
					D = E;

				if(bi < hwidth - 1)
					F = pu[(bi+1) + (bj * hwidth)];
				else
					F = E;

				if(j < (hheight - 1))
					H = pu[bi + ((bj+1) * hwidth)];
				else
					H = E;

				switch(scale)
				{
					case 3:
					{
						if(bj > 0 && bi > 0)
							A = pu[(bi-1) + ((bj-1) * hwidth)];
						else
							A = E;

						if(bj > 0 && bi < (hwidth -1))
							C = pu[(bi+1) + ((bj-1) * hwidth)];
						else
							C = E;

						if(bj < (hheight - 1) && bi > 0)
							G = pu[(bi-1) + ((bj+1) * hwidth)];
						else
							G = E;

						if(bj < (hheight - 1) && bi < (hwidth -1))
							I = pu[(bi+1) + ((bj+1) * hwidth)];
						else
							I = E;

						if (B != H && D != F)
						{
							E0 = (D == B) ? D : E;
							E1 = (D == B && E != C) || (B == F && E != A) ? B : E;
							E2 = (B == F) ? F : E;
							E3 = (D == B && E != G) || (D == H && E != A) ? D : E;
							E4 = E;
							E5 = (B == F && E != I) || (H == F && E != C) ? F : E;
							E6 = (D == H) ? D : E;
							E7 = (D == H && E != I) || (H == F && E != G) ? H : E;
							E8 = (H == F) ? F : E;
						}
						else
						{
							E0 = E;
							E1 = E;
							E2 = E;
							E3 = E;
							E4 = E;
							E5 = E;
							E6 = E;
							E7 = E;
							E8 = E;
						}

						chroma = (E0 + E1 + E2 + E3 + E4 + E5 + E6 + E7 + E8)/9;
						pu[bi + (bj * hwidth)] = chroma;
					}
					break;

					case 2:
					default:
					{
						if( B != H && D != F)
						{
							E0 = (D == B) ? D : E;
							E1 = (B == F) ? F : E;
							E2 = (D == H) ? D : E;
							E3 = (H == F) ? F : E;
						}
						else
						{
							E0 = E;
							E1 = E;
							E2 = E;
							E3 = E;
						}
						chroma = (E0 + E1 + E2 + E3);
						pu[bi + (bj * hwidth)] = chroma >> 2; //div by 4
					}
					break;
				}

				//chroma V
				E = pv[bi + (bj * hwidth)];

				if(j > 0)
					B = pv[bi + ((bj-1) * hwidth)];
				else
					B = E;

				if(bi > 0)
					D = pv[(bi-1) + (bj * hwidth)];
				else
					D = E;

				if(bi < hwidth - 1)
					F = pv[(bi+1) + (bj * hwidth)];
				else
					F = E;

				if(j < (hheight - 1))
					H = pv[bi + ((bj+1) * hwidth)];
				else
					H = E;

				switch(scale)
				{
					case 3:
					{
						if(bj > 0 && bi > 0)
							A = pv[(bi-1) + ((bj-1) * hwidth)];
						else
							A = E;

						if(bj > 0 && bi < (hwidth -1))
							C = pv[(bi+1) + ((bj-1) * hwidth)];
						else
							C = E;

						if(bj < (hheight - 1) && bi > 0)
							G = pv[(bi-1) + ((bj+1) * hwidth)];
						else
							G = E;

						if(bj < (hheight - 1) && bi < (hwidth -1))
							I = pv[(bi+1) + ((bj+1) * hwidth)];
						else
							I = E;

						if (B != H && D != F)
						{
							E0 = (D == B) ? D : E;
							E1 = (D == B && E != C) || (B == F && E != A) ? B : E;
							E2 = (B == F) ? F : E;
							E3 = (D == B && E != G) || (D == H && E != A) ? D : E;
							E4 = E;
							E5 = (B == F && E != I) || (H == F && E != C) ? F : E;
							E6 = (D == H) ? D : E;
							E7 = (D == H && E != I) || (H == F && E != G) ? H : E;
							E8 = (H == F) ? F : E;
						}
						else
						{
							E0 = E;
							E1 = E;
							E2 = E;
							E3 = E;
							E4 = E;
							E5 = E;
							E6 = E;
							E7 = E;
							E8 = E;
						}

						chroma = (E0 + E1 + E2 + E3 + E4 + E5 + E6 + E7 + E8)/9;
						pv[bi + (bj * hwidth)] = chroma;
					}
					break;

					case 2:
					default:
					{
						if( B != H && D != F)
						{
							E0 = (D == B) ? D : E;
							E1 = (B == F) ? F : E;
							E2 = (D == H) ? D : E;
							E3 = (H == F) ? F : E;
						}
						else
						{
							E0 = E;
							E1 = E;
							E2 = E;
							E3 = E;
						}
						chroma = (E0 + E1 + E2 + E3);
						pv[bi + (bj * hwidth)] = chroma >> 2; //div by 4
					}
					break;
				}
			}

		}
	}
}

/*
 * distort (lens effect)
 * args:
 *    frame  - pointer to frame buffer (yu12 format)
 *    width  - frame width
 *    height - frame height
 *    box_width - central box width where distort is to be applied (if < 10 use frame width)
 *    box_height - central box height where distort is to be applied (if < 10 use frame height)
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
void fx_yu12_distort(uint8_t* frame, int width, int height, int box_width, int box_height, int type)
{
  assert(frame != NULL);

  if(!tmpbuffer)
		tmpbuffer = malloc(width * height * 3 / 2);

  memcpy(tmpbuffer, frame, width * height * 3 / 2);
  uint8_t *pu = frame + (width*height);
  uint8_t *pv = pu + (width*height)/4;
  uint8_t *tpu = tmpbuffer + (width*height);
  uint8_t *tpv = tpu + (width*height)/4;

	//index table
	uint32_t *idx_table = NULL;
	uint32_t* tb_pu = NULL;
	uint32_t* tb_pv = NULL;

  int j = 0;
  int i = 0;
	int den_x = 0;
	int den_y = 0;
  double x = 0;
  double y = 0;
  double xnew = 0;
  double ynew = 0;
	uint32_t ind = 0;

	int start_x = 0;
	int start_y = 0;

	if(box_width > 10 && width > box_width)
		start_x = (width - box_width)/2;
	else
		box_width = width;

	if(box_height > 10 && height > box_height)
		start_y = (height - box_height)/2;
	else
		box_height = height;

	//choose lookup table
	switch(type)
	{
		case REND_FX_YUV_POW_DISTORT:
			idx_table = TB_Pow_ind;
			break;

		case REND_FX_YUV_POW2_DISTORT:
			idx_table = TB_Pow2_ind;
			break;

		case REND_FX_YUV_SQRT_DISTORT:
		default:
			idx_table = TB_Sqrt_ind;
			break;
	}

	if(idx_table == NULL) //fill lookup table
	{
		idx_table = calloc(width * height * 3 / 2, sizeof(uint32_t));

		tb_pu = idx_table + (width * height);
		tb_pv  = tb_pu + (width * height)/4;

		for(j = 0; j < height; ++j)
		{
			y = normY(j, height);
			for(i = 0; i < width; ++i)
			{
				x = normX(i, width);
				eval_coordinates(x, y, &xnew, &ynew, type);

				den_x = denormX(xnew, width);
				den_y = denormY(ynew, height);

				idx_table[i + (j * width)] = den_x + (den_y * width);
			}
		}

		for(j = 0; j < height/2; ++j)
		{
			y = normY(j, height/2);
			for(i = 0; i < width/2; ++i)
			{
				x = normX(i, width/2);
				eval_coordinates(x, y, &xnew, &ynew, type);

				den_x = denormX(xnew, width/2);
				den_y = denormY(ynew, height/2);

				tb_pu[i + (j * width/2)] = den_x + (den_y * width/2);
				tb_pv[i + (j * width/2)] = den_x + (den_y * width/2);
			}
		}
		//store the table pointer in the matching global
		switch(type)
		{
			case REND_FX_YUV_POW_DISTORT:
				TB_Pow_ind = idx_table;
				break;

			case REND_FX_YUV_POW2_DISTORT:
				TB_Pow2_ind = idx_table;
				break;

			case REND_FX_YUV_SQRT_DISTORT:
			default:
				TB_Sqrt_ind = idx_table;
				break;
		}
	}

	//apply the distortion
	//(luma)
  for (j=0; j< box_height; j++)
  {
    for(i=0; i< box_width; i++)
    {
			int bi = i + start_x;
			int bj = j + start_y;

			ind = bi + (bj * box_width);

			frame[ind] = tmpbuffer[idx_table[ind]];
    }
	}
	//chroma
	tb_pu = idx_table + (width * height);
	tb_pv  = tb_pu + (width * height)/4;

	for (j=0; j< box_height/2; j++)
  {
  	for(i=0; i< box_width/2; i++)
    {
			int bi = i + start_x/2;
			int bj = j + start_y/2;

			ind = bi + (bj * box_width/2);

			pu[ind] = tpu[tb_pu[ind]];
			pv[ind] = tpv[tb_pv[ind]];
		}
	}

}

/*
 * Apply fx filters
 * args:
 *    frame - pointer to frame buffer (yu12 format)
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
            fx_yu12_mirror(frame, width, height);

        if(mask & REND_FX_YUV_HALF_MIRROR)
			fx_yu12_half_mirror (frame, width, height);

		if(mask & REND_FX_YUV_UPTURN)
            fx_yu12_upturn(frame, width, height);

		if(mask & REND_FX_YUV_HALF_UPTURN)
			fx_yu12_half_upturn(frame, width, height);

		if(mask & REND_FX_YUV_NEGATE)
			fx_yuv_negative (frame, width, height);

		if(mask & REND_FX_YUV_MONOCR)
			fx_yu12_monochrome (frame, width, height);

#ifdef HAS_GSL
		if(mask & REND_FX_YUV_PIECES)
			fx_yu12_pieces(frame, width, height, 16 );
#endif
        if(mask & REND_FX_YUV_SQRT_DISTORT)
			fx_yu12_distort(frame, width, height, 0, 0, REND_FX_YUV_SQRT_DISTORT);

		if(mask & REND_FX_YUV_POW_DISTORT)
            fx_yu12_distort(frame, width, height, 0, 0, REND_FX_YUV_POW_DISTORT);

        if(mask & REND_FX_YUV_POW2_DISTORT)
			fx_yu12_distort(frame, width, height, 0, 0, REND_FX_YUV_POW2_DISTORT);

		if(mask & REND_FX_YUV_BLUR)
			fx_yu12_gauss_blur2(frame, width, height, 2);
			//fx_yu12_gauss_blur(frame, width, height, 2);

		if(mask & REND_FX_YUV_ANTIALIAS_SCALE3X)
			fx_yu12_antialiasing(frame, width, height, 3);
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

	if(Gkernel != NULL)
	{
			free(Gkernel);
			Gkernel = NULL;
	}

	if(bSizes != NULL)
	{
			free(bSizes);
			bSizes = NULL;
	}

	if(tmpbuffer != NULL)
  {
    free(tmpbuffer);
    tmpbuffer = NULL;
  }

	if(TB_Sqrt_ind != NULL)
	{
		free(TB_Sqrt_ind);
		TB_Sqrt_ind = NULL;
	}

	if(TB_Pow_ind != NULL)
	{
		free(TB_Pow_ind);
		TB_Pow_ind = NULL;
	}

	if(TB_Pow2_ind != NULL)
	{
		free(TB_Pow2_ind);
		TB_Pow2_ind = NULL;
	}
}
