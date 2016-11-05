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


uint8_t *tmpbuffer = NULL;

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

#define SIGN(x)     ((x > 0) ? 1: -1)

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

            *xnew = radius * fast_cos(phi);
            *ynew = radius * fast_sin(phi);
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

            *xnew = radius * fast_cos(phi);
            *ynew = radius * fast_sin(phi);
            break;

    }
}

/*
 * distort (lens effect)
 * args:
 *    frame  - pointer to frame buffer (yu12 format)
 *    width  - frame width
 *    height - frame height
 *
 * asserts:
 *    frame is not null
 *
 * returns: void
 */
void fx_yu12_distort(uint8_t* frame, int width, int height, int type)
{
    assert(frame != NULL);

    if(!tmpbuffer)
        tmpbuffer = malloc(width * height * 3 / 2);

    memcpy(tmpbuffer, frame, width * height * 3 / 2);
    uint8_t *pu = frame + (width*height);
    uint8_t *pv = pu + (width*height)/4;
    uint8_t *tpu = tmpbuffer + (width*height);
    uint8_t *tpv = tpu + (width*height)/4;
    int j = 0;
    int i = 0;
    double x = 0;
    double y = 0;
    double xnew = 0;
    double ynew = 0;


    for (j=0; j< height; j++)
    {
        y = normY(j, height);

        for(i=0; i< width; i++)
        {
            x = normX(i, width);

            eval_coordinates(x, y, &xnew, &ynew, type);

            //get luma
            frame[i + (j * width)] = 
                tmpbuffer[denormX(xnew, width) + (denormY(ynew, height) * width)];

            if((i % 2 == 0) && (j % 2 == 0))
            {
                //get U
                pu[i/2 + (j * width/4)] = 
                    tpu[denormX(xnew, width/2) + (denormY(ynew, height/2) * (width/2))];
                //get V
                pv[i/2 + (j * width/4)] = 
                    tpv[denormX(xnew, width/2) + (denormY(ynew, height/2) * (width/2))];
            }
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
                    fx_yu12_distort(frame, width, height, REND_FX_YUV_SQRT_DISTORT);

                if(mask & REND_FX_YUV_POW_DISTORT)
                    fx_yu12_distort(frame, width, height, REND_FX_YUV_POW_DISTORT);
                
                if(mask & REND_FX_YUV_POW2_DISTORT)
                    fx_yu12_distort(frame, width, height, REND_FX_YUV_POW2_DISTORT);
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
	
	if(tmpbuffer != NULL)
        {
            free(tmpbuffer);
            tmpbuffer = NULL;
        }
}
