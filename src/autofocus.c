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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static double sumAC[64];
static int ACweight[64] = {
	0,1,2,3,4,5,6,7,
	1,1,2,3,4,5,6,7,
	2,2,2,3,4,5,6,7,
	3,3,3,3,4,5,6,7,
	4,4,4,4,4,5,6,7,
	5,5,5,5,5,5,6,7,
	7,7,7,7,7,7,7,7
};

void initFocusData (struct focusData *AFdata) {
	memset(AFdata,0,sizeof(struct focusData));
    	AFdata->right=255;
	AFdata->left=8; /*start with focus at 8*/
	AFdata->focus=-1;
	AFdata->focus_wait=0;
	memset(sumAC,0,64);
    	/*all other values are 0 */
}

static void bubble_sort (struct focusData *AFdata, int size) {
	int swapped = 0;
    	int temp=0;
    	if (size>=20) {
		printf("WARNING: focus array size=%d exceeds 20\n",size);
		size = 10;
	}
    	int i;
    	do {
    		swapped = 0;
    		size--;
    		for (i=0;i<=size;i++) {
      			if (AFdata->arr_sharp[i+1] > AFdata->arr_sharp[i]) {
        			temp = AFdata->arr_sharp[i];
				AFdata->arr_sharp[i]=AFdata->arr_sharp[i+1];
				AFdata->arr_sharp[i+1]=temp;
				temp=AFdata->arr_foc[i];
				AFdata->arr_foc[i]=AFdata->arr_foc[i+1];
				AFdata->arr_foc[i+1]=temp;
				swapped = 1;
			}
		}
	 } while (swapped);
}

/* extract lum (y) data from image    (YUYV)                */
/* img - image data pointer                                 */
/* dataY - pointer for lum (y) data                         */
/* width - width of img (in pixels)                         */
/* height - height of img (in pixels)                       */
static INT16* extractY (BYTE* img, INT16* dataY, int width, int height) {
	int i=0;
    	BYTE *pimg;
    	pimg=img;
    	
    	for (i=0;i<(height*width);i++) {
		dataY[i]=(INT16) *pimg++;
		pimg++;
	}
	
	return (dataY);
}

/* measure sharpness in MCU                 */
/* data - MCU data [8x8]                    */
/* t - highest order coef.                  */
static void getSharpnessMCU (INT16 *data, double weight) {

	int i=0;
	int j=0;
		

	levelshift (data);
	DCT (data);

	for (i=0;i<8;i++) {
		for(j=0;j<8;j++) {
			sumAC[i*8+j]+=data[i*8+j]*data[i*8+j]*weight;
		}	
	}	
}

/* sharpness in focus window */
int getSharpness (BYTE* img, int width, int height, int t) {

	float res=0;
	int numMCUx = width/(8*2); /*covers 1/4 of width - width must be even*/
    	int numMCUy = height/(8*2); /*covers 1/4 of height- height must be even*/
	INT16 dataMCU[64];
    	INT16* data;
    	INT16 dataY[width*height];
	INT16 *Y = dataY;
	
	double weight;
	double xp_;
	double yp_;
	int ctx = numMCUx >> 1; /*center*/ 
	int cty = numMCUy >> 1;
	double rad=ctx/2; 
	if (cty<ctx) { rad=cty/2; }
	rad=rad*rad;
    	int cnt2 =0;
	
	data=dataMCU;
    
	Y = extractY (img, Y, width, height);
	
    	int i=0;
    	int j=0;
    	int xp=0;
    	int yp=0;
	/*calculate MCU sharpness*/
	for (yp=0;yp<numMCUy;yp++) {
		yp_=yp-cty;
    		for (xp=0;xp<numMCUx;xp++) {
			xp_=xp-ctx;
			weight = exp(-(xp_*xp_)/rad-(yp_*yp_)/rad);
			for (i=0;i<8;i++) {
				for(j=0;j<8;j++) {
				    	/*center*/
					dataMCU[i*8+j]=Y[(((height-(numMCUy-(yp*2))*8)>>1)+i)*width+(((width-(numMCUx-(xp*2))*8)>>1)+j)];
				}
			}
			getSharpnessMCU(data,weight);
			cnt2++;
		}
	}
    	
	
	for (i=0;i<=t;i++) {
		for(j=0;j<t;j++) {
			sumAC[i*8+j]/=(double) (cnt2); /*average = mean*/
			res+=sumAC[i*8+j]*ACweight[i*8+j];
		}
	}
	return (roundf(res*10)); /*round to int (4 digit precision)*/
}


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

	/* get Sharpness (bayes-spectral-entropy)        */
	/* consider an upper-left-triangle of components */
	/* in the matrix such that  j+i <= t             */
	for (i=0;i<=t;i++) {
		for(j=0;j<=(t-i);j++) {
		    	//if((i!=0) || (j!=0)) { /*skip DC*/ 
				sumAC+=abs(data[i*8+j]);
				sumSqrAC+=abs(data[i*8+j])*abs(data[i*8+j]);
			//}
		}	
	}
     	if (sumAC == 0) res=0;
    	else res=(1-(sumSqrAC/(sumAC*sumAC)));
	return (res);
}

