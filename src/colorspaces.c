/*******************************************************************************#
#           guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#           Nobuhiro Iwamatsu <iwamatsu@nigauri.org>                            # 
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
#include "colorspaces.h"
#include "v4l2uvc.h"

/*------------------------------- Color space conversions --------------------*/
/* regular yuv (YUYV) to rgb24*/
void 
yuyv2rgb (BYTE *pyuv, BYTE *prgb, int width, int height)
{
	int l=0;
	int SizeYUV=height * width * 2; /* 2 bytes per pixel*/
	for(l=0;l<SizeYUV;l=l+4) 
	{	/*iterate every 4 bytes*/
		/* standart: r = y0 + 1.402 (v-128) */
		/* logitech: r = y0 + 1.370705 (v-128) */
		*prgb++=CLIP(pyuv[l] + 1.402 * (pyuv[l+3]-128));
		/* standart: g = y0 - 0.34414 (u-128) - 0.71414 (v-128)*/
		/* logitech: g = y0 - 0.337633 (u-128)- 0.698001 (v-128)*/
		*prgb++=CLIP(pyuv[l] - 0.34414 * (pyuv[l+1]-128) -0.71414*(pyuv[l+3]-128));
		/* standart: b = y0 + 1.772 (u-128) */
		/* logitech: b = y0 + 1.732446 (u-128) */
		*prgb++=CLIP(pyuv[l] + 1.772 *( pyuv[l+1]-128));
		/* standart: r1 =y1 + 1.402 (v-128) */
		/* logitech: r1 = y1 + 1.370705 (v-128) */
		*prgb++=CLIP(pyuv[l+2] + 1.402 * (pyuv[l+3]-128));
		/* standart: g1 = y1 - 0.34414 (u-128) - 0.71414 (v-128)*/
		/* logitech: g1 = y1 - 0.337633 (u-128)- 0.698001 (v-128)*/
		*prgb++=CLIP(pyuv[l+2] - 0.34414 * (pyuv[l+1]-128) -0.71414 * (pyuv[l+3]-128));
		/* standart: b1 = y1 + 1.772 (u-128) */
		/* logitech: b1 = y1 + 1.732446 (u-128) */
		*prgb++=CLIP(pyuv[l+2] + 1.772*(pyuv[l+1]-128));
	}
}

/* yuv (YUYV) to bgr with lines upsidedown */
/* used for bitmap files (DIB24)           */
void 
yuyv2bgr (BYTE *pyuv, BYTE *pbgr, int width, int height)
{

	int l=0;
	int k=0;
	BYTE *preverse;
	int bytesUsed;
	int SizeBGR=height * width * 3; /* 3 bytes per pixel*/
	/* BMP byte order is bgr and the lines start from last to first*/
	preverse=pbgr+SizeBGR;/*start at the end and decrement*/
	for(l=0;l<height;l++) 
	{	/*iterate every 1 line*/
		preverse-=width*3;/*put pointer at begin of unprocessed line*/
		bytesUsed=l*width*2;
		for (k=0;k<(width*2);k=k+4)/*iterate every 4 bytes in the line*/
		{                              
			/* standart: b = y0 + 1.772 (u-128) */
			/* logitech: b = y0 + 1.732446 (u-128) */
			*preverse++=CLIP(pyuv[k+bytesUsed] + 1.772 *( pyuv[k+1+bytesUsed]-128)); 
			/* standart: g = y0 - 0.34414 (u-128) - 0.71414 (v-128)*/
			/* logitech: g = y0 - 0.337633 (u-128)- 0.698001 (v-128)*/
			*preverse++=CLIP(pyuv[k+bytesUsed] - 0.34414 * (pyuv[k+1+bytesUsed]-128) 
				-0.71414*(pyuv[k+3+bytesUsed]-128));
			/* standart: r = y0 + 1.402 (v-128) */
			/* logitech: r = y0 + 1.370705 (v-128) */
			*preverse++=CLIP(pyuv[k+bytesUsed] + 1.402 * (pyuv[k+3+bytesUsed]-128));
			/* standart: b1 = y1 + 1.772 (u-128) */
			/* logitech: b1 = y1 + 1.732446 (u-128) */
			*preverse++=CLIP(pyuv[k+2+bytesUsed] + 1.772*(pyuv[k+1+bytesUsed]-128));
			/* standart: g1 = y1 - 0.34414 (u-128) - 0.71414 (v-128)*/
			/* logitech: g1 = y1 - 0.337633 (u-128)- 0.698001 (v-128)*/
			*preverse++=CLIP(pyuv[k+2+bytesUsed] - 0.34414 * (pyuv[k+1+bytesUsed]-128) 
				-0.71414 * (pyuv[k+3+bytesUsed]-128)); 
			/* standart: r1 =y1 + 1.402 (v-128) */
			/* logitech: r1 = y1 + 1.370705 (v-128) */
			*preverse++=CLIP(pyuv[k+2+bytesUsed] + 1.402 * (pyuv[k+3+bytesUsed]-128));
		}
		preverse-=width*3;/*get it back at the begin of processed line*/
	}
	preverse=NULL;
}

