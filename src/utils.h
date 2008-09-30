/*******************************************************************************#
#	    guvcview              http://guvcview.berlios.de                    #
#                                                                               #
#           Paulo Assis <pj.assis@gmail.com>                                    #
#										#
# This program is free software; you can redistribute it and/or modify         	#
# it under the terms of the GNU General Public License as published by   	#
# the Free Software Foundation; either version 2 of the License, or           	#
# (at your option) any later version.                                          	#
#                                                                              	#
# This program is distributed in the hope that it will be useful,              	#
# but WITHOUT ANY WARRANTY; without even the implied warranty of             	#
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the  		#
# GNU General Public License for more details.                                 	#
#                                                                              	#
# You should have received a copy of the GNU General Public License           	#
# along with this program; if not, write to the Free Software                  	#
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA	#
#                                                                              	#
********************************************************************************/

/*******************************************************************************#
#                                                                               #
#  MJpeg decoding and frame capture taken from luvcview                         #
#                                                                               # 
#                                                                               #
********************************************************************************/

typedef unsigned char BYTE;
typedef  unsigned int DWORD;
typedef  unsigned int LONG;
typedef  unsigned int UINT;
typedef  unsigned long ULONG;
typedef  unsigned short int WORD;

/*----------- guvcview version ----------------*/
//#define VERSION ("") /*defined in config.h*/
#define DEBUG (0)
/*---------- Thread Stack Size (bytes) --------*/
#define TSTACK (128*64*1024) /* Debian Default 128 pages of 64k = 8388608 bytes*/

/*----------- AVI max file size ---------------*/
#define AVI_MAX_SIZE 2000000000

/*------------- portaudio defs ----------------*/
/*---- can be override in rc file or GUI ------*/

#define SAMPLE_RATE  (0) /* 0 device default*/
//#define FRAMES_PER_BUFFER (4096)

#define NUM_SECONDS     (2) /* captures 2 second bloks */
/* sound can go for more 2 seconds than video          */

#define BUFF_FACTOR		(2) /* audio buffer Multiply factor */
/*Audio Buffer = audio block frames total size x Buff_factor   */

#define NUM_CHANNELS    (0) /* 0-device default 1-mono 2-stereo */

//#define DITHER_FLAG     (paDitherOff) 
#define DITHER_FLAG     (0) 

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

#define WINSIZEX 575
#define WINSIZEY 610

#define AUTO_EXP 8
#define MAN_EXP	1

#define DHT_SIZE 432

#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480

#define DEFAULT_IMAGE_FNAME	"Image.jpg"
#define DEFAULT_AVI_FNAME	"capture.avi"
#define DEFAULT_FPS	25
#define DEFAULT_FPS_NUM 1
#define SDL_WAIT_TIME 30 /*SDL - Thread loop sleep time */
#define SND_WAIT_TIME 5 /*sound capture- Thread loop sleep time*/
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

/*FILTER FLAGS*/
#define YUV_NOFILT 0x0000
#define YUV_MIRROR 0x0001
#define YUV_UPTURN 0x0002
#define YUV_NEGATE 0x0004
#define YUV_MONOCR 0x0008

typedef struct _sndDev {
 int id;
 int chan;
 int samprate;
 //PaTime Hlatency;
 //PaTime Llatency;
} sndDev;

/* 0 is device default*/
static const int stdSampleRates[] = { 0, 8000,  9600, 11025, 12000,
					16000, 22050, 24000,
					32000, 44100, 48000,
					88200, 96000,
					-1 }; /* Negative terminated list. */

typedef char* pchar;


/*global variables used in guvcview*/
struct GLOBAL {
    	int debug;
	char *videodevice;
	int stack_size;
	char *confPath;
	pchar* aviFPath;
	pchar* imgFPath;
	//~ char *sndfile; /*temporary snd filename*/
	char *avifile; /*avi filename passed through argument options with -n */
	pchar* profile_FPath;
	char *WVcaption; /*video preview title bar caption*/
	char *imageinc_str; /*label for File inc*/
	int vid_sleep;
	int Capture_time; /*avi capture time passed through argument options with -t */
	int lprofile; /* flag for command line -l option */ 
	int imgFormat;
	int AVIFormat; /*0-"MJPG"  1-"YUY2" 2-"DIB "(rgb32)*/
    	long AVI_MAX_LEN;
	DWORD snd_begintime;/*begin time for audio capture*/
	DWORD currtime;
	DWORD lasttime;
	DWORD AVIstarttime;
	DWORD AVIstoptime;
	DWORD framecount;
	unsigned char frmrate;
	short Sound_enable; /*Enable Sound by Default*/
	int Sound_SampRate;
	int Sound_SampRateInd;
	int Sound_numInputDev;
	sndDev Sound_IndexDev[20]; /*up to 20 input devices (should be alocated dinamicly like controls)*/
	int Sound_DefDev; 
	int Sound_UseDev;
	int Sound_NumChan;
	int Sound_NumChanInd;
	int Sound_NumSec;
	short Sound_BuffFactor;
    	SAMPLE *avi_sndBuff;
	int snd_numBytes; 
    	short audio_flag;/*audio flag used for avi writes*/
	int PanStep;/*step angle for Pan*/
	int TiltStep;/*step angle for Tilt*/
	float DispFps;
	DWORD frmCount;
	short FpsCount;
	int timer_id;/*fps count timer*/
	int image_timer_id;/*auto image capture timer*/
	int image_timer;/*auto image capture time*/
	DWORD image_inc;/*image name increment*/
	int image_npics;/*number of captures*/
	int fps;
	int fps_num;
	int bpp; //current bytes per pixel
	int hwaccel; //use hardware acceleration
	int grabmethod;//default mmap(1) or read(0)
	int width;
	int height;
	int winwidth;
	int winheight;
	int boxvsize;
	int spinbehave; /*spin: 0-non editable 1-editable*/
	char *mode; /*jpg (default) or yuv*/
	int format;
	int formind; /*0-MJPG 1-YUYV*/
	int Frame_Flags;
   	BYTE *jpeg;
	int   jpeg_size;
	//int   jpeg_quality;
	unsigned int   jpeg_bufsize; /* width*height/2 */
	int autofocus;/*some autofocus flags*/
	int AFcontrol;
}   __attribute__ ((packed));



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
	BYTE density;/* 0 - No units, aspect ratio only specified
		        1 - Pixels per Inch on quickcam5000pro
			2 - Pixels per Centimetre                */
	BYTE xdensity[2];/*120 on quickcam5000pro*/
	BYTE ydensity[2];/*120 on quickcam5000pro*/
	BYTE WTN;/*width Thumbnail 0*/
	BYTE HTN;/*height Thumbnail 0*/	
} __attribute__ ((packed)) JPGFILEHEADER, *PJPGFILEHEADER;

int jpeg_decode(unsigned char **pic, unsigned char *buf, int *width,
		int *height);
		
int yuv420_to_yuyv (BYTE *framebuffer, BYTE *tmpbuffer, int width, 
		int height);

int initGlobals(struct GLOBAL *global);
int closeGlobals(struct GLOBAL *global);
void cleanBuff(BYTE* Buff,int size);
pchar* splitPath(char *FullPath, char* splited[2]);
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

int 
SaveJPG(const char *Filename,int imgsize,BYTE *ImagePix);

int 
SaveBuff(const char *Filename,int imgsize,BYTE *data);

int SaveBPM(const char *Filename, long width, long height, int BitCount, BYTE *ImagePix);

int write_png(char *file_name, int width, int height,BYTE *prgb_data);
