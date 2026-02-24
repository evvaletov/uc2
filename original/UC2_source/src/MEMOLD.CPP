/*
   MEM.C (C) 1992, all rights reserved, Nico de Vries. Unlimited
   non-commercial use is granted to Jean-loup Gailly.

   This "library" has two functions:
      (a) enlarging the amount of direct accessible memory
      (b) giving universal access to not direct accessible memory

   It does this by using EMS memory (e.g. EMM386, QEMM, 386^MAX or a
   real EMS board) and the XMM (extended memory manager, mostly
   HIMEM.SYS). The aproach is as safe as possible meaning that when
   anything at all is not standard EMM/XMM is not used.

   *** NOTE ***
   While debugging, notice that if the ExitMem function isn't called
   (e.g. becuase of termination with the debugger) all allocated
   UMB's, EMS etc are lost "forever". Rebooting (DOS) or restaring
   the DOS box (OS/2) are needed to restore the memory. The Turbo Debugger
   allows calling ExitMem by hand.

   *** NOTE 2.0 ***
   Beware of rules when calling functions (e.g. no too large values
   for to16). Almost no parameter checking is done.

   (Far :-)) future options:
      More accurate memory allocation for META4.
      Usage of free video memory.
      Concurrent XMS+EMS META16 blocks.
      Option to "donate" memory as META16 blocks. (Priority)
      Option to use MEM4 as MEM16. (Priority)

   The library has been tested with the large memory model.
*/

#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <mem.h>
#include <alloc.h>
#include <string.h>
#include "main.h"
#include "video.h"
#include "diverse.h"
#include "mem.h"

#define MAX_META16 256 // number of available 16k block slots (cache memory)
#define MAX_META4  128 // number of 4k slots (direct memory, 384k at most)
                       // the last few blocks will ALWAYS be ST_EMPTY

// 16k memory block slot status
#define ST_EMPTY 1 // no memory in this slot
#define ST_FREE  2 // slot free
#define ST_USED  3 // slot used (first of long block)
#define ST_USEDP 4 // slot used (part of long block)

static int fXMM=0; // Extended memory manager (e.g. HIMEM.SYS) present?
static int fXMS=0; // Extended memory present?
static int fEMS=0; // EMS memory present?

static struct {
   char fStatus;
} META16[MAX_META16];

static struct {
   char fStatus;
   unsigned para; // paragraph in memory
} META4[MAX_META4];

static int EMS_handle;
static int EMS_frame;
static int XMS_handle;
static void far *XMS_Control; // needed for XMS calls

// Function to determine largest available UMB.
static int UMBmax (void){
   if (!fXMM) return 0;
   _AH = 0x10;
   _DX = 0x7000; // Extreme large claim (impossible).
   (*((void far (*)(void))XMS_Control))();
   return _DX; // Now contains size of largest UMB.
}

// Function to allocate an UMB.
static unsigned UMBmalloc (int size){
   if (!fXMM) return 0;
   _DX = size;
   _AH = 0x10;
   (*((void far (*)(void))XMS_Control))();
   if (_AX==1)
      return _BX;
   else // Some error, give up.
      return 0;
}

// Function to release UMB.
static void UMBfree (unsigned para){
   _DX = para;
   _AH = 0x11;
   (*((void far (*)(void))XMS_Control))();
}

// Map EMS page to physical memory (we use only a single handle).
// Perfect parameters are assumed.
static void EMS_map (int pageno, int offset){
   union REGS regs;

   // Select specific page.
   offset+=0x4400;
   regs.x.ax = offset; // Frame offset=0..3
   regs.x.bx = pageno;
   regs.x.dx = EMS_handle;
   int86 (0x67, &regs, &regs);
}