/*convert yyuv (packed) to yuyv (packed)
* args: 
*      framebuffer: pointer to frame buffer (yuyv)
*      tmpbuffer: pointer to temp buffer containing yyuv packed data frame
*      width: picture width
*      height: picture height
*/
void yyuv_to_yuyv (BYTE *framebuffer, BYTE *tmpbuffer, int width, int height)
{
	BYTE *ptmp=NULL;
	BYTE *pfmb=NULL;
	ptmp = tmpbuffer;
	pfmb = framebuffer;
	
	int h=0;
	int w=0;
	
	for(h=0;h<height;h++) 
	{
		for(w=0;w<(width*2);w+=4) 
		{
			/* Y0 */
			pfmb[0] = ptmp[0];
			/* U */
			pfmb[1] = ptmp[2];
			/* Y1 */
			pfmb[2] = ptmp[1];
			/* V */
			pfmb[3] = ptmp[3];
			
			ptmp += 4;
			pfmb += 4;
		}
	}
}


/*convert uyvy (packed) to yuyv (packed)
* args: 
*      framebuffer: pointer to frame buffer (yuyv)
*      tmpbuffer: pointer to temp buffer containing uyvy packed data frame
*      width: picture width
*      height: picture height
*/
void uyvy_to_yuyv (BYTE *framebuffer, BYTE *tmpbuffer, int width, int height)
{
	BYTE *ptmp = tmpbuffer;
	BYTE *pfmb = framebuffer;
	int h=0;
	int w=0;
	
	for(h=0;h<height;h++) 
	{
		for(w=0;w<(width*2);w+=4) 
		{
			/* Y0 */
			pfmb[0] = ptmp[1];
			/* U */
			pfmb[1] = ptmp[0];
			/* Y1 */
			pfmb[2] = ptmp[3];
			/* V */
			pfmb[3] = ptmp[2];
			
			ptmp += 4;
			pfmb += 4;
		}
	}
}


/*convert yvyu (packed) to yuyv (packed)
* args: 
*      framebuffer: pointer to frame buffer (yuyv)
*      tmpbuffer: pointer to temp buffer containing yvyu packed data frame
*      width: picture width
*      height: picture height
*/
void yvyu_to_yuyv (BYTE *framebuffer, BYTE *tmpbuffer, int width, int height)
{
	BYTE *ptmp=NULL;
	BYTE *pfmb=NULL;
	ptmp = tmpbuffer;
	pfmb = framebuffer;
	
	int h=0;
	int w=0;
	
	for(h=0;h<height;h++) 
	{
		for(w=0;w<(width*2);w+=4) 
		{
			/* Y0 */
			pfmb[0] = ptmp[0];
			/* U */
			pfmb[1] = ptmp[3];
			/* Y1 */
			pfmb[2] = ptmp[2];
			/* V */
			pfmb[3] = ptmp[1];
			
			ptmp += 4;
			pfmb += 4;
		}
	}
}

