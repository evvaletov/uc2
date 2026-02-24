// Copyright 1992, all rights reserved, AIP, Nico de Vries
// VMEM.CPP

/**********************************************************************/
/*$ INCLUDES */

//#define STANDALONE // enable for standalone VMEM usage
//#define TESTME     // enable test program

#include <stdio.h>
#include <mem.h>
#include <alloc.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "video.h"
#include "diverse.h"
#include "mem.h"

#include "vmem.h"
#include "llio.h"
#include "dirman.h"

// On redefenition of BLOCK_SIZE please also redefine CUT !!!
#define BLOCK_SIZE  1054   // allow 2kb blocks (6 bytes overhead)
#define RAM_BLOCKS  16     // max 250 blocks (determines max ref number !)
#define MAX_RAM_BLOCKS 250
#define TOT_BLOCKS  64512U // currently only 256 Mb of vmem (max!)
#define DIR_BLOCKS  8192   // direct created blocks (8Mb)
#define GRW_BLOCKS  1024   // block grow size
#define DEEP        10     // number of valid successive 'A' calls
#define CUT 15             // vmem boosterchunk

static WORD lastblock = 65000U;


#ifdef DEBUG_VMEM
   // create room for debugger overhead
   #undef BLOCK_SIZE
   #define BLOCK_SIZE 1200
   #undef RAM_BLOCKS
   #define RAM_BLOCKS 18
   #undef DEEP
   #define DEEP 12
   #undef CUT
   #define CUT 13

   long allocctr=0;

   VPTR rootptr=VNULL;



   struct inforec {
      VPTR next;
      long magic;
      long ctr;
      char file[20];
      long line;
      WORD size;
      int free;
   };


//#define FAST // dummy debug mode

#ifdef FAST
   #pragma argsused
   VPTR DBVmalloc (WORD size, char *file, long line){
      return LLVmalloc (size);
   }

   #pragma argsused
   void DBVfree (VPTR vpAdr, char *file, long line){
      LLVfree (vpAdr);
   }

   void SumIt (void){
   }
#else
   void Check (VPTR vpAdr, char *file, long line){
      inforec *ip = (inforec *)Acc(vpAdr);
      if (ip->magic!=123456789L){
	 FatalError("VMEM: %s(%ld) invalid adress", file, line);
      }
      for (int i=0;i<10;i++){
	 if (((BYTE*)ip)[49-i] != 123){
	    FatalError ("VMEM: %s(%ld) underflow %s(%ld) Vmalloc(%d) %ld)", file, line,
		     ip->file, ip->line, ip->size, ip->ctr);
	 }
	 if (((BYTE*)ip)[50+ip->size+i] != 123){
	    FatalError ("VMEM: %s(%ld) overflow %s(%ld) Vmalloc(%d) %ld)", file, line,
		     ip->file, ip->line, ip->size, ip->ctr);
	 }
      }
      UnAcc (vpAdr);
   }

   void SumIt (void){
      return;
      VPTR walk = rootptr;
      while (!IS_VNULL(walk)){
	 Check (walk, "SumIt", 0);
	 inforec *ip = (inforec*)Acc(walk);
	 if (!ip->free){
	    Out (7,"{BUSY %s(%ld) Vmalloc(%d) %ld)}\n\r",
		     ip->file, ip->line, ip->size, ip->ctr);
	 }
	 VPTR oldwalk = walk;
	 walk = ip->next;
	 UnAcc (oldwalk);
      }
   }

   void CheckAll (char *file, long line){
      VPTR walk = rootptr;
      while (!IS_VNULL(walk)){
	 Check (walk, file, line);
	 inforec *ip = (inforec*)Acc(walk);
	 VPTR oldwalk = walk;
	 walk = ip->next;
	 UnAcc (oldwalk);
      }
   }

   VPTR DBVmalloc (WORD size, char *file, long line){
      CheckAll (file, line);

      allocctr++;
      Out (7,"{%s(%ld) Vmalloc(%d) %ld}\n\r",file,line,size,allocctr);
      VPTR tmp = LLVmalloc (size+100);

      inforec *ip = (inforec *)Acc(tmp);
      ip->magic=123456789L;
      strcpy (ip->file, file);
      ip->line = line;
      ip->size = size;
      ip->ctr = allocctr;
      ip->free = 0;
      ip->next = rootptr;
      rootptr = tmp;


      for (int i=0;i<10;i++){
	 ((BYTE*)ip)[49-i] = 123;
	 ((BYTE*)ip)[50+size+i] = 123;
      }

      UnAcc (tmp);

      tmp.wOffset+=50;

      return tmp;
   }

   void DBVfree (VPTR vpAdr, char *file, long line){
      CheckAll (file, line);

      vpAdr.wOffset-=50;

      inforec *ip = (inforec *)Acc(vpAdr);
      Out (7,"{%s(%ld) Vfree (%s(%ld) Vmalloc(%d) %ld)}\n\r",file,line,
	       ip->file, ip->line, ip->size, ip->ctr);
      ip->free=1;
      UnAcc (vpAdr);

      Check (vpAdr, file, line);
   }