// Automatically called with atexit to release memory.
static void ExitMem (void){
   union REGS regs;

   if (fEMS){ // EMS has been allocated.
      // Call FreeEmsHandle.
      regs.h.ah = 0x45;
      regs.x.dx = EMS_handle;
      int86 (0x67, &regs, &regs);
   } else if (fXMS){ // XMS has been allocated.
      // Free XMS memory.
      _DX = XMS_handle;
      _AH = 0xA;
      (*((void far (*)(void))XMS_Control))();
   }
   if (fXMM){ // (Possible) UMB's have been allocated.
      int ctr=0;
      if (fEMS) ctr=17; // First UMB is mapped EMS frame here.
      // Just after each "hole" the real UMB starts.
      for (;ctr<MAX_META4;ctr++)
         if ((!ctr || (META4[ctr-1].fStatus==ST_EMPTY)) &&
             META4[ctr].fStatus!=ST_EMPTY)
            UMBfree (META4[ctr].para);
   }
}

static unsigned freex[7]; // free memory sorted by group 1..6

int coreleft16r(); // "real" coreleft16

int fExitMem=0;

// Init this memory manager. Normally it is called with 1 as parameter
// but if for some reason EMS/XMM should not be used (e.g. because the
// user wants this) 0 should be used. Notice InitMem should ALWAYS be
// called.
void InitMem (char fUseAll){
   union REGS regs;
   struct SREGS sregs;
   FILE *file;
   int i, tmp, ctr;

   // Automatic dealocate EMS etc at program exit.
   fExitMem = 1;

   // Mark all slots empty.
   for (i=0;i<MAX_META16;i++)
      META16[i].fStatus = ST_EMPTY;
   for (i=0;i<MAX_META4;i++)
      META4[i].fStatus = ST_EMPTY;

   if (!fUseAll) return; // Don't use EMS, XMM.

   // Detect if XMM installed.
   regs.x.ax = 0x4300;
   int86 (0x2f, &regs, &regs);
   if (regs.h.al!=0x80)
      fXMM=0; // No XMM installed.
   else {
      fXMM=1;
      // Get XMS_Control vector.
      regs.x.ax = 0x4310;
      segread (&sregs);
      int86x (0x2f, &regs, &regs, &sregs);
      XMS_Control = MK_FP (sregs.es, regs.x.bx);
   }

   // Detect if EMS installed.
   file = fopen ("EMMXXXX0","r");
   if (!file)
      fEMS=0;
   else {
      fclose (file);
      fEMS=1;

      // Get EMS frame address.
      regs.h.ah = 0x41;
      int86 (0x67, &regs, &regs);
      EMS_frame = regs.x.bx;
   }

// By putting fEMS=0 here EMS is disabled for XMS testing.
//   fEMS=0;

   if (fEMS){
      // Call QueryFreePages.
      regs.h.ah = 0x42;
      int86 (0x67, &regs, &regs);
      if (regs.h.ah || (regs.x.bx<4)){ // some problem or <4 pages
         fEMS=0;
      } else {
         // Don't claim too much memory.
         if (regs.x.bx > MAX_META16+4)
            regs.x.bx = MAX_META16+4;
         // Call ClaimPages (bx still contains nuber of pages).
         regs.h.ah = 0x43;
         int86 (0x67, &regs, &regs);
         if (regs.h.ah) // Bad result, give up.
            fEMS=0;
         else {
            // Remeber handle and adapt meta table.
            EMS_handle = regs.x.dx;
            for (i=4;i<regs.x.bx;i++)
               META16[i-4].fStatus = ST_FREE;
         }
      }
   }
   if (fXMM && !fEMS){
      // Querry free extended memory.
      _AH = 8;
      (*((void far (*)(void))XMS_Control))();
      tmp = _AX; // Beware of register changes!
      tmp /= 16;
      if (tmp){
         if (tmp>MAX_META16) tmp=MAX_META16;
         // Allocate extended memory.
         _DX = tmp*16;
         _AH = 9;
         (*((void far (*)(void))XMS_Control))();
         if (_AX==1){
            XMS_handle = _DX;
            fXMS=1;
            // Adapt meta table.
            for (i=0;i<tmp;i++)
               META16[i].fStatus = ST_FREE;
         } else { // Problem, give up.
            fXMS=0;
         }
      } else // Not enough extended memory available.
         fXMS=0;
   }

   ctr=0; // Next free slot to enter new UMB in META4 table.

   // First UMB is (if possible) the EMS page frame.
   if (fEMS){
      // Convert EMS to UMB.
      EMS_map (0,0);
      EMS_map (1,1);
      EMS_map (2,2);
      EMS_map (3,3);
      // Notice ALL EMS related stuff should PRESERVE this! TSR's etc
      // will not cause problems when using EMS because they restore
      // paging anyway.

      for (i=0;i<16;i++){
         META4[i].fStatus = ST_FREE;
         META4[i].para = EMS_frame + i*256; // 256 paragraphs is 4 kb
      }
      // Mark 1 slot EMPTY to notify that the next memory block is not
      // connected to the current one.
      ctr=17;
   }

   // Now go hunting for "real" UMB's.
   if (fXMM){
      tmp = UMBmax() / 256; // 256 paragraphs is 4 kb
      while (tmp){
         unsigned p = UMBmalloc (tmp*256);
         if (p!=0){
            for (i=0;i<tmp;i++){
               META4[ctr].fStatus = ST_FREE;
               META4[ctr++].para = p + i*256;
            }
            // Mark 1 slot EMPTY to notify that the next memory block is not
            // connected to the current one.
            ctr++;
            tmp = UMBmax() / 256;
         } else // Very unlikely but e.g. a TSR could have allocated memory.
            break;
      }
   }
   int bb=coreleft16r();
   if (bb>3){
      bb-=4;
      freex[6]=4; // most recently used master cache
#ifdef UCPROX
      if (beta && 0==strcmp(getenv("NOCLRUMAS"),"ON")) freex[6]=0;
#endif
   }
   if (bb>2){
      bb-=3;
      freex[1]=3; // super master storage
#ifdef UCPROX
      if (beta && 0==strcmp(getenv("NOCMASST"),"ON")) freex[1]=0;
#endif
   }
   if (bb>9){
      bb-=10;
      freex[2]=10; // most recently use hash scheme cache
#ifdef UCPROX
      if (beta && 0==strcmp(getenv("NOCHASH"),"ON")) freex[2]=0;
#endif
   }
   if (bb){
      int q=bb/10; // 10%
      bb-=2*q;
      freex[3]=2*q;    // master storage (20%)
#ifdef UCPROX
      if (beta && 0==strcmp(getenv("NOCMAST"),"ON")) freex[3]=0;
#endif
      bb-=6*q;
      freex[4]=6*q;    // disk cache (60%)
#ifdef UCPROX
      if (beta && 0==strcmp(getenv("NOCDISK"),"ON")) freex[4]=0;
#endif
      freex[5]=bb;     // vmem support (20%)
#ifdef UCPROX
      if (beta && 0==strcmp(getenv("NOCVMEM"),"ON")) freex[5]=0;
#endif
   }
}