/*convert yuv 420 planar (yu12) to yuv 422
* args: 
*      framebuffer: pointer to frame buffer (yuyv)
*      tmpbuffer: pointer to temp buffer containing yuv420 planar data frame
*      width: picture width
*      height: picture height
*/
int yuv420_to_yuyv (BYTE *framebuffer, BYTE *tmpbuffer, int width, int height) 
{
	BYTE *py;
	BYTE *pu;
	BYTE *pv;
	
	int linesize = width * 2;
	int uvlinesize = width / 2;
	py=tmpbuffer;
	pu=py+(width*height);
	pv=pu+(width*height/4);
	
	int h=0;
	int w=0;
	
	int wy=0;
	int huv=0;
	int wuv=0;
	
	for(h=0;h<height;h+=2) 
	{
		wy=0;
		wuv=0;
		for(w=0;w<linesize;w+=4) 
		{
			/*y00*/
			framebuffer[h * linesize + w] = py[h * width + wy];
			/*u0*/
			framebuffer[h * linesize + (w + 1)] = pu[huv * uvlinesize + wuv];
			/*y01*/
			framebuffer[h * linesize + (w + 2)] = py[h * width + (wy + 1)];
			/*v0*/
			framebuffer[h * linesize + (w + 3)] = pv[huv * uvlinesize + wuv];
			
			/*y10*/
			framebuffer[(h + 1) * linesize + w] = py[(h + 1) * width + wy];
			/*u0*/
			framebuffer[(h + 1) * linesize + ( w + 1)] = pu[huv * uvlinesize + wuv];
			/*y11*/
			framebuffer[(h + 1) * linesize + (w + 2)] = py[(h + 1) * width + (wy + 1)];
			/*v0*/
			framebuffer[(h + 1) * linesize + (w + 3)] = pv[huv * uvlinesize + wuv];
			wuv++;
			wy+=2;
		}
		huv++;
	}
	return (0);
}

/*convert yvu 420 planar (yv12) to yuv 422
* args: 
*      framebuffer: pointer to frame buffer (yuyv)
*      tmpbuffer: pointer to temp buffer containing yuv420 planar data frame
*      width: picture width
*      height: picture height
*/
int yvu420_to_yuyv (BYTE *framebuffer, BYTE *tmpbuffer, int width, int height) 
{
	BYTE *py;
	BYTE *pv;
	BYTE *pu;
	
	int linesize = width * 2;
	int uvlinesize = width / 2;
	py=tmpbuffer;
	pv=py+(width*height);
	pu=pv+((width*height)/4);
	
	int h=0;
	int w=0;
	
	int wy=0;
	int huv=0;
	int wuv=0;
	
	for(h=0;h<height;h+=2) 
	{
		wy=0;
		wuv=0;
		for(w=0;w<linesize;w+=4) 
		{
			/*y00*/
			framebuffer[h * linesize + w] = py[h * width + wy];
			/*u0*/
			framebuffer[h * linesize + (w + 1)] = pu[huv * uvlinesize + wuv];
			/*y01*/
			framebuffer[h * linesize + (w + 2)] = py[h * width + (wy + 1)];
			/*v0*/
			framebuffer[h * linesize + (w + 3)] = pv[huv * uvlinesize + wuv];
			
			/*y10*/
			framebuffer[(h + 1) * linesize + w] = py[(h + 1) * width + wy];
			/*u0*/
			framebuffer[(h + 1) * linesize + (w + 1)] = pu[huv * uvlinesize + wuv];
			/*y11*/
			framebuffer[(h + 1) * linesize + (w + 2)] = py[(h + 1) * width + (wy + 1)];
			/*v0*/
			framebuffer[(h + 1) * linesize + (w + 3)] = pv[huv * uvlinesize + wuv];
			wuv++;
			wy+=2;
		}
		huv++;
	}
	return (0);
}

