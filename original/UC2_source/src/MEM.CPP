/*
   MEM.H (C) 1992, all rights reserved, Nico de Vries. Unlimited
   use is granted to Jean-loup Gailly and Robert Jung.
   Partial (C) 1993 Jan-Pieter Cornet.

   This "library" has two functions:
      (a) enlarging the amount of direct accessible memory
      (b) giving universal access to not direct accessible memory

   It does this by using EMS memory (e.g. EMM386, QEMM, 386^MAX or a
   real EMS board) and the XMM (extended memory manager, mostly
   HIMEM.SYS). The aproach is as safe as possible meaning that when
   anything at all is not standard EMM/XMM is not used.

   *** NOTE 1 ***
   While debugging, notice that if the ExitMem function isn't called
   (e.g. becuase of termination with the debugger) all allocated
   UMB's, EMS etc are lost "forever". Rebooting (DOS) or restaring
   the DOS box (OS/2) are needed to restore the memory. The Turbo Debugger
   allows calling ExitMem directly with 'evaluate'.

   *** NOTE 2 ***
   Beware of rules when calling functions (e.g. no too large values
   for to16). Almost no parameter checking is done.

   *** NOTE 3 ***
   The library has only been tested with the large memory model.
*/

#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <mem.h>
#include <alloc.h>
#include <io.h>
#include <string.h>
#include <bios.h>

#include "mem.h"

//#define STAND_ALONE

#ifdef STAND_ALONE
   #define IE() {fprintf(stderr,"\nMEM: Internal Error\n");exit(EXIT_FAILURE);}
   #define FatalError(s) {fprintf(stderr,s);exit(EXIT_FAILURE);}
   #define RapidCopy _fmemcpy
#else // UltraCompressor
   #include "main.h"
   #include "video.h"
   #include "diverse.h"
   #undef farmalloc
   #undef farfree
#endif

// No stack checking! interrupt function included
#pragma option -N-

#define MAX_META16      256 // max no of available 16k block slots (cache)
#define MAX_META4       80  // number of 4k slots (80 blks = 320K)
#define MAX_DIRMETA16   32  // maximum number of blocks that can be donated
                            // with donate16 (32 blocks = 512K)

// FP_LIN. Returns the linear address of p
#define FP_LIN(p) ((unsigned long)(16L*(unsigned)FP_SEG(p)+(unsigned)FP_OFF(p)))

// 4k and 16k memory block slot status
#define ST_EMPTY 1 // no memory in this slot
#define ST_FREE  2 // slot free
#define ST_USED  3 // slot used (first of long block)
#define ST_USEDP 4 // slot used (part of long block)

// 4k block memory types
#define MT_XMSUMB    1 // Corresponding paragraph is the start of an XMS UMB
#define MT_DOS5UMB   2 // Corresponding paragraph is the start of a DOS5 UMB
#define MT_NONE      3 // Not the start of a memory block
#define MT_EMSUMB    4 // Corresponding paragraph is the EMS page frame
// MT_NONE should be the highest numbered. MT_FIRSTTYPE is the first
// memory type that is tried to allocate, then until MT_NONE
#define MT_FIRSTTYPE 1

unsigned gmaxI15, gmaxEMS, gmaxXMS, gmaxUMB;

static struct {
   char fStatus;
} META16[MAX_META16];

static struct {
   char nMemType; // Memory type (none, DOS5UMB, XMSUMB, EMSUMB) indicator
   char fStatus;  // Free/used indicator
   unsigned para; // paragraph in memory
} META4[MAX_META4];

static void far* dir_meta16[MAX_DIRMETA16];

static int fXMM;  // Extended memory manager (e.g. HIMEM.SYS) present?
static int fEMS;  // EMS memory present?
static int fI15;  // INT15 extended memory present?
static int nDIR;  // the number of direct META16 blocks available
static int nXMS;  // the number of XMS META16 blocks available
static int nEMS;  // the number of EMS META16 blocks available
static int nI15;  // the number of INT 15 META16 blocks available
static int fUMB;  // UMB alive?

static unsigned hEMS;            // handle of EMS
static unsigned EMS_frame;       // segment of EMS frame
static unsigned hXMS;            // handle of XMS
static void far* XMM_control;    // needed for XMS calls
static void interrupt (* old_int15)(...); // Original INT15 handler
static unsigned top_of_int15;    // needed for INT15 allocation & usage
static int UMBlinkstate;         // old UMB link state, or -1
static int allocstrat;           // old allocation strategy or -1
static unsigned convmemend;      // paragraph where conventional memory ends

static unsigned freex[8];        // free memory sorted by group 1..7

// typedef for the registers passed in an interrupt call
// Borland didn't document this, unfortunately! This might change in
// future BC++ releases :( (found this in DrDobbs)
typedef struct {
   unsigned bp,di,si,ds,es,dx,cx,bx,ax,ip,cs,fl;
} IREGS;

// typedefs for use in from16/to16 internals
// t_XMemCopy is the type of a function that copies Xmem
// t_pXMemCopy is the type of a pointer to a function that copies Xmem
typedef void t_XMemCopy(unsigned, unsigned long, unsigned,
      unsigned long, unsigned);
typedef t_XMemCopy* t_pXMemCopy;

// Function prototypes
static void interrupt NewInt15(IREGS);
static int InitDOS5UMB(void);
static void ExitDOS5UMB(void);
static int UMBmax(int);
static unsigned UMBmalloc(int, int);
static void UMBfree(int, unsigned);
static void EMS_map (int, int);
static void DeallocateEMS(unsigned);
static void DeallocateXMS(unsigned);
static int DetectXMM(void);
static int QueryFreeEMS(void);
static int DetectEMS(void);
static int AllocateEMS(int);
static int QueryFreeXMS(void);
static unsigned AllocateXMS(int);
static int AllocateInt15(int);
static int coreleft16r (void);
static t_XMemCopy CopyExtended; // a function prototype via a typedef!
static t_XMemCopy CopyXMS;
static void DealWithOddLength(t_pXMemCopy, unsigned, unsigned long,
      unsigned, unsigned long, unsigned);


// New INT 15 handler. Catches get extended memory size call, and returns
// the start of our block of INT15 memory, otherwise links to original
// handler.
// THIS SHOULD REMAIN RESIDENT EVEN WHEN UC SWAPS OUT!
static void interrupt NewInt15(IREGS ir) {
   // The new INT 15 handler for allocating INT15 memory.

   if ( (ir.ax & 0xFF00) == 0x8800 ) {
      // It's a getsize call
      ir.ax = top_of_int15;
      ir.fl &= ~1; // Clear carry flag
   } else {
      // It's not a getsize call, continue to original handler.
      _chain_intr(old_int15);
   }
}