int getSharpMeasure (BYTE* img, int width, int height, int t) {

	float res=0;
	double sumMCU=0;
	int numMCUx = width/32; /*covers 1/4 of width - width must be even*/
    	int numMCUy = height/32; /*covers 1/4 of height- height must be even*/
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
	return (roundf(res*10000)); /*round to int (4 digit precision)*/
}



int getFocusVal (struct focusData *AFdata) {
    	int step = 20;
    	int step2 = 2;
    
	switch (AFdata->flag) {
	    /*--------- first time - run sharpness algorithm -----------------*/
	    if(AFdata->ind >= 20) {
		printf ("WARNING ind=%d exceeds 20\n",AFdata->ind);
		AFdata->ind = 10;
	    }
	    case 0: /*sample left to right*/
	    	AFdata->arr_sharp[AFdata->ind] = AFdata->sharpness;
		AFdata->arr_foc[AFdata->ind] = AFdata->focus;
		if (AFdata->focus > (AFdata->right - step)) { /*get left and right from arr_sharp*/	
			bubble_sort(AFdata,AFdata->ind);
		       	/*get a window around the best value*/
			AFdata->left = (AFdata->arr_foc[0]- step/2);
			AFdata->right = (AFdata->arr_foc[0] + step/2);
			if (AFdata->left < 0) AFdata->left=0;
			if (AFdata->right > 255) AFdata->right=255;
			AFdata->focus = AFdata->left;
		    	AFdata->ind=0;
			AFdata->flag = 1;
		} else { 
		    	AFdata->focus=AFdata->arr_foc[AFdata->ind] + step; /*next focus*/
			AFdata->ind=AFdata->ind+1;;
			AFdata->flag = 0;
		}
		
	    	break;
	    case 1: /*sample left to right*/ 
	    	AFdata->arr_sharp[AFdata->ind] = AFdata->sharpness;
		AFdata->arr_foc[AFdata->ind] = AFdata->focus;
		if (AFdata->focus > (AFdata->right - step2)) { /*get left and right from arr_sharp*/	
			bubble_sort(AFdata,AFdata->ind);
		       	/*get the best value*/
			AFdata->focus = AFdata->arr_foc[0];
			AFdata->focus_sharpness = AFdata->arr_sharp[0];
			AFdata->flag = 2;
		} else { 
		    	AFdata->focus=AFdata->arr_foc[AFdata->ind] + step2; /*next focus*/
			AFdata->ind=AFdata->ind+1;;
			AFdata->flag = 1;
		}
		
	    	break;
	    case 2: /* set treshold in order to sharpness*/
		if (AFdata->setFocus) {
			AFdata->setFocus = 0;
		    	AFdata->flag= 0;
		    	AFdata->right = 255;
		    	AFdata->left = 8;
		    	AFdata->ind = 0;
			AFdata->FSi = 0;
		} else {
		    if ((abs(AFdata->sharpness - AFdata->focus_sharpness) > 200) &&
			 (abs(AFdata->sharpness - AFdata->focus_sharpness) < 500)) {
			AFdata->right=AFdata->focus + 10;
			AFdata->left=AFdata->focus - 10;
			AFdata->flag = 1;
			AFdata->ind = 0;
			AFdata->FSi = 0;
			AFdata->focus = AFdata->left;
		    } else if (abs(AFdata->sharpness - AFdata->focus_sharpness) > 500) {
			AFdata->right=255;
			AFdata->left=8;
			AFdata->flag = 0;
			AFdata->ind = 0;
			AFdata->FSi = 0;
			AFdata->focus = 8;
		    } else {
			if(AFdata->FSi < SHARP_SAMP) {
				/* get the sharpness average from first values while in focus*/
				AFdata->FS[AFdata->FSi] = AFdata->sharpness;
				int i;
				int temp = 0;
				for (i=0;i<=AFdata->FSi;i++)
					temp += AFdata->FS[i];
				AFdata->focus_sharpness = temp / (AFdata->FSi + 1);
				AFdata->FSi++;
			}
		    }
	   	//~ if (AFdata->focus_sharpness < 2500) {
	    		//~ if (abs(AFdata->sharpness - AFdata->focus_sharpness) > treshold_2500) {
				//~ AFdata->right=255;
				//~ AFdata->left=8;
				//~ AFdata->flag = 0;
				//~ AFdata->ind = 0;
				//~ AFdata->FSi = 0;
				//~ AFdata->focus = 0;
			//~ } else {
				//~ if(AFdata->FSi < SHARP_SAMP) {
				    	//~ /* get the sharpness average from first values while in focus*/
					//~ AFdata->FS[AFdata->FSi] = AFdata->sharpness;
				    	//~ int i;
				    	//~ int temp = 0;
				    	//~ for (i=0;i<=AFdata->FSi;i++)
				    		//~ temp += AFdata->FS[i];
				    	//~ AFdata->focus_sharpness = temp / (AFdata->FSi + 1);
				    	//~ AFdata->FSi++;
				//~ }
			//~ }
		//~ } else if (AFdata->focus_sharpness < 3500) {
			//~ if (abs(AFdata->sharpness - AFdata->focus_sharpness) > treshold_3500) {
			    	//~ AFdata->right=255;
				//~ AFdata->left=8;
				//~ AFdata->flag = 0;
				//~ AFdata->ind = 0;
			    	//~ AFdata->FSi = 0;
				//~ AFdata->focus = 0;
			//~ } else {
				//~ if(AFdata->FSi < SHARP_SAMP) {
				    	//~ /* get the sharpness average from first values while in focus*/
					//~ AFdata->FS[AFdata->FSi] = AFdata->sharpness;
				    	//~ int i;
				    	//~ int temp = 0;
				    	//~ for (i=0;i<=AFdata->FSi;i++)
				    		//~ temp += AFdata->FS[i];
				    	//~ AFdata->focus_sharpness = temp / (AFdata->FSi + 1);
				    	//~ AFdata->FSi++;
				//~ }
			//~ }
		//~ } else if (AFdata->focus_sharpness < 4500) {
			//~ if (abs(AFdata->sharpness - AFdata->focus_sharpness) > treshold_4500) {
			    	//~ AFdata->right=255;
				//~ AFdata->left=8;
				//~ AFdata->flag = 0;
				//~ AFdata->ind = 0;
			    	//~ AFdata->FSi = 0;
				//~ AFdata->focus = 0;
			//~ } else {
				//~ if(AFdata->FSi < SHARP_SAMP) {
				    	//~ /* get the sharpness average from first values while in focus*/
					//~ AFdata->FS[AFdata->FSi] = AFdata->sharpness;
				    	//~ int i;
				    	//~ int temp = 0;
				    	//~ for (i=0;i<=AFdata->FSi;i++)
				    		//~ temp += AFdata->FS[i];
				    	//~ AFdata->focus_sharpness = temp / (AFdata->FSi + 1);
				    	//~ AFdata->FSi++;
				//~ }
			//~ }
		//~ } else if (AFdata->focus_sharpness < 5500) {
			//~ if (abs(AFdata->sharpness - AFdata->focus_sharpness) > treshold_5500) {
			    	//~ AFdata->right=255;
				//~ AFdata->left=8;
				//~ AFdata->flag = 0;
				//~ AFdata->ind = 0;
			    	//~ AFdata->FSi = 0;
				//~ AFdata->focus = 0;
			//~ } else {
				//~ if(AFdata->FSi < SHARP_SAMP) {
				    	//~ /* get the sharpness average from first values while in focus*/
					//~ AFdata->FS[AFdata->FSi] = AFdata->sharpness;
				    	//~ int i;
				    	//~ int temp = 0;
				    	//~ for (i=0;i<=AFdata->FSi;i++)
				    		//~ temp += AFdata->FS[i];
				    	//~ AFdata->focus_sharpness = temp / (AFdata->FSi + 1);
				    	//~ AFdata->FSi++;
				//~ }
			//~ }
		//~ } else if (AFdata->focus_sharpness < 6500) {
			//~ if (abs(AFdata->sharpness - AFdata->focus_sharpness) > treshold_6500) {
			    	//~ AFdata->right=255;
				//~ AFdata->left=8;
				//~ AFdata->flag = 0;
				//~ AFdata->ind = 0;
			    	//~ AFdata->FSi = 0;
				//~ AFdata->focus = 0;
			//~ } else {
				//~ if(AFdata->FSi < SHARP_SAMP) {
				    	//~ /* get the sharpness average from first values while in focus*/
					//~ AFdata->FS[AFdata->FSi] = AFdata->sharpness;
				    	//~ int i;
				    	//~ int temp = 0;
				    	//~ for (i=0;i<=AFdata->FSi;i++)
				    		//~ temp += AFdata->FS[i];
				    	//~ AFdata->focus_sharpness = temp / (AFdata->FSi + 1);
				    	//~ AFdata->FSi++;
				//~ }
			//~ }
		//~ } else if (AFdata->focus_sharpness < 7500) {
			//~ if (abs(AFdata->sharpness - AFdata->focus_sharpness) > treshold_7500) {
			    	//~ AFdata->right=255;
				//~ AFdata->left=8;
				//~ AFdata->flag = 0;
				//~ AFdata->ind = 0;
			    	//~ AFdata->FSi = 0;
				//~ AFdata->focus = 0;
			//~ } else {
				//~ if(AFdata->FSi < SHARP_SAMP) {
				    	//~ /* get the sharpness average from first values while in focus*/
					//~ AFdata->FS[AFdata->FSi] = AFdata->sharpness;
				    	//~ int i;
				    	//~ int temp = 0;
				    	//~ for (i=0;i<=AFdata->FSi;i++)
				    		//~ temp += AFdata->FS[i];
				    	//~ AFdata->focus_sharpness = temp / (AFdata->FSi + 1);
				    	//~ AFdata->FSi++;
				//~ }
			//~ }
		//~ }  else if (AFdata->focus_sharpness < 8500) {
			//~ if (abs(AFdata->sharpness - AFdata->focus_sharpness) > treshold_8500) {
			    	//~ AFdata->right=255;
				//~ AFdata->left=8;
				//~ AFdata->flag = 0;
				//~ AFdata->ind = 0;
			    	//~ AFdata->FSi = 0;
				//~ AFdata->focus = 0;
			//~ } else {
				//~ if(AFdata->FSi < SHARP_SAMP) {
				    	//~ /* get the sharpness average from first values while in focus*/
					//~ AFdata->FS[AFdata->FSi] = AFdata->sharpness;
				    	//~ int i;
				    	//~ int temp = 0;
				    	//~ for (i=0;i<=AFdata->FSi;i++)
				    		//~ temp += AFdata->FS[i];
				    	//~ AFdata->focus_sharpness = temp / (AFdata->FSi + 1);
				    	//~ AFdata->FSi++;
				//~ }
			//~ }
		//~ } else {
			//~ if (abs(AFdata->sharpness - AFdata->focus_sharpness) > treshold_9999) {
			    	//~ AFdata->right=255;
				//~ AFdata->left=8;
				//~ AFdata->flag = 0;
				//~ AFdata->ind = 0;
			    	//~ AFdata->FSi = 0;
				//~ AFdata->focus = 0;
			//~ } else {
				//~ if(AFdata->FSi < SHARP_SAMP) {
				    	//~ /* get the sharpness average from first values while in focus*/
					//~ AFdata->FS[AFdata->FSi] = AFdata->sharpness;
				    	//~ int i;
				    	//~ int temp = 0;
				    	//~ for (i=0;i<=AFdata->FSi;i++)
				    		//~ temp += AFdata->FS[i];
				    	//~ AFdata->focus_sharpness = temp / (AFdata->FSi + 1);
				    	//~ AFdata->FSi++;
				//~ }
			//~ }
		//~ }
		}
	    	break;	   
		  
	}	
    	
    	/*clip focus, right and left*/
    	AFdata->focus=(AFdata->focus>255)?255:((AFdata->focus<0)?0:AFdata->focus);
	AFdata->right=(AFdata->right>255)?255:((AFdata->right<0)?0:AFdata->right);
    	AFdata->left=(AFdata->left>255)?255:((AFdata->left<0)?0:AFdata->left);
    
	return AFdata->focus;
}
