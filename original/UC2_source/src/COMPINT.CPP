// Copyright 1992, all rights reserved, AIP, Nico de Vries
// COMPINT.CPP

#include <alloc.h>
#include <string.h>
#include <stdlib.h>
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
//   Out (7,"[%d]",type);
   db->size = type;
   db->ctr = 0;
   for (int i=0;i<8;i++)
      db->arra[i] = 0;
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
//      Out (7,"[[%d]]",bDelta); // DDD
      wMethod=4;
   } else if (wMethod>=40 && wMethod<50){
      bDelta = wMethod-39; // (1..8)
//      Out (7,"[[%d]]",bDelta); // DDD
      wMethod=5;
   } else if (wMethod>=50 && wMethod<60){
      bDelta = wMethod-49; // (1..8)
//      Out (7,"[[%d]]",bDelta); // DDD
      wMethod=1;
   } else if (wMethod>=60 && wMethod<70){
      bDelta = wMethod-59; // (1..8)
//      Out (7,"[[%d]]",bDelta); // DDD
      wMethod=2;
   } else
      bDelta=0;
#ifdef STORE_ALL
   wMethod = 0;
#endif
   CReader = ARReader;
   CWriter = Writer;
   if (wMethod == 2){ // -TF compression
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
   } else if (wMethod == 3){ // -TN compression
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
   } else if (wMethod == 1){ // internal testmode
	TuneComp (5,2,5,25);
//      TuneComp (140,50,7,50);
//      TuneComp (270,80,8,100);
//      TuneComp (135,40,6,100);
      if (bDelta){
	 EnhanceRead(bDelta);
	 UltraCompressor(dwMaster,bDelta);
	 UnenhanceRead();
      } else
	 UltraCompressor(dwMaster,bDelta);
   } else if (wMethod == 4){ // -TT compression
//      TuneComp (5700,3900,400,380);
      TuneComp (600,50,40,100);
//      TuneComp (1100,300,200,120);
      if (bDelta){
	 EnhanceRead(bDelta);
	 UltraCompressor(dwMaster,bDelta);
	 UnenhanceRead();
      } else
	 UltraCompressor(dwMaster,bDelta);
   } else if (wMethod == 5){ // -TSX compression
      if (strcmp(strupr(getenv("TUX")),"ON")==0)
	 TuneComp (60000U,60000U,500,500);
      else
//	 TuneComp (15000,8000,400,200);
	 TuneComp (10000,5000,200,100);
      if (bDelta){
	 EnhanceRead(bDelta);
	 UltraCompressor(dwMaster,bDelta);
	 UnenhanceRead();
      } else
	 UltraCompressor(dwMaster,bDelta);
   } else if (wMethod==80) {
      TurboCompressor(dwMaster);
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

extern char fContinue;

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
//      Out (7,"[[%d]]",bDelta); // DDD
      InitDelta (&dbDelt, bDelta);
      OldWriter=CWriter;
      CWriter=NewWriter;
      bbbuf = xmalloc (33000L, TMP);
      UltraDecompressor(dwMaster,bDelta,len);
      xfree (bbbuf, TMP);
   } else if (wMethod>=40 && wMethod<50){
      bDelta = wMethod-39; // (1..8)
//      Out (7,"[[%d]]",bDelta); // DDD
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
   } else if (wMethod==80){
      TurboDecompressor(dwMaster);
   } else {
      Error (90,"central directory is damaged");
      if (!fContinue) FatalError (200,"you should repair this archive with 'UC T'");
   }
   CReader = SReader;
   CWriter = SWriter;
//   IOut ("[%04X]",Fletcher(&Fout));
   return CDRET_OK;
}

static int iHan;
static DWORD skipper;
static DWORD blks;
#define SKIPN 1
#define SKIPF 2048

//#define ITEST

static WORD far pascal Reader (BYTE *buf, WORD len){
   if (skipper){
      Seek (iHan, skipper);
      skipper+=SKIPF;
   }
   WORD ret = Read (buf, iHan, len);
   return ret;
}

static DWORD total;

#pragma argsused
void far pascal BWrite(BYTE far *pbBuffer, WORD w){
   total+=w;
}

void NoSpec (void);

extern int factor;