// Initialise allocation of DOS5 UMBs. This should be called before
// DOS5 UMBs can be allocated. Returns whether DOS5 UMBs could be present.
// If this returns true, ExitDOS5UMB should be called when finished
// with the allocation (of course the memory isn't deallocated then!).
// Notice that although DOS5 UMBs exist also under DR-DOS (well you can
// allocate upper memory there), this is a bit flaky, but more important,
// the same memory can be allocated under DR-DOS using XMS UMBs, so there's
// no point in trying for DR-DOS here...
// IMPORTANT! You cannot rely on malloc() to succeed after this returned
// success and before ExitDOS5UMB() is called!!!
static int InitDOS5UMB(void) {
   unsigned tmp;

   // Get the original UMB link state.
   _AX = 0x5802;
   geninterrupt(0x21);
   tmp = _AX;
   if ( _FLAGS & 0x1 ) {
      // Call failed, no such function, no DOS5 UMBs
      UMBlinkstate = allocstrat = -1;
      return 0;
   }
   else
      UMBlinkstate = (tmp & 0xFF);

   // Get original allocation strategy (DOS 3+)
   _AX = 0x5800;
   geninterrupt(0x21);
   tmp = _AX;
   if ( _FLAGS & 0x1 ) {
      // Call failed, cannot get allocation strategy (unlikely).
      allocstrat = UMBlinkstate = -1;
      return 0;
   } else
      allocstrat = (tmp & 0xFF);

   // Set new UMB link state to 1 (UMBs part of mem chain), this
   // enables DOS memory allocation in the UMBs
   _BX = 1;
   _AX = 0x5803;
   geninterrupt(0x21);
   if ( _FLAGS & 0x1 ) {
      // Call failed, probably DOS5 without DOS=UMB in CONFIG.SYS
      UMBlinkstate = allocstrat = -1;
      return 0;
   }

   // Set new allocation strategy to 0x41 (best fit, high mem only).
   // Notice that care should be taken NOT to allocate lower memory
   // because malloc() will fail then. UMBmalloc has extra provision
   // for that. This is also the reason malloc() cannot be called
   // between InitDOS5UMB...ExitDOS5UMB
   _BX = 0x41;
   _AX = 0x5801;
   geninterrupt(0x21); // Ignore errors, they shouldn't possibly occur here.

   // Calculate the paragraph where conventional memory ends, for comparing
   // against when allocating memory, making sure we don't alloc conv mem...
   // (yeah replace this with 0xA000 if you're not as paranoid as I am,
   // nothing will go badly wrong anyway... I guess... I hope) (ever heard
   // about DOS extenders giving 900K solid DOS memory? That's why...)
   convmemend = _bios_memsize() * 0x40;

   return 1; // Return success
}


// ExitDOS5UMB is needed for cleanup after DOS5 UMBs are allocated.
// Resets the memory allocation state and UMB link state, if they were
// set before (does nothing if called after InitDOS5UMB returned false).
static void ExitDOS5UMB(void) {
   if ( UMBlinkstate != -1 ) { // Reset UMB link state if necessary
      _BX = UMBlinkstate;
      _AX = 0x5803;
      geninterrupt(0x21); // ignore errors
   }
   if ( allocstrat != -1 ) { // Reset allocation strategy if necessary
      _BX = allocstrat;
      _AX = 0x5801;
      geninterrupt(0x21); // ignore errors
   }
}


// Function to determine largest available UMB.
static int UMBmax(int memtype) {
   if ( memtype == MT_XMSUMB ) {
      if (!fXMM) return 0;
      _AH = 0x10;
      _DX = 0x7000; // Extreme large claim (impossible).
      (*((void far (*)(void))XMM_control))();
      return _DX; // Now contains size of largest UMB.
   } else if ( memtype == MT_DOS5UMB ) {
      unsigned seg;
      int tmp;

      tmp = allocmem(0xFFFF, &seg); // Extremely large clain
      // errno = 0; // Uncomment if you care for errno == 0 during exec
      return tmp;
   }
   IE();
   return 0;
}


// Function to allocate an UMB.
static unsigned UMBmalloc(int memtype, int size) {
   if ( memtype == MT_XMSUMB ) {
      if (!fXMM) return 0;
      _DX = size;
      _AH = 0x10;
      (*((void far (*)(void))XMM_control))();
      if (_AX==1)
         return _BX;
      else // Some error, give up.
         return 0;
   } else if ( memtype == MT_DOS5UMB ) {
      unsigned seg;

      if ( allocmem(size, &seg) != -1 ) // allocmem returns error?
         return 0;
      if ( seg < convmemend ) {
         // Whoops! We allocated conventional memory! Free & return error
         freemem(seg);
         return 0;
      }
      return seg;
   }
   IE();
   return 0;
}


// Function to release UMB.
static void UMBfree (int memtype, unsigned para) {
   if ( memtype == MT_XMSUMB ) {
      _DX = para;
      _AH = 0x11;
      (*((void far (*)(void))XMM_control))();
   } else if ( memtype == MT_DOS5UMB ) {
      freemem(para);
   } else {
      IE();
   }
}


// Map EMS page to physical memory (we use only a single handle).
// Perfect parameters are assumed.
static void EMS_map (int pageno, int offset){
   // Select specific page.
   _DX = hEMS;
   _BX = pageno;
   _AH = 0x44;
   _AL = offset; // Frame offset=0..3
   geninterrupt(0x67);
   if ( _AH != 0 ) {
      FatalError (163,"EMS error: map failed");
   }
}


// Deallocate an EMS handle
static void DeallocateEMS(unsigned hEMS)
{
   _DX = hEMS;
   _AH = 0x45;
   geninterrupt(0x67);
}


// Deallocate an XMS handle
static void DeallocateXMS(unsigned hXMS)
{
   // Free XMS memory.
   _DX = hXMS;
   _AH = 0xA;
   (*((void far (*)(void))XMM_control))();
}


// Automatically called with atexit to release memory.
void ExitMem(void) {
   Critical();

   int index4;

   if ( fEMS && hEMS ) // EMS has been allocated.
      DeallocateEMS(hEMS);
   if ( fXMM && hXMS ) // XMS has been allocated.
      DeallocateXMS(hXMS);
   if ( fI15 ) // INT15 is allocated, reinstall the original INT 15 handler.
      setvect(0x15, old_int15);

   // Check for allocated UMBs.
   // Allocated UMBs have fStatus != ST_EMPTY && nMemType != MT_NONE
   // But we don't have to free the EMS page frame with nMemType == MT_EMSUMB

   for (index4 = 0; index4 < MAX_META4; index4++)
      if ( META4[index4].fStatus != ST_EMPTY &&
	    META4[index4].nMemType != MT_NONE &&
	    META4[index4].nMemType != MT_EMSUMB )
	 UMBfree(META4[index4].nMemType, META4[index4].para);

   Normal();
}


// Detect if XMM is present.
static int DetectXMM(void) {
   if ( gmaxXMS == 0 && gmaxUMB == 0 )
      return 0; // Who'd need an XMM when XMS and UMBs are not allowed?

   _AX = 0x4300;
   geninterrupt(0x2F);
   if ( _AL != 0x80 )
      return 0;  // No XMM installed.
   // Get XMM_control vector.
   _AX = 0x4310;
   geninterrupt(0x2F);
   XMM_control = MK_FP(_ES, _BX);
   return 1;
}


