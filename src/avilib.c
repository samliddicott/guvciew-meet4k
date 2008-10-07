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
#   Some utilities for writing and reading AVI files.                           # 
#   These are not intended to serve for a full blown                            #
#   AVI handling software (this would be much too complex)                      #
#   The only intention is to write out MJPEG encoded                            #
#   AVIs with sound and to be able to read them back again.                     #
#   These utilities should work with other types of codecs too, however.        #
#                                                                               #
#   Copyright (C) 1999 Rainer Johanni <Rainer@Johanni.de>                       #
********************************************************************************/
/*  Paulo Assis (6-4-2008): removed reading functions, cleaned build wranings  */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "config.h"
#include "avilib.h"

/* The following variable indicates the kind of error */

long AVI_errno = 0;

/*******************************************************************
 *                                                                 *
 *    Utilities for writing an AVI File                            *
 *                                                                 *
 *******************************************************************/

#ifndef O_BINARY
/* win32 wants a binary flag to open(); this sets it to null
   on platforms that don't have it. */
#define O_BINARY 0
#endif

#define INFO_LIST

#define MAX_INFO_STRLEN 64
static char id_str[MAX_INFO_STRLEN];

#ifndef PACKAGE
#define PACKAGE "guvcview"
#endif
#ifndef VERSION
#define VERSION "0.9"
#endif

/* AVI_MAX_LEN: The maximum length of an AVI file, we stay a bit below
    the 2GB limit (Remember: 2*10^9 is smaller than 2 GB) */

ULONG AVI_MAX_LEN = AVI_MAX_SIZE;

void AVI_set_MAX_LEN(ULONG len) {
    
	AVI_MAX_LEN = len;
}

/* HEADERBYTES: The number of bytes to reserve for the header */

#define HEADERBYTES 2048

#define PAD_EVEN(x) ( ((x)+1) & ~1 )


/* Copy n into dst as a 4 byte, little endian number.
   Should also work on big endian machines */

static void long2str(unsigned char *dst, int n)
{
   dst[0] = (n    )&0xff;
   dst[1] = (n>> 8)&0xff;
   dst[2] = (n>>16)&0xff;
   dst[3] = (n>>24)&0xff;
}

/* Convert a string of 4 or 2 bytes to a number,
   also working on big endian machines */

//~ static unsigned long str2ulong(BYTE *str)
//~ {
   //~ return ( str[0] | (str[1]<<8) | (str[2]<<16) | (str[3]<<24) );
//~ }
//~ static unsigned long str2ushort(BYTE *str)
//~ {
   //~ return ( str[0] | (str[1]<<8) );
//~ }

/* Calculate audio sample size from number of bits and number of channels.
   This may have to be adjusted for eg. 12 bits and stereo */

static int avi_sampsize(struct avi_t *AVI)
{
   int s;
   if (AVI->a_fmt == ISO_FORMAT_MPEG12) 
   {
	s = 1;
   } else {
	s = ((AVI->a_bits+7)/8)*AVI->a_chans;
	if(s==0) s=1; /* avoid possible zero divisions */
   }
   return s;
}

/* Add a chunk (=tag and data) to the AVI file,
   returns -1 on write error, 0 on success */

static int avi_add_chunk(struct avi_t *AVI, BYTE *tag, BYTE *data, int length)
{
   BYTE c[8];

   /* Copy tag and length int c, so that we need only 1 write system call
      for these two values */

   memcpy(c,tag,4);
   long2str(c+4,length);

   /* Output tag, length and data, restore previous position
      if the write fails */

   length = PAD_EVEN(length);

   if( write(AVI->fdes,c,8) != 8 ||
       write(AVI->fdes,data,length) != length )
   {
      lseek(AVI->fdes,AVI->pos,SEEK_SET);
      AVI_errno = AVI_ERR_WRITE;
      return -1;
   }

   /* Update file position */

   AVI->pos += 8 + length;

   return 0;
}

