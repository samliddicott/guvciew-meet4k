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
typedef  unsigned short int WORD;

#define VERSION "0.5.3"

/*portaudio defs*/
#define SAMPLE_RATE  (0) /* 0 device default*/
#define FRAMES_PER_BUFFER (4096)
#define NUM_SECONDS     (2) /*allways writes 2 second bloks*/
/* sound can go for more 2 seconds than video, must clip in post processing*/

#define NUM_CHANNELS    (0) /* 0-device default 1-mono 2-stereo */
/* #define DITHER_FLAG     (paDitherOff)  */
#define DITHER_FLAG     (0) /**/

/* Select sample format to 16bit. */
#if 0
#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"
#elif 1
#define PA_SAMPLE_TYPE  paInt16 /* Default */
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#elif 0
#define PA_SAMPLE_TYPE  paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#else
#define PA_SAMPLE_TYPE  paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE  (128)
#define PRINTF_S_FORMAT "%d"
#endif

/*video defs*/
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

#define DEFAULT_IMAGE_FNAME "Image.jpg"
#define DEFAULT_BMP_FNAME "Image.bmp"
#define IMGTYPE (0) /*default type (varies if on MJPEG(jpg) or YUYV(bmp) input)*/
#define DEFAULT_AVI_FNAME	"capture.avi"
#define DEFAULT_FPS	25
#define DEFAULT_FPS_NUM 1
#define SDL_WAIT_TIME 30 /*SDL - Thread loop sleep time */
#define SND_WAIT_TIME 40 /*sound capture- Thread loop sleep time*/
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

#define CLIP(color) (unsigned char)(((color)>0xFF)?0xff:(((color)<0)?0:(color)))


typedef struct _sndDev {
 int id;
 int chan;
 int samprate;
 //PaTime Hlatency;
 //PaTime Llatency;
} sndDev;

/* 0 is device default*/
static int stdSampleRates[] = { 0, 8000,  9600, 11025, 12000,
                                   16000, 22050, 24000,
                                   32000, 44100, 48000,
                                   88200, 96000,
                                   -1 }; /* Negative terminated list. */

//~ typedef struct _Pix {
//~ //unsigned int pixel1;
//~ //unsigned int pixel2;
//~ BYTE y;
//~ BYTE u;
//~ BYTE v;
//~ BYTE y1;
//~ BYTE r;
//~ BYTE g;
//~ BYTE b;
//~ BYTE r1;
//~ BYTE g1;
//~ BYTE b1;
	
//~ } Pix;


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

typedef struct tagJPGFILEHEADER {
	BYTE SOI[2];/*SOI Marker 0xFFD8*/
	BYTE APP0[2];/*APP0 MARKER 0xFF0E*/
	BYTE length[2];/*length of header without APP0 in bytes*/
	BYTE JFIF[5];/*set to JFIF0 0x4A46494600*/
	BYTE VERS[2];/*1-2 0x0102*/
	BYTE density;/*0 - No units, aspect ratio only specified
				   1 - Pixels per Inch on quickcam5000pro
				   2 - Pixels per Centimetre                */
	BYTE xdensity[2];/*120 on quickcam5000pro*/
	BYTE ydensity[2];/*120 on quickcam5000pro*/
	BYTE WTN;/*width Thumbnail 0*/
	BYTE HTN;/*height Thumbnail 0*/	
} __attribute__ ((packed)) JPGFILEHEADER, *PJPGFILEHEADER;

int 
SaveJPG(const char *Filename,int imgsize,BYTE *ImagePix);
//~ int 
//~ get_picture(unsigned char *buf,int size);

//~Pix *yuv2rgb(unsigned int YUVMacroPix, int format, Pix *pixe);


int SaveBPM(const char *Filename, long width, long height, int BitCount, BYTE *ImagePix);
