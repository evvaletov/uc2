// Copyright 1992, all rights reserved, AIP, Nico de Vries
// MENU.CPP

#include <process.h>
#include <dos.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <conio.h>
#include "handle.h"
#include "main.h"
#include "video.h"
#include "dirman.h"
#include "menu.h"
#include "diverse.h"
#include "llio.h"

/*******************************/
// critical error handling stuff

jmp_buf jbCritRep;
int ceflag=0;
int cestat=0;
int cehan=0;
int nobreak=0;
int cerro=0;

static char *xerr_msg[] = // Values for extended error code
{
   "no error",                                                // 00h
   "function number invalid",                                 // 01h
   "file not found",                                          // 02h
   "path not found",                                          // 03h
   "too many open files (no handles available)",              // 04h
   "access denied",                                           // 05h
   "invalid handle",                                          // 06h
   "memory control block destroyed",                          // 07h
   "insufficient memory",                                     // 08h
   "memory block address invalid",                            // 09h
   "environment invalid (usually >32K in length)",            // 0Ah
   "format invalid",                                          // 0Bh
   "access code invalid",                                     // 0Ch
   "data invalid",                                            // 0Dh
   "unknown",                                                 // 0Eh
   "invalid drive",                                           // 0Fh
   "attempted to remove current directory",                   // 10h
   "not same device",                                         // 11h
   "no more files",                                           // 12h

   "disk write-protected",                                    // 13h
   "unknown unit",                                            // 14h
   "drive not ready",                                         // 15h
   "unknown command",                                         // 16h
   "data error (CRC)",                                        // 17h
   "bad request structure length",                            // 18h
   "seek error",                                              // 19h
   "unknown media type (non-DOS disk)",                       // 1Ah
   "sector not found",                                        // 1Bh
   "printer out of paper",                                    // 1Ch
   "write fault",                                             // 1Dh
   "read fault",                                              // 1Eh
   "general failure",                                         // 1Fh
   "file sharing violation",                                  // 20h
   "file lock violation",                                     // 21h
   "disk change invalid",                                     // 22h

   "FCB unavailable",                                         // 23h
   "sharing buffer overflow",                                 // 24h
   "code page mismatch",                             // 25h
   "cannot complete file operation (out of input)",  // 26h
   "insufficient disk space",                        // 27h

   "unknown",                                                // 28h
   "unknown",                                                // 29h
   "unknown",                                                // 30h
   "unknown",                                                // 31h

   "network request not supported",                           // 32h
   "remote computer not listening",                           // 33h
   "duplicate name on network",                               // 34h
   "network name not found",                                  // 35h
   "network busy",                                            // 36h
   "network device no longer exists",                         // 37h
   "network BIOS command limit exceeded",                     // 38h
   "network adapter hardware error",                          // 39h
   "incorrect response from network",                         // 3Ah
   "unexpected network error",                                // 3Bh
   "incompatible remote adapter",                             // 3Ch
   "network connection lost",                                 // 3Dh (print queue??)
   "queue not full",                                          // 3Eh
   "not enough space to print file",                          // 3Fh
   "network name was deleted",                                // 40h
   "(network) access denied",                                  // 41h
   "network device type incorrect",                           // 42h
   "network name not found",                                  // 43h
   "network name limit exceeded",                             // 44h
   "network BIOS session limit exceeded",                     // 45h
   "temporarily paused",                                      // 46h
   "network request not accepted",                            // 47h
   "network print/disk redirection paused",                   // 48h
   "(LANtastic) invalid network version",                     // 49h
   "(LANtastic) account expired",                             // 4Ah
   "(LANtastic) password expired",                            // 4Bh
   "(LANtastic) login attempt invalid at this time",          // 4Ch
   "(LANtastic) disk limit exceeded on network node",     // 4Dh
   "(LANtastic) not logged in to network node",           // 4Eh
   "unknown",                                                // 4Fh
   "file exists",                                             // 50h
   "reserved",                                                // 51h
   "cannot create directory",                                 // 52h
   "fail on INT 24h",                                         // 53h
   "too many redirections",                        // 54h
   "duplicate redirection",                        // 55h
   "invalid password",                             // 56h
   "invalid parameter",                            // 57h
   "no space in directory for new entry OR network write fault",                          // 58h
   "function not supported on network",              // 59h
   "required system component not installed"         // 5Ah
};

