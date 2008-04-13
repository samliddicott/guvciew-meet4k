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
#  entropy huffman encoder for Jpeg encoder                                     #
#                                                                               # 
#  Adapted for linux, Paulo Assis, 2007 <pj.assis@gmail.com>                    #
********************************************************************************/

#include "prototype.h"
#include "huffdata.h"

#define PUTBITS	\
{	\
	bits_in_next_word = (INT16) (jpeg_encoder_structure->bitindex + numbits - 32);	\
	if (bits_in_next_word < 0)	\
	{	\
		jpeg_encoder_structure->lcode = (jpeg_encoder_structure->lcode << numbits) | data;	\
		jpeg_encoder_structure->bitindex += numbits;	\
	}	\
	else	\
	{	\
		jpeg_encoder_structure->lcode = (jpeg_encoder_structure->lcode << (32 - jpeg_encoder_structure->bitindex)) | (data >> bits_in_next_word);	\
		if ((*output_ptr++ = (UINT8)(jpeg_encoder_structure->lcode >> 24)) == 0xff)	\
			*output_ptr++ = 0;	\
		if ((*output_ptr++ = (UINT8)(jpeg_encoder_structure->lcode >> 16)) == 0xff)	\
			*output_ptr++ = 0;	\
		if ((*output_ptr++ = (UINT8)(jpeg_encoder_structure->lcode >> 8)) == 0xff)	\
			*output_ptr++ = 0;	\
		if ((*output_ptr++ = (UINT8) jpeg_encoder_structure->lcode) == 0xff)	\
			*output_ptr++ = 0;	\
		jpeg_encoder_structure->lcode = data;	\
		jpeg_encoder_structure->bitindex = bits_in_next_word;	\
	}	\
}

UINT8* huffman (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, UINT16 component, UINT8 *output_ptr)
{
	UINT16 i;
	UINT16 *DcCodeTable, *DcSizeTable, *AcCodeTable, *AcSizeTable;

	INT16 *Temp_Ptr, Coeff, LastDc;
	UINT16 AbsCoeff, HuffCode, HuffSize, RunLength=0, DataSize=0, index;

	INT16 bits_in_next_word;
	UINT16 numbits;
	UINT32 data;

	Temp_Ptr = jpeg_encoder_structure->Temp;
	Coeff = *Temp_Ptr++;/* Coeff = DC */
    
	/* code DC - Temp[0] */
	if (component == 1)/* luminance - Y */
	{
		DcCodeTable = luminance_dc_code_table;
		DcSizeTable = luminance_dc_size_table;
		AcCodeTable = luminance_ac_code_table;
		AcSizeTable = luminance_ac_size_table;

		LastDc = jpeg_encoder_structure->ldc1;
		jpeg_encoder_structure->ldc1 = Coeff;
	}
	else /* Chrominance - U V */
	{
		DcCodeTable = chrominance_dc_code_table;
		DcSizeTable = chrominance_dc_size_table;
		AcCodeTable = chrominance_ac_code_table;
		AcSizeTable = chrominance_ac_size_table;

		if (component == 2) /* Chrominance - U */
		{
			LastDc = jpeg_encoder_structure->ldc2;
			jpeg_encoder_structure->ldc2 = Coeff;
		}
		else/* Chrominance - V */
		{
			LastDc = jpeg_encoder_structure->ldc3;
			jpeg_encoder_structure->ldc3 = Coeff;
		}
	}

	Coeff = Coeff - LastDc; /* DC - LastDC */

	AbsCoeff = (Coeff < 0) ? -(Coeff--) : Coeff;

	/*calculate data size*/
	while (AbsCoeff != 0)
	{
		AbsCoeff >>= 1;
		DataSize++;
	}

	HuffCode = DcCodeTable [DataSize];
	HuffSize = DcSizeTable [DataSize];

	Coeff &= (1 << DataSize) - 1;
	data = (HuffCode << DataSize) | Coeff;
	numbits = HuffSize + DataSize;

	PUTBITS
		
    /* code AC */
	for (i=63; i>0; i--)
	{
		
		if ((Coeff = *Temp_Ptr++) != 0)
		{
			while (RunLength > 15)
			{
				RunLength -= 16;
				data = AcCodeTable [161];   /* ZRL 0xF0 ( 16 - 0) */
				numbits = AcSizeTable [161];/* ZRL                */
				PUTBITS
			}

			AbsCoeff = (Coeff < 0) ? -(Coeff--) : Coeff;

			if (AbsCoeff >> 8 == 0) /* Size <= 8 bits */
				DataSize = bitsize [AbsCoeff];
			else /* 16 => Size => 8 */
				DataSize = bitsize [AbsCoeff >> 8] + 8;

			index = RunLength * 10 + DataSize;
			
			
			HuffCode = AcCodeTable [index];
			HuffSize = AcSizeTable [index];

			Coeff &= (1 << DataSize) - 1;
			data = (HuffCode << DataSize) | Coeff;
			numbits = HuffSize + DataSize;
			
			PUTBITS
			RunLength = 0;
		}
		else
			RunLength++;/* Add while Zero */
	}

	if (RunLength != 0)
	{
		data = AcCodeTable [0];   /* EOB - 0x00 end of block */
		numbits = AcSizeTable [0];/* EOB                     */ 
		PUTBITS
	}
	return output_ptr;
}

/* For bit Stuffing and EOI marker */
UINT8* close_bitstream (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, UINT8 *output_ptr)
{
	UINT16 i, count;
	UINT8 *ptr;
	
	

	if (jpeg_encoder_structure->bitindex > 0)
	{
		jpeg_encoder_structure->lcode <<= (32 - jpeg_encoder_structure->bitindex);
		count = (jpeg_encoder_structure->bitindex + 7) >> 3;

		ptr = (UINT8 *) &jpeg_encoder_structure->lcode + 3;

		for (i=count; i>0; i--)
		{
			if ((*output_ptr++ = *ptr--) == 0xff)
				*output_ptr++ = 0;
		}
	}

	/* End of image marker (EOI) */
	*output_ptr++ = 0xFF;
	*output_ptr++ = 0xD9;
	return output_ptr;
}