#endif
#endif

VPTR pvpFirst[257];
   // boostcache sizes 4..1024 (0..255) and 1028 and larger (256)

struct PRE {
   WORD wSize;  // size of memory block
   VPTR vpNext; // next block in memory chain
};

void RInit (void);

#ifdef UCPROX
static DWORD VmemLeft (void){
   DWORD res=0;
   for (int i=0;i<257;i++){
      DWORD t=res;
      VPTR walk=pvpFirst[i];
      if (!IS_VNULL(walk)){
	 PRE *pre = (PRE *)Acc(walk);
         while (walk.wBlock!=VNULL.wBlock){
            if ((*pre).wSize==BLOCK_SIZE-6){ // save lots of time
               res+=(DWORD)(TOT_BLOCKS-walk.wBlock)*(BLOCK_SIZE-6);
               break;
            }
            res+=(*pre).wSize;
	    VPTR tmp = walk;
            walk = pre->vpNext;
	    UnAcc (tmp);
            if (IS_VNULL(walk)) goto next;
            pre = (PRE *)Acc(walk);
         }
         UnAcc(walk);
      }
next:
      if (t!=res){
         Out (7," <%d#%ld>",i,res-t);
      }
   }
   return res;
}
#endif

unsigned ram_blocks;

static BYTE rotor=0;

VPTR VNULL = {65535U, 65535U}; // 0,0 is a valid address !

char pcVmemPath[260]; // can be overridden if needed

static BYTE **ppbBlock;   // RAM blocks
static DWORD *pdwCounter; // RAM block reference counters
static BYTE *pbClean;     // RAM block clean (BLOCK_EMPTY)
static WORD *pwVBlock;    // VMEM block refering to this RAM block
static BYTE *pbStat;      // either 0..ram_blocks-1 or 254 (ondisk) or
                          // 255 (empty)
static WORD wSize;
static int hVmem;         // VMEM file
static int rinit=0;

static VPTR pvpQueue[DEEP]; // 'A' queue
static int iQueuePtr;       // current position

// special status values (0..249 are block references)
#define IN_RAM(x)   ((x)<254)
#define ON_DISK     254
#define BLOCK_EMPTY 255

void SetStat (WORD wIndex, BYTE bValue){
   if (wSize<=wIndex){
      if (wSize==TOT_BLOCKS) IE();
      if (bValue==BLOCK_EMPTY) return;
      wSize+=GRW_BLOCKS;
      pbStat = (BYTE *)realloc (pbStat, wSize);
      if (!pbStat) FatalError (165,"out of virtual memory");
      for (WORD i=wSize-GRW_BLOCKS;i<wSize;i++){
	 SetStat(i, BLOCK_EMPTY);
      }
   }
   pbStat[wIndex] = bValue;
}

BYTE GetStat (WORD wIndex){
//   static WORD max=0;
//   if (wIndex>max){
//      max=wIndex;
//      Out (7,"[%d]",max);
//   }
   if (wSize<=wIndex) return BLOCK_EMPTY;
   return pbStat[wIndex];
}

void CloseVmem (void){
#ifdef DEBUG_VMEM
   SumIt();
#endif
   if (rinit){
      Close (hVmem);
      Delete (pcVmemPath);
   }
}

