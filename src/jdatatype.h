#ifndef JDATATYPE_H
#define JDATATYPE_H

#include "datatype.h"

typedef struct JPEG_ENCODER_STRUCTURE
{
	UINT16	mcu_width;
	UINT16	mcu_height;
	UINT16	horizontal_mcus;
	UINT16	vertical_mcus;

	UINT16	rows;
	UINT16	cols;

	UINT16	length_minus_mcu_width;
	UINT16	length_minus_width;
	UINT16	incr;
	UINT16	mcu_width_size;
	UINT16	offset;

	INT16 ldc1;
	INT16 ldc2;
	INT16 ldc3;
	
	UINT32 lcode;
	UINT16 bitindex;
	/* MCUs */
	INT16	Y1 [64];
	INT16	Y2 [64];
	INT16	Temp [64];
	INT16	CB [64];
	INT16	CR [64];
	/* Quantization Tables */
	UINT8	Lqt [64];
	UINT8	Cqt [64];
	UINT16	ILqt [64];
	UINT16	ICqt [64];

} _JPEG_ENCODER_STRUCTURE;

#endif
