/*******************************************************************************
#	    guvcview              http://berlios																				#
#     Paulo Assis <pj.assis@gmail.com>																		#
#																																		#
# This program is free software; you can redistribute it and/or modify         				#
# it under the terms of the GNU General Public License as published by         				#
# the Free Software Foundation; either version 2 of the License, or            					#
# (at your option) any later version.                                          										#
#                                                                              														#
# This program is distributed in the hope that it will be useful,              						#
# but WITHOUT ANY WARRANTY; without even the implied warranty of               		#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                		#
# GNU General Public License for more details.                                 							#
#                                                                              														#
# You should have received a copy of the GNU General Public License            			#
# along with this program; if not, write to the Free Software                  						#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    			#
#                                                                              														#
*******************************************************************************/

#define ERR_NO_SOI 1
#define ERR_NOT_8BIT 2
#define ERR_HEIGHT_MISMATCH 3
#define ERR_WIDTH_MISMATCH 4
#define ERR_BAD_WIDTH_OR_HEIGHT 5
#define ERR_TOO_MANY_COMPPS 6
#define ERR_ILLEGAL_HV 7
#define ERR_QUANT_TABLE_SELECTOR 8
#define ERR_NOT_YCBCR_221111 9
#define ERR_UNKNOWN_CID_IN_SCAN 10
#define ERR_NOT_SEQUENTIAL_DCT 11
#define ERR_WRONG_MARKER 12
#define ERR_NO_EOI 13
#define ERR_BAD_TABLES 14
#define ERR_DEPTH_MISMATCH 15

typedef struct _Pix {
unsigned int pixel1;
unsigned int pixel2;
unsigned char y;
unsigned char u;
unsigned char v;
unsigned char y1;
unsigned char r;
unsigned char g;
unsigned char b;
unsigned char r1;
unsigned char g1;
unsigned char b1;
	
} Pix;

int jpeg_decode(unsigned char **pic, unsigned char *buf, int *width,
		int *height);
//~ int 
//~ get_picture(unsigned char *buf,int size);

Pix *yuv2rgb(unsigned int YUVMacroPix, int format, Pix *pixe);