// Allocate "extra" memory. The used method might SEEM inefficient but
// looking at the CRTL will change that thought in a few seconds. It
// is recomended to only call the function for the larger blocks. (e.g. >3k)
void far *exmalloc (long size){
   int i,j;
   unsigned no = (unsigned)((size+4095L)/4096L); // no of 4k blocks

   // Find a row of "no" connected free blocks.
   for (i=0;i<MAX_META4-no;i++){
      if (META4[i].fStatus==ST_FREE){
         for (j=i;j<i+no;j++)
            if (META4[j].fStatus!=ST_FREE){
               i=j; // Saves time.
               goto failed;
            }

         // Mark first block USED, others USEDP.
         META4[i].fStatus = ST_USED;
         for (j=i+1;j<i+no;j++)
            META4[j].fStatus = ST_USEDP;

         // Return address of first block.
         return MK_FP (META4[i].para,0);
      }
failed:; // ANSI required ; (BCC can live without it)
   }
   return NULL;
}

// Free "extra" memory. (NOT foolproof!)
void exfree (void far *adr){
   int i;
   unsigned para = FP_SEG(adr);

   // Locate memory in list of blocks.
   for (i=0;i<MAX_META4;i++)
      if (META4[i].para==para){
         // Mark first block free.
         META4[i].fStatus =ST_FREE;
         i++;

         // Mark connected blocks free as well (are marked USEDP).
         while (META4[i].fStatus == ST_USEDP){
            META4[i].fStatus =ST_FREE;
            i++;
         }
      }
}

