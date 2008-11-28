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

#ifndef COLORSPACES_H
#define COLORSPACES_H

#include "defs.h"

int 
yuv420_to_yuyv (BYTE *framebuffer, BYTE *tmpbuffer, int width, int height);

/* regular yuv (YUYV) to rgb24*/
void 
yuyv2rgb (BYTE *pyuv, BYTE *prgb, int width, int height);

/* regular yuv (UYVY) to rgb24*/
void 
uyvy2rgb (BYTE *pyuv, BYTE *prgb, int width, int height);

/* yuv (YUYV) to bgr with lines upsidedown */
/* used for bitmap files (DIB24)           */
void 
yuyv2bgr (BYTE *pyuv, BYTE *pbgr, int width, int height);

/* yuv (UYVY) to bgr with lines upsidedown */
/* used for bitmap files (DIB24)           */
void 
uyvy2bgr (BYTE *pyuv, BYTE *pbgr, int width, int height);

void 
bayer_to_rgb24(BYTE *pBay, BYTE *pRGB24, int width, int height, int pix_order);

void
rgb2yuyv(BYTE *prgb, BYTE *pyuv, int width, int height);

/* Flip YUYV frame - horizontal*/
void 
yuyv_mirror (BYTE *frame, int width, int height);

/* Flip UYVY frame - horizontal*/
void 
uyvy_mirror (BYTE *frame, int width, int height);

/* Flip YUYV and UYVY frame - vertical*/
void  
yuyv_upturn(BYTE* frame, int width, int height);

/* negate YUYV and UYVY frame */
void 
yuyv_negative(BYTE* frame, int width, int height);

/* monochrome effect for YUYV frame */
void 
yuyv_monochrome(BYTE* frame, int width, int height);

/* monochrome effect for UYVY frame */
void 
uyvy_monochrome(BYTE* frame, int width, int height);

#endif

