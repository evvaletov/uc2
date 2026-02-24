// Copyright 1992, all rights reserved, AIP, Nico de Vries
// VMEM.H

/* SUMMARY of the most important functions :

   VPTR                      virtual pointer type

   VNULL                     virtual NULL pointer
   IS_VNULL (VPTR)           test if pointer is VNULL

   void InitVmem (void)      Initialize (call once)
   VPTR Vmalloc (WORD size)  allocate virtual memory
   void Vfree (VPTR)         free virtual memory
   BYTE *A(VPTR)             get access to virtual memory (WATCH OUT!!)

   void BoosterOn ()         enable vmem booster (takes all RAM)
   void BoosterOff ()        disable vmem booster (releases all RAM)

   PIPE                      dynamic pipe type

   OpenPipe
   ReadPipe
   WritePipe
   ClosePipe

*/

#ifndef UC_MAIN_H // use VMEM as standalone package !!!

typedef unsigned char BYTE;
typedef unsigned WORD;
typedef unsigned long DWORD;

#endif

struct VPTR {
   WORD wBlock;
   WORD wOffset;
};

struct PIPE {
   VPTR vpStart, vpCurrent, vpTail;
   WORD wOff;
   BYTE buf[1900];
   WORD wBufInUse;
};

struct PIPENODE {
   VPTR vpNext;
   WORD wLen;
   VPTR vpDat;
};

extern VPTR VNULL; // virtual NULL pointer

inline int IS_VNULL (VPTR me){
   return (me.wBlock==VNULL.wBlock);
}

inline int CMP_VPTR (VPTR a, VPTR b){
   return (a.wBlock==b.wBlock) && (a.wOffset==b.wOffset);
}

inline VPTR MK_VP(WORD block, WORD offset){
   VPTR tmp;
   tmp.wBlock = block;
   tmp.wOffset = offset;
   return tmp;
}

void InitVmem (void);
   // init VMEM software

void BoosterOn ();

void BoosterOff ();

VPTR LLVmalloc (WORD size);
   // malloc VMEM (max size BLOCK_SIZE - 6!)
   // succeeds or stops computer (unlikely)

void LLVfree (VPTR vpAdr);
   // free VMEM

BYTE *Acc (VPTR vpAdr);
   // get access to VMEM block (does lock block in memory)

void UnAcc (VPTR vpAdr);
   // unget access to VMEM block (unlock)

BYTE *V (VPTR vpAdr);
   // get access (automatic UnAcc after 'DEEP' succesive calls)

void MakePipe (PIPE &p);
   // create new PIPE

void WritePipe (PIPE &p, BYTE *pbData, WORD wLen);
   // write data to pipe

WORD ReadPipe (PIPE &p, BYTE *pbData, WORD wLen);
   // read data fom pipe

void ClosePipe (PIPE &p);
   // close down pipe

#ifdef UCPROX
   void Vforever_do();
   #define Vforever Vforever_do
#else
   #define Vforever()
#endif

//#define DEBUG_VMEM

#ifdef DEBUG_VMEM
   VPTR DBVmalloc (WORD size, char *file, long line);
   void DBVfree (VPTR vpAdr, char *file, long line);
   #define Vmalloc(a) DBVmalloc(a,__FILE__,__LINE__);
   #define Vfree(a) DBVfree(a,__FILE__,__LINE__);
#else
   #define Vmalloc LLVmalloc
   #define Vfree LLVfree
#endif