/*

static char *err_cls[] = // Values for Error Class
{
   "out of resource (storage space or I/O channels)",         // 01h
   "temporary situation (file or record lock)",               // 02h
   "authorization (denied access)",                           // 03h
   "internal (system software bug)",                          // 04h
   "hardware failure",                                        // 05h
   "system failure (configuration file missing or incorrect)",// 06h
   "application program error",                               // 07h
   "not found",                                               // 08h
   "bad format",                                              // 09h
   "locked",                                                  // 0Ah
   "media error",                                             // 0Bh
   "already exists",                                          // 0Ch
   "unknown"                                                  // 0Dh
};

static char *err_sug[] = // Values for Suggested Action
{
   "retry",                                                   // 01h
   "delayed retry",                                           // 02h
   "prompt user to reenter input",                            // 03h
   "abort after cleanup",                                     // 04h
   "immediate abort",                                         // 05h
   "ignore",                                                  // 06h
   "retry after user intervention"                            // 07h
};

static char *err_loc[] = // Values for Error Locus
{
   "unknown or not appropriate",                              // 01h
   "block device (disk error)",                               // 02h
   "network related",                                         // 03h
   "serial device (timeout)",                                 // 04h
   "memory related",                                          // 05h
};


*/

/**************************************************/
// BREAK HANDLING

int cdecl breakNono (){ // ignore break signals
   return 1;
}

int fBreak=0;

int cdecl breakHan (){
   if (fBreak==0){
      fBreak=1; // if detectable, detect
   }
   return 1;
}

void BKeep (void);
void BBack (void);

void EKeep (void);
void EBack (void);

void syst (void){
   static int ses1=1;
#ifdef UE2
   FSOut (7,"\x7Shelling to DOS...\n\rUse\x6 EXIT\x7 to return to UltraExpander II\n\r");
#else
   FSOut (7,"\x7Shelling to DOS...\n\rUse\x6 EXIT\x7 to return to UltraCompressor II\n\r");
#endif
   BKeep();
   if (ses1){
      GBack();
      ses1=0;
   } else {
      EBack();
   }
   char tmp[100];
   if (getenv ("COMSPEC"))
      sprintf (tmp,"%s <con >con",getenv("COMSPEC"));
   else
      sprintf (tmp,"command <con >con");
   ssystem (tmp);
   EKeep();
   BBack();
}

int skipstat=0;
char noskip=0;

extern int breaker;

int bmenu(){
   struct text_info ti;
   gettextinfo(&ti);
   int oldattr = ti.attribute;
   Menu ("\x8\BREAK detected");
again:
   if ((skipstat!=1) || noskip){
      Option ("",'A',"bort");
      Option ("",'C',"ontinue");
      Option ("",'S',"hell to DOS");
      breaker=1;
      switch (Choice()){
	 case 1:
            breaker=0;
	    FatalError (100,"program aborted by user");
	 case 2:
            breaker=0;
	    fBreak=0; // allow detection
	    textattr (oldattr);
	    return 1;
	 case 3:
            breaker=0;
	    syst();
	    break;
      }
   } else {
      Option ("",'A',"bort");
      Option ("s",'K',"ip rest of ADD command");
      Option ("",'C',"ontinue");
      Option ("",'S',"hell to DOS");
      switch (Choice()){
	 case 1:
	    FatalError (100,"program aborted by user");
	 case 2:
	    Warning (25,"user chose to skip rest of ADD command");
	    skipstat=2;
	    return 1;
	 case 3:
	    fBreak=0; // allow detection
	    textattr (oldattr);
	    return 1;
	 case 4:
	    syst();
	    break;
      }
   }
   Menu ("\x8(PENDING) BREAK detected");
   goto again;
}

void BrkQ (void){
   kbhit();
   if (fBreak==1 && nobreak) {
      return; // handle later on
   } else {
      if (fBreak==1){ // if detected
	 fBreak=2; // set 'processing' state
	 bmenu(); // process break
      }
   }
}

char cerror[200];
DOSERROR info;
int netw;
int fatal=0;
int os2=0;
extern int win;