// raw bayer functions 
// from libv4l bayer.c, (C) 2008 Hans de Goede <j.w.r.degoede@hhs.nl>
//Note: original bayer_to_bgr24 code from :
//  1394-Based Digital Camera Control Library
// 
//  Bayer pattern decoding functions
// 
//  Written by Damien Douxchamps and Frederic Devernay
static void convert_border_bayer_line_to_bgr24( BYTE* bayer, BYTE* adjacent_bayer,
	BYTE *bgr, int width, gboolean start_with_green, gboolean blue_line)
{
	int t0, t1;

	if (start_with_green) 
	{
	/* First pixel */
		if (blue_line) 
		{
			*bgr++ = bayer[1];
			*bgr++ = bayer[0];
			*bgr++ = adjacent_bayer[0];
		} 
		else 
		{
			*bgr++ = adjacent_bayer[0];
			*bgr++ = bayer[0];
			*bgr++ = bayer[1];
		}
		/* Second pixel */
		t0 = (bayer[0] + bayer[2] + adjacent_bayer[1] + 1) / 3;
		t1 = (adjacent_bayer[0] + adjacent_bayer[2] + 1) >> 1;
		if (blue_line) 
		{
			*bgr++ = bayer[1];
			*bgr++ = t0;
			*bgr++ = t1;
		} 
		else 
		{
			*bgr++ = t1;
			*bgr++ = t0;
			*bgr++ = bayer[1];
		}
		bayer++;
		adjacent_bayer++;
		width -= 2;
	} 
	else 
	{
		/* First pixel */
		t0 = (bayer[1] + adjacent_bayer[0] + 1) >> 1;
		if (blue_line) 
		{
			*bgr++ = bayer[0];
			*bgr++ = t0;
			*bgr++ = adjacent_bayer[1];
		} 
		else 
		{
			*bgr++ = adjacent_bayer[1];
			*bgr++ = t0;
			*bgr++ = bayer[0];
		}
		width--;
	}

	if (blue_line) 
	{
		for ( ; width > 2; width -= 2) 
		{
			t0 = (bayer[0] + bayer[2] + 1) >> 1;
			*bgr++ = t0;
			*bgr++ = bayer[1];
			*bgr++ = adjacent_bayer[1];
			bayer++;
			adjacent_bayer++;

			t0 = (bayer[0] + bayer[2] + adjacent_bayer[1] + 1) / 3;
			t1 = (adjacent_bayer[0] + adjacent_bayer[2] + 1) >> 1;
			*bgr++ = bayer[1];
			*bgr++ = t0;
			*bgr++ = t1;
			bayer++;
			adjacent_bayer++;
		}
	} 
	else 
	{
		for ( ; width > 2; width -= 2) 
		{
			t0 = (bayer[0] + bayer[2] + 1) >> 1;
			*bgr++ = adjacent_bayer[1];
			*bgr++ = bayer[1];
			*bgr++ = t0;
			bayer++;
			adjacent_bayer++;

			t0 = (bayer[0] + bayer[2] + adjacent_bayer[1] + 1) / 3;
			t1 = (adjacent_bayer[0] + adjacent_bayer[2] + 1) >> 1;
			*bgr++ = t1;
			*bgr++ = t0;
			*bgr++ = bayer[1];
			bayer++;
			adjacent_bayer++;
		}
	}

	if (width == 2) 
	{
		/* Second to last pixel */
		t0 = (bayer[0] + bayer[2] + 1) >> 1;
		if (blue_line) 
		{
			*bgr++ = t0;
			*bgr++ = bayer[1];
			*bgr++ = adjacent_bayer[1];
		} 
		else 
		{
			*bgr++ = adjacent_bayer[1];
			*bgr++ = bayer[1];
			*bgr++ = t0;
		}
		/* Last pixel */
		t0 = (bayer[1] + adjacent_bayer[2] + 1) >> 1;
		if (blue_line) 
		{
			*bgr++ = bayer[2];
			*bgr++ = t0;
			*bgr++ = adjacent_bayer[1];
		}
		else 
		{
			*bgr++ = adjacent_bayer[1];
			*bgr++ = t0;
			*bgr++ = bayer[2];
		}
	} 
	else 
	{
		/* Last pixel */
		if (blue_line) 
		{
			*bgr++ = bayer[0];
			*bgr++ = bayer[1];
			*bgr++ = adjacent_bayer[1];
		} 
		else 
		{
			*bgr++ = adjacent_bayer[1];
			*bgr++ = bayer[1];
			*bgr++ = bayer[0];
		}
	}
}

