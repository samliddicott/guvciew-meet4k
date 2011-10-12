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


#include <glib.h>
#include <glib/gprintf.h>

#include "defs.h"
#include "guvcview.h"
#include "v4l2uvc.h"
#include "colorspaces.h"
#include "jpgenc.h"
#include "picture.h"
#include "ms_time.h"
#include "string_utils.h"

int store_picture(void *data)
{
	struct ALL_DATA *all_data = (struct ALL_DATA *) data;

	struct GLOBAL *global = all_data->global;
	struct vdIn *videoIn = all_data->videoIn;
	
	struct JPEG_ENCODER_STRUCTURE *jpeg_struct = NULL;
	BYTE *pim =  NULL;
	BYTE *jpeg = NULL;
	int jpeg_size = 0;
	
	switch(global->imgFormat) 
	{
		case 0:/*jpg*/
			/* Save directly from MJPG frame */
			if((global->Frame_Flags==0) && (global->format==V4L2_PIX_FMT_MJPEG)) 
			{
				if(SaveJPG(videoIn->ImageFName,videoIn->buf.bytesused,videoIn->tmpbuffer)) 
				{
					g_printerr ("Error: Couldn't capture Image to %s \n",
						videoIn->ImageFName);
					return(-1);
				}
			} 
			else if ((global->Frame_Flags==0) && (global->format==V4L2_PIX_FMT_JPEG))
			{
				if (SaveBuff(videoIn->ImageFName,videoIn->buf.bytesused,videoIn->tmpbuffer))
				{
					g_printerr ("Error: Couldn't capture Image to %s \n",
						videoIn->ImageFName);
					return(-1);
				}
			}
			else 
			{ /* use built in encoder */
				jpeg = g_new0(BYTE, ((global->width)*(global->height))>>1);
				jpeg_struct = g_new0(struct JPEG_ENCODER_STRUCTURE, 1);
					
				/* Initialization of JPEG control structure */
				initialization (jpeg_struct,global->width,global->height);
	
				/* Initialization of Quantization Tables  */
				initialize_quantization_tables (jpeg_struct);
				 

				jpeg_size = encode_image(videoIn->framebuffer, jpeg, 
					jpeg_struct, 1, global->width, global->height);
							
				if(SaveBuff(videoIn->ImageFName, jpeg_size, jpeg)) 
				{ 
					g_printerr ("Error: Couldn't capture Image to %s \n",
						videoIn->ImageFName);
					return(-1);
				}
			}
			break;

		case 1:/*bmp*/
			/*24 bits -> 3bytes     32 bits ->4 bytes*/
			pim = g_new0(BYTE, (global->width)*(global->height)*3);
			yuyv2bgr(videoIn->framebuffer,pim,global->width,global->height);
					
			if(SaveBPM(videoIn->ImageFName, global->width, global->height, 24, pim)) 
			{
				g_printerr ("Error: Couldn't capture Image to %s \n",
					videoIn->ImageFName);
				return(-1);
			} 
			break;
			
		case 2:/*png*/
			/*24 bits -> 3bytes     32 bits ->4 bytes*/
			pim = g_new0(BYTE, (global->width)*(global->height)*3);
			yuyv2rgb(videoIn->framebuffer,pim,global->width,global->height);
			write_png(videoIn->ImageFName, global->width, global->height, pim);
			break;

       case 3:/*raw*/
            videoIn->cap_raw = 1;
            return 1;
	}
	
	if(jpeg_struct) g_free(jpeg_struct);
	jpeg_struct=NULL;
	if(jpeg) g_free(jpeg);
	jpeg = NULL;
	if(pim) g_free(pim);
	pim=NULL;
	
	return 0;
}
	
	
	
