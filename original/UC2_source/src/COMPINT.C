// Copyright 1992, all rights reserved, AIP, Nico de Vries
// COMPINT.CPP

#include <alloc.h>
#include <mem.h>
#include "main.h"
#include "compint.h"
#include "vmem.h"
#include "superman.h"
#include "comp_tt.h"
#include "video.h"
#include "diverse.h"
#include "bitio.h"
#include "ultracmp.h"
#include "fletch.h"
#include "tree.h"
#include "llio.h"

BYTE bDelta;

//#define STORE_ALL // debug option

WORD (far pascal *CReader)(BYTE far *pbBuffer, WORD wSize);
void (far pascal *CWriter)(BYTE far *pbBuffer, WORD wSize);

#pragma argsused
void InitDelta (DeltaBlah *db, BYTE type){
   Out (7,"{{%d}}",bDelta);
   db->size = type;
   db->ctr = 0;
   for (int i=0;i<8;i++)
      db->arra[i] = 0;
}

void Delta (DeltaBlah *db, BYTE far *pbData, WORD size){
   for (WORD i=0;i<size;i++){
      BYTE tmp=pbData[i];
      pbData[i] = tmp - db->arra[db->ctr];
      db->arra[db->ctr] = tmp;
      if (++db->ctr==db->size) db->ctr=0;
   }
}

void UnDelta (DeltaBlah *db, BYTE far *pbData, WORD size){
   for (WORD i=0;i<size;i++){
      pbData[i] += db->arra[db->ctr];
      db->arra[db->ctr] = pbData[i];
      if (++db->ctr==db->size) db->ctr=0;
   }
}

/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       Optional CReader speedup.
*/

static DeltaBlah dbDelt;

WORD (far pascal *OldReader)(BYTE far *pbBuffer, WORD wSize);

WORD far pascal NewRead (BYTE far *pbBuffer, WORD wSize){
   WORD r=OldReader (pbBuffer, wSize);
   if (bDelta && r){
      Delta (&dbDelt, pbBuffer, wSize);
   }
   return r;
}

static void EnhanceRead (BYTE type){
   InitDelta (&dbDelt, type);
   OldReader = CReader;
   CReader = NewRead;
}

static void UnenhanceRead (){
}

#define T0_BLOCK 60000U

/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       General purpose datacompressor bridge.
*/

WORD (far pascal *RReader)(BYTE far *pbBuffer, WORD wSize);
DWORD dwInCtr;
WORD far pascal ARReader (BYTE far *pbBuffer, WORD wSize){
   WORD ret = (*RReader)(pbBuffer,wSize);
   dwInCtr+=ret;
   return ret;
}

#pragma argsused
WORD pascal far Compressor (
   WORD wMethod,         // defined in SUPERMAN.H
   WORD (far pascal *TReader)(BYTE far *pbBuffer, WORD wSize),
   void (far pascal *Writer)(BYTE far *pbBuffer, WORD wSize),
   DWORD dwMaster)
{
   WORD (far pascal *SReader)(BYTE far *pbBuffer, WORD wSize);
   WORD (far pascal *SRReader)(BYTE far *pbBuffer, WORD wSize);
   void (far pascal *SWriter)(BYTE far *pbBuffer, WORD wSize);
   DeltaBlah dbBack;
   BYTE SbDelta;
   WORD SwMethod;

   SReader = CReader;
   SRReader = RReader;
   SWriter = CWriter;
   SbDelta = bDelta;
   dbBack = dbDelt;
   SwMethod = wMethod;

   RReader = TReader;

   if (wMethod>=20 && wMethod<30){
      bDelta=1;
      wMethod-=20;
   } else if (wMethod>=30 && wMethod<40){
      bDelta = wMethod-29; // (1..8)
//      Out (7,"[[%d]]",bDelta); // QQQ
      wMethod=4;
   } else
      bDelta=0;
#ifdef STORE_ALL
   wMethod = 0;
#endif
   CReader = ARReader;
   CWriter = Writer;
   if (wMethod == 2){ // T1
//      TuneComp (10,3,3,25);
	TuneComp (15,2,15,25);
//      TuneComp (20,5,5,25);
//	TuneComp (40,25,5,25);
      if (bDelta){
	 EnhanceRead(bDelta);
	 UltraCompressor(dwMaster,bDelta);
	 UnenhanceRead();
      } else
	 UltraCompressor(dwMaster,bDelta);
   } else if (wMethod == 3){ // T2
	TuneComp (70,10,30,50);
//      TuneComp (140,50,7,50);
//      TuneComp (270,80,8,100);
//      TuneComp (135,40,6,100);
      if (bDelta){
	 EnhanceRead(bDelta);
	 UltraCompressor(dwMaster,bDelta);
	 UnenhanceRead();
      } else
	 UltraCompressor(dwMaster,bDelta);
   } else if (wMethod == 4){ // T3
//      TuneComp (5700,3900,400,380);
      TuneComp (600,50,40,100);
//      TuneComp (1100,300,200,120);
      if (bDelta){
	 EnhanceRead(bDelta);
	 UltraCompressor(dwMaster,bDelta);
	 UnenhanceRead();
      } else
	 UltraCompressor(dwMaster,bDelta);
   } else if (wMethod == 5){ // TQ
      TuneComp (10000,5000,200,100);
      if (bDelta){
	 EnhanceRead(bDelta);
	 UltraCompressor(dwMaster,bDelta);
	 UnenhanceRead();
      } else
	 UltraCompressor(dwMaster,bDelta);
   } else if (wMethod == 6){ // TQ
      TuneComp (60000U,60000U,500,500);
      if (bDelta){
	 EnhanceRead(bDelta);
	 UltraCompressor(dwMaster,bDelta);
	 UnenhanceRead();
      } else
	 UltraCompressor(dwMaster,bDelta);
   } else {
      IE();
   }
   CReader = SReader;
   RReader = SRReader;
   CWriter = SWriter;
   bDelta = SbDelta;
   dbDelt = dbBack;
   wMethod = SwMethod;
//   IOut ("[%04X]",Fletcher(&Fin));
   return CDRET_OK;
}