static void ClearVmem (void){
   WORD i;
   pvpFirst[256] = MK_VP(0,0); // start of free memory chain
   for (i=0;i<256;i++){
      pvpFirst[i] = VNULL;
   }
   for (i=0;i<ram_blocks;i++){
      // add item to free memory chain
      (*((PRE *)ppbBlock[i])).wSize = BLOCK_SIZE - sizeof(PRE);
      (*((PRE *)ppbBlock[i])).vpNext = MK_VP(i+1,0);

      // no references yet
      pdwCounter[i]=0;

      // 1:1 relation between RAM and "DISK" to start with
      pwVBlock[i]=i;
      SetStat(i,i); // block in RAM
   }
   for (i=ram_blocks;i<wSize;i++) pbStat[i] = BLOCK_EMPTY;
   for (i=0;i<DEEP;i++) pvpQueue[i] = VNULL;
   iQueuePtr = 0;
   rotor=0;
}

WORD NotRam (){
   for (WORD i=0;i<1000;i++)
      if (!(IN_RAM(GetStat(i)))) return i;
   IE();
   return 0;
}

// MEM ram table management.
static int ramtab[100];
static int grow=1;
static int size=0;

void DGet (void *data, WORD block){
   int rb=block/CUT;
   if (rb<size){
      int ofs=(block-rb*CUT)*BLOCK_SIZE;
      from16 (data, ramtab[rb], ofs, BLOCK_SIZE);
   } else {
      RInit();
      Seek (hVmem, (DWORD)(block)*(DWORD)BLOCK_SIZE);
      Read ((BYTE *)data, hVmem, BLOCK_SIZE);
   }
}

void DPut (void *data, WORD block){
   int rb=block/CUT;
again:
   if (rb<size){
      int ofs=(block-rb*CUT)*BLOCK_SIZE;
      to16 (ramtab[rb], ofs, data, BLOCK_SIZE);
   } else {
      if (grow){
	 if (coreleft16(5)){
	    ramtab[size++] = malloc16(5);
	    if (size==100) grow=0;
	    goto again;
	 } else
	    grow=0;
      }
      RInit();
      Seek (hVmem, (DWORD)(block)*(DWORD)BLOCK_SIZE);
      Write ((BYTE *)data, hVmem, BLOCK_SIZE);
   }
}

#ifdef UCPROX
extern DWORD minimal;
static DWORD kkp;
#endif

void HeapDump (void);

void BoosterOn (){ // put VMEM in turbo boost mode (uses most RAM !)
    lastblock=65000U;
#ifdef UCPROX
   if (beta && heavy) return;
   kkp = minimal;
#endif
   // keep 64k+ free for the Ultra Caching Architecture
   while ((ram_blocks<=MAX_RAM_BLOCKS) && (farcoreleft()>75000L)){
      // create new RAM block
      ppbBlock[ram_blocks] = (BYTE *)xmalloc (BLOCK_SIZE,TMP);
      pdwCounter[ram_blocks] = 0;
      pbClean[ram_blocks]=1;

      // locate block NOT in RAM
      WORD block = pwVBlock[ram_blocks] = NotRam();

      // simulate Acc
      if (GetStat(block)==ON_DISK){
         DGet (ppbBlock[ram_blocks], block);
//       fseek (pfVmem, (DWORD)(block)*(DWORD)BLOCK_SIZE, SEEK_SET);
//       fread (ppbBlock[ram_blocks], BLOCK_SIZE, 1, pfVmem);
#ifdef UCPROX
         if (debug&heavy) Out (7,"{{VRead}}");
#endif
         pbClean[ram_blocks]=0;
      } else {
	 (*((PRE *)(ppbBlock[ram_blocks]))).wSize = BLOCK_SIZE - sizeof(PRE);
         if (block!=TOT_BLOCKS-1)
            (*((PRE *)(ppbBlock[ram_blocks]))).vpNext =
               MK_VP(block+1,0);
         else // last block
            (*((PRE *)(ppbBlock[ram_blocks]))).vpNext = VNULL;
      }
      SetStat(block,ram_blocks);


      ram_blocks++;
   }
   rotor=0;
}

static void ToDisk (BYTE b);

void BoosterOff (){ // back to normal mode (all 'A' discarded)
   lastblock=65000U;
#ifdef UCPROX
   if (beta && heavy) return;
#endif
   // flust to disk & deallocate
   for (int i=RAM_BLOCKS; i<ram_blocks; i++){
      if (pbClean[i]){
	 SetStat(pwVBlock[i],BLOCK_EMPTY);
      } else {
         ToDisk (i);
      }
      xfree (ppbBlock[i],TMP);
   }

   // make queue empty
   for (i=0;i<DEEP;i++) pvpQueue[i] = VNULL;
   for (i=0;i<ram_blocks;i++) pdwCounter[i] = 0;

   ram_blocks = RAM_BLOCKS;

   rotor = 0;
#ifdef UCPROX
   minimal = kkp;
#endif
}