#pragma argsused
void cdecl errHan(unsigned deverr, unsigned errval, unsigned far *devhdr){
  /* if this not disk error then another device having trouble */
  netw=0;
  unsigned em = errval+0x13;
  if (dosexterr(&info)){
     em = info.de_exterror;
  }

  if (deverr & 0x8000) {
     sprintf (cerror, "%s (on device)", xerr_msg[em]);
  } else {
     sprintf (cerror, "%s (on %c:)", xerr_msg[em], 'A'+deverr&0xFF);
     if ((em==0x05)|| // access denied
	 (em==0x20)|| // sharing violation
	 (em==0x41)|| // network access denied
	 (em==0x21)){ // locking violation
	netw=1;
     }
  }

  if (fatal) goto leave;
  if (cestat==0 || cestat==4){
      cestat=2;
      Menu ("\x8\CRITICAL ERROR: %s",cerror);
      Option ("",'A',"bort");
      Option ("",'T',"ry again");
      switch (Choice()){
	 case 1:
	    bdos (0x54,0,0);       // clean up DOS
	    cerro=1;
            FatalError (170,"critical error '%s' followed by user abort",cerror);
	 case 2:
	   _hardresume (_HARDERR_RETRY);
      }
  } else if (cestat==2){
leave:
     directvideo=0;
     textattr (LIGHTRED);
     cputs("\n\r\n\rFATAL ERROR: error handling&logging failed\n\r");
     cputs("Unable to handle error, program aborted\n\r");
     if (!os2){
	if (win){
	   cputs("You are advised to close your Win-DOS session\n\r");
	} else {
	   cputs("You are advised to reset your computer\n\r");
	}
     } else {
	cputs("You are advised to close your OS/2-MDOS session\n\r");
     }
     cputs ("Some temporary files (U$~?????.TMP) might still exist on your disk\n\r");
     Reset();
     _exit (255); // terminate NOW
  } else { // 1 or 3
     if (cestat==1)
	cehan=1;
     else
	cehan=0;
     cestat=0; // handle next error locally
     bdos (0x54,0,0);       // clean up DOS
     ceflag=1;
     longjmp (jbCritRep,1); // restart at start of critical section
  }
}

void diskfull (char* file){
   if (fatal) errHan (0,0,NULL);
   Menu ("\x8\SYSTEM ERROR: write to %s failed (disk full?)",file);
again:
   Option ("",'A',"bort");
   Option ("",'T',"ry again");
   Option ("",'S',"hell to DOS");
   switch (Choice()){
      case 1:
	 cerro=1;
         FatalError (170,"system error 'write to %s failed' followed by user abort",file);
      case 2:
	 return;
      case 3:
	 syst();
	 break;
   }
   Menu ("\x8\(PENDING) SYSTEM ERROR: write to %s failed (disk full?)",file);
   goto again;
}

void PreFab (void){
   unsigned em=0x05;
   if (dosexterr(&info)){
      em = info.de_exterror;
   }
   netw=0;
   if ((em==0x05)|| // access denied
       (em==0x20)|| // sharing violation
       (em==0x41)|| // network access denied
       (em==0x21)){ // locking violation
      netw=1;
   }
   sprintf (cerror, "%s", xerr_msg[em]);
}

int CeAskOpen (char *name, char *why, int skip){
   if ((CONFIG.fNet==2) && netw && skip){
      Warning (30,"auto-skip triggered by '%s' (%s %s)",cerror,why,name);
      return 0; // network error
   }
   if ((CONFIG.fNet==2) && skip && !Exists(name)){
      Warning (30,"auto-skip triggered by '%s' (%s)",cerror,name);
      return 0; // network error
   }
   Menu ("\x8\SYSTEM ERROR: %s (attempting to %s %s)",cerror,why,name);
again:
   Option ("",'A',"bort");
   Option ("",'T',"ry again");
   Option ("",'S',"hell to DOS");
   if (skip) Option ("S",'k',"ip this file");
   switch (Choice()){
      case 1:
	 cerro=1;
         FatalError (170,"system error '%s' (%s %s) followed by user abort",cerror,why,name);
      case 2:
	 return 1;
      case 3:
	 syst();
	 break;
      case 4:
	 return 0;
   }
   Menu ("\x8\(PENDING) SYSTEM ERROR: %s (attempting to %s %s)",cerror,why,name);
   goto again;
}

void ceask (void){
   if (!cehan) return; // mode 3, retry imidiately
   Menu ("\x8\SYSTEM ERROR: %s",cerror);
again:
   Option ("",'A',"bort");
   Option ("",'T',"ry again");
   Option ("",'S',"hell to DOS");
   switch (Choice()){
      case 1:
	 cerro=1;
         FatalError (170,"system error '%s' followed by user abort",cerror);
      case 2:
	 return;
      case 3:
	 syst();
	 break;
   }
   Menu ("\x8\(PENDING) SYSTEM ERROR: %s",cerror);
   goto again;
}

