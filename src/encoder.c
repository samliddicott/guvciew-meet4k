//#include "datatype.h"
//#include "jdatatype.h"
//#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "prototype.h"

UINT8	Lqt [BLOCK_SIZE];
UINT8	Cqt [BLOCK_SIZE];
UINT16	ILqt [BLOCK_SIZE];
UINT16	ICqt [BLOCK_SIZE];

INT16	Y1 [BLOCK_SIZE];
INT16	Y2 [BLOCK_SIZE];
INT16	Y3 [BLOCK_SIZE];
INT16	Y4 [BLOCK_SIZE];
INT16	CB [BLOCK_SIZE];
INT16	CR [BLOCK_SIZE];
INT16	Temp [BLOCK_SIZE];
UINT32 lcode = 0;
UINT16 bitindex = 0;
struct JPEG_ENCODER_STRUCTURE *jpeg_encoder_structure=NULL; 

void (*read_format) (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, UINT8 *input_ptr);

void* initialization (struct JPEG_ENCODER_STRUCTURE * jpeg, UINT32 image_format, UINT32 image_width, UINT32 image_height)
{
	UINT16 mcu_width, mcu_height, bytes_per_pixel;

	if (image_format == FOUR_ZERO_ZERO || image_format == FOUR_FOUR_FOUR)
	{
		jpeg->mcu_width = mcu_width = 8;
		jpeg->mcu_height = mcu_height = 8;

		jpeg->horizontal_mcus = (UINT16) ((image_width + mcu_width - 1) >> 3);
		jpeg->vertical_mcus = (UINT16) ((image_height + mcu_height - 1) >> 3);

		if (image_format == FOUR_ZERO_ZERO)
		{
			bytes_per_pixel = 1;
			read_format = read_400_format;
		}
		else
		{
			bytes_per_pixel = 3;
			read_format = read_444_format;
		}
	}
	else
	{
		jpeg->mcu_width = mcu_width = 16;
		jpeg->horizontal_mcus = (UINT16) ((image_width + mcu_width - 1) >> 4);

		if (image_format == FOUR_TWO_ZERO)
		{
			jpeg->mcu_height = mcu_height = 16;
			jpeg->vertical_mcus = (UINT16) ((image_height + mcu_height - 1) >> 4);

			bytes_per_pixel = 3;
			read_format = read_420_format;
		}
		else
		{
			jpeg->mcu_height = mcu_height = 8;
			jpeg->vertical_mcus = (UINT16) ((image_height + mcu_height - 1) >> 3);

			bytes_per_pixel = 2;
			read_format = read_422_format;
		}
	}

	jpeg->rows_in_bottom_mcus = (UINT16) (image_height - (jpeg->vertical_mcus - 1) * mcu_height);
	jpeg->cols_in_right_mcus = (UINT16) (image_width - (jpeg->horizontal_mcus - 1) * mcu_width);

	jpeg->length_minus_mcu_width = (UINT16) ((image_width - mcu_width) * bytes_per_pixel);
	jpeg->length_minus_width = (UINT16) ((image_width - jpeg->cols_in_right_mcus) * bytes_per_pixel);

	jpeg->mcu_width_size = (UINT16) (mcu_width * bytes_per_pixel);

	if (image_format != FOUR_TWO_ZERO)
		jpeg->offset = (UINT16) ((image_width * (mcu_height - 1) - (mcu_width - jpeg->cols_in_right_mcus)) * bytes_per_pixel);
	else
		jpeg->offset = (UINT16) ((image_width * ((mcu_height >> 1) - 1) - (mcu_width - jpeg->cols_in_right_mcus)) * bytes_per_pixel);

	jpeg->ldc1 = 0;
	jpeg->ldc2 = 0;
	jpeg->ldc3 = 0;

	
}


UINT8* encode_image (UINT8 *input_ptr,UINT8 *output_ptr, UINT32 quality_factor, UINT32 image_format,int huff, UINT32 image_width, UINT32 image_height)
{
	UINT16 i, j;

	/*set global variables*/
	if((jpeg_encoder_structure=(struct JPEG_ENCODER_STRUCTURE *) calloc(1, sizeof(struct JPEG_ENCODER_STRUCTURE)))==NULL){
   		printf("couldn't allocate memory for: global\n");
		exit(1); 
    }
	

	if (image_format == RGB)
	{
		image_format = FOUR_FOUR_FOUR;
		RGB_2_444 (input_ptr, output_ptr, image_width, image_height);
	}

	/* Initialization of JPEG control structure */
	initialization (jpeg_encoder_structure,image_format,image_width,image_height);

	/* Quantization Table Initialization */
	initialize_quantization_tables (quality_factor);

	/* Writing Marker Data */
	output_ptr = write_markers (output_ptr, image_format, huff, image_width, image_height);

	for (i=1; i<=jpeg_encoder_structure->vertical_mcus; i++)
	{
		if (i < jpeg_encoder_structure->vertical_mcus)
			jpeg_encoder_structure->rows = jpeg_encoder_structure->mcu_height;
		else
			jpeg_encoder_structure->rows = jpeg_encoder_structure->rows_in_bottom_mcus;

		for (j=1; j<=jpeg_encoder_structure->horizontal_mcus; j++)
		{
			if (j < jpeg_encoder_structure->horizontal_mcus)
			{
				jpeg_encoder_structure->cols = jpeg_encoder_structure->mcu_width;
				jpeg_encoder_structure->incr = jpeg_encoder_structure->length_minus_mcu_width;
			}
			else
			{
				jpeg_encoder_structure->cols = jpeg_encoder_structure->cols_in_right_mcus;
				jpeg_encoder_structure->incr = jpeg_encoder_structure->length_minus_width;
			}

			read_format (jpeg_encoder_structure, input_ptr);

			/* Encode the data in MCU */
			output_ptr = encodeMCU (jpeg_encoder_structure, image_format, output_ptr);

			input_ptr += jpeg_encoder_structure->mcu_width_size;
		}

		input_ptr += jpeg_encoder_structure->offset;
	}

	/* Close Routine */
	close_bitstream (output_ptr);
	free(jpeg_encoder_structure);
	return output_ptr;
}

UINT8* encodeMCU (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, UINT32 image_format, UINT8 *output_ptr)
{
	levelshift (Y1);
	DCT (Y1);
	quantization (Y1, ILqt);
	output_ptr = huffman (jpeg_encoder_structure, 1, output_ptr);

	if (image_format == FOUR_TWO_ZERO || image_format == FOUR_TWO_TWO)
	{
		levelshift (Y2);
		DCT (Y2);
		quantization (Y2, ILqt);
		output_ptr = huffman (jpeg_encoder_structure, 1, output_ptr);

		if (image_format == FOUR_TWO_ZERO)
		{
			levelshift (Y3);
			DCT (Y3);
			quantization (Y3, ILqt);
			output_ptr = huffman (jpeg_encoder_structure, 1, output_ptr);

			levelshift (Y4);
			DCT (Y4);
			quantization (Y4, ILqt);
			output_ptr = huffman (jpeg_encoder_structure, 1, output_ptr);
		}
	}

	if (image_format != FOUR_ZERO_ZERO)
	{
		levelshift (CB);
		DCT (CB);
		quantization (CB, ICqt);
		output_ptr = huffman (jpeg_encoder_structure, 2, output_ptr);

		levelshift (CR);
		DCT (CR);
		quantization (CR, ICqt);
		output_ptr = huffman (jpeg_encoder_structure, 3, output_ptr);
	}
	return output_ptr;
}
