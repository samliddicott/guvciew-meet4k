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
#include <math.h>

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
     	if (sumAC == 0) res=0;
    	else res=(1-(sumSqrAC/(sumAC*sumAC)));
	return (res);
}

float getSharpMeasure (BYTE* img, int width, int height, int t) {

	float res=0;
	double sumMCU=0;
	int numMCUx = width/16; /*covers half the width - width must be even*/
    	int numMCUy = height/16; /*covers half the height- height must be even*/
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



int getFocusVal (int focus,int *old_focus, int* right, int* left,float *rightS, float *leftS, float sharpness, float* focus_sharpness, int step, int* flag, int fps) {

    	float treshold = 0.005;
    	int middle=(*left+*right)/2;
    	
	switch (*flag) {
	    /*--------- first time - run sharpness algorithm -----------------*/
	    case 0:
		if (focus < middle ) { /*start left*/
		    focus = *left;
		    *flag = 1;
		} else {
		    focus = *right; /*start right*/
		    *flag = 2;
		}
		break;
	    case 1:
	    	*leftS = sharpness;
		focus = *left + step;
		*flag = 3;
		break;
	    case 2:
	    	*rightS = sharpness;
		focus = *right - step;
		*flag = 4;
		break;
	    case 3:
	    	*old_focus = focus;
	    	*rightS = sharpness;
		focus = *old_focus + step;
		*flag = 5;
		break;
	    case 4:
	    	*old_focus = focus;
	    	*leftS = sharpness;
		focus = *old_focus - step;
		*flag = 6;
		break;
	    case 5:
	    	if (*rightS > *leftS) {
			if (sharpness > *rightS) {
			    *left = focus;
			    *flag = 1;
			} else {
			    *right = focus;
			    focus = *left;
			    *flag = 11;
			}
		} else {
		    *right = *old_focus;
		    focus = *left;
		    *flag = 11;
		}
		break;
	    case 6:
	    	if (*leftS > *rightS) {
			if (sharpness > *leftS) {
			    *right = focus;
			    *flag = 2;
			} else {
			    *left = focus;
			    focus = *right;
			    *flag = 12;
			}
		} else {
		    *left = *old_focus;
		    focus = *right;
		    *flag = 12;
		}
		break;
	    case 11:
	    	*leftS = sharpness;
		focus = *left + (step/10);
		*flag = 13;
		break;
	    case 12:
	    	*rightS = sharpness;
		focus = *right - (step/10);
		*flag = 14;
		break;
	    case 13:
	    	*old_focus = focus;
	    	*rightS = sharpness;
		focus = *old_focus + (step/10);
		*flag = 15;
		break;
	    case 14:
	    	*old_focus = focus;
	    	*leftS = sharpness;
		focus = *old_focus - (step/10);
		*flag = 16;
		break;
	    case 15:
	    	if (*rightS > *leftS) {
			if (sharpness > *rightS) {
			    *left = focus;
			    *flag = 11;
			} else {
			    *focus_sharpness = *rightS;
			    focus= *old_focus;
			    *flag = 17;
			}
		} else {
		    *focus_sharpness = *leftS; //FIXME
		    focus = middle;
		    *flag = 17;
		}
		break;
	    case 16:
	    	if (*leftS > *rightS) {
			if (sharpness > *leftS) {
			    *right = focus;
			    *flag = 12;
			} else {
			    *focus_sharpness = *leftS;
			    focus = *old_focus;
			    *flag = 17;
			}
		} else {
		    *focus_sharpness = *rightS; //FIXME
		    focus = middle;
		    *flag = 17;
		}
		break;
	    //~ case 10: /* left first */
		//~ *old_focus=focus;
	    	//~ focus=(*left+middle)/2;
	    	//~ *flag = 12;
	    	//~ break;
	    //~ case 11: /*right first */
		//~ *old_focus=focus;
		//~ focus=(*right+middle)/2;
	    	//~ *flag = 13;
	    	//~ break;
	    //~ case 12: /*left first - get left value*/
	    	//~ *old_focus = focus; /*store old focus*/
		//~ focus = (*right+middle)/2;
		//~ *leftS=sharpness;
	    	//~ *flag = 14;
		//~ break;
	    //~ case 13: /*right first - get right value*/
		//~ *old_focus = focus; /*store old focus*/
	    	//~ focus=(*left+middle)/2;
	    	//~ *rightS=sharpness;
	    	//~ *flag = 15;
		//~ break;
	    //~ case 14: /*left first - get right value*/
		//~ *rightS=sharpness;
	    	//~ if ((*right-*left)>2) {
	    		//~ if (*leftS>*rightS) { *right=middle; } else { *left=middle; }
      			//~ *flag=abs((*left+middle)/2-*old_focus)<abs((*right+middle)/2-*old_focus);
		//~ } else {
		    	//~ focus = middle;
		    	//~ *focus_sharpness = sharpness;
			//~ *flag = 16;
		//~ }
		//~ break;
	    //~ case 15: /* right first - get left value*/
		//~ *leftS=sharpness;
	    	//~ if ((*right-*left)>2) {
	    		//~ if (*leftS>*rightS) { *right=middle; } else { *left=middle; }
      			//~ *flag=abs((*left+middle)/2-*old_focus)<abs((*right+middle)/2-*old_focus);
		//~ } else {
		    	//~ focus = middle;
		    	//~ *focus_sharpness = sharpness;
			//~ *flag = 16;
		//~ }
		//~ break;
	    case 17: /* focus */
		*right = 256;
		*left = 0;
		if (fabs(sharpness - *focus_sharpness) > treshold) *flag = 0;
		else *flag = 17;    
		break;
		   
		  
	}	
    	
    	/*clip focus*/
    	focus=(focus>255)?255:((focus<0)?0:focus);
	
	return focus;
}
