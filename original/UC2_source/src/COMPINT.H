// Copyright 1992, all rights reserved, AIP, Nico de Vries
// COMPINT.H

extern WORD (far pascal *CReader)(BYTE far *pbBuffer, WORD wSize);
extern void (far pascal *CWriter)(BYTE far *pbBuffer, WORD wSize);

WORD pascal far Compressor (
   WORD wMethod,         // defined in SUPERMAN.H
   WORD (far pascal *Reader)(BYTE far *pbBuffer, WORD wSize),
   void (far pascal *Writer)(BYTE far *pbBuffer, WORD wSize),
   DWORD dwMaster
);

WORD pascal far Decompressor (
   WORD wMethod,
   WORD (far pascal *Reader)(BYTE far *pbBuffer, WORD wSize),
   void (far pascal *Writer)(BYTE far *pbBuffer, WORD wSize),
   DWORD dwMaster,
   DWORD len
);

#define CDRET_OK  0 // no problems
#define CDRET_MEM 1 // not enough memory
#define CDRET_DER 2 // decompression data error
#define CDRET_TER 3 // unknown method

/* function parameter description :
   Reader should read 1..pwSize bytes into pbBuffer. The number of actually
   read bytes should be returned. If there is no more input Reader should
   return 0. On decompression always excactly pwSize bytes should be read
   the decompressor makes sure this is possible.

   Writer writes :-)
*/

struct DeltaBlah {
   BYTE size;
   BYTE ctr;
   BYTE arra[8];
};

void InitDelta (DeltaBlah *db, BYTE type);
void Delta (DeltaBlah *db, BYTE far *pbData, WORD size);
void UnDelta (DeltaBlah *db, BYTE far *pbData, WORD size);