static int avi_add_index_entry(struct avi_t *AVI, BYTE *tag, long flags, long pos, long len)
{
   void *ptr;

   if(AVI->n_idx>=AVI->max_idx)
   {
      ptr = realloc((void *)AVI->idx,(AVI->max_idx+4096)*16);
      if(ptr == 0)
      {
         AVI_errno = AVI_ERR_NO_MEM;
         return -1;
      }
      AVI->max_idx += 4096;
      AVI->idx = (BYTE((*)[16]) ) ptr;
   }

   /* Add index entry */

   memcpy(AVI->idx[AVI->n_idx],tag,4);
   long2str(AVI->idx[AVI->n_idx]+ 4,flags);
   long2str(AVI->idx[AVI->n_idx]+ 8,pos);
   long2str(AVI->idx[AVI->n_idx]+12,len);

   /* Update counter */

   AVI->n_idx++;

   return 0;
}

/*
   AVI_open_output_file: Open an AVI File and write a bunch
                         of zero bytes as space for the header.

   returns a pointer to avi_t on success, a zero pointer on error
*/

int AVI_open_output_file(struct avi_t *AVI, const char * filename)
{
   int i;
   BYTE AVI_header[HEADERBYTES];
   int mask = 0; 
   
   /*resets AVI struct*/ 
   memset((void *)AVI,0,sizeof(struct avi_t));
   
   /* Since Linux needs a long time when deleting big files,
      we do not truncate the file when we open it.
      Instead it is truncated when the AVI file is closed */

   AVI->fdes = open(filename,O_RDWR|O_CREAT|O_BINARY,0644 &~mask);
   if (AVI->fdes < 0)
   {
      AVI_errno = AVI_ERR_OPEN;
      //free(AVI);
      return -1;
   }

   /* Write out HEADERBYTES bytes, the header will go here
      when we are finished with writing */

   for (i=0;i<HEADERBYTES;i++) AVI_header[i] = 0;
   i = write(AVI->fdes,AVI_header,HEADERBYTES);
   if (i != HEADERBYTES)
   {
      close(AVI->fdes);
      AVI_errno = AVI_ERR_WRITE;
      //free(AVI);
      return -2;
   }

   AVI->pos  = HEADERBYTES;
   AVI->mode = AVI_MODE_WRITE; /* open for writing */

   return 0;
}

void AVI_set_video(struct avi_t *AVI, int width, int height, double fps, char *compressor)
{
   /* may only be called if file is open for writing */

   if(AVI->mode==AVI_MODE_READ) return;

   AVI->width  = width;
   AVI->height = height;
   AVI->fps    = fps;
   memcpy(AVI->compressor,compressor,4);
   AVI->compressor[4] = 0;

}

void AVI_set_audio(struct avi_t *AVI, int channels, long rate, int bits, int format)
{
   /* may only be called if file is open for writing */

   if(AVI->mode==AVI_MODE_READ) return;

   AVI->a_chans = channels;
   AVI->a_rate  = rate;
   AVI->a_bits  = bits;
   AVI->a_fmt   = format;

}

#define OUT4CC(s) \
   if(nhb<=HEADERBYTES-4) memcpy(AVI_header+nhb,s,4); nhb += 4

#define OUTLONG(n) \
   if(nhb<=HEADERBYTES-4) long2str(AVI_header+nhb,n); nhb += 4

#define OUTSHRT(n) \
   if(nhb<=HEADERBYTES-2) { \
      AVI_header[nhb  ] = (n   )&0xff; \
      AVI_header[nhb+1] = (n>>8)&0xff; \
   } \
   nhb += 2

/*
  Write the header of an AVI file and close it.
  returns 0 on success, -1 on write error.
*/