void RInit (void){
   if (!rinit){
      strcpy (pcVmemPath, TmpFile((char *)CONFIG.pbTPATH,1,".TMP"));
      if (-1==(hVmem = Open (pcVmemPath, MAY|CR|NOC))){
         strcpy (pcVmemPath, TmpFile((char *)CONFIG.pbTPATH,1,".TMP"));
         if (-1==(hVmem = Open (pcVmemPath, MAY|CR|NOC))){
	    strcpy (pcVmemPath, TmpFile((char *)CONFIG.pbTPATH,1,".TMP"));
            hVmem = Open (pcVmemPath, MUST|CR|NOC);
         }
      }
      rinit=1;
   }
}

int fCloseVmem=0;

int tfirst=1;

extern CONF SPARE;

extern void GKeep2 (void);

void InitVmem (void){
   GKeep2();
   if (!TstPath((char *)CONFIG.pbTPATH)){
      FatalError (110,"the location for temporary files does not exist (see 'UC -!' S)");
   }

   int i;
   // determine ram_blocks
   ram_blocks = RAM_BLOCKS;

   // allocate memory
   ppbBlock = (BYTE **)xmalloc (sizeof(BYTE*)*MAX_RAM_BLOCKS,PERM);
   for (i=0; i<ram_blocks; i++){
      ppbBlock[i] = (BYTE *)xmalloc (BLOCK_SIZE,PERM);
   }
   pdwCounter = (DWORD *)xmalloc (sizeof(DWORD)*MAX_RAM_BLOCKS,PERM);
   pbClean = (BYTE *)xmalloc (sizeof(BYTE)*MAX_RAM_BLOCKS,PERM);
   pwVBlock = (WORD *)xmalloc (sizeof(WORD)*MAX_RAM_BLOCKS,PERM);
   wSize = DIR_BLOCKS;
   if (getenv("UC2_VMEM")){
      wSize = 1024 * atoi (getenv("UC2_VMEM"));
      if (wSize<4096) wSize = 4096;
      if (wSize>49152U) wSize = 49152U;
//      Out (7,"[%d]", wSize);
   }
   pbStat = (BYTE *)xmalloc (wSize,PERM);

   ClearVmem();

   fCloseVmem=1;
}

#ifdef UCPROX
long claimed=0;

void Vforever_do (){
   claimed--;
}
#endif


WORD hint=256;

VPTR LLVmalloc (WORD size){
#ifdef UCPROX
   DWORD mm,nn;
   if (debug&heavy){
      Out (7," <%ld,%d,",mm=VmemLeft(),size);
   }
#ifdef DEBUG_VMEM
   if (size>1224) IE();
#else
   if (size>1024) IE();
#endif
#endif
   VPTR tmp, prev=VNULL, walk;
   size = ((size+3)/4)*4;
   WORD list = (size/4)-1;
   if ((list>255) || IS_VNULL(pvpFirst[list])){ // no perfect match
      // try to use the hint Vfree generates (last freed block)
      if ((hint>=list) && !IS_VNULL(pvpFirst[hint]))
         list = hint;
      else {
         // try all blocks larges than wanted one
         while (++list<256)
            if (!IS_VNULL(pvpFirst[list]))
               goto done;
         // try the superlist (132Mb, shouldn't fail)
         list = 256;
      }
   }
done:
   walk = pvpFirst[list];
   PRE *pre;

#ifdef UCPROX
   claimed++;
#endif

   // locate large enough block
   pre = (PRE *)Acc(walk);
   while ((*pre).wSize<size){ // too small
      prev = walk; // keep previous pointer
      tmp=(*pre).vpNext;

      UnAcc (walk);
      walk = tmp;
      pre = (PRE *)Acc(walk);
   }

   // remove block from chain using prev (previous block)
   if (prev.wOffset==VNULL.wOffset) // first block of list
      pvpFirst[list] = (*pre).vpNext;
   else {
      (*((PRE *)Acc(prev))).vpNext = (*pre).vpNext;
      UnAcc (prev);
   }

   // split but not make too smal elements
   if (pre->wSize>size+16){
      ((PRE *)(((BYTE *)pre)+sizeof(PRE)+size))->wSize
         = pre->wSize-sizeof(PRE)-size;
      pre->wSize=size;
      VPTR tmp = MK_VP (walk.wBlock, walk.wOffset+2*sizeof(PRE)+size);
#ifdef UCPROX
      claimed++; // compensate for "extra" Vfree
#endif
      LLVfree (tmp);
   }

   // determine return value
   tmp = walk;
   tmp.wOffset+=sizeof(PRE);
   UnAcc(walk);
   memset (Acc(tmp), 0xDE, size);
   UnAcc (tmp);
#ifdef UCPROX
   if (debug&heavy){
      Out (7,"%ld:",nn=VmemLeft());
      Out (7,"%ld> ",mm-nn);
   }
#endif
   return tmp;
}

