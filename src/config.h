#ifndef CONFIG_H
#define CONFIG_H
#include "jdatatype.h"

#define		FOUR_ZERO_ZERO			0
#define		FOUR_TWO_ZERO			1
#define		FOUR_TWO_TWO			2
#define		FOUR_FOUR_FOUR			3
#define		RGB						4

#define		BLOCK_SIZE				64


extern UINT8	Lqt [BLOCK_SIZE];
extern UINT8	Cqt [BLOCK_SIZE];
extern UINT16	ILqt [BLOCK_SIZE];
extern UINT16	ICqt [BLOCK_SIZE];

extern INT16	Y1 [BLOCK_SIZE];
extern INT16	Y2 [BLOCK_SIZE];
extern INT16	Y3 [BLOCK_SIZE];
extern INT16	Y4 [BLOCK_SIZE];
extern INT16	CB [BLOCK_SIZE];
extern INT16	CR [BLOCK_SIZE];
extern INT16	Temp [BLOCK_SIZE];

extern UINT32 lcode;
extern UINT16 bitindex;

#endif