#pragma argsused
WORD Analyze (char *file, WORD comp, int handle, DWORD master){
   if ((comp!=4) && (comp!=5)){
      factor=1;
      return comp;
   }
   WORD oldcomp = comp;
   int bestsys=0;
   DWORD bestval=1000;
   BYTE buf[1500];
   BYTE dbuf[1500];
   WORD fq[2*NR_COMBINATIONS];
   BYTE ln[NR_COMBINATIONS];
   DeltaBlah db;
   WORD num=1450,nm;

   DWORD fs=GetFileSize (handle);
   if (fs>1500){
      Hint();
      NoSpec();
      if (fs<num) num=(WORD)fs;
      Seek (handle, (fs-num)/2);
      Read (buf, handle, num);
      Seek (handle, 0);

      RapidCopy (dbuf,buf,num);
      memset (fq, 0, NR_COMBINATIONS*2);
      nm=num;
      for (int i=0;i<num;i++)
	 fq[dbuf[i]]++;
      if (nm){
	 fq[0]=0;
	 TreeGen (fq, NR_COMBINATIONS, 13, ln);
	 DWORD tot=0;
	 for (i=0;i<256;i++)
	    tot+=fq[i]*ln[i];
	 if ((tot*100)/nm<(bestval-5)){
	    bestval=(tot*100)/nm;
	    bestsys=0;
	 }
#ifdef ITEST
	 Out (7,"[0>%lu",(tot*100)/nm);
#endif
      }
      for (int n=1;n<5;n++){
	 Hint();
	 RapidCopy (dbuf,buf,num);
	 InitDelta (&db, n);
	 Delta (&db, dbuf, num);
	 memset (fq, 0, NR_COMBINATIONS*2);
	 nm=num;
	 for (int i=0;i<5;i++)
	    fq[dbuf[i]]++;
	 for (i=5;i<num;i++){
	    if (!dbuf[i] && !dbuf[i-1] && !dbuf[i-2])
	       nm--;
	    else
	       fq[dbuf[i]]++;
	 }
	 if (nm){
	    TreeGen (fq, 256, 13, ln);
	    DWORD tot=0;
	    for (i=0;i<256;i++)
	       tot+=fq[i]*ln[i];
	    if ((tot*100)/nm<(bestval-5)){
	       bestval=(tot*100)/nm;
	       bestsys=n;
	    }
#ifdef ITEST
	    Out (7," %d>%lu",n,(tot*100)/nm);
#endif
	 }
      }

#ifdef ITEST
      Out (7,"] best=%d\n\r",bestsys);
#endif
   } else {
      Hint();
      factor=1;
      return oldcomp;
   }

   if (bestsys!=0){
      if (comp==4)
	 comp=29+bestsys;
      else
	 comp=39+bestsys;
      skipper=0;

      DWORD q1,q2;

#ifdef ITEST
      DWORD t1,t2;
      Seek (handle, 0);
      iHan = handle;
      total=0;
      blks=0;
      Compressor (4, Reader, BWrite, NOMASTER);
      NoSpec();
      Out (7,"Bare result = %ld\n\r",t1=total);

      Seek (handle, 0);
      iHan = handle;
      total=0;
      blks=0;
      Compressor (29+bestsys, Reader, BWrite, NOMASTER);
      Out (7,"Delta %d result = %ld\n\r",bestsys,t2=total);

      if (t2>t1){
	 Warning ("SDC STO: %s speeddelta=%d t1=%ld t2=%ld",file,bestsys,t1,t2);
      }

      Seek (handle, 0);
      iHan = handle;
      total=0;
      blks=0;
      Compressor (1, Reader, BWrite, NOMASTER);
      NoSpec();
      Out (7,"Bare result = %ld\n\r",q1=total);

      Seek (handle, 0);
      iHan = handle;
      total=0;
      blks=0;
      Compressor (49+bestsys, Reader, BWrite, NOMASTER);
      NoSpec();
      Out (7,"Delta %d result = %ld\n\r",bestsys,q2=total);

      if (q2>q1){
	 Warning ("SFC STO: %s speeddelta=%d q1=%ld q2=%ld t1=%ld t2=%ld",file,bestsys,q1,q2,t1,t2);
      }
      if ((t2>t1) && (q1>q2)){
	 Error ("SDC-SFC conflict");
      }
      if ((t2<t1) && (q1<q2)){
	 Error ("SDC-SFC conflict");
      }
#endif

      skipper=SKIPN;
      if (oldcomp==5) skipper=0; // special case!
      Seek (handle, 0);
      iHan = handle;
      total=0;
      blks=0;
      if (oldcomp==4)
	 Compressor (1, Reader, BWrite, NOMASTER);
      else
	 Compressor (2, Reader, BWrite, NOMASTER);
      NoSpec();
      q1=total;

#ifdef ITEST
      Out (7,"Bare result = %ld\n\r",q1=total);
#endif

      skipper=SKIPN;
      if (oldcomp==5) skipper=0; // special case!
      Seek (handle, 0);
      iHan = handle;
      total=0;
      blks=0;
      if (oldcomp==4)
	 Compressor (49+bestsys, Reader, BWrite, NOMASTER);
      else
	 Compressor (59+bestsys, Reader, BWrite, NOMASTER);
      NoSpec();
      q2=total;
#ifdef ITEST
      Out (7,"Delta %d result = %ld\n\r",bestsys,q2=total);
#endif
      if (q2>q1){
#ifdef ITEST
	 Warning ("UFC STO: %s speeddelta=%d q1=%ld q2=%ld t1=%ld t2=%ld",file,bestsys,q1,q2,t1,t2);
#endif
	 comp=oldcomp;
      }
#ifdef ITEST
      if ((t2>t1) && (q1>q2)){
	 Error ("SDC-UFC conflict");
      }
      if ((t2<t1) && (q1<q2)){
	 Error ("SDC-UFC conflict");
      }
#endif
      Seek (handle, 0);
   }
   factor=1;
   return comp;
}