static int avi_close_output_file(struct avi_t *AVI)
{

   int ret, njunk, sampsize, hasIndex, ms_per_frame, idxerror, flag;
   int movi_len, hdrl_start, strl_start;
   BYTE AVI_header[HEADERBYTES];
   long nhb;
   /* Calculate length of movi list */

   movi_len = AVI->pos - HEADERBYTES + 4;
    
#ifdef INFO_LIST
   long info_len;
#endif
    
   /* Try to ouput the index entries. This may fail e.g. if no space
      is left on device. We will report this as an error, but we still
      try to write the header correctly (so that the file still may be
      readable in the most cases */

   idxerror = 0;
 
   ret = avi_add_chunk(AVI,(BYTE *)"idx1",(void *)AVI->idx,AVI->n_idx*16);
   hasIndex = (ret==0);
   if(ret)
   {
      idxerror = 1;
      AVI_errno = AVI_ERR_WRITE_INDEX;
   }

   /* Calculate Microseconds per frame */

   if(AVI->fps < 0.001)
      ms_per_frame = 0;
   else
      ms_per_frame = 1000000./AVI->fps + 0.5;

   /* Prepare the file header */

   nhb = 0;

   /* The RIFF header */

   OUT4CC ("RIFF");
   OUTLONG(AVI->pos - 8);    /* # of bytes to follow */
   OUT4CC ("AVI ");

   /* Start the header list */

   OUT4CC ("LIST");
   OUTLONG(0);        /* Length of list in bytes, don't know yet */
   hdrl_start = nhb;  /* Store start position */
   OUT4CC ("hdrl");

   /* The main AVI header */

   /* The Flags in AVI File header */

#define AVIF_HASINDEX           0x00000010      /* Index at end of file */
#define AVIF_MUSTUSEINDEX       0x00000020
#define AVIF_ISINTERLEAVED      0x00000100
#define AVIF_TRUSTCKTYPE        0x00000800      /* Use CKType to find key frames */
#define AVIF_WASCAPTUREFILE     0x00010000
#define AVIF_COPYRIGHTED        0x00020000

   OUT4CC ("avih");
   OUTLONG(56);                 /* # of bytes to follow */
   OUTLONG(ms_per_frame);       /* Microseconds per frame */
   OUTLONG(10000000);           /* MaxBytesPerSec, I hope this will never be used */
   OUTLONG(0);                  /* PaddingGranularity (whatever that might be) */
                                /* Other sources call it 'reserved' */
   flag = AVIF_WASCAPTUREFILE;
   if(hasIndex) flag |= AVIF_HASINDEX;
   if(hasIndex && AVI->must_use_index) flag |= AVIF_MUSTUSEINDEX;
   OUTLONG(flag);               /* Flags */
   OUTLONG(AVI->video_frames);  /* TotalFrames */
   OUTLONG(0);                  /* InitialFrames */
   if (AVI->audio_bytes)
      { OUTLONG(2); }           /* Streams */
   else
      { OUTLONG(1); }           /* Streams */
   OUTLONG(0);                  /* SuggestedBufferSize */
   OUTLONG(AVI->width);         /* Width */
   OUTLONG(AVI->height);        /* Height */
                                /* MS calls the following 'reserved': */
   OUTLONG(0);                  /* TimeScale:  Unit used to measure time */
   OUTLONG(0);                  /* DataRate:   Data rate of playback     */
   OUTLONG(0);                  /* StartTime:  Starting time of AVI data */
   OUTLONG(0);                  /* DataLength: Size of AVI data chunk    */


   /* Start the video stream list ---------------------------------- */

   OUT4CC ("LIST");
   OUTLONG(0);        /* Length of list in bytes, don't know yet */
   strl_start = nhb;  /* Store start position */
   OUT4CC ("strl");

   /* The video stream header */

   OUT4CC ("strh");
   OUTLONG(64);                 /* # of bytes to follow */
   OUT4CC ("vids");             /* Type */
   OUT4CC (AVI->compressor);    /* Handler */
   OUTLONG(0);                  /* Flags */
   OUTLONG(0);                  /* Reserved, MS says: wPriority, wLanguage */
   OUTLONG(0);                  /* InitialFrames */
   OUTLONG(ms_per_frame);       /* Scale */
   OUTLONG(1000000);            /* Rate: Rate/Scale == samples/second */
   OUTLONG(0);                  /* Start */
   OUTLONG(AVI->video_frames);  /* Length */
   OUTLONG(0);                  /* SuggestedBufferSize */
   OUTLONG(-1);                 /* Quality */
   OUTLONG(0);                  /* SampleSize */
   OUTLONG(0);                  /* Frame */
   OUTLONG(0);                  /* Frame */
   OUTLONG(0);                  /* Frame */
   OUTLONG(0);                  /* Frame */

   /* The video stream format i- this is a BITMAPINFO structure*/
 
   OUT4CC ("strf");
   OUTLONG(40);                 /* # of bytes to follow (biSize) ?????*/
   OUTLONG(40);                 /* biSize */
   OUTLONG(AVI->width);         /* biWidth */
   OUTLONG(AVI->height);        /* biHeight */
   OUTSHRT(1);     /* Planes - allways 1 */ 
   if (strcmp(AVI->compressor,"DIB ")==0) {  /* Compression ->for DIB 24 = BI_RGB */
		OUTSHRT(24); /*Count - bitsperpixel - 1,4,8 or 24  32*/  		
		OUTLONG(0);
   	}
   	else {
		OUTSHRT(24); /*Count - bitsperpixel - 1,4,8 or 24  32*/
   		OUT4CC (AVI->compressor);    
   	}
   OUTLONG(AVI->width*AVI->height);  /* SizeImage (in bytes?) should be biSizeImage = ((((biWidth * biBitCount) + 31) & ~31) >> 3) * biHeight*/
   OUTLONG(0);                  /* XPelsPerMeter */
   OUTLONG(0);                  /* YPelsPerMeter */
   OUTLONG(0);                  /* ClrUsed: Number of colors used */
   OUTLONG(0);                  /* ClrImportant: Number of colors important */

   /* Finish stream list, i.e. put number of bytes in the list to proper pos */

   long2str(AVI_header+strl_start-4,nhb-strl_start);
   
   if (AVI->a_chans && AVI->audio_bytes)
   {

   sampsize = avi_sampsize(AVI);

   /* Start the audio stream list ---------------------------------- */

   OUT4CC ("LIST");
   OUTLONG(0);        /* Length of list in bytes, don't know yet */
   strl_start = nhb;  /* Store start position */
   OUT4CC ("strl");

   /* The audio stream header */

   OUT4CC ("strh");
   OUTLONG(64);            /* # of bytes to follow */
   OUT4CC ("auds");
   OUT4CC ("\0\0\0\0");
   OUTLONG(0);             /* Flags */
   OUTLONG(0);             /* Reserved, MS says: wPriority, wLanguage */
   OUTLONG(0);             /* InitialFrames */
   OUTLONG(sampsize);      /* Scale */
   if (AVI->a_fmt == ISO_FORMAT_MPEG12)
   {
	OUTLONG(20000); /* 20000 Bps/160 Kbps */
   } else {
	OUTLONG(sampsize*AVI->a_rate); /* Rate: Rate/Scale == samples/second */
   }
   OUTLONG(0);             /* Start */
   OUTLONG(AVI->audio_bytes/sampsize);   /* Length */
   OUTLONG(0);             /* SuggestedBufferSize */
   OUTLONG(-1);            /* Quality */
   OUTLONG(sampsize);      /* SampleSize */
   OUTLONG(0);             /* Frame */
   OUTLONG(0);             /* Frame */
   OUTLONG(0);             /* Frame */
   OUTLONG(0);             /* Frame */

   /* The audio stream format */

   OUT4CC ("strf");
   OUTLONG(16);                   /* # of bytes to follow */
   OUTSHRT(AVI->a_fmt);           /* Format */
   OUTSHRT(AVI->a_chans);         /* Number of channels */
   if (AVI->a_fmt == ISO_FORMAT_MPEG12)
   {
	OUTLONG(AVI->a_rate); /* freq. */
	OUTLONG(20000); /* 20000 Bps/ 160 Kbps */
	OUTSHRT(1);     /* BlockAlign */
   } else {
	OUTLONG(AVI->a_rate);          /* SamplesPerSec */
	OUTLONG(sampsize*AVI->a_rate); /* AvgBytesPerSec */
	OUTSHRT(sampsize);             /* BlockAlign */   
   }
   OUTSHRT(AVI->a_bits);          /* BitsPerSample */

   /* Finish stream list, i.e. put number of bytes in the list to proper pos */

   long2str(AVI_header+strl_start-4,nhb-strl_start);

   }

   /* Finish header list */

   long2str(AVI_header+hdrl_start-4,nhb-hdrl_start);
    
// add INFO list --- (0.6.0pre4)

#ifdef INFO_LIST
   OUT4CC ("LIST");
   
   //FIXME
   info_len = MAX_INFO_STRLEN + 12;
   OUTLONG(info_len);
   OUT4CC ("INFO");

//   OUT4CC ("INAM");
//   OUTLONG(MAX_INFO_STRLEN);

//   sprintf(id_str, "\t");
//   memset(AVI_header+nhb, 0, MAX_INFO_STRLEN);
//   memcpy(AVI_header+nhb, id_str, strlen(id_str));
//   nhb += MAX_INFO_STRLEN;

   OUT4CC ("ISFT");
   OUTLONG(MAX_INFO_STRLEN);

   sprintf(id_str, "%s-%s", PACKAGE, VERSION);
   memset(AVI_header+nhb, 0, MAX_INFO_STRLEN);
   memcpy(AVI_header+nhb, id_str, strlen(id_str));
   nhb += MAX_INFO_STRLEN;

//   OUT4CC ("ICMT");
//   OUTLONG(MAX_INFO_STRLEN);

//   calptr=time(NULL); 
//   sprintf(id_str, "\t%s %s", ctime(&calptr), "");
//   memset(AVI_header+nhb, 0, MAX_INFO_STRLEN);
//   memcpy(AVI_header+nhb, id_str, 25);
//   nhb += MAX_INFO_STRLEN;
#endif
    

   /* Calculate the needed amount of junk bytes, output junk */

   njunk = HEADERBYTES - nhb - 8 - 12;

   /* Safety first: if njunk <= 0, somebody has played with
      HEADERBYTES without knowing what (s)he did.
      This is a fatal error */

   if(njunk<=0)
   {
      fprintf(stderr,"AVI_close_output_file: # of header bytes too small\n");
      exit(1);
   }

   OUT4CC ("JUNK");
   OUTLONG(njunk);
   memset(AVI_header+nhb,0,njunk);
   nhb += njunk;

   /* Start the movi list */

   OUT4CC ("LIST");
   OUTLONG(movi_len); /* Length of list in bytes */
   OUT4CC ("movi");

   /* Output the header, truncate the file to the number of bytes
      actually written, report an error if someting goes wrong */

   if ( lseek(AVI->fdes,0,SEEK_SET)<0 ||
        write(AVI->fdes,AVI_header,HEADERBYTES)!=HEADERBYTES || ftruncate(AVI->fdes,AVI->pos)<0 
       )
   {
      AVI_errno = AVI_ERR_CLOSE;
      return -1;
   }

   if(idxerror) return -1;

   return 0;
}
/*
   AVI_write_data:
   Add video or audio data to the file;

   Return values:
    0    No error;
   -1    Error, AVI_errno is set appropriatly;

*/