// Notice! QueryFreeEMS can only be called before any EMS is allocated,
// otherwise the usage maximum in gmaxEMS might easily be exceeded.
static int QueryFreeEMS(void) {
   int tmp;

   _AH = 0x42;
   geninterrupt(0x67);
   tmp = _BX;
   if ( _AH || tmp == 0 ) {
      // some problem or no pages
      return 0;
   }

   // Don't allocate too much EMS
   if (tmp > gmaxEMS)
      tmp = gmaxEMS;
   return tmp;
}

int emshi, emslo;

// Detect if EMS installed.
static int DetectEMS(void) {
   FILE* file;
   int tmp;

   if ( gmaxEMS == 0 )
      return 0; // No EMS allowed, so don't detect it either.

   file = fopen ("EMMXXXX0","r");
   if (!file)
      return 0;
   // Test if some idiot didn't make a file "EMMXXXX0" in the current dir
   if ( (tmp = ioctl(fileno(file), 0)) == -1 ) {
      fclose (file);
      return 0; // IOCTL returns an error (this is being extremely paranoid)
   }
   if ( (tmp & 0x80) != 0x80 ) {
      fclose (file);
      return 0; // It's not a character device.
   }
   // Test the output status.
   if ( (tmp = ioctl(fileno(file), 7)) == -1 ) {
      fclose (file);
      return 0; // IOCTL returned an error again (this really can't happen)
   }
   fclose (file);
   if ( (tmp & 0xFF) != 0xFF )
      return 0; // The EMS function handler isn't ready.
#ifdef PARANOID_EMSTEST
   // This test might fail if something hooked int67. I won't recommend it.
   if ( strncmp((char far*) MK_FP(FP_SEG(getvect(0x67)), 0xA),
	 "EMMXXXX0", 8) != 0 )
      return 0; // Not the right device at the segment of the int67 handler
#endif

   _AH = 0x46;
   geninterrupt(0x67);
   int ax=_AX;
   if (_AH) return 0;
   _AX=ax;
   int al = _AL;

   if (CONFIG.fEMS==1 &&
	  ((al/16)<4)) return 0;

   emshi=al/16;
   emslo=al%16;

   // Get EMS frame address.
   _AH = 0x41;
   geninterrupt(0x67);
   EMS_frame = _BX;
   if ( _AH != 0 )
      return 0; // Error in EMS handler
   // Check number of mappable pages. Some weird memory managers
   // allow page frames < 64K...
   _AX = 0x5801;
   geninterrupt(0x67);
   if ( ! (_AH == 0x84 || // Unknown function, this is EMS3, always OK
         ( _AH == 0 && _CX > 3 )) )
      // Either some error other than "unknown function" or page
      // frame < 64K
      return 0;

   return 1; // return true
}

// Allocates blocks EMS blocks, returns the handle to the memory.
static int AllocateEMS(int blocks) {
   unsigned handle, pages;

   // Call ClaimPages
   _BX = blocks;
   _AH = 0x43;
   geninterrupt(0x67);
   // Remember handle and size;
   handle = _DX;
   pages = _BX;
   if ( _AH ) // Bad result, give up.
      return 0;
   if ( pages != blocks ) {
      // Strange, EMS allocation returned less than asked for.
      DeallocateEMS(handle);
      return 0;
   }

   return handle;
}


// Same warning as QueryFreeEMS, can only be called before AllocateXMS.
static int QueryFreeXMS(void) {
   int tmp;

   _AH = 8;
   _BL = 0; // needed for some XMS managers
   (*((void far (*)(void))XMM_control))();
   tmp = _AX; // Beware of register changes!
   if ( _BL != 0 )
      return 0; // Error during query free.
   // convert to 16K blocks
   tmp /= 16;
   // Don't allocate too much
   if ( tmp > gmaxXMS )
      tmp = gmaxXMS;
   return tmp;
}


// Allocate XMS memory, returns handle
static unsigned AllocateXMS(int blocks) {
   unsigned handle;

   _DX = blocks * 16;
   _AH = 9;
   (*((void far (*)(void))XMM_control))();
   handle = _DX;
   if ( _AX != 1 )
      return 0; // Handler error.

   return handle;
}


// Allocate INT15 memory, if present. Returns the number of 16K blocks
// of allocated memory. This is collected in a single function, unlike
// EMS/XMS handling, because this is a little more complicated. maxI15
// is the maximum amount of INT15 memory allocated
static int AllocateInt15(int maxI15) {
   unsigned bottomused; // Number of K used at the bottom of extended mem.
   unsigned extfree; // Number of K promised free by INT 15
   int tmp;

   if ( maxI15 == 0 )
      return 0; // No Int15 Allowed so return failure.

   // Query the amount of usable extended memory.
   _AH = 0x88;
   geninterrupt(0x15);
   extfree = _AX;
   if ( _FLAGS & 1 || !extfree ) {
      // Carry set (error) or no INT15 extended memory at all.
      return 0;
   }
   // Test for the presence of DOS 3+ PRINT.COM resident
   _AX = 0x0100;
   geninterrupt(0x2F);
   if ( _AL == 0xFF )
      return 0; // PRINT.COM installed, can't check VDISK, can't use INT15.
   if ( strncmp((char far*) MK_FP(FP_SEG(getvect(0x19)), 0x12),
	 "VDISK  V", 8) == 0 ) {
      // There is a VDISK device. The three bytes at the int19 handler seg,
      // offset 2C give the linear address of the first byte of extended
      // memory usable. Calculate number of K we must skip.
      bottomused = (unsigned) ( (
         (*((unsigned long*) MK_FP(FP_SEG(getvect(0x19)), 0x2C)) >> 8) -
         0x100000L + 1023) / 1024 );
         // 0x100000 is the start of extended memory. Round up to 1K
   } else {
      // INT15 memory and an XMM are very uncommon, but who knows...
      // In fact this can be replaced by bottomused = 0, since if fXMM is
      // true, this routine shouldn't even be called at all. No INT15 usage
      // if there is an XMM.
      bottomused = fXMM ? 64 : 0; // Don't clobber HMA, if any
   }

   if ( extfree <= bottomused ) {
      // No INT 15 memory free after all
      return 0;
   }
   if ( (tmp = (extfree - bottomused) / 16) == 0 )
      return 0; // No 16K blocks free in extended memory.

   // Don't claim too much memory.
   if ( tmp > maxI15 )
      tmp = maxI15;

   // Allocate the memory
   top_of_int15 = extfree - tmp * 16;
   // Hook INT15
   old_int15 = getvect(0x15);
   fI15 = 1; // Set INT15 memory allocated
   setvect(0x15, (void interrupt (*)(...)) NewInt15);

   return tmp;
}

int fExitMem=0;

void UCBoost (BYTE *data, int size);

