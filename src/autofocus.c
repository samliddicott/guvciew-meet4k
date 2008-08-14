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
#  autofocus - using dct                                                        #
#                                                                               # 
# 							                        #
********************************************************************************/

#include "prototype.h"
#include "utils.h"
#include "autofocus.h"
#include <stdlib.h>

/* extract lum (y) data from image focus window             */
/* img - image data pointer                                 */
/* dataY - pointer for lum (y) data  dataY[winsize*winsize] */
/* width - width of img (in pixels)                         */
/* height - height of img (in pixels)                       */
/* winsize - size of focus window (square width=height)     */
static INT16* extractY (BYTE* img, INT16* dataY, int width, int height) {
	int i=0;
    	BYTE *pimg;
    	pimg=img;
    	
    	for (i=0;i<(height*width);i++) {
		dataY[i]=(INT16) *pimg++;
		pimg++;
	}
	
	return (dataY);
};


/* measure sharpness in MCU                 */
/* data - MCU data [8x8]                    */
/* t - highest order coef.                  */
static float getSharpMeasureMCU (INT16 *data, int t) {

	int i=0;
	int j=0;
	float res=0.0;
	double sumAC=0;
	double sumSqrAC=0;

	levelshift (data);
	DCT (data);

	/* get Sharpness (bayes-spectral-entropy)*/
	for (i=0;i<t;i++) {
		for(j=0;j<t;j++) {
		    	//if((i==0) && (j==0)) break; /*skip DC*/ 
			sumAC+=abs(data[i*8+j]);
			sumSqrAC+=abs(data[i*8+j])*abs(data[i*8+j]);
			
		}	
	}
	res=(1-(sumSqrAC/(sumAC*sumAC)));
	return (res);
}

float getSharpMeasure (BYTE* img, int width, int height, int t) {

	float res=0;
	double sumMCU=0;
	int numMCUx = 10;
    	int numMCUy = 10;
	INT16 dataMCU[64];
    	INT16* data;
    	INT16 dataY[width*height];
	INT16 *Y = dataY;
	
    	data=dataMCU;
    
	Y = extractY (img, Y, width, height);
	
    	int i=0;
    	int j=0;
    	int MCUx=0;
    	int MCUy=0;
	/*calculate MCU sharpness*/
	for (MCUy=0;MCUy<numMCUy;MCUy++) {
    		for (MCUx=0;MCUx<numMCUx;MCUx++) {
			for (i=0;i<8;i++) {
				for(j=0;j<8;j++) {
				    	/*center*/
					dataMCU[i*8+j]=Y[(((height-(numMCUy-(MCUy*2))*8)>>1)+i)*width+(((width-(numMCUx-(MCUx*2))*8)>>1)+j)];
				}
			}
			sumMCU+=getSharpMeasureMCU(data,t);
		}
	}
    	
	res = sumMCU/(numMCUx*numMCUy); /*average = mean*/
	return res;
}

int getFocusVal (int focus, float sharpness, float old_sharpness, int step, int* flag, int fps) {

    	float treshold = 0.0001;
    	int cycles=0;
    	/*cycles > 11*/
    	if (fps<=20) cycles=13;
        else cycles=16;
    	
	switch (*flag) {
	    /*-------------------------- predicton -------------------------*/
	    case 0: if (focus < 126) { /*first run - based on camera focus*/
	    		focus+=step; /*closer ?*/
			*flag=1;
	    	} else {
			focus-=step; /*further ?*/
			*flag=2;
		}
	    	break;
	    /*-----------------------test predicton-----------------------*/
	    case 1: if (sharpness > (old_sharpness + treshold)) { /*closer*/
	    		focus+=2*step; /*closer ?*/
			*flag=5;
	    	} else {
			focus-=step; /*revert - further ?*/
			*flag=3;
		}
	    	break;
	    case 2: if (sharpness > (old_sharpness + treshold)) { /*further*/
	    		focus-=2*step; /*further ?*/
			*flag=7;
	    	} else {
			focus+=step; /*revert - closer ?*/
			*flag=4;
		}
	    	break;
	    case 3: focus-=step; /*further ?*/
	    	*flag=6;
	    	break;
	    case 4: focus+=step; /*closer ?*/
	    	*flag=8;
	   	break;
	    case 5: if (sharpness > (old_sharpness + treshold)) { /*closer*/
	    		focus+=4*step; /*closer ?*/
			*flag=10;
	    	} else {
			focus-=2*step; /*focus*/
			*flag=cycles; /*cycle delay for next focus cycle*/
		}
	    	break;
	    case 6: if (sharpness > (old_sharpness + treshold)) { /*reverted - further*/
	    		focus-=2*step; /*further ?*/
			*flag=7;
	    	} else {
			focus+=step; /*focus*/
			*flag=cycles;/*cycle delay for next focus cycle*/
		}
	    	break;
	    case 7: if (sharpness > (old_sharpness + treshold)) { /*further*/
	    		focus-=4*step; /*further ?*/
			*flag=9;
	    	} else {
			focus+=2*step; /*focus*/
			*flag=cycles;/*cycle delay for next focus cycle*/
		}
	    	break;
	    case 8: if (sharpness > (old_sharpness + treshold)) { /*reverted - closer*/
	    		focus+=2*step; /*closer ?*/
			*flag=5;
	    	} else {
			focus-=step; /*focus*/
			*flag=cycles;/*cycle delay for next focus cycle*/
		}
	    	break;
	    case 9: if (sharpness > (old_sharpness + treshold)) { /*further*/
	    		focus-=8*step; /*further ?*/
			*flag=9;
	    	} else {
			focus+=4*step; /*focus*/
			*flag=cycles;/*cycle delay for next focus cycle*/
		}
	    	break;
	    case 10: if (sharpness > (old_sharpness + treshold)) { /*closer*/
	    		focus+=8*step; /*closer ?*/
			*flag=10;
	    	} else {
			focus-=4*step; /*focus*/
			*flag=cycles; /*cycle delay for next focus cycle*/
		}
	    	break;
	    default: *flag=0;
	}
    	
    	/*clip focus*/
    	CLIP(focus);
	
	return focus;
}