static int avi_write_data(struct avi_t *AVI, BYTE *data, long length, int audio, int keyframe)
{
   int n;
   /* Check for maximum file length */

   if ( (AVI->pos + 8 + length + 8 + (AVI->n_idx+1)*16) > AVI_MAX_LEN )
   {
      AVI_errno = AVI_ERR_SIZELIM;
      return -1;
   }

   /* Add index entry */
   if(audio)
     n = avi_add_index_entry(AVI,(BYTE *)"01wb",0x00,AVI->pos,length);
   else
     n = avi_add_index_entry(AVI,(BYTE *) "00dc",((keyframe)?0x10:0x0),AVI->pos,length);

   if(n) return -1;

   /* Output tag and data */
    if(audio)
     n = avi_add_chunk(AVI,(BYTE *) "01wb",(BYTE *)data,length);
   else
     n = avi_add_chunk(AVI,(BYTE *)"00dc",(BYTE *)data,length);

   if (n) return -1;

   return 0;
}

int AVI_write_frame(struct avi_t *AVI, BYTE *data, long bytes, int keyframe)
{
   long pos;

   if(AVI->mode==AVI_MODE_READ) { AVI_errno = AVI_ERR_NOT_PERM; return -1; }

   pos = AVI->pos;
   if( avi_write_data(AVI,data,bytes,0, keyframe) ) {
       return -1;
   }
   AVI->last_pos = pos;
   AVI->last_len = bytes;
   AVI->video_frames++;
   return 0;
}