// Init this memory manager. Should always be called before ANY other
// mem function.
// Usage of EMS, XMS, Extended, UMBs via gmax variables
void InitMem (void){
   Critical();
   int i, index16, tmp, nEMSUMB, maxI15, index4, mt, tmpmt, fDOS5UMB;
   unsigned p, UMBleft;

   // Init the global variables.
   fXMM = fEMS = fI15 =
   hEMS = hXMS =
   nDIR = nEMS = nXMS = 0;
   nEMSUMB = 0; // Counts number of pages available in the EMS frame

   // Mark all slots empty.
   for (i=0;i<MAX_META16;i++)
      META16[i].fStatus = ST_EMPTY;
   for (i=0;i<MAX_META4;i++)
      META4[i].fStatus = ST_EMPTY;
   for (i=0;i<MAX_DIRMETA16;i++)
      dir_meta16[i] = (void far*) NULL;

   // Automatic deallocate EMS etc at program exit.
   fExitMem=1;

   fXMM = DetectXMM(); // Detect an XMM (and setup the XMM_control)
   fEMS = DetectEMS(); // Detect EMS memory

   index16 = 0; // indexes the META16[] array, points to first free item.

   // Allocate EMS...
   if ( fEMS && ( (tmp = QueryFreeEMS()) > 0 ) ) {
      if ( index16 + tmp > MAX_META16 )
	 tmp = MAX_META16 - index16;
      if ( (hEMS = AllocateEMS(tmp)) != 0 ) {
	 // reserve 4 pages for UMBs in the page frame
	 nEMSUMB = (tmp > 4) ? 4 : tmp;
	 if ( (tmp -= nEMSUMB) > 0 ) {
	    // Alloc succeeded and memory left after UMBs, update META16[]
	    nEMS = tmp;
	    tmp += index16;
	    for (; index16 < tmp; index16++)
	       META16[index16].fStatus = ST_FREE;
	 }
      }
   }

   // Allocate XMS...
   if ( fXMM && ( (tmp = QueryFreeXMS()) > 0 ) ) {
      if ( index16 + tmp > MAX_META16 )
	 tmp = MAX_META16 - index16;
      if ( (hXMS = AllocateXMS(tmp)) != 0 ) {
	 // Alloc succeeded, update META16[]
	 nXMS = tmp;
	 tmp += index16;
         for (; index16 < tmp; index16++)
            META16[index16].fStatus = ST_FREE;
      }
   }

   // Allocate Int 15 extended memory... only if no EMS and no XMM!!
   maxI15 = MAX_META16 - index16 > gmaxI15 ? gmaxI15 : MAX_META16 - index16;
   if ( !fEMS && !fXMM && (tmp = AllocateInt15(maxI15)) != 0 ) {
      // We have allocated tmp blocks of INT15 memory, tmp <= maxI15
      // Update META16[]
      nI15 = tmp;
      tmp += index16;
      for (; index16 < tmp; index16++)
	 META16[index16].fStatus = ST_FREE;
   }

   index4=0; // Next free slot to enter new UMB in META4 table.
   UMBleft = gmaxUMB / 256; // No. of 4K blocks of UMBs left to allocate

   // First UMB is (if possible) the EMS page frame. nEMSUMB indicates
   // the number of usable pages. Note that this usage of UMBs is
   // independent of gmaxUMB!!
   if ( nEMSUMB ) {
      // Map nEMSUMB pages
      for (i = 0; i < nEMSUMB; i++)
	 EMS_map(i, i);
      // Notice ALL EMS related stuff should PRESERVE this! TSR's etc
      // will not cause problems when using EMS because they restore
      // paging anyway.

      tmp = 4*nEMSUMB;
      tmpmt = MT_EMSUMB;
      for (i = 0; i < tmp; i++) {
	 META4[index4].fStatus = ST_FREE;
	 META4[index4].nMemType = tmpmt;
	 META4[index4++].para = EMS_frame + i*256; // 256 paragraphs is 4 kb
	 tmpmt = MT_NONE; // Only first block has MT_EMSUMB
      }
   }

   // Now go hunting for "real" UMB's.
   if (UMBleft){
      fDOS5UMB = InitDOS5UMB();
   } else
      fDOS5UMB = 0;

   for (mt = MT_FIRSTTYPE; mt != MT_NONE; mt++) { // For all memtypes
      // Check if memtype exists, else continue with next memtype
      if ( mt == MT_XMSUMB && !fXMM )
	 continue;
      if ( mt == MT_DOS5UMB && !fDOS5UMB )
	 continue;

      tmp = UMBmax(mt) / 256; // 256 paragraphs is 4 kb
      if ( tmp > UMBleft )
	 tmp = UMBleft;
      if ( tmp + index4 > MAX_META4 )
	 tmp = MAX_META4 - index4; // Don't go beyond the end of META4[]
      while ( tmp ) {
	 p = UMBmalloc (mt, tmp*256);
	 if ( p != 0 ) {
            fUMB=1;
            UMBleft -= tmp;
            tmpmt = mt;
            for (i = 0; i < tmp; i++) {
               META4[index4].fStatus = ST_FREE;
               META4[index4].nMemType = tmpmt;
               META4[index4++].para = p + i*256;
               tmpmt = MT_NONE; // Only the first block has MT_{DOS5|XMS}UMB
            }
	    tmp = UMBmax(mt) / 256;
            if ( tmp > UMBleft )
	       tmp = UMBleft;
            if ( tmp + index4 > MAX_META4 )
               tmp = MAX_META4 - index4;
         } else // Very unlikely but e.g. a TSR could have allocated memory.
            break;
      }
   }

   if ( fDOS5UMB )
      ExitDOS5UMB();

   // Don't divide the memory just yet, wait for calls to donate16() to
   // complete.
   freex[1] = 0xFFFF; // Value that indicates memory isn't divided yet

//   if (getenv("UC2_VMEM")) return; // hyper-virtual mode, leave mem alone

#ifndef STAND_ALONE
   #define MAX_MEM_USE 1265000L // qqq
   unsigned long spare = farcoreleft()-MAX_MEM_USE;
   if (getenv("UC2_VMEM")){
      WORD wSize = 1024 * atoi (getenv("UC2_VMEM"));
      if (wSize<4096) wSize = 4096;
      if (wSize>49152U) wSize = 49152U;
      spare = spare + 8192 - wSize;
   }
   if (spare<1000000L){ // no overflow
      void *bp;
      int ucb=0;

      if (spare<28U*1024){
	 bp=exmalloc(28U*1024);
	 if (bp){
	    UCBoost ((unsigned char *)bp, 28);
	    ucb=1;
	 } else if (spare<18U*1024){
	    bp=exmalloc(18U*1024);
	    if (bp){
	       UCBoost ((unsigned char *)bp, 18);
	       ucb=1;
	    }
	 }
      }

      while (NULL!=(bp=(unsigned char*)exmalloc (16384))){
	 donate16 (bp);
      }
      if (!ucb){
	 if (spare>28U*1024){
	    UCBoost ((unsigned char *)farmalloc(28U*1024),28);
	 } else {
	    if (spare>18U*1024){
	       UCBoost ((unsigned char *)farmalloc(18U*1024),18);
	    }
	 }
      }
      spare = farcoreleft()-MAX_MEM_USE;
      if (getenv("UC2_VMEM")){
	 WORD wSize = 1024 * atoi (getenv("UC2_VMEM"));
	 if (wSize<4096) wSize = 4096;
	 if (wSize>49152U) wSize = 49152U;
	 spare = spare + 8192 - wSize;
      }
      if (spare<1000000L){ // no overflow
	 for (unsigned i=0;i<(unsigned)(spare/16450L);i++){ // include overhead
	    donate16(farmalloc(16384));
	 }
      }
   }
#endif
   Normal();
}


