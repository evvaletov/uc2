/*

   Comp_TT.cpp                                   Danny Bezemer


   (C) Copyright 1992, all rights reserved, AIP

*/

#include <mem.h>
#include <alloc.h>
#include "main.h"
#include "compint.h"
#include "comp_tt.h"
#include "video.h"
#include "diverse.h"
#include "fletch.h"
#include "vmem.h"
#include "superman.h"
#include "neuroman.h"

/*
   Externals
*/
extern WORD (far pascal *CReader)(BYTE far *pbBuffer, WORD wSize);
extern void (far pascal *CWriter)(BYTE far *pbBuffer, WORD wSize);


void pascal far TurboCompressor(DWORD dwMaster)
{
   int i, j;

   WORD BytesRead, BytesToWrite;
   BYTE far *Control, chPrev = 0, chPrevPrev = 0;
   BYTE far *SmallInBuffer, far *HashTable;
   BYTE far *InBuffer, far *OutBuffer;

   HashTable = (BYTE *)xmalloc(HASH_TABLE_SIZE,TMP);

   {
      BYTE far* tmp = (BYTE*) xmalloc (64000U, TMP);
      memset(HashTable,0,HASH_TABLE_SIZE);
      WORD wLen;
      Transfer (tmp, &wLen, dwMaster);
      if (wLen>10){
	 wLen-=10;
	 for (WORD i=0;i<wLen;i++){
	    HashTable[Hash(tmp[i],tmp[i+1])] = tmp[i+2];
	 }
      }
      xfree (tmp, TMP);
   }

   FletchInit (&Fin);
   FletchInit (&Fout);

   InBuffer  = (BYTE *)xmalloc(BLOCK_SIZE,TMP);
   OutBuffer = (BYTE *)xmalloc(BLOCK_SIZE+MAX_EXTRA_BYTES,TMP);
   BytesRead = CReader(InBuffer,BLOCK_SIZE);
   FletchUpdate (&Fin, InBuffer, BytesRead);
   while (BytesRead != 0)
   {
      BytesToWrite = 0;
      for(i = 0;i < BytesRead;i += 8)
      {
	 SmallInBuffer = InBuffer + i;
	 Control = OutBuffer + BytesToWrite++;
	 *Control = 0;
	 for(j = 0;(j < 8) && (i+j < BytesRead);j++)
	 {
	    if (SmallInBuffer[j] == HashTable[Hash(chPrevPrev,chPrev)])
	       *Control |= (BYTE)(1 << (8-1 - j));
	    else {
	       HashTable[Hash(chPrevPrev,chPrev)] = SmallInBuffer[j];
	       OutBuffer[BytesToWrite++] = SmallInBuffer[j];
	    }
	    chPrevPrev = chPrev;
	    chPrev = SmallInBuffer[j];
	 }
      }
      CWriter((BYTE *)&BytesToWrite,2);
//      FletchUpdate (&Fout,(BYTE *)&BytesToWrite,2);
      CWriter(OutBuffer,BytesToWrite);
//      FletchUpdate (&Fout,OutBuffer,BytesToWrite);
      BytesRead = CReader(InBuffer,BLOCK_SIZE);
      FletchUpdate (&Fin, InBuffer, BytesRead);
      Hint();
   }
   CWriter((BYTE *)&BytesRead,2);
//   FletchUpdate (&Fout,(BYTE *)&BytesRead,2);
   xfree(HashTable,TMP);
   xfree(InBuffer,TMP);
   xfree (OutBuffer,TMP);
}


void pascal far TurboDecompressor(DWORD dwMaster)
{
   int i;
   WORD BytesToRead, BytesWritten;
   BYTE Control, chPrev = 0, chPrevPrev = 0;
   BYTE far *InBuffer, far *OutBuffer;
   BYTE far *HashTable, far *pNextByte;

   HashTable = (BYTE *)xmalloc(HASH_TABLE_SIZE,TMP);

   {
      BYTE far* tmp = (BYTE*) xmalloc (64000U, TMP);
      memset(HashTable,0,HASH_TABLE_SIZE);
      WORD wLen;
      Transfer (tmp, &wLen, dwMaster);
      if (wLen>10){
	 wLen-=10;
	 for (WORD i=0;i<wLen;i++){
	    HashTable[Hash(tmp[i],tmp[i+1])] = tmp[i+2];
	 }
      }
      xfree (tmp, TMP);
   }

   FletchInit (&Fin);
   FletchInit (&Fout);

   InBuffer  = (BYTE *)xmalloc(BLOCK_SIZE + MAX_EXTRA_BYTES,TMP);
   OutBuffer = (BYTE *)xmalloc(BLOCK_SIZE,TMP);

   CReader((BYTE *)&BytesToRead,2);
//   FletchUpdate (&Fin, (BYTE *)&BytesToRead,no);
   while (BytesToRead != 0)
   {
      BytesWritten = 0;
      pNextByte = InBuffer;
      CReader(InBuffer,BytesToRead);
//      FletchUpdate (&Fin, InBuffer, no);
      while (pNextByte - InBuffer < BytesToRead)
      {
	 Control = *pNextByte++;
	 for(i = 0;(i < 8) &&
		   ((pNextByte-InBuffer < BytesToRead) || Control);i++)
	 {
	    if (Control & 0x80)
	       OutBuffer[BytesWritten] = HashTable[Hash(chPrevPrev,chPrev)];
	    else {
	       HashTable[Hash(chPrevPrev,chPrev)] = *pNextByte;
	       OutBuffer[BytesWritten] = *pNextByte++;
	    }

	    chPrevPrev = chPrev;
	    chPrev = OutBuffer[BytesWritten++];
	    Control <<= 1;
	 }
      }
      CWriter(OutBuffer,BytesWritten);
      FletchUpdate (&Fout,OutBuffer,BytesWritten);
      Hint();
      CReader((BYTE *)&BytesToRead,2);
//      FletchUpdate (&Fin, (BYTE *)&BytesToRead, no);
   }
   xfree(HashTable,TMP);
   xfree(OutBuffer,TMP);
   xfree(InBuffer,TMP);
}
