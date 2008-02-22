#ifndef PROTOTYPE_H
#define PROTOTYPE_H
#include "config.h"

void * initialization (struct JPEG_ENCODER_STRUCTURE * jpeg, UINT32 image_format, UINT32 image_width, UINT32 image_height);

UINT16 DSP_Division (UINT32 numer, UINT32 denom);
void* initialize_quantization_tables (UINT32 quality_factor);

UINT8* write_markers (UINT8 * output_ptr, UINT32 image_format,int huff, UINT32 image_width, UINT32 image_height);

UINT8* encode_image (UINT8 * input_ptr,UINT8 * output_ptr, UINT32 quality_factor, UINT32 image_format,int huff, UINT32 image_width, UINT32 image_height);

void* read_400_format (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, UINT8 *input_ptr);
void* read_420_format (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, UINT8 *input_ptr);
void* read_422_format (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, UINT8 *input_ptr);
void* read_444_format (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, UINT8 *input_ptr);
void* RGB_2_444 (UINT8 *input_ptr, UINT8 *output_ptr, UINT32 image_width, UINT32 image_height);

UINT8* encodeMCU (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, UINT32 image_format, UINT8 *output_ptr);

void* levelshift (INT16* const data);
void* DCT (INT16 *data);
void* quantization (INT16* const data, UINT16* const quant_table_ptr);
UINT8* huffman (struct JPEG_ENCODER_STRUCTURE * jpeg_encoder_structure, UINT16 component, UINT8 *output_ptr);

UINT8* close_bitstream (UINT8 *output_ptr);
//void* jemalloc(size_t);
//void* jefree(void);
#endif
