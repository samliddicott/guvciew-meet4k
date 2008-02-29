#include "prototype.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>


UINT8* read_422_format (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, UINT8 *input_ptr)
{
	INT32 i, j;
	
	
	
	INT16 *Y1_Ptr = jpeg_encoder_structure->Y1; /*64 int16 block*/ 
	INT16 *Y2_Ptr = jpeg_encoder_structure->Y2;
	INT16 *CB_Ptr = jpeg_encoder_structure->CB;
	INT16 *CR_Ptr = jpeg_encoder_structure->CR;

	UINT16 incr = jpeg_encoder_structure->incr;
	
	UINT8 *tmp_ptr=NULL;
	tmp_ptr=input_ptr;
	
	for (i=8; i>0; i--) /*8 rows*/
	{
		for (j=4; j>0; j--) /* 8 cols*/
		{
			*Y1_Ptr++ = *tmp_ptr++;
			*CB_Ptr++ = *tmp_ptr++;
			*Y1_Ptr++ = *tmp_ptr++;
			*CR_Ptr++ = *tmp_ptr++;
		}

		for (j=4; j>0; j--) /* 8 cols*/
		{
			*Y2_Ptr++ = *tmp_ptr++;
			*CB_Ptr++ = *tmp_ptr++;
			*Y2_Ptr++ = *tmp_ptr++;
			*CR_Ptr++ = *tmp_ptr++;
		}


		tmp_ptr += incr; /* next row (width - mcu_width)*/
	}
	tmp_ptr=NULL;/*clean*/
	return (input_ptr);
}