/* From libdc1394, which on turn was based on OpenCV's Bayer decoding */
static void bayer_to_rgbbgr24(BYTE *bayer,
	BYTE *bgr, int width, int height,
	gboolean start_with_green, gboolean blue_line)
{
	/* render the first line */
	convert_border_bayer_line_to_bgr24(bayer, bayer + width, bgr, width,
		start_with_green, blue_line);
	bgr += width * 3;

	/* reduce height by 2 because of the special case top/bottom line */
	for (height -= 2; height; height--) 
	{
		int t0, t1;
		/* (width - 2) because of the border */
		BYTE *bayerEnd = bayer + (width - 2);

		if (start_with_green) 
		{
			/* OpenCV has a bug in the next line, which was
			t0 = (bayer[0] + bayer[width * 2] + 1) >> 1; */
			t0 = (bayer[1] + bayer[width * 2 + 1] + 1) >> 1;
			/* Write first pixel */
			t1 = (bayer[0] + bayer[width * 2] + bayer[width + 1] + 1) / 3;
			if (blue_line) 
			{
				*bgr++ = t0;
				*bgr++ = t1;
				*bgr++ = bayer[width];
			} 
			else 
			{
				*bgr++ = bayer[width];
				*bgr++ = t1;
				*bgr++ = t0;
			}

			/* Write second pixel */
			t1 = (bayer[width] + bayer[width + 2] + 1) >> 1;
			if (blue_line) 
			{
				*bgr++ = t0;
				*bgr++ = bayer[width + 1];
				*bgr++ = t1;
			} 
			else 
			{
				*bgr++ = t1;
				*bgr++ = bayer[width + 1];
				*bgr++ = t0;
			}
			bayer++;
		} 
		else 
		{
			/* Write first pixel */
			t0 = (bayer[0] + bayer[width * 2] + 1) >> 1;
			if (blue_line) 
			{
				*bgr++ = t0;
				*bgr++ = bayer[width];
				*bgr++ = bayer[width + 1];
			} 
			else 
			{
				*bgr++ = bayer[width + 1];
				*bgr++ = bayer[width];
				*bgr++ = t0;
			}
		}

		if (blue_line) 
		{
			for (; bayer <= bayerEnd - 2; bayer += 2) 
			{
				t0 = (bayer[0] + bayer[2] + bayer[width * 2] +
					bayer[width * 2 + 2] + 2) >> 2;
				t1 = (bayer[1] + bayer[width] +
					bayer[width + 2] + bayer[width * 2 + 1] +
					2) >> 2;
				*bgr++ = t0;
				*bgr++ = t1;
				*bgr++ = bayer[width + 1];

				t0 = (bayer[2] + bayer[width * 2 + 2] + 1) >> 1;
				t1 = (bayer[width + 1] + bayer[width + 3] +
					1) >> 1;
				*bgr++ = t0;
				*bgr++ = bayer[width + 2];
				*bgr++ = t1;
			}
		} 
		else 
		{
			for (; bayer <= bayerEnd - 2; bayer += 2) 
			{
				t0 = (bayer[0] + bayer[2] + bayer[width * 2] +
					bayer[width * 2 + 2] + 2) >> 2;
				t1 = (bayer[1] + bayer[width] +
					bayer[width + 2] + bayer[width * 2 + 1] +
					2) >> 2;
				*bgr++ = bayer[width + 1];
				*bgr++ = t1;
				*bgr++ = t0;

				t0 = (bayer[2] + bayer[width * 2 + 2] + 1) >> 1;
				t1 = (bayer[width + 1] + bayer[width + 3] +
					1) >> 1;
				*bgr++ = t1;
				*bgr++ = bayer[width + 2];
				*bgr++ = t0;
			}
		}

		if (bayer < bayerEnd) 
		{
			/* write second to last pixel */
			t0 = (bayer[0] + bayer[2] + bayer[width * 2] +
				bayer[width * 2 + 2] + 2) >> 2;
			t1 = (bayer[1] + bayer[width] +
				bayer[width + 2] + bayer[width * 2 + 1] +
				2) >> 2;
			if (blue_line) 
			{
				*bgr++ = t0;
				*bgr++ = t1;
				*bgr++ = bayer[width + 1];
			} 
			else 
			{
				*bgr++ = bayer[width + 1];
				*bgr++ = t1;
				*bgr++ = t0;
			}
			/* write last pixel */
			t0 = (bayer[2] + bayer[width * 2 + 2] + 1) >> 1;
			if (blue_line) 
			{
				*bgr++ = t0;
				*bgr++ = bayer[width + 2];
				*bgr++ = bayer[width + 1];
			} 
			else 
			{
				*bgr++ = bayer[width + 1];
				*bgr++ = bayer[width + 2];
				*bgr++ = t0;
			}
			bayer++;
		} 
		else
		{
			/* write last pixel */
			t0 = (bayer[0] + bayer[width * 2] + 1) >> 1;
			t1 = (bayer[1] + bayer[width * 2 + 1] + bayer[width] + 1) / 3;
			if (blue_line) 
			{
				*bgr++ = t0;
				*bgr++ = t1;
				*bgr++ = bayer[width + 1];
			} 
			else 
			{
				*bgr++ = bayer[width + 1];
				*bgr++ = t1;
				*bgr++ = t0;
			}
		}

		/* skip 2 border pixels */
		bayer += 2;

		blue_line = !blue_line;
		start_with_green = !start_with_green;
	}

	/* render the last line */
	convert_border_bayer_line_to_bgr24(bayer + width, bayer, bgr, width,
		!start_with_green, !blue_line);
}

