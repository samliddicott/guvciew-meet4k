/*************************************************************************************************
#	    guvcview              http://guvcview.berlios.de												#
#     Paulo Assis <pj.assis@gmail.com>																#
#																														#
# This program is free software; you can redistribute it and/or modify         				#
# it under the terms of the GNU General Public License as published by   				#
# the Free Software Foundation; either version 2 of the License, or           				#
# (at your option) any later version.                                          								#
#                                                                              										#
# This program is distributed in the hope that it will be useful,              					#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             		#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 					#
#                                                                              										#
# You should have received a copy of the GNU General Public License           		#
# along with this program; if not, write to the Free Software                  					#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA		#
#                                                                              										#
*************************************************************************************************/
typedef unsigned char BYTE;
typedef  unsigned int DWORD;
typedef  unsigned int LONG;
typedef  unsigned int UINT;
typedef  unsigned short  int WORD;

#define VERSION "0.4.1"

#define BI_RGB 0;
#define BI_RLE4 1;
#define BI_RLE8 2;
#define BI_BITFIELDS 3;

/* Fixed point arithmetic */
#define FIXED	Sint32
#define FIXED_BITS 16
#define TO_FIXED(X) (((Sint32)(X))<<(FIXED_BITS))
#define FROM_FIXED(X) (((Sint32)(X))>>(FIXED_BITS))

#define INCPANTILT 64 // 1°

#define WINSIZEX 700
#define WINSIZEY 420

#define AUTO_EXP 8
#define MAN_EXP	1

#define NB_BUFFER 4
#define DHT_SIZE 432

#define DEFAULT_WIDTH 320
#define DEFAULT_HEIGHT 240

#define DEFAULT_IMAGE_FNAME "Image.bmp"
#define DEFAULT_AVI_FNAME	"capture.avi"
#define DEFAULT_FPS	25
#define SDL_WAIT_TIME 10

/*                      */
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
//unsigned int pixel1;
//unsigned int pixel2;
BYTE y;
BYTE u;
BYTE v;
BYTE y1;
BYTE r;
BYTE g;
BYTE b;
BYTE r1;
BYTE g1;
BYTE b1;
	
} Pix;


typedef struct tagBITMAPFILEHEADER { 
  WORD    bfType; //Specifies the file type, must be BM
  DWORD   bfSize; //Specifies the size, in bytes, of the bitmap file
  WORD    bfReserved1; //Reserved; must be zero
  WORD    bfReserved2; //Reserved; must be zero
  DWORD   bfOffBits; /*Specifies the offset, in bytes, 
			    from the beginning of the BITMAPFILEHEADER structure 
			    to the bitmap bits= FileHeader+InfoHeader+RGBQUAD(0 for 24bit BMP)=64*/
}   __attribute__ ((packed)) BITMAPFILEHEADER, *PBITMAPFILEHEADER;


typedef struct tagBITMAPINFOHEADER{
  DWORD  biSize; 
  LONG   biWidth; 
  LONG   biHeight; 
  WORD   biPlanes; 
  WORD   biBitCount; 
  DWORD  biCompression; 
  DWORD  biSizeImage; 
  LONG   biXPelsPerMeter; 
  LONG   biYPelsPerMeter; 
  DWORD  biClrUsed; 
  DWORD  biClrImportant; 
}  __attribute__ ((packed)) BITMAPINFOHEADER, *PBITMAPINFOHEADER;


int jpeg_decode(unsigned char **pic, unsigned char *buf, int *width,
		int *height);
//~ int 
//~ get_picture(unsigned char *buf,int size);

Pix *yuv2rgb(unsigned int YUVMacroPix, int format, Pix *pixe);


int SaveBPM(const char *Filename, long width, long height, int BitCount, BYTE *ImagePix);