//VPTR Vmalloc (WORD size){
//   return VmallocI (size);
//}

void LLVfree (VPTR vpAdr){
#ifdef UCPROX
   claimed--;
#endif
   VPTR tmp = vpAdr;
   tmp.wOffset-=sizeof(PRE);
   PRE *pre = (PRE *)Acc(tmp);
   WORD list=pre->wSize/4-1;
   if (list>255) list=256;
   hint=list; // hint Vmalloc for the next allocate!
   pre->vpNext = pvpFirst[list];
   pvpFirst[list] = tmp;
   UnAcc(tmp);
}

//void Vfree (VPTR p){
//   VfreeI (p);
//}

static void ToDisk (BYTE rot){
   DPut (ppbBlock[rot],pwVBlock[rot]);
//   fseek (pfVmem, (DWORD)(pwVBlock[rot])*(DWORD)BLOCK_SIZE, SEEK_SET);
//   fwrite (ppbBlock[rot], BLOCK_SIZE, 1, pfVmem));
#ifdef UCPROX
   if (debug&heavy) Out (7,"{{VWrite}}");
#endif
   SetStat(pwVBlock[rot],ON_DISK); // ondisk
}

static BYTE Flush (void){
   int panic=0;
   // locate unreferenced block (rotate as "poor mans" LRU)
   do {
      rotor++;
      if (rotor==ram_blocks){
         rotor=0;
         if (panic++==3) IE();
      }
   } while (pdwCounter[rotor]!=0);
   ToDisk (rotor);
//   printf ("[%d]",rotor);
   return rotor;
}

BYTE *Acc (VPTR vpAdr){
   WORD block;
   if (IN_RAM(block=GetStat(vpAdr.wBlock))){
      pdwCounter[block]++;
      pbClean[block]=0;
      return ppbBlock[block]+vpAdr.wOffset;
   } else if (GetStat(vpAdr.wBlock)==ON_DISK){
      block = Flush();
      DGet (ppbBlock[block],vpAdr.wBlock);
//      fseek (pfVmem, (DWORD)(vpAdr.wBlock)*(DWORD)BLOCK_SIZE, SEEK_SET);
//      fread (ppbBlock[block], BLOCK_SIZE, 1, pfVmem);
#ifdef UCPROX
      if (debug&heavy) Out (7,"{{Vread}}");
#endif
      SetStat(vpAdr.wBlock,block);
      pwVBlock[block]=vpAdr.wBlock;
   } else if (GetStat(vpAdr.wBlock)==BLOCK_EMPTY){
      block = Flush();
      (*((PRE *)(ppbBlock[block]))).wSize = BLOCK_SIZE - sizeof(PRE);
      if (vpAdr.wBlock!=TOT_BLOCKS-1)
	 (*((PRE *)(ppbBlock[block]))).vpNext = MK_VP(vpAdr.wBlock+1,0);
      else // last block
	 (*((PRE *)(ppbBlock[block]))).vpNext = VNULL;
      SetStat(vpAdr.wBlock,block);
      pwVBlock[block]=vpAdr.wBlock;
   } else {
      block = GetStat(vpAdr.wBlock);
   }
   pdwCounter[block]++;
   pbClean[block]=0;
   return ppbBlock[block]+vpAdr.wOffset;
}

