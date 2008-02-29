#ifndef PROTOTYPE_H
#define PROTOTYPE_H
#include "jdatatype.h"

void * initialization (struct JPEG_ENCODER_STRUCTURE * jpeg, int image_width,
					                                       int image_height);

UINT16 DSP_Division (UINT32 numer, UINT32 denom);

void* initialize_quantization_tables (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure);

UINT8* write_markers (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, 
					  UINT8 * output_ptr,int huff, UINT32 image_width, 
					                                       UINT32 image_height);

int encode_image (UINT8 * input_ptr,UINT8 * output_ptr, 
				  struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,
				             int huff, UINT32 image_width, UINT32 image_height);

UINT8* read_422_format (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, 
						UINT8 *input_ptr);

UINT8* encodeMCU (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, 
				                                             UINT8 *output_ptr);

void* levelshift (INT16* const data);

void* DCT (INT16 *data);

void* quantization (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, 
					          INT16* const data, UINT16* const quant_table_ptr);

UINT8* huffman (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, 
				                           UINT16 component, UINT8 *output_ptr);

UINT8* close_bitstream (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure,
						                                     UINT8 *output_ptr);
#endif
