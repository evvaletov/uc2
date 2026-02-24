/*

   Comp_TT.h                                     Danny Bezemer



   (C) Copyright 1992, all rights reserved, AIP

*/

#define HASH_TABLE_SIZE                32768U
#define BLOCK_SIZE                     25000U
#define MAX_EXTRA_BYTES                (BLOCK_SIZE/8)

//#define Hash(c1,c2)                    ((((c1)&0x7F)<<6) ^ (c2&0x7F))
#define Hash(c1,c2) (((c1)<<7) ^ c2)


/*
   Prototypes
*/
void far pascal TurboCompressor (DWORD dwMaster);
void far pascal TurboDecompressor (DWORD dwMaster);