void oe1 (void);
void oe2 (void);


#ifndef STAND_ALONE

void repit (void){
   if (fEMS && hEMS){
      oe1();
      Out (3|8,"EMS %d.%d",emshi,emslo);
      oe2();
   }
   if (fXMM && hXMS){
      oe1();
      Out (3|8,"XMS");
      oe2();
   }
   if (fI15){
      oe1();
      Out (3|8,"RawExt");
      oe2();
   }
   if (fUMB){
      oe1();
      Out (3|8,"UMA");
      oe2();
   }
}

#endif

// Allocate "extra" memory. The used method might SEEM inefficient but
// looking at the CRTL will change that thought in a few seconds. It
// is recomended to only call the function for the larger blocks. (e.g. >3k)
void far* exmalloc (long size){
   int i,j;
   unsigned no = (unsigned)((size+4095L)/4096L); // no of 4k blocks

   // Find a row of "no" connected free blocks.
   for (i=0;i<MAX_META4-no;i++) {
      if (META4[i].fStatus==ST_FREE) {
	 for (j = i+1; j < i+no; j++) {
	    // subsequent blocks should be free & not the beginning of
	    // another UMB (indicated by nMemType != MT_NONE)
	    if ( META4[j].fStatus != ST_FREE ||
		  META4[j].nMemType != MT_NONE ) {
	       i=j; // Saves time.
	       goto failed;
	    }
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
void exfree (void far* adr){
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
int extest (void far* adr){
   int i;
   unsigned para = FP_SEG(adr);

   // Locate memory in list of blocks.
   for (i=0;i<MAX_META4;i++)
      if (META4[i].para==para)
         return 1; // Found.
   return 0; // Not found.
}


// Return number of free 4k blocks.
int coreleft4 (void){
   int i,c=0;
   for (i=0;i<MAX_META4;i++)
      if (META4[i].fStatus==ST_FREE) c++;
   return c;
}


// Donate a 16K block of memory. This can only be called BEFORE malloc16
// because this invalidates the contents of all meta16 blocks (shifts)
// There is no protection against this.
void donate16(void far* m) {
   if ( nDIR == MAX_DIRMETA16 )
      return; // Too much memory donated

   dir_meta16[nDIR] = m; // Save location of block
   if ( nEMS+nXMS+nI15+nDIR >= MAX_META16 ) {
      // whoops! too much memory. Nuke an extra memory block, we prefer DIR.
      if ( nI15 )
         nI15--;
      else if ( nEMS )
         nEMS--;
      else if ( nXMS )
         nXMS--;
      else {
         // Impossible if MAX_META16 > MAX_DIRMETA16 (if no bugs ;)
         IE();
         return;
      }
   } else {
      // Mark block as free. Since all is still ST_FREE, at the end is OK
      META16[nEMS+nXMS+nI15+nDIR].fStatus = ST_FREE;
   }
   nDIR++; // Increment direct meta16 blocks available
}


// This checks if memory needs to be divided over the various groups and
// does so if needed. Called on coreleft16() and malloc16(). You shouldn't
// call donate16() afterwards.
// Option: call this explicitly after doing the donate16() calls and don't
// call this from inside coreleft and/or malloc (depends on how often
// malloc16 is called that the reduced overhead is worth it).
void DivideMem(void) {
   int bb, q1, q2;

   if ( freex[1] != 0xFFFF ) {
      // Memory is already divided, return immediately.
      return;
   }

   // Divide extra memory over various groups
   bb = coreleft16r();

   if (bb>=1){
      bb--;
      freex[4]=1;
   } else {
      freex[4]=0;
   }
   if (bb>=1){
      bb--;
      freex[4]++;
   }
   if (bb>=1){
      bb--;
      freex[4]++;
   }
   if (bb>=4){
      bb-=4;
      freex[6]=4; // most recently used master cache
#ifdef UCPROX
      if (beta && 0==strcmp(getenv("NOCLRUMAS"),"ON")) freex[6]=0;
#endif
   } else
      freex[6]=0;
   if (bb>=3){
      bb-=3;
      freex[1]=3; // super master storage
#ifdef UCPROX
      if (beta && 0==strcmp(getenv("NOCMASST"),"ON")) freex[1]=0;
#endif
   } else
      freex[1]=0;
   if (bb>=10){
      bb-=10;
      freex[2]=10; // most recently use hash scheme cache
#ifdef UCPROX
      if (beta && 0==strcmp(getenv("NOCHASH"),"ON")) freex[2]=0;
#endif
   } else
      freex[2]=0;
   if (bb){ // some cache
      bb--;
      freex[7]=1; // at least 16k boost I/O
   }
   if (bb>8){ // 'mucho' cache
     bb-=3;
     freex[7]+=3; // 64k booster I/O
   }
   if (bb){
      q1=20*bb/100;  // 20%
      q2=60*bb/100;  // 60%
      bb-=q1;
      freex[3]=q1;   // master storage (20%)
#ifdef UCPROX
      if (beta && 0==strcmp(getenv("NOCMAST"),"ON")) freex[3]=0;
#endif
      bb-=q2;
      freex[4]+=q2;   // disk cache (60%)
#ifdef UCPROX
      if (beta && 0==strcmp(getenv("NOCDISK"),"ON")) freex[4]=0;
#endif
      freex[5]=bb;   // vmem support (rest, 20%)
#ifdef UCPROX
      if (beta && 0==strcmp(getenv("NOCVMEM"),"ON")) freex[5]=0;
#endif
   }
   else
      freex[3] = freex[5] = 0;
}


// Allocate a 16k cache block of type g
int malloc16 (char g) {
   int i;

   DivideMem();
   freex[g]--; // No checking??
   // Simple, just go looking for a free entry in the table.
   for (i=0;i<MAX_META16;i++)
      if ( META16[i].fStatus == ST_FREE ) {
	 META16[i].fStatus = ST_USED;
	 return i+1; // Handles are 1..MAX_META16, value 0 is reserved.
      }
   IE(); // Internal Error
   return 0; // No block available.
}


// Release a 16k cache block. Allows call with 0.
void free16 (char g, int i){
   if (!i) return;
   freex[g]++;
   if (META16[i-1].fStatus==ST_FREE){
//      Out (7,"[[[%d %d]]]",g,i);
//      return;
      IE();
   }
   if (i-2>MAX_META16) IE();
   META16[i-1].fStatus = ST_FREE;
}


// Return number of free 16k blocks.
static int coreleft16r (void){
   int i,c=0;
   for (i=0;i<MAX_META16;i++)
      if (META16[i].fStatus==ST_FREE) c++;
   return c;
}


// Return number of available 16k blocks in group g.
int coreleft16 (char g){
#ifdef UCPROX
   if (debug){
      Out (7,"[CL16 ");
      for (int i=1;i<6;i++){
	 Out (7,"%d ",freex[i]);
      }
      Out (7,"]");
   }
#endif
   DivideMem();
   return (int)freex[g];
}


// Copy to/from INT 15 extended memory. fdest, fsrc are indicators whether
// dest, src are far pointers to something in the first meg (fdest or
// fsrc == 0) or a linear address to anything (fdest or fsrc != 0).
// Called by DealWithOddLength.
static void CopyExtended(unsigned fdest, unsigned long dest,
      unsigned fsrc, unsigned long src, unsigned words)
{
   struct { // on the stack is fine
      unsigned zero1[8];      // 16 zero bytes
      unsigned srclen;        // source segment length
      unsigned long srcdesc;  // source segment address + access
      unsigned zero2;         // zero word
      unsigned dstlen;        // destination segment length
      unsigned long dstdesc;  // destination segment address + access
      unsigned zero3[9];      // 18 zero bytes
   } gdt;

   // fill the struct with zero's
   memset(&gdt, 0, sizeof(gdt));
   gdt.srclen = gdt.dstlen = 0x4000; // 16K segments
   // 0x93 is the access rights byte. Should be in the highbyte of the long
   // Fill src & dest descriptors. Must always be linear addresses.
   gdt.srcdesc = (0x93L << 24) | (fsrc ? src : FP_LIN((void far*) src));
   gdt.dstdesc = (0x93L << 24) | (fdest ? dest : FP_LIN((void far*) dest));

   // Now copy it.
   _ES = FP_SEG((void far*) &gdt);
   _SI = FP_OFF((void far*) &gdt);
   _CX = words; // Number of words to copy!
   _AH = 0x87;
   geninterrupt(0x15);
   if ( _AH ) {
      FatalError (163,"failed to copy from/to extended memory");
   }
}


// Copy to/from XMS memory. See comment at CopyExtended.
static void CopyXMS(unsigned hdest, unsigned long destofs,
      unsigned hsrc, unsigned long srcofs, unsigned words)
{
   static struct { // MUST be in the DS segment!
      unsigned long size;
      unsigned source;
      unsigned long soffset;
      unsigned dest;
      unsigned long doffset;
   } trs;

   trs.size = 2*words;
   trs.source = hsrc;
   trs.soffset = srcofs;
   trs.dest = hdest;
   trs.doffset = destofs;
   _SI = FP_OFF((void far*)&trs); // DS:SI points to trs now
   _AH = 0x0B;
   (*((void far (*)(void))XMM_control))();
   if ( _AX != 1 ) {
      FatalError (163,"the XMS driver (HIMEM.SYS) failed to copy extended memory");
   }
}


// DealWithOddLength.
// Copy the memory identified by (i_src, l_src) of length len bytes to the
// memory identified by (i_dest, l_dest), using *pXMemCopy as the copying
// function. This assumes that *pXMemCopy should be called with the number
// of words to copy as length, and also that arithmetic on the l_src, l_dest
// parameters is possible. This also assumes that _src and _dest are
// exchangable.
// Finally, this assumes that normal memory is addressed when the i_*
// parameter is zero. The l_* parameter is in that case a far* cast to
// an unsigned long.
// CopyXMS() and CopyExtended() apply to these restrictions.
// This function specifically checks for three separate cases:
// len equals 1, len is odd, len is even.
// Note that the case where len is odd (or 1), a lot of extra code is
// thrown in to minimize the number of calls to the (expensive) *pXMemCopy
// routine, copying 1 extra byte if necessary and correcting the extra byte
// before or after the copy. This has to deal with a number of border cases
// like the region to be copied being at the very end of one of our memory
// blocks (maybe even the end of physical memory).
// The extra code means that, for UC, normally only 1 call to *pXMemCopy
// is needed, except when copying an odd number of bytes to cache, in
// which case two calls to *pXMemCopy are needed.
// This specifically does NOT optimize for nicely aligned copying
// regions by first copying some loose bytes and then copying a big chunk
// aligned on 2**n-byte boundaries. Let the memory manager do that if it
// improves things...
static void DealWithOddLength(t_pXMemCopy pXMemCopy,
      unsigned i_dest, unsigned long l_dest,
      unsigned i_src, unsigned long l_src, unsigned len)
{
   if ( len % 2 == 0 ) {
      // len is even, it can be handled by *pXMemCopy
      (*pXMemCopy)(i_dest, l_dest, i_src, l_src, len/2);
   } else if ( len != 1 ) {
      char far* p;
      int i;
      char tmp;

      // 3 or more odd bytes to copy. If destination is directly accessible
      // one call to XMemCopy suffices, otherwise we need two... For the
      // hairy details about this see comment for 1 byte copies, which is
      // even more complex. Note that this still works if you want to copy
      // the last 3 bytes of memory to offset 0 in real memory (the general case
      // with 2 calls is used in that specific case)

      // Check if destination is directly accessible.
      p = NULL;
      if ( i_dest == 0 ) {
	 // Check if last byte of src/dest isn't on a 16K/64K boundary
	 if ( ((l_src + len-1) & 0x3FFF) != 0x3FFF &&
	       ((l_dest + len-1) & 0xFFFF) != 0xFFFF ) {
	    // Both not on a boundary, OK to copy src+len and dest+len extra.
	    p = (char far*) (l_dest + len);
	    i = 0; // Copy at src,dest
	 } else if ( (l_dest & 0xFFFF) > 0 && (l_src & 0x3FFF) > 0 ) {
	    // last byte of src or dest at a boundary, first bytes not at 0
	    i = -1; // OK to copy byte at src-1,dest-1
	    p = (char far*) (l_dest - 1);
	 } // else
	 // either src, dest at opposite boundaries, unsafe to either
	 // copy one byte before or one byte after.
      }
      if ( p != NULL ) {
	 // dest is directly accessible, grab the byte that will be
	 // overwritten when we copy 1 byte extra, and replace that after
	 // the copy.
	 tmp = *p;
	 (*pXMemCopy)(i_dest, l_dest + i, i_src, l_src + i, (len+1)/2);
	 *p = tmp;
      } else {
	 // The general case, dest isn't directly accessible or other trouble
	 // First copy the first (len-1)/2 bytes...
	 (*pXMemCopy)(i_dest, l_dest, i_src, l_src, (len-1)/2);
	 // For the last byte copy the word at l_src + len-2
	 (*pXMemCopy)(i_dest, l_dest + len-2, i_src, l_src + len-2, 1);
      }
   } else {
      // len == 1, Waaa!!! This is really hairy...
      // The basic idea here is to get the word at src, then get the
      // word at dest+1, and finally copy the word consisting of the
      // first byte of src and the first byte of dest+1 to dest.
      // This leaves dest+1 unmodified.
      // But... since we might be addressing the very last byte of
      // physical memory, we cannot assume dest+1 or src+1 exists. So we
      // have to check for a 16K boundary and use dest-1 or src-1 then.
      // Also, calls to *pXMemCopy are very expensive (usually they switch
      // to protected mode), so we throw in a lot of extra code to avoid
      // calling *pXMemCopy and doing things directly if we can.
      // scratch[] is the area used as "interim" storage where the words
      // to be copied are built.

      char scratch[3];

      // Get the byte at src into scratch[0]...
      if ( i_src == 0 ) {
	 // src is directly accessible memory, so just grab the byte we want
	 scratch[0] = *(char far*) l_src; // That's all there is to it.
      } else if ( (l_src & 0x3FFF) != 0x3FFF ) {
	 // src is not at a 16K boundary...
	 (*pXMemCopy)(0, (unsigned long) scratch, i_src, l_src, 1);
      } else {
	 // src is at a 16K boundary... copy the word at src-1 instead.
	 (*pXMemCopy)(0, (unsigned long) scratch, i_src, l_src-1, 1);
	 scratch[0] = scratch[1]; // We need the byte at src
      }

      // Copy the byte at scratch[0] to dest, possibly by putting dest+1 at
      // scratch[1] to avoid changing dest+1 when copying a word
      if ( i_dest == 0 ) {
	 // dest is directly accessible memory, so just put the byte there
	 *(char far*) l_dest = scratch[0]; // That's all folks...
      } else if ( (l_dest & 0x3FFF) != 0x3FFF ) {
	 // dest is not at a 16K boundary...
	 (*pXMemCopy)(0, (unsigned long) (scratch+1), i_dest, l_dest, 1);
	 scratch[1] = scratch[2]; // scratch[1] should be the byte at dest+1
	 (*pXMemCopy)(i_dest, l_dest, 0, (unsigned long) scratch, 1);
      } else {
	 // dest is at a 16K boundary. Use the word at dest-1...
	 (*pXMemCopy)(0, (unsigned long) (scratch+1), i_dest, l_dest-1, 1);
	 scratch[2] = scratch[0]; // Now scratch+1 contains the word...
	 (*pXMemCopy)(i_dest, l_dest-1, 0, (unsigned long) (scratch+1), 1);
      }
   }
}


// Copy 0..16k of memory to a cache block.
void to16 (int itx, unsigned ofs, void far* ptr, int len) {
//   if (itx==12) Out (7,"[T %u %u]",ofs,len);

   Critical();
   if (META16[itx-1].fStatus==ST_FREE) IE();
   int tmp;

   if ( len == 0 ){
      Normal();
      return; // Nothing to copy
   }

   itx--; // Handles are numbered from 1.
   // hack explanation: inside every if() the assignment calculates the
   // offset (page nr) for the next memory type, if approriate. If the
   // result of that calculation is negative, we found the correct memory
   // type.
   // Memory usage is: donated, Int15. OR: donated, XMS, EMS.
   if ( (tmp = itx - nDIR) < 0 ) { // page is in donated memory
      RapidCopy((void far*) ((char far*) dir_meta16[itx] + ofs), ptr, len);
   } else if ( fI15 ) { // page is in INT15 extended memory.
      // Note that tmp contains the "page" of INT15 memory.
      // Let DealWithOddLength handle the copying...
      DealWithOddLength(CopyExtended,
	    // The 3rd arg is the linear address in our INT15 memory block.
	    1, 0x100000L + 1024L*top_of_int15 + 16384L*tmp + ofs,
	    0, (unsigned long) ptr, len);
   } else if ( (itx = tmp - nXMS) < 0 ) { // page is in XMS
      // tmp still contains the page into XMS
      // Again use DealWithOddLength to call the copy functions
      DealWithOddLength(CopyXMS, hXMS, 16384L*tmp + ofs,
	    0, (unsigned long) ptr, len);
   } else { // page is in EMS
      // itx contains the page number of EMS memory minus 4 pages for EMS UMB
      int page=0;

      // This part is really tricky. Since the EMS page frame is used
      // to emulate UMB the "ptr" variable can point into this page frame.
      // This part makes sure that in case ptr is inside page 0 page 3 is
      // used to assure proper handling of this problem.
      // First absolute adresses are calculated, then they are compared.
      long lptr = FP_LIN(ptr);
      long lems = FP_LIN(MK_FP(EMS_frame,0)); // Nice macro calls eh? :)
      // check if region (lptr,lptr+ofs) overlaps with (lems,lems+16K)
      if ( lptr + ofs >= lems && lptr < lems+16384L )
	 page=3;
      EMS_map (itx + 4, page);
      RapidCopy(MK_FP(EMS_frame, page*16384U + ofs), ptr, len);
      EMS_map (page, page); // Restore EMS for use as UMB.
      // What if page 3 wasnt mapped? Then there's no cache EMS memory anyway
   }
   Normal();
}


// Copy 0..16k of memory from a cache block.
// Comments removed for brevity, see to16()
void from16 (void far* ptr, int itx, unsigned ofs, int len){
//   if (itx==12) Out (7,"[F %u %u]",ofs,len);

   Critical();

   if (META16[itx-1].fStatus==ST_FREE) IE();
   int tmp;

   if ( len == 0 ){
      Normal();
      return;
   }

   itx--;
   if ( (tmp = itx - nDIR) < 0 ) {
      RapidCopy(ptr, (void far*) ((char far*) dir_meta16[itx] + ofs), len);
   } else if ( fI15 ) {
      DealWithOddLength(CopyExtended, 0, (unsigned long) ptr,
	    1, 0x100000L + 1024L*top_of_int15 + 16384L*tmp + ofs, len);
   } else if ( (itx = tmp - nXMS) < 0 ) {
      DealWithOddLength(CopyXMS, 0, (unsigned long) ptr,
	    hXMS, 16384L*tmp + ofs, len);
   } else {
      int page=0;

      long lptr = FP_LIN(ptr);
      long lems = FP_LIN(MK_FP(EMS_frame,0));
      if ( (lptr + ofs >= lems) && (lptr < lems+16384L) )
	 page=3;
      EMS_map (itx + 4, page);
      RapidCopy(ptr, MK_FP(EMS_frame, page*16384U + ofs), len);
      EMS_map (page, page);
   }
   Normal();
}

// Optional program to test memory management.
//#define TESTIT
#ifdef TESTIT

#define T1 50
#define T2 100

static struct
{
   char* arg;
   unsigned* varp;
} argtbl[] =
{
   {"EMS", &gmaxEMS},
   {"XMS", &gmaxXMS},
   {"I15", &gmaxI15},
   {"INT15", &gmaxI15},
   {"UMB", &gmaxUMB}
};

void main (int argc, char** argv){
   int i, j, n, totex, totdir, sum, tmph;
   void far* table[T1];
   unsigned far* lpu;
   int nh[7]; // Number of handles per memory type
   int h[7][T2]; // handles per memory type
   unsigned char hval[7*T2]; // value for all handles
   unsigned x1, x2, x3; // some other values to set
   unsigned char scratch[5]; // scratch space to test moving small amounts

   gmaxI15 = gmaxEMS = gmaxXMS = gmaxUMB = 0xFFFF; // allow all memory
   for (++argv, --argc; argc; ++argv, --argc) {
      for (i = 0; i < sizeof(argtbl)/sizeof(argtbl[0]); i++) {
        if ( stricmp(*argv, argtbl[i].arg) == 0 ) {
           if ( argc == 1 )
              *argtbl[i].varp = 0;
           else
           {
              *argtbl[i].varp = atoi(*++argv);
              --argc;
           }
           break;
        }
      }
   }

   InitMem();

   printf("Helpful Hint: pressing Ctrl-C now will leave all extra memory allocated\n\n");

   printf("fEMS=%d\n",fEMS && hEMS); // if hEMS == 0, then no EMS allocated
   printf("fXMM=%d\n",fXMM);
   printf("fXMS=%d\n",fXMM && hXMS); // same for hXMS
   printf("fI15=%d\n\n",fI15);

   printf("coreleft4=%d\n", coreleft4());
   if ( coreleft4() > 0 ) {
      printf("Peeking at MEM internals: upper memory layout:\n");
      for (i = 0; i < MAX_META4; i++) {
         if ( META4[i].fStatus != ST_EMPTY && META4[i].nMemType != MT_NONE ) {
            for (j = i+1; j < MAX_META4; j++)
               if ( META4[j].nMemType != MT_NONE )
                  break;
            printf("  %04X - %04X : %s\n", META4[i].para, META4[j-1].para + 255,
                  META4[i].nMemType == MT_EMSUMB ? "UMB in the EMS page frame" :
                  (META4[i].nMemType == MT_XMSUMB ? "XMSUMB" : "DOS5 UMB") );
         }
      }
   }

   printf("\nAllocating memory...\n");

   totdir = totex = 0;
   // allocate max T1 blocks, first exmem, then mem
   for (i = 0; i < T1; i++) {
      table[i] = exmalloc(16384);
      if ( ! table[i] ) {
         table[i] = farmalloc(16384);
         if ( ! table[i] )
            break;
         else
            totdir++;
      } else
         totex++;
   }
   printf("Allocated with exmalloc %ld, with farmalloc %ld bytes\n\n",
         16384L*totex, 16384L*totdir);

   if ( totdir+totex < 4 ) {
      printf("Not enough memory allocated for further tests\n");
      exit(0);
   }

   printf("[Return]\n");
   (void) getchar();

   for (i = 4; i < totdir+totex; i++)
      donate16(table[i]);

   printf("Donated all but 64K of the allocated memory to META16\n\n");
   printf("coreleft16r = %d\n", coreleft16r());
   sum = 0;
   for (i = 1; i <= 6; i++) {
      printf("coreleft16(%d) = %d blks\n", i, coreleft16(i));
      sum += coreleft16(i);
   }
   printf(   "               ----\n");
   printf(   "                %d\n\n", sum);

   printf("Peeking at MEM internals: META16 blocks per memory type:\n\n");
   printf("DIRect META16: %d blks\n", nDIR);
   printf("   XMS META16: %d blks\n", nXMS);
   printf("   EMS META16: %d blks\n", nEMS);
   printf("INT 15 META16: %d blks\n", nI15);
   printf("              ----\n");
   printf("               %d\n\n", nDIR+nXMS+nEMS+nI15);

   printf("[Return]\n");
   (void) getchar();

   for (i = 1; i <= 6; i++) { // For all mem types
      printf("Memory type %d:", i);
      if ( (nh[i] = coreleft16(i)) > 0 ) {
         if ( nh[i] > T2 )
            nh[i] = T2;
         printf(" Allocating and filling %d blks\n", nh[i]);
         for (j = 0; j < nh[i]; j++) {
            h[i][j] = tmph = malloc16(i);
            hval[tmph] = rand() & 0xFF;
            x1 = rand() & 3;
            _fmemset(table[x1], hval[tmph], 16384);
            to16(tmph, 0, table[x1], 16384);
         }
      } else printf(" empty\n");
   }

   printf("\nTesting all allocated memory");
   for (i = 1; i <= 6; i++) {
      for (j = 0; j < nh[i]; j++) {
         if ( h[i][j] % 20 == 0 )
            printf(".");
         from16(table[0], h[i][j], 0, 16384);
         x1 = (unsigned) hval[h[i][j]] * 0x101;
         if ( random(20) == 3 ) {
            lpu = (unsigned far*) table[0];
            for (n = 0; n < 8192; n++, lpu++)
               if ( *lpu != x1 ) {
                  printf ("[ERROR]");
                  break;
               }
         } else {
            for (n = 0; n < 8192; n+=256)
               if ( ((unsigned*)(table[0]))[n] != x1 ) {
                  printf ("[ERROR]");
                  break;
               }
         }
      }
   }
   printf("\n");

   printf("Testing short and odd moves");
   for (i = 1; i <= 6; i++) {
      for (j = 0; j < nh[i]; j++) {
         if ( h[i][j] % 20 == 0 )
            printf(".");
         x1 = hval[h[i][j]];
         x2 = x1 ^ 0xFF;
         x3 = x1 ^ 0x55;
         scratch[0] = scratch[4] = x3; // buffer space, should not be changed
         scratch[1] = scratch[2] = scratch[3] = x2;
         from16(scratch+1, h[i][j], random(16380), 3);
         for (n = 1; n <= 3; n++)
            if ( scratch[n] != x1 ) {
               printf("[ERROR]");
               break;
            }
         scratch[1] = x2;
         to16(h[i][j], 0, scratch+1, 1);
         scratch[1] = x1; scratch[2] = x2;
	 from16(scratch+1, h[i][j], 0, 2);
         if ( scratch[1] != x2 || scratch[2] != x1 )
            printf("[ERROR]");
         to16(h[i][j], 16381, scratch+1, 3);
         scratch[1] = scratch[2] = scratch[3] = x3;
         from16(scratch+1, h[i][j], 16381, 3);
         if ( scratch[0] != x3 || scratch[4] != x3 ||
               scratch[1] != x2 || scratch[2] != x1 || scratch[3] != x1 )
            printf("[ERROR]");
         to16(h[i][j], 0, scratch+2, 1);
         to16(h[i][j], 16381, scratch+2, 1);
      }
   }

   printf("\nTesting all allocated memory");
   for (i = 1; i <= 6; i++) {
      for (j = 0; j < nh[i]; j++) {
         if ( h[i][j] % 20 == 0 )
            printf(".");
         from16(table[0], h[i][j], 0, 16384);
         x1 = (unsigned) hval[h[i][j]] * 0x101;
         if ( random(20) == 3 ) {
            lpu = (unsigned far*) table[0];
            for (n = 0; n < 8192; n++, lpu++)
               if ( *lpu != x1 ) {
                  printf ("[ERROR]");
                  break;
               }
         } else {
            for (n = 0; n < 8192; n+=256)
               if ( ((unsigned*)(table[0]))[n] != x1 ) {
                  printf ("[ERROR]");
                  break;
               }
         }
      }
   }
   printf("\nFreeing all memory\n");

   for (i = 1; i <= 6; i++)
      for (j = 0; j < nh[i]; j++)
         free16(i, h[i][j]);

   for (i = 1; i <= 6; i++)
      printf("coreleft16(%d) = %d blks\n", i, coreleft16(i));

   // Free donated memory! We REALLY shouldn't use any META16 routine anymore
   for (i = 0; i < totdir+totex; i++)
      if ( extest(table[i]) )
         exfree(table[i]);
      else
         farfree(table[i]);

   printf("coreleft4=%d\n", coreleft4());

   printf ("Test completed\n");
}

#endif