BYTE *bbbuf;
void (far pascal *OldWriter)(BYTE far *, WORD);
void far pascal NewWriter(BYTE far *pbBuffer, WORD w){
   if (w){
      RapidCopy (bbbuf, pbBuffer, w);
      UnDelta (&dbDelt, bbbuf, w);
      OldWriter (bbbuf, w);
   }
}

#pragma argsused
WORD pascal far Decompressor (
   WORD wMethod,
   WORD (far pascal *Reader)(BYTE far *pbBuffer, WORD wSize),
   void (far pascal *Writer)(BYTE far *pbBuffer, WORD wSize),
   DWORD dwMaster,
   DWORD len)
{
   WORD (far pascal *SReader)(BYTE far *pbBuffer, WORD wSize);
   void (far pascal *SWriter)(BYTE far *pbBuffer, WORD wSize);
   SReader = CReader;
   SWriter = CWriter;
#ifdef STORE_ALL
   wMethod = 0;
#endif
   CReader = Reader;
   CWriter = Writer;
   if (wMethod >=1 && wMethod <= 9){
      UltraDecompressor(dwMaster,0,len);
   } else if (wMethod>=30 && wMethod<40){
      bDelta = wMethod-29; // (1..8)
//      Out (7,"[[%d]]",bDelta); // QQQ
      InitDelta (&dbDelt, bDelta);
      OldWriter=CWriter;
      CWriter=NewWriter;
      bbbuf = xmalloc (33000L, TMP);
      UltraDecompressor(dwMaster,bDelta,len);
      xfree (bbbuf, TMP);
   } else if (wMethod >=21 && wMethod <= 29){
      InitDelta (&dbDelt, 1);
      OldWriter=CWriter;
      CWriter=NewWriter;
      bbbuf = xmalloc (33000L, TMP);
      UltraDecompressor(dwMaster,1,len);
      xfree (bbbuf, TMP);
   } else {
      Doing (NULL);
      Error ("central directory is damaged");
   }
   CReader = SReader;
   CWriter = SWriter;
//   IOut ("[%04X]",Fletcher(&Fout));
   return CDRET_OK;
}

void Analyze (int handle){
   BYTE *buf=xmalloc(16384,NTMP);
   BYTE *dbuf=xmalloc(16384,NTMP);
   WORD fq[NR_COMBINATIONS];
   BYTE ln[NR_COMBINATIONS];
   DeltaBlah db;
   WORD num=1024;

   DWORD fs=GetFileSize (handle);
   if (fs<1024) return;
   if (fs<num) num=(WORD)fs;
   Seek (handle, (fs-1024)/2);
   Read (buf, handle, 1024);

   RapidCopy (buf,dbuf,num);
   memset (fq, 0, NR_COMBINATIONS*2);
   for (int i=0;i<num;i++)
      fq[dbuf[i]]++;
   TreeGen (fq, NR_COMBINATIONS, 13, ln);
   DWORD tot=0;
   for (i=0;i<256;i++)
      tot+=fq[i]*ln[i];
   Out (7,"[0->%lu",(tot*100)/num);

   for (int n=0;n<8;n++){
      RapidCopy (buf,dbuf,num);
      InitDelta (&db, n);
      Delta (&db, dbuf, num);
      memset (fq, 0, NR_COMBINATIONS*2);
      for (i=0;i<num;i++)
	fq[dbuf[i]]++;
      TreeGen (fq, 256, 13, ln);
      DWORD tot=0;
      for (i=0;i<256;i++)
	tot+=fq[i]*ln[i];
      Out (7," %d->%lu",n,(tot*100)/num);
   }
   Out (7,"]");
   xfree (buf,NTMP);
   xfree (dbuf,NTMP);
}