// Test if memory is "extra" memory.
// Usefull to detect if a memory block should be released with free
// or with exfree. This allows easier mixing of memory and "extra"
// memory. (The UC "div" module's xmalloc and xfree already mix
// memory and "extra" memory automatically, recomended).
// It is tempting to let exfree handle this automatically but
// in some memory models this might give problems.
int extest (void far *adr){
   int i;
   unsigned para = FP_SEG(adr);

   // Locate memory in list of blocks.
   for (i=0;i<MAX_META4;i++)
      if (META4[i].para==para)
         return 1; // Found.
   return 0; // Not found.
}


// Allocate a 16k cache block.
int malloc16 (char g){
   freex[g]--;
   int i;
   // Simple, just go looking for a free entry in the table.
   for (i=0;i<MAX_META16;i++)
      if (META16[i].fStatus==ST_FREE){
         META16[i].fStatus = ST_USED;
         return i+1; // Handles are 1..MAX_META16, value 0 is reserved.
      }
   IE();
   return 0; // No block available.
}

// Release a 16k cache block. Allows call with 0.
void free16 (char g,int i){
   freex[g]++;
   if (!i) return;
   META16[i-1].fStatus = ST_FREE;
}

// Copy 0..16k of memory to a cache block.
void to16 (int itx, unsigned ofs, void far *ptr, int len){
   static struct { // MUST be in the DS segment!
      unsigned long size;
      unsigned source;
      unsigned long soffset;
      unsigned dest;
      unsigned long doffset;
   } trs;
   if (fEMS){
      int page=0;

      // This part is really tricky. Since the EMS page frame is used
      // to emulate UMB the "ptr" variable can point into this page frame.
      // This part makes sure that in case ptr is inside page 0 page 3 is
      // used to assure proper handling of this problem.
      // First absolute adresses are calculated, then they are compared.
      long lptr = ((unsigned)FP_SEG(ptr))*16L + FP_OFF(ptr);
      long lems = ((unsigned)EMS_frame)*16L;
      if ((lptr>=lems) && (lptr<(lems+16384L))) page=3;

      EMS_map (itx+3,page); // -1 for 1.. itx; +4 for EMS pages used for UMB.
//      movedata (FP_SEG(ptr), FP_OFF(ptr), EMS_frame, page*16384U+ofs, len);
      RapidCopy (MK_FP(EMS_frame, page*16384U+ofs),ptr,len);
      EMS_map (page,page); // Restore EMS for use as UMB.

   } else { // XMS
      trs.size = 2*((len+1)/2); // Make even. (!!! beware !!!).
      trs.source = 0;
      trs.soffset = (unsigned long)ptr;
      trs.dest = XMS_handle;
      trs.doffset = 16384L*(itx-1)+ofs; // -1 for 1.. handle range.

      _SI = FP_OFF((void far *)&trs); // DS:SI points to trs now
      _AH = 0x0B;
      (*((void far (*)(void))XMS_Control))();
   }
}