int AVI_dup_frame(struct avi_t *AVI)
{
   if(AVI->mode==AVI_MODE_READ) { AVI_errno = AVI_ERR_NOT_PERM; return -1; }

   if(AVI->last_pos==0) return 0; /* No previous real frame */
   	
   if(avi_add_index_entry(AVI,(BYTE *) "00dc",0x10,AVI->last_pos,AVI->last_len)) return -1;
   AVI->video_frames++;
   AVI->must_use_index = 1;
   return 0;
}

int AVI_write_audio(struct avi_t *AVI, BYTE *data, long bytes)
{
   if(AVI->mode==AVI_MODE_READ) { AVI_errno = AVI_ERR_NOT_PERM; return -1; }

   if( avi_write_data(AVI,data,bytes,1,0) ) {
       return -1;
   }
   AVI->audio_bytes += bytes;
   return 0;
}

/*doesn't check for size limit - called when closing avi*/
int AVI_append_audio(struct avi_t *AVI, BYTE *data, long bytes)
{  
   /* Add index entry */
   if (avi_add_index_entry(AVI,(BYTE *) "01wb",0x00,AVI->pos, bytes)) 
	return -1;
   
   /* Output tag and data */
   
   if(avi_add_chunk(AVI,(BYTE *) "01wb", (BYTE *)data, bytes))   
   	return -1;
  
   AVI->audio_bytes += bytes; 
   return 0;  
}