/*convert bayer raw data to rgb24
* args: 
*      pBay: pointer to buffer containing Raw bayer data data
*      pRGB24: pointer to buffer containing rgb24 data
*      width: picture width
*      height: picture height
*      pix_order: bayer pixel order (0=gb/rg   1=gr/bg  2=bg/gr  3=rg/bg)
*/
void 
bayer_to_rgb24(BYTE *pBay, BYTE *pRGB24, int width, int height, int pix_order)
{
	switch (pix_order) 
	{
		//conversion functions are build for bgr, by switching b and r lines we get rgb
		case 0: /* gbgbgb... | rgrgrg... (V4L2_PIX_FMT_SGBRG8)*/
			bayer_to_rgbbgr24(pBay, pRGB24, width, height, TRUE, FALSE);
			break;
		
		case 1: /* grgrgr... | bgbgbg... (V4L2_PIX_FMT_SGRBG8)*/
			bayer_to_rgbbgr24(pBay, pRGB24, width, height, TRUE, TRUE);
			break;
		
		case 2: /* bgbgbg... | grgrgr... (V4L2_PIX_FMT_SBGGR8)*/
			bayer_to_rgbbgr24(pBay, pRGB24, width, height, FALSE, FALSE);
			break;
		
		case 3: /* rgrgrg... ! gbgbgb... (V4L2_PIX_FMT_SRGGB8)*/
			bayer_to_rgbbgr24(pBay, pRGB24, width, height, FALSE, TRUE);
			break;
			
		default: /* default is 0*/
			bayer_to_rgbbgr24(pBay, pRGB24, width, height, TRUE, FALSE);
			break;
	}
}


void
rgb2yuyv(BYTE *prgb, BYTE *pyuv, int width, int height) 
{

	int i=0;
	for(i=0;i<(width*height*3);i=i+6) 
	{
		/* y */ 
		*pyuv++ =CLIP(0.299 * (prgb[i] - 128) + 0.587 * (prgb[i+1] - 128) + 0.114 * (prgb[i+2] - 128) + 128);
		/* u */
		*pyuv++ =CLIP(((- 0.147 * (prgb[i] - 128) - 0.289 * (prgb[i+1] - 128) + 0.436 * (prgb[i+2] - 128) + 128) +
			(- 0.147 * (prgb[i+3] - 128) - 0.289 * (prgb[i+4] - 128) + 0.436 * (prgb[i+5] - 128) + 128))/2);
		/* y1 */ 
		*pyuv++ =CLIP(0.299 * (prgb[i+3] - 128) + 0.587 * (prgb[i+4] - 128) + 0.114 * (prgb[i+5] - 128) + 128); 
		/* v*/
		*pyuv++ =CLIP(((0.615 * (prgb[i] - 128) - 0.515 * (prgb[i+1] - 128) - 0.100 * (prgb[i+2] - 128) + 128) +
			(0.615 * (prgb[i+3] - 128) - 0.515 * (prgb[i+4] - 128) - 0.100 * (prgb[i+5] - 128) + 128))/2);
	}
}

