// Copyright 1992, all rights reserved, AIP, Nico de Vries
// MAIN.H

#define TRANSLEN 72

#define UC_MAIN_H

// SBETA  private beta squad & special stuff
// PBETA  public beta/pre-release
// UC2    real version
#include "exetype.h"

#ifdef UC2
   #define OK
#endif

#ifdef PBETA
   #define UCPROX
   #define TRACE
   #define OK
#endif

#ifdef SBETA
   #error SBETA not longer valid
#endif

#ifndef OK
   #error unknown to make EXE type
#endif

#define TMPPREF "U$~"

#ifdef UCPROX
   extern int beta;  // look at environment variables
   extern int debug; // debug mode activated
   extern int heavy; // heavy mode activated
#endif

typedef unsigned char BYTE;
typedef unsigned WORD;
typedef unsigned long DWORD;
extern struct CONF {
   BYTE finstall;    // 1 -> has to be installed !!!
   DWORD exesize;    // size of executable in install tool
   DWORD archsize;   // size of archive with other stuff in install tool
   BYTE fHID;        // store hidden/system files 0->NO 1->YES 2->ASK
   BYTE fEA;         // OS/2 2.x extended attribute management
   DWORD dwMagic;    // magic number to assure correct .EXE
   BYTE fEMS;        // EMS Usage?
   BYTE fXMS;        // XMS Usage?
   BYTE fMul;        // multimedia support? 0=ASK 1=ALWAYS 2=NEVER
   BYTE fSwap;       // Shell swapping ?
   DWORD dSerial;    // UC, UC-PRO, UC-PRO-X serial number
   BYTE fx86;        // 386/486/Pentium usage ?
   BYTE bVideo;      // enhanced video usage ? 1=color  2=B&W  3=color/bios 4=B&W bios
   BYTE fNet;        // Network support ? 0=off 1=on 2=auto
   BYTE fSMSkip;     // Smart skipping ON/OFF ?
   BYTE bRelia;      // 0==detect, 1=protect, 2=ensure
   BYTE bDComp;      // 0=TF 1=TN 2=TT 3=TM
   BYTE fInc;        // default incremental mode ?
   BYTE fAutoCon;    // auto conversion of archives to UC2 archives
   BYTE fVscan;      // check files for viruses during (auto)conversion
   BYTE fOut;        // 1=verbose, 2=basic, 4=minimal
   DWORD dwSoffset;  // Offset of super master in .EXE file.
   BYTE pbTPATH[80]; // temporary files path
   char pcMan[80];   // manuals path
   char pcLog[80];   // logfile path
   char pcBat[80];   // batchfiles path
} CONFIG;

#define MAGIC 0x1AC283746L

#define malloc ERROR_USE_xmalloc_PLEASE
#define farmalloc ERROR_USE_xmalloc_PLEASE
#define free ERROR_USE_xfree_PLEASE
#define farfree ERROR_USE_xfree_PLEASE

extern char pcPath[120]; // path of .EXE file (and .CFG etc)
extern char pcTMP[120];  // path for temporary files

extern int m386; // 386 instructions allowed?

void doexit (int);

void Out (unsigned char l, char *fmt, ...);

#ifdef TRACER
   #define TRACEM(x) Out(7,"\x7[[%s %ld => %s]]\n\r",__FILE__,(long)__LINE__,x)
#else
   #define TRACEM(x)
#endif

// QS == string in special buffer, QSC == string in static data of .EXE
// QSC: special errors, very often printed messages, very small strings
#define QS(n,x)  x
#define QSC(n,x) x