ULONG AVI_bytes_remain(struct avi_t *AVI)
{
   if(AVI->mode==AVI_MODE_READ) return 0;

   return ( AVI_MAX_LEN - (AVI->pos + 8 + 16*AVI->n_idx));
}

/*******************************************************************
 *                                                                 *
 *    Utilities for reading video and audio from an AVI File       *
 *                                                                 *
 *******************************************************************/

int AVI_close(struct avi_t *AVI)
{
   int ret;

   /* If the file was open for writing, the header and index still have
      to be written */

   if(AVI->mode == AVI_MODE_WRITE)
      ret = avi_close_output_file(AVI);
   else
      ret = 0;

   /* Even if there happened a error, we first clean up */

   close(AVI->fdes);
   //if(AVI->idx) free(AVI->idx);
   //if(AVI->video_index) free(AVI->video_index);
   //if(AVI->audio_index) free(AVI->audio_index);
   //free(AVI);
   //AVI=NULL;	
   return ret;
}


#define ERR_EXIT(x) \
{ \
   AVI_close(AVI); \
   AVI_errno = x; \
   return 0; \
}

int AVI_getErrno() 
{
    return AVI_errno;
}

/* AVI_print_error: Print most recent error (similar to perror) */

char *(avi_errors[]) =
{
  /*  0 */ "avilib - No Error",
  /*  1 */ "avilib - AVI file size limit reached",
  /*  2 */ "avilib - Error opening AVI file",
  /*  3 */ "avilib - Error reading from AVI file",
  /*  4 */ "avilib - Error writing to AVI file",
  /*  5 */ "avilib - Error writing index (file may still be useable)",
  /*  6 */ "avilib - Error closing AVI file",
  /*  7 */ "avilib - Operation (read/write) not permitted",
  /*  8 */ "avilib - Out of memory (malloc failed)",
  /*  9 */ "avilib - Not an AVI file",
  /* 10 */ "avilib - AVI file has no header list (corrupted?)",
  /* 11 */ "avilib - AVI file has no MOVI list (corrupted?)",
  /* 12 */ "avilib - AVI file has no video data",
  /* 13 */ "avilib - operation needs an index",
  /* 14 */ "avilib - Unkown Error"
};
static int num_avi_errors = sizeof(avi_errors)/sizeof(char*);

static char error_string[4096];

void AVI_print_error(char *str)
{
   int aerrno;

   aerrno = (AVI_errno>=0 && AVI_errno<num_avi_errors) ? AVI_errno : num_avi_errors-1;

   fprintf(stderr,"%s: %s\n",str,avi_errors[aerrno]);

   /* for the following errors, perror should report a more detailed reason: */

   if(AVI_errno == AVI_ERR_OPEN ||
      AVI_errno == AVI_ERR_READ ||
      AVI_errno == AVI_ERR_WRITE ||
      AVI_errno == AVI_ERR_WRITE_INDEX ||
      AVI_errno == AVI_ERR_CLOSE )
   {
      perror("REASON");
   }
}

char *AVI_strerror()
{
   int aerrno;

   aerrno = (AVI_errno>=0 && AVI_errno<num_avi_errors) ? AVI_errno : num_avi_errors-1;

   if(AVI_errno == AVI_ERR_OPEN ||
      AVI_errno == AVI_ERR_READ ||
      AVI_errno == AVI_ERR_WRITE ||
      AVI_errno == AVI_ERR_WRITE_INDEX ||
      AVI_errno == AVI_ERR_CLOSE )
   {
      sprintf(error_string,"%s - %s",avi_errors[aerrno],strerror(errno));
      return error_string;
   }
   else
   {
      return avi_errors[aerrno];
   }
}