void UnAcc (VPTR vpAdr){
   pdwCounter[GetStat(vpAdr.wBlock)]--;
#ifdef UCPROX
   if (pdwCounter[GetStat(vpAdr.wBlock)]>10000)
      IE();;
#endif
}


BYTE *V (VPTR vpAdr){
   if (lastblock==vpAdr.wBlock){
      return ppbBlock[pbStat[vpAdr.wBlock]]+vpAdr.wOffset;
   }
#ifdef UCPROX
   if (heavy){
      if (IS_VNULL(vpAdr)) IE();
      if (vpAdr.wBlock>30000) IE();
      if (vpAdr.wOffset>30000) IE();
      if (heapcheck() == _HEAPCORRUPT) IE();
   }
#endif
   if (!IS_VNULL(pvpQueue[iQueuePtr])){
      pdwCounter[pbStat[pvpQueue[iQueuePtr].wBlock]]--;
#ifdef UCPROX
   if (beta && heavy){
      static long ctr=0;
      if ((ctr++&0xFF)==0) Out (7,"[%ld]",ctr);
      static bb[BLOCK_SIZE];
      BYTE *pp;
      if (!pdwCounter[pbStat[pvpQueue[iQueuePtr].wBlock]]){
	 for (int i=0; i<ram_blocks; i++){
	    if (0==pdwCounter[i] && i!=pbStat[pvpQueue[iQueuePtr].wBlock]){
               RapidCopy (bb,ppbBlock[i],BLOCK_SIZE);
               RapidCopy (ppbBlock[i],ppbBlock[pbStat[pvpQueue[iQueuePtr].wBlock]], BLOCK_SIZE);
               RapidCopy (ppbBlock[pbStat[pvpQueue[iQueuePtr].wBlock]], bb, BLOCK_SIZE);
               pp = ppbBlock[i];
               ppbBlock[i] = ppbBlock[pbStat[pvpQueue[iQueuePtr].wBlock]];
               ppbBlock[pbStat[pvpQueue[iQueuePtr].wBlock]]=pp;
               break;
            }
         }
      }
   }
#endif
   }
   BYTE *ret;
   WORD block;
   if (wSize>vpAdr.wBlock && IN_RAM(block=pbStat[vpAdr.wBlock])){ // speed
      pdwCounter[block]++;
      pbClean[block]=0;
      ret = ppbBlock[block]+vpAdr.wOffset;
   } else
      ret = Acc(vpAdr);
   pvpQueue[iQueuePtr] = vpAdr;
   iQueuePtr++;
   if (iQueuePtr==DEEP) iQueuePtr = 0;
#ifdef UCPROX
   if (heavy){
      if( heapcheck() == _HEAPCORRUPT) IE();
   }
#endif
   lastblock = vpAdr.wBlock;
   return ret;
}

void MakePipe (PIPE &p){
   p.vpStart = p.vpCurrent = p.vpTail = VNULL;
   p.wOff = 0;
   p.wBufInUse = 0;
}

static void FlushPipe (PIPE &p){
   if (!p.wBufInUse) return;
   if (IS_VNULL(p.vpStart)){
      p.vpStart = LLVmalloc (sizeof(PIPENODE));
      p.vpCurrent = p.vpStart;
      p.vpTail = p.vpStart;
   } else {
      VPTR nw = LLVmalloc (sizeof(PIPENODE));
      ((PIPENODE *)Acc(p.vpTail))->vpNext = nw;
      UnAcc (p.vpTail);
      p.vpTail = nw;
   }
   PIPENODE *pnp = (PIPENODE *)Acc(p.vpTail);
   pnp->vpNext = VNULL;
   pnp->wLen = p.wBufInUse;
   pnp->vpDat = LLVmalloc (p.wBufInUse);
   RapidCopy (Acc(pnp->vpDat),p.buf,p.wBufInUse);
   UnAcc (pnp->vpDat);
   UnAcc (p.vpTail);
   p.wBufInUse=0;
}

void WritePipe (PIPE &p, BYTE *pbData, WORD wLen){
again:
   if (wLen+p.wBufInUse<900){
      RapidCopy (p.buf+p.wBufInUse,pbData,wLen);
      p.wBufInUse+=wLen;
   } else {
      WORD m = 900-p.wBufInUse;
      RapidCopy (p.buf+p.wBufInUse,pbData,m);
      p.wBufInUse+=m;
      FlushPipe (p);
      wLen-=m;
      if (!wLen) return;
      pbData+=m;
      goto again;
   }
}