void
bgr2yuyv(BYTE *pbgr, BYTE *pyuv, int width, int height) 
{

	int i=0;
	for(i=0;i<(width*height*3);i=i+6) 
	{
		/* y */ 
		*pyuv++ =CLIP(0.299 * (pbgr[i+2] - 128) + 0.587 * (pbgr[i+1] - 128) + 0.114 * (pbgr[i] - 128) + 128);
		/* u */
		*pyuv++ =CLIP(((- 0.147 * (pbgr[i+2] - 128) - 0.289 * (pbgr[i+1] - 128) + 0.436 * (pbgr[i] - 128) + 128) +
			(- 0.147 * (pbgr[i+5] - 128) - 0.289 * (pbgr[i+4] - 128) + 0.436 * (pbgr[i+3] - 128) + 128))/2);
		/* y1 */ 
		*pyuv++ =CLIP(0.299 * (pbgr[i+5] - 128) + 0.587 * (pbgr[i+4] - 128) + 0.114 * (pbgr[i+3] - 128) + 128); 
		/* v*/
		*pyuv++ =CLIP(((0.615 * (pbgr[i+2] - 128) - 0.515 * (pbgr[i+1] - 128) - 0.100 * (pbgr[i] - 128) + 128) +
			(0.615 * (pbgr[i+5] - 128) - 0.515 * (pbgr[i+4] - 128) - 0.100 * (pbgr[i+3] - 128) + 128))/2);
	}
}

/*use in utils.c for jpeg decoding  420 planar to 422
* args: 
*      out: pointer to data output of idct (macroblocks yyyy u v)
*      pic: pointer to picture buffer (yuyv)
*      width: picture width
*/
void yuv420pto422(int * out,unsigned char *pic,int width)
{
	int j, k;
	unsigned char *pic0, *pic1;
	int *outy, *outu, *outv;
	int outy1 = 0;
	int outy2 = 8;

	//yyyyuv
	pic0 = pic;
	pic1 = pic + width;
	outy = out;
	outu = out + 64 * 4;
	outv = out + 64 * 5;    
	for (j = 0; j < 8; j++) 
	{
		for (k = 0; k < 8; k++)
		{
			if( k == 4) 
			{ 
				outy1 += 56;
				outy2 += 56;
			}
			*pic0++ = CLIP(outy[outy1]);   //y1 line 1
			*pic0++ = CLIP(128 + *outu);   //u  line 1-2
			*pic0++ = CLIP(outy[outy1+1]); //y2 line 1
			*pic0++ = CLIP(128 + *outv);   //v  line 1-2
			*pic1++ = CLIP(outy[outy2]);   //y1 line 2
			*pic1++ = CLIP(128 + *outu);   //u  line 1-2
			*pic1++ = CLIP(outy[outy2+1]); //y2 line 2
			*pic1++ = CLIP(128 + *outv);   //v  line 1-2
			outy1 +=2; outy2 += 2; outu++; outv++;
		}
		if(j==3)
		{
			outy = out + 128;
		} 
		else 
		{
			outy += 16;
		}
		outy1 = 0;
		outy2 = 8;
		pic0 += 2 * (width -16);
		pic1 += 2 * (width -16);
	}
}