// Copy 0..16k of memory from a cache block.
void from16 (void far *ptr, int itx, unsigned ofs, int len){
   static struct { // MUST be in the DS segment!
      unsigned long size;
      unsigned source;
      unsigned long soffset;
      unsigned dest;
      unsigned long doffset;
   } trs;
   if (fEMS){
      int page=0;

      // This part is really tricky. Since the EMS page frame is used
      // to emulate UMB the "ptr" variable can point into this page frame.
      // This part makes sure that in case ptr is inside page 0 page 3 is
      // used to assure proper handling of this problem.
      // First absolute adresses are calculated, then they are compared.
      long lptr = ((unsigned)FP_SEG(ptr))*16L + FP_OFF(ptr);
      long lems = ((unsigned)EMS_frame)*16L;
      if ((lptr>=lems) && (lptr<(lems+16384L))) page=3;

      EMS_map (itx+3,page); // -1 for 1.. itx; +4 for EMS pages used for UMB.
//      movedata (EMS_frame, page*16384U+ofs, FP_SEG(ptr), FP_OFF(ptr), len);
      RapidCopy (ptr, MK_FP(EMS_frame,page*16384U+ofs),len);
      EMS_map (page,page); // Restore EMS for use as UMB.

   } else { // XMS
      trs.size = 2*((len+1)/2); // Make even. (!!! beware !!!).
      trs.source = XMS_handle;
      trs.soffset = 16384L*(itx-1)+ofs; // -1 for 1.. handle range.
      trs.dest = 0;
      trs.doffset = (unsigned long)ptr;

      _SI = FP_OFF((void far *)&trs); // DS:SI points to trs now
      _AH = 0x0B;
      (*((void far (*)(void))XMS_Control))();
   }
}

// Return number of free 16k blocks.
int coreleft16r (void){
   int i,c=0;
   for (i=0;i<MAX_META16;i++)
      if (META16[i].fStatus==ST_FREE) c++;
   return c;
}

int coreleft16 (char g){
//   SOut ("[");
//   for (int i=1;i<6;i++){
//      SOut ("%d ",freex[i]);
//   }
//   SOut ("]");
   return (int)freex[g];
}

// Return number of free 4k blocks.
int coreleft4 (void){
   int i,c=0;
   for (i=0;i<MAX_META4;i++)
      if (META4[i].fStatus==ST_FREE) c++;
   return c;
}

// Optional program to test memory management.
//#define TESTIT
#ifdef TESTIT

#define T1 30
#define T2 100

void main (void){
   int i, tot=0;
   void far* table[T1];
   int tval[T1];
   int han[T2];
   int hval[T2];

   InitMem(1);
   printf ("fEMS=%d\n",fEMS);
   printf ("fXMM=%d\n",fXMM);
   printf ("fXMS=%d\n",fXMS);
   printf ("coreleft16=%d\n",coreleft16());
   printf ("coreleft4=%d\n",coreleft4());

   // allocate 20 blocks, first exmem, than mem
   for (i=0;i<T1;i++){
      table[i]=exmalloc (16384);
      if (!table[i])
         table[i] = farmalloc(16384);
      else
         tot++;
      if (!table[i]) doexit(EXIT_FAILURE);
   }
   printf ("Allocated with exmalloc %ld\n",16384L*tot);

   for (i=0;i<T2;i++){
      han[i] = malloc16();
      if (!han[i]) doexit (EXIT_FAILURE);
      hval[i] = random (120);
      _fmemset (table[0],hval[i],16384);
      to16 (han[i],0,table[0],16384);
   }

   for (i=0;i<2500;i++){
      int j,k,n;
      if (i%100==0) printf ("*");
      j = random (T1);
      k = random (T2);
      tval[j] = hval[k];
      from16 (table[j],han[k],0,16384);
      if (random(20)==3)
         for (n=0;n<16384;n++)
            if (((char *)(table[j]))[n]!=hval[k]){
               printf ("[ERROR]");
               break;
            }
      else
         for (n=0;n<16384;n+=512)
            if (((char *)(table[j]))[n]!=hval[k]){
               printf ("[ERROR]");
               break;
            }
      if (random(5)==3){
         tval[j] = hval[k] = random(120);
         _fmemset (table[j],hval[k],16384);
         to16 (han[k],0,table[j],16384);
      }
   }

   printf ("\n");

   for (i=0;i<T1;i++){
      if (extest(table[i]))
         exfree (table[i]);
      else
         xfree (table[i]);
   }

   for (i=0;i<T2;i++)
      free16 (han[i]);

   printf ("coreleft16=%d\n",coreleft16());
   printf ("coreleft4=%d\n",coreleft4());

   printf ("Test completed.\n");
}

#endif