WORD ReadPipe (PIPE &p, BYTE *pbData, WORD wLen){
   VPTR tmp;
   WORD total=0;    // amount copied
   WORD wBLen=wLen; // origional request
   FlushPipe (p);
cont:
   if (!IS_VNULL(p.vpCurrent)){
      PIPENODE *pn = (PIPENODE *)Acc(p.vpCurrent);
      if (pn->wLen-p.wOff>=wLen){
         RapidCopy (pbData, Acc(pn->vpDat)+p.wOff, wLen);
         UnAcc (pn->vpDat);
         total+=wLen;
         p.wOff+=wLen;
         tmp = p.vpCurrent;
         if (pn->wLen==p.wOff){ // exact match
            p.vpCurrent = pn->vpNext;
            p.wOff = 0;
         }
         UnAcc (tmp);
      } else {
         RapidCopy (pbData, Acc(pn->vpDat)+p.wOff, pn->wLen-p.wOff);
         UnAcc (pn->vpDat);
         total += pn->wLen-p.wOff;
         wLen -= pn->wLen-p.wOff;
         pbData +=  pn->wLen-p.wOff;
         tmp = p.vpCurrent;
         p.vpCurrent = pn->vpNext;
         p.wOff = 0;
         UnAcc(tmp);
         goto cont; // tail recursion
      }
   }
   if (total<wBLen){
      memset (pbData, 0, wBLen-total);
   }
   return total;
}

void ClosePipe (PIPE &p){
   VPTR walk = p.vpStart;
   while (!IS_VNULL(walk)){
      VPTR next = ((PIPENODE *)Acc(walk))->vpNext;
      UnAcc (walk);
      LLVfree(((PIPENODE *)Acc(walk))->vpDat);
      UnAcc (walk);
      LLVfree (walk);
      walk = next;
   }
   p.vpStart = p.vpCurrent = p.vpTail = VNULL;
   p.wOff = 0;
}

#ifdef TESTME

DWORD VmemLeft (void){
   DWORD res=0;
   VPTR walk=pvpFirst[256];
   PRE *pre = (PRE *)Acc(walk);
   while (walk.wBlock!=VNULL.wBlock){
      if ((*pre).wSize==BLOCK_SIZE-6){ // save lots of time
         res+=(DWORD)(TOT_BLOCKS-walk.wBlock)*(BLOCK_SIZE-6);
         break;
      }
      res+=(*pre).wSize;
      VPTR tmp = walk;
      walk = pre->vpNext;
      UnAcc (tmp);
      pre = (PRE *)Acc(walk);
   }
   UnAcc(walk);
   return res;
}


#define LIM 512
#define SIZ 100

void main (void){
   long claimed=0;
   BYTE val[LIM];
   BYTE siz[LIM];
   VPTR vp[LIM];
   BYTE *bp;
   int i,j;
   long x;
   InitVmem();
   printf ("Test started ('ERROR' on problems)\n");
   for (i=0;i<LIM;i++) val[i]=0;
   for (x=0;x<10000;x++){
      if (x%400 == 0){
         printf ("%3ld %lu ",24-x/400,VmemLeft());
         j=0;
         for (i=0;i<LIM;i++) if (val[i]) j++;
         printf ("%d (%d)\n",j,LIM);
      }
      j = random (LIM);
      if (val[j]){
         BYTE nw = random(235)+10;
         bp = (BYTE *)A(vp[j]);
         for (i=0;i<siz[j];i++){
            if (bp[i]!=val[j]) printf ("ERROR ");
            bp[i] = nw;
         }
         val[j]=nw;
         if (random(5)==1){
            claimed--;
	    LLVfree(vp[j]);
	    val[j]=0;
         }
      } else {
         if (random(2)==1){
            siz[j] = random(SIZ-4)+5;
            claimed++;
	    vp[j] = LLVmalloc (siz[j]);
            val[j] = random(235)+10;
            bp = (BYTE *)A(vp[j]);
            for (i=0;i<siz[j];i++)
               bp[i] = val[j];
         }
      }
   }
   for (i=0;i<512;i++){
      if (val[i]){
	 LLVfree(vp[i]);
         claimed--;
      }
   }
   printf ("Test completed (error should be %ld)\n",claimed);
}

#endif
