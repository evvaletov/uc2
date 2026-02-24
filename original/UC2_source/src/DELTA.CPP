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

/* BASIC FUNCTIONS

void Delta (DeltaBlah *db, BYTE far *pbData, WORD size){
   Out (7,"{D%u}",size);
   for (WORD i=0;i<size;i++){
      BYTE tmp=pbData[i];
      pbData[i] = tmp - db->arra[db->ctr];
      db->arra[db->ctr] = tmp;
      if (++db->ctr==db->size) db->ctr=0;
   }
}

void UnDelta (DeltaBlah *db, BYTE far *pbData, WORD size){
   Out (7,"{U%u}",size);
   for (WORD i=0;i<size;i++){
      pbData[i] += db->arra[db->ctr];
      db->arra[db->ctr] = pbData[i];
      if (++db->ctr==db->size) db->ctr=0;
   }
}

*/

//#define Delta DUMMY1
//#define UnDelta DUMMY2

static BYTE SDelta (BYTE a, WORD size, BYTE far* pbData){
   _CL = a;
   _SI = size;
   _DI = 0;
   asm les bx, dword ptr pbData
loop:
   asm mov al, byte ptr es:[bx];
   asm mov dl, al
   asm sub al, cl
   asm mov byte ptr es:[bx],al
   asm mov cl, dl
   asm inc bx
   _DI++;
   if (_DI!=_SI) goto loop;
   return _CL;
/*
   for (WORD i=0;i<size;i++){
      BYTE tmp=pbData[i];
      pbData[i] = tmp - a;
      a = tmp;
   }
   return a;
*/
}

static BYTE SUnDelta (BYTE a, WORD size, BYTE far *pbData){
   _CL = a;
   _SI = size;
   _DI = 0;
   asm les bx, dword ptr pbData
loop:
   asm add byte ptr es:[bx], cl
   asm mov cl, byte ptr es:[bx]
   asm inc bx
   _DI++;
   if (_DI!=_SI) goto loop;
   return _CL;
/*
   for (WORD i=0;i<size;i++){
      pbData[i] += a;
      a = pbData[i];
   }
   return a;
*/
}

void Delta (DeltaBlah *db, BYTE far *pbData, WORD size){
   BYTE arra[8];
   arra[0] = db->arra[0];
   arra[1] = db->arra[1];
   arra[2] = db->arra[2];
   arra[3] = db->arra[3];
   arra[4] = db->arra[4];
   arra[5] = db->arra[5];
   arra[6] = db->arra[6];
   arra[7] = db->arra[7];
   WORD ctr=db->ctr;
   WORD siz=db->size;

   if (siz==1){
      arra[0] = SDelta (arra[0],size,pbData);
   } else {
      for (WORD i=0;i<size;i++){
	 BYTE tmp=pbData[i];
	 pbData[i] = tmp - arra[ctr];
	 arra[ctr] = tmp;
	 if (++ctr==siz) ctr=0;
      }
   }

   db->ctr = ctr;
   db->arra[0] = arra[0];
   db->arra[1] = arra[1];
   db->arra[2] = arra[2];
   db->arra[3] = arra[3];
   db->arra[4] = arra[4];
   db->arra[5] = arra[5];
   db->arra[6] = arra[6];
   db->arra[7] = arra[7];
}

void UnDelta (DeltaBlah *db, BYTE far *pbData, WORD size){
   BYTE arra[8];
   arra[0] = db->arra[0];
   arra[1] = db->arra[1];
   arra[2] = db->arra[2];
   arra[3] = db->arra[3];
   arra[4] = db->arra[4];
   arra[5] = db->arra[5];
   arra[6] = db->arra[6];
   arra[7] = db->arra[7];
   WORD ctr=db->ctr;
   WORD siz=db->size;

   if (siz==1){
      arra[0] = SUnDelta (arra[0], size, pbData);
   } else {
      for (WORD i=0;i<size;i++){
	 pbData[i] += arra[ctr];
	 arra[ctr] = pbData[i];
	 if (++ctr==siz) ctr=0;
      }
   }

   db->ctr = ctr;
   db->arra[0] = arra[0];
   db->arra[1] = arra[1];
   db->arra[2] = arra[2];
   db->arra[3] = arra[3];
   db->arra[4] = arra[4];
   db->arra[5] = arra[5];
   db->arra[6] = arra[6];
   db->arra[7] = arra[7];
}