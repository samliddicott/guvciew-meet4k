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

#ifndef DEFS_H
#define DEFS_H
#include <inttypes.h>
#include <sys/types.h>

/* support for internationalization - i18n */
#ifndef _
#  define _(String) dgettext (PACKAGE, String)
#endif

#ifndef N_
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#endif

#define E_OK					  (0)
#define E_ALLOC_ERR				  (-1)
#define E_QUERYCAP_ERR  		  (-2)
#define E_READ_ERR     		 	  (-3)
#define E_MMAP_ERR      		  (-4)
#define E_QUERYBUF_ERR   		  (-5)
#define E_QBUF_ERR       		  (-6)
#define E_DQBUF_ERR				  (-7)
#define E_STREAMON_ERR   		  (-8)
#define E_STREAMOFF_ERR  		  (-9)
#define E_FORMAT_ERR    		  (-10)
#define E_REQBUFS_ERR    		  (-11)
#define E_DEVICE_ERR     		  (-12)
#define E_SELECT_ERR     		  (-13)
#define E_SELECT_TIMEOUT_ERR	  (-14)
#define E_FBALLOC_ERR			  (-15)
#define E_NO_STREAM_ERR           (-16)
#define E_NO_DATA                 (-17)
#define E_NO_CODEC                (-18)
#define E_DECODE_ERR              (-19)
#define E_BAD_TABLES_ERR          (-20)
#define E_NO_SOI_ERR              (-21)
#define E_NOT_8BIT_ERR            (-22)
#define E_BAD_WIDTH_OR_HEIGHT_ERR (-23)
#define E_TOO_MANY_COMPPS_ERR     (-24)
#define E_ILLEGAL_HV_ERR          (-25)
#define E_QUANT_TBL_SEL_ERR       (-26)
#define E_NOT_YCBCR_ERR           (-27)
#define E_UNKNOWN_CID_ERR         (-28)
#define E_WRONG_MARKER_ERR        (-29)
#define E_NO_EOI_ERR              (-30)
#define E_UNKNOWN_ERR    		  (-40)


#ifndef TRUE
#define TRUE (1)
#endif

#ifndef FALSE
#define FALSE (0)
#endif

#define CLEAR_LINE "\x1B[K"

#ifdef WORDS_BIGENDIAN
  #define BIGENDIAN 1
#else
  #define BIGENDIAN 0
#endif

#define IO_MMAP 1
#define IO_READ 2

#define ODD(x) ((x%2)?TRUE:FALSE)

#define __THREAD_TYPE pthread_t
#define __THREAD_CREATE(t,f,d) (pthread_create(t,NULL,f,d))
#define __THREAD_JOIN(t) (pthread_join(t, NULL))


#define __MUTEX_TYPE pthread_mutex_t
#define __COND_TYPE pthread_cond_t
#define __INIT_MUTEX(m) ( pthread_mutex_init(m, NULL) )
#define __CLOSE_MUTEX(m) ( pthread_mutex_destroy(m) )
#define __LOCK_MUTEX(m) ( pthread_mutex_lock(m) )
#define __UNLOCK_MUTEX(m) ( pthread_mutex_unlock(m) )

#define __INIT_COND(c)  ( pthread_cond_init (c, NULL) )
#define __CLOSE_COND(c) ( pthread_cond_destroy(c) )
#define __COND_BCAST(c) ( pthread_cond_broadcast(c) )
#define __COND_TIMED_WAIT(c,m,t) ( pthread_cond_timedwait(c,m,t) )

/*next index of ring buffer with size elements*/
#define NEXT_IND(ind,size) ind++;if(ind>=size) ind=0
/*previous index of ring buffer with size elements*/
//#define PREV_IND(ind,size) ind--;if(ind<0) ind=size-1

#define VIDBUFF_SIZE 45    //number of video frames in the ring buffer

#define MPG_NUM_SAMP 1152  //number of samples in a audio MPEG frame
#define AUDBUFF_SIZE 2     //number of audio mpeg frames in each audio buffer
                           // direct impact on latency as buffer is only processed when full
#define AUDBUFF_NUM  80    //number of audio buffers

typedef char* pchar;

typedef float SAMPLE;

/* 0 is device default*/
static const int stdSampleRates[] =
{
	0, 8000,  9600, 11025, 12000,
	16000, 22050, 24000,
	32000, 44100, 48000,
	88200, 96000,
	-1   /* Negative terminated list. */
};

#define DEBUG (0)

#define INCPANTILT 64 // 1°

#define WINSIZEX 560
#define WINSIZEY 560

#define AUTO_EXP 8
#define MAN_EXP	1

#define DHT_SIZE 432

#define DEFAULT_WIDTH 640
#define DEFAULT_HEIGHT 480

#define DEFAULT_FPS	25
#define DEFAULT_FPS_NUM 1

/*clip value between 0 and 255*/
#define CLIP(value) (uint8_t)(((value)>0xFF)?0xff:(((value)<0)?0:(value)))

/*MAX macro - gets the bigger value*/
#ifndef MAX
#define MAX(a,b) (((a) < (b)) ? (b) : (a))
#endif

/*FILTER FLAGS*/
#define YUV_NOFILT (0)
#define YUV_MIRROR (1<<0)
#define YUV_UPTURN (1<<1)
#define YUV_NEGATE (1<<2)
#define YUV_MONOCR (1<<3)
#define YUV_PIECES (1<<4)
#define YUV_PARTICLES (1<<5)

/*Audio Effects*/
#define SND_NOEF   (0)
#define SND_ECHO   (1<<0)
#define SND_FUZZ   (1<<1)
#define SND_REVERB (1<<2)
#define SND_WAHWAH (1<<3)
#define SND_DUCKY  (1<<4)

/* On Screen Display flags*/
#define OSD_METER  (1<<0)

#endif