/*use in utils.c for jpeg decoding 422 planar to 422
* args: 
*      out: pointer to data output of idct (macroblocks yyyy u v)
*      pic: pointer to picture buffer (yuyv)
*      width: picture width
*/
void yuv422pto422(int * out,unsigned char *pic,int width)
{
	int j, k;
	unsigned char *pic0, *pic1;
	int *outy, *outu, *outv;
	int outy1 = 0;
	int outy2 = 8;
	int outu1 = 0;
	int outv1 = 0;
 
	//yyyyuv
	pic0 = pic;
	pic1 = pic + width;
	outy = out;
	outu = out + 64 * 4;
	outv = out + 64 * 5;    
	for (j = 0; j < 4; j++) 
	{
		for (k = 0; k < 8; k++) 
		{
			if( k == 4)
			{ 
				outy1 += 56;
				outy2 += 56;
			}
			*pic0++ = CLIP(outy[outy1]);        //y1 line 1
			*pic0++ = CLIP(128 + outu[outu1]);  //u  line 1
			*pic0++ = CLIP(outy[outy1+1]);      //y2 line 1
			*pic0++ = CLIP(128 + outv[outv1]);  //v  line 1
			*pic1++ = CLIP(outy[outy2]);        //y1 line 2
			*pic1++ = CLIP(128 + outu[outu1+8]);//u  line 2
			*pic1++ = CLIP(outy[outy2+1]);      //y2 line 2
			*pic1++ = CLIP(128 + outv[outv1+8]);//v  line 2
			outv1 += 1; outu1 += 1;
			outy1 +=2; outy2 +=2;
		}
		outy += 16;outu +=8; outv +=8;
		outv1 = 0; outu1=0;
		outy1 = 0;
		outy2 = 8;
		pic0 += 2 * (width -16);
		pic1 += 2 * (width -16);
	}
}

/*use in utils.c for jpeg decoding 444 planar to 422
* args: 
*      out: pointer to data output of idct (macroblocks yyyy u v)
*      pic: pointer to picture buffer (yuyv)
*      width: picture width
*/
void yuv444pto422(int * out,unsigned char *pic,int width)
{
	int j, k;
	unsigned char *pic0, *pic1;
	int *outy, *outu, *outv;
	int outy1 = 0;
	int outy2 = 8;
	int outu1 = 0;
	int outv1 = 0;

	//yyyyuv
	pic0 = pic;
	pic1 = pic + width;
	outy = out;
	outu = out + 64 * 4; // Ooops where did i invert ??
	outv = out + 64 * 5;    
	for (j = 0; j < 4; j++) 
	{
		for (k = 0; k < 4; k++) 
		{
			*pic0++ =CLIP( outy[outy1]);        //y1 line 1
			*pic0++ =CLIP( 128 + outu[outu1]);  //u  line 1
			*pic0++ =CLIP( outy[outy1+1]);      //y2 line 1
			*pic0++ =CLIP( 128 + outv[outv1]);  //v  line 1
			*pic1++ =CLIP( outy[outy2]);        //y1 line 2
			*pic1++ =CLIP( 128 + outu[outu1+8]);//u  line 2
			*pic1++ =CLIP( outy[outy2+1]);      //y2 line 2
			*pic1++ =CLIP( 128 + outv[outv1+8]);//v  line 2
			outv1 += 2; outu1 += 2;
			outy1 +=2; outy2 +=2;
		}
		outy += 16;outu +=16; outv +=16;
		outv1 = 0; outu1=0;
		outy1 = 0;
		outy2 = 8;
		pic0 += 2 * (width -8);
		pic1 += 2 * (width -8);
	}
}

/*use in utils.c for jpeg decoding 400 planar to 422
* args: 
*      out: pointer to data output of idct (macroblocks yyyy )
*      pic: pointer to picture buffer (yuyv)
*      width: picture width
*/
void yuv400pto422(int * out,unsigned char *pic,int width)
{
	int j, k;
	unsigned char *pic0, *pic1;
	int *outy ;
	int outy1 = 0;
	int outy2 = 8;
	pic0 = pic;
	pic1 = pic + width;
	outy = out;

	//yyyy
	for (j = 0; j < 4; j++) 
	{
		for (k = 0; k < 4; k++) 
		{
			*pic0++ = CLIP(outy[outy1]);  //y1 line 1
			*pic0++ = 128 ;               //u
			*pic0++ = CLIP(outy[outy1+1]);//y2 line 1
			*pic0++ = 128 ;               //v
			*pic1++ = CLIP(outy[outy2]);  //y1 line 2
			*pic1++ = 128 ;               //u
			*pic1++ = CLIP(outy[outy2+1]);//y2 line 2
			*pic1++ = 128 ;               //v
			outy1 +=2; outy2 +=2;  
		}
		outy += 16;
		outy1 = 0;
		outy2 = 8;
		pic0 += 2 * (width -8);
		pic1 += 2 * (width -8);
	}
}

