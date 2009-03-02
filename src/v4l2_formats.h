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
#ifndef V4L2_FORMATS_H
#define V4L2_FORMATS_H 

#include <linux/videodev2.h>

/* (Patch) define all supported formats - already done in videodev2.h*/
#ifndef V4L2_PIX_FMT_MJPEG
#define V4L2_PIX_FMT_MJPEG  v4l2_fourcc('M', 'J', 'P', 'G') /*  MJPEG stream     */
#endif

#ifndef V4L2_PIX_FMT_JPEG
#define V4L2_PIX_FMT_JPEG  v4l2_fourcc('J', 'P', 'E', 'G')  /*  JPEG stream      */
#endif

#ifndef V4L2_PIX_FMT_YUYV
#define V4L2_PIX_FMT_YUYV    v4l2_fourcc('Y','U','Y','V')   /* YUV 4:2:2        */
#endif

#ifndef V4L2_PIX_FMT_YVYU
#define V4L2_PIX_FMT_YVYU    v4l2_fourcc('Y','V','Y','U')   /* YUV 4:2:2        */
#endif

#ifndef V4L2_PIX_FMT_UYVY
#define V4L2_PIX_FMT_UYVY    v4l2_fourcc('U','Y','V','Y')   /* YUV 4:2:2        */
#endif

#ifndef V4L2_PIX_FMT_YYUV
#define V4L2_PIX_FMT_YYUV    v4l2_fourcc('Y','Y','U','V')   /* YUV 4:2:2        */
#endif

#ifndef V4L2_PIX_FMT_YUV420
#define V4L2_PIX_FMT_YUV420  v4l2_fourcc('Y','U','1','2')   /* YUV 4:2:0 Planar  */
#endif

#ifndef V4L2_PIX_FMT_YVU420
#define V4L2_PIX_FMT_YVU420  v4l2_fourcc('Y','V','1','2')   /* YUV 4:2:0 Planar  */
#endif


#ifndef V4L2_PIX_FMT_SGBRG8
#define V4L2_PIX_FMT_SGBRG8  v4l2_fourcc('G', 'B', 'R', 'G') /* GBGB.. RGRG..    */
#endif

typedef struct _SupFormats
{
	int format;          //v4l2 software(guvcview) supported format
	char mode[5];        //mode (fourcc - lower case)
	int hardware;        //hardware supported (1 or 0)
} SupFormats;

typedef struct _VidCap 
{
	int width;            //width 
	int height;           //height
	int *framerate_num;   //list of numerator values - should be 1 in almost all cases
	int *framerate_denom; //list of denominator values - gives fps
	int numb_frates;      //number of frame rates (numerator and denominator lists size)
} VidCap;

typedef struct _VidFormats
{
	int format;          //v4l2 pixel format
	char fourcc[5];      //corresponding fourcc (mode)
	int numb_res;        //available number of resolutions for format (VidCap list size)
	VidCap *listVidCap;  //list of VidCap for format
} VidFormats;

typedef struct _LFormats
{
	VidFormats *listVidFormats; //list of VidFormats
	int numb_formats;           //total number of VidFormats (VidFormats list size)
	int current_format;         //index of current format in listVidFormats
} LFormats;

/* check if format is supported by guvcview
 * args:
 * pixfmt: V4L2 pixel format
 * return index from supported devices list 
 * or -1 if not supported                    */
int check_PixFormat(int pixfmt);

/* check if format is supported by hardware
 * args:
 * pixfmt: V4L2 pixel format
 * return index from supported devices list
 * or -1 if not supported                    */
int check_SupPixFormat(int pixfmt);

/* set hardware flag for v4l2 pix format
 * args:
 * pixfmt: V4L2 pixel format
 * return index from supported devices list
 * or -1 if not supported                    */
int set_SupPixFormat(int pixfmt);

/* convert v4l2 pix format to mode (Fourcc)
 * args:
 * pixfmt: V4L2 pixel format
 * mode: fourcc string (lower case)
 * returns 1 on success 
 * and -1 on failure (not supported)         */
int get_PixMode(int pixfmt, char *mode);

/* converts mode (fourcc) to v4l2 pix format
 * args:
 * mode: fourcc string (lower case)
 * returns v4l2 pix format 
 * defaults to MJPG if mode not supported    */
int get_PixFormat(char *mode);

/* enumerate frames (formats, sizes and fps)
 * args:
 * width: current selected width
 * height: current selected height
 * fd: device file descriptor
 *
 * returns: pointer to LFormats struct containing list of available frame formats */
LFormats *enum_frame_formats( int *width, int *height, int fd);

/* get Format index from available format list
 * args:
 * listFormats: available video format list
 * format: v4l2 pix format
 *
 * returns format list index */
int get_FormatIndex( LFormats *listFormats, int format);

/* clean video formats list
 * args: 
 * listFormats: struct containing list of available video formats
 *
 * returns: void  */
void freeFormats(LFormats *listFormats);

#endif
