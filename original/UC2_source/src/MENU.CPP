#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <dos.h>
#include <conio.h>

#include "main.h"
#include "video.h"
#include "diverse.h"

static char head[200];
static int ctr;
static char tp1[5][20];
static char tcc[5];
static char tp2[5][50];

extern int factor;

int oldfactor=0;

void Menu (char *fmt, ...){
   if (!oldfactor) oldfactor=factor;
   factor=1;
   va_list argptr;
   va_start(argptr, fmt);
   vsprintf(head, fmt, argptr);
   ctr=0;
   va_end(argptr);
}

void Option (char *p1, char cc, char *p2){
   strcpy (tp1[ctr], p1);
   tcc[ctr] = cc;
   strcpy (tp2[ctr], p2);
   ctr++;
}

void Cursor (void);
void NoCursor (void);

void syst (void);

int breaker=0;

int Choice (void){
   char c;
   for (;;){
      FSOut (7,"\n\r%s\x7\n\r",head);
      for (int i=0;i<ctr;i++)
	 FSOut (7,"\x6   %i \x7-> %s\x6%c\x7%s\n\r", i+1,tp1[i],tcc[i],tp2[i]);
again:
      if (breaker)
         FSOut (7,"\x6\CHOICE ? ");
      else
         FSOut (7,"\x6\CHOICE \x7(\x6+\x7=Abort \x6-\x7=Shell) \x6? ");
      while (kbhit()) GetKey(); // empty keyboard buffer
      c = Echo(GetKey());
      for (i=0;i<ctr;i++){
	 if ((c==toupper(tcc[i])) || (c==i+'1')){
	    FSOut (7,"\n\r\x7");
	    factor=oldfactor;
	    oldfactor=0;
	    return i+1;
	 }
      }
      if (c=='+'){
         FatalError (100,"program aborted by user");
      } else if (c=='-'){
	 Out (7,"\n\r");
         syst();
      } else {
         FSOut (7,"\x8 *** WRONG CHOICE ***\r");
         delay (500);
         FSOut (7,"                                                         \r");
         goto again;
      }
   }
}

void OnOff (int i);
void UpdateCFG (void);
void ask (char *q, char *a);
void NoLastS (char *buf);
void Default (void);




void ConfigMenu (){
#ifndef UE2
   int upd=0;
   CONF old;
   old=CONFIG;
//   textattr (attri);
   FSOut (7,"\x7");
   clrscr();
   for (;;){
      NoCursor();
      gotoxy(1,1);
      FSOut(7,"\x5\GENERAL OPTIONS:\x7");
      FSOut(7,"\n\r \x6  A\x7 -> Default compression method         ");
      if (CONFIG.bDComp==0) FSOut (7,"\x5\FAST\x7\ (normal) (tight) (s-tight)");
      if (CONFIG.bDComp==1) FSOut (7,"\x5\NORMAL\x7\ (tight) (s-tight) (fast)");
      if (CONFIG.bDComp==2) FSOut (7,"\x5\TIGHT\x7\ (s-tight) (fast) (normal)");
      if (CONFIG.bDComp==3) FSOut (7,"\x5\S-TIGHT\x7\ (fast) (normal) (tight)");
      FSOut(7,"\n\r \x6  B\x7 -> Default operation                  ");
      if (CONFIG.fInc==0) FSOut (7,"\x5\BASIC MODE\x7\ (incremental mode)");
      if (CONFIG.fInc==1) FSOut (7,"\x5\INCREMENTAL MODE\x7\ (basic mode)");
      FSOut(7,"\n\r \x6  C\x7 -> Reliability level                  ");
      if (CONFIG.bRelia==0) FSOut (7,"\x5\DETECT\x7\ (protect) (ensure)");
      if (CONFIG.bRelia==1) FSOut (7,"\x5\PROTECT\x7\ (ensure) (detect)");
      if (CONFIG.bRelia==2) FSOut (7,"\x5\ENSURE\x7\ (detect) (protect)");
      FSOut(7,"\n\r \x6  D\x7 -> Automatic archive conversion       "); OnOff (CONFIG.fAutoCon);
      FSOut(7,"\n\r \x6  E\x7 -> Virus scan during conversion       "); OnOff (CONFIG.fVscan);
      FSOut(7,"\n\r \x6  F\x7 -> Smart skipping                     "); OnOff (CONFIG.fSMSkip);
      FSOut(7,"\n\r \x6  G\x7 -> Amount of output/information       ");
      if (CONFIG.fOut==1) FSOut (7,"\x5\VERBOSE\x7 (normal) (minimal)");
      if (CONFIG.fOut==2) FSOut (7,"\x5\NORMAL\x7 (minimal) (verbose)");
      if (CONFIG.fOut==4) FSOut (7,"\x5\MINIMAL\x7 (verbose) (normal)");
      FSOut(7,"\n\r \x6  H\x7 -> Show (multimedia) banners          ");
      if (CONFIG.fMul==0) FSOut (7,"\x5\ASK\x7 (on) (off)");
      if (CONFIG.fMul==1) FSOut (7,"\x5\ON\x7 (off) (ask)");
      if (CONFIG.fMul==2) FSOut (7,"\x5\OFF\x7 (ask) (on)");
      FSOut(7,"\n\r \x6  I\x7 -> Store OS/2 2.x extended attributes "); OnOff (CONFIG.fEA);
      FSOut(7,"\n\r \x6  J\x7 -> Store system/hidden files          ");
      if (CONFIG.fHID==0) FSOut (7,"\x5\OFF\x7\ (ask) (on)");
      if (CONFIG.fHID==1) FSOut (7,"\x5\ON\x7\ (off) (ask)");
      if (CONFIG.fHID==2) FSOut (7,"\x5\ASK\x7\ (on) (off)");

      FSOut(7,"\n\r\x5\SYSTEM OPTIONS:\x7");
      FSOut(7,"\n\r \x6  M\x7 -> Video mode                       ");
      if (CONFIG.bVideo==1) FSOut (7,"\x5\COLOR\x7\ (mono) (color-bios) (mono-bios)");
      if (CONFIG.bVideo==2) FSOut (7,"\x5\MONO\x7\ (color-bios) (mono-bios) (color)");
      if (CONFIG.bVideo==3) FSOut (7,"\x5\COLOR-BIOS\x7\ (mono-bios) (color) (mono)");
      if (CONFIG.bVideo==4) FSOut (7,"\x5\MONO-BIOS\x7\ (color) (mono) (color-bios)");
      FSOut(7,"\n\r \x6  N\x7 -> Dynamic program swapping         "); OnOff (CONFIG.fSwap);
      FSOut(7,"\n\r \x6  O\x7 -> Use EMS                          ");
      if (CONFIG.fEMS==0) FSOut (7,"\x5\OFF\x7 (4.0+ only) (any version)");
      if (CONFIG.fEMS==1) FSOut (7,"4.0+ ONLY\x7 (any version) (off)");
      if (CONFIG.fEMS==2) FSOut (7,"\x5\ANY VERSION\x7 (off) (4.0+ only)");
      FSOut(7,"\n\r \x6  P\x7 -> Use XMS                          "); OnOff (CONFIG.fXMS);
      FSOut(7,"\n\r \x6  Q\x7 -> Use 386/486/Pentium/M1 features  "); OnOff (CONFIG.fx86);
      FSOut(7,"\n\r \x6  R\x7 -> Advanced networking              ");
      if (CONFIG.fNet==0) FSOut (7,"\x5\OFF\x7 (on) (auto-skip)");
      if (CONFIG.fNet==1) FSOut (7,"\x5\ON\x7 (auto-skip) (off)");
      if (CONFIG.fNet==2) FSOut (7,"\x5\AUTO-SKIP\x7 (off) (on)");
      FSOut(7,"\n\r \x6  S\x7 -> Location for temporary files     ");
      FSOut(7,"[\x5%s\\\x7\]",CONFIG.pbTPATH);
      FSOut(7,"\n\r \x6  T\x7 -> First loc for manuals (*.DOC)    ");
      FSOut(7,"[\x5%s\\\x7\]",CONFIG.pcMan);
      FSOut(7,"\n\r \x6  U\x7 -> Location for error logfile       ");
      if (CONFIG.pcLog[3]=='*')
         FSOut(7,"\x5NOT ACTIVE");
      else
         FSOut(7,"[\x5%s\\\x7\]",CONFIG.pcLog);
      FSOut(7,"\n\r \x6  V\x7 -> First loc for batch&script files ");
      FSOut(7,"[\x5%s\\\x7\]",CONFIG.pcBat);

      FSOut(7,"\n\r\x5\QUICK SETUP:\x6 1\x7->default\x6  2\x7->max speed\x6  3\x7->max compress\x6  4\x7->max safe\x6  5\x7->UNDO");

      FSOut(7,"\n\r\n\r");
again:
      FSOut(7,"\x6\CHOICE (Escape to leave configuration menu)?                               \r");
      FSOut(7,"\x6\CHOICE (Escape to leave configuration menu)? ");
      Cursor();
      char c = Echo(GetKey());
      switch(c){
         case 27:
//          textattr (attri);
            FSOut (7,"\x7");
            clrscr();
            goto end;
         case 'A':
            CONFIG.bDComp++;
            if (CONFIG.bDComp==4) CONFIG.bDComp=0;
            upd=1;
            break;
         case 'B':
            CONFIG.fInc^=1;
            upd=1;
            break;
         case 'C':
            CONFIG.bRelia++;
            if (CONFIG.bRelia==3) CONFIG.bRelia=0;
            upd=1;
            break;
         case 'D':
            CONFIG.fAutoCon^=1;
            upd=1;
            break;
         case 'E':
            CONFIG.fVscan^=1;
            upd=1;
            break;
         case 'F':
            CONFIG.fSMSkip^=1;
            upd=1;
            break;
         case 'G':
            CONFIG.fOut*=2;
            if (CONFIG.fOut==8) CONFIG.fOut=1;
            upd=1;
            break;
         case 'H':
            CONFIG.fMul++;
            if (CONFIG.fMul==3) CONFIG.fMul=0;
            upd=1;
            break;
         case 'I':
            CONFIG.fEA^=1;
            upd=1;
            break;
         case 'J':
            if (CONFIG.fHID==0) CONFIG.fHID=3;
            CONFIG.fHID--;
            upd=1;
            break;
         case 'M':
            CONFIG.bVideo=CONFIG.bVideo+1;
            if (CONFIG.bVideo==5) CONFIG.bVideo=1;
            upd=1;
            // ensure BIOS mode in case of trouble
            if (CONFIG.bVideo<3)
               Mode(CONFIG.bVideo+2);
            else
               Mode(CONFIG.bVideo);
            Out (7,"\x7 ");
            clrscr();
            break;
         case 'N':
            CONFIG.fSwap^=1;
            upd=1;
            break;
         case 'O':
            CONFIG.fEMS++;
            if (CONFIG.fEMS==3) CONFIG.fEMS=0;
            upd=1;
            break;
         case 'P':
            CONFIG.fXMS^=1;
            upd=1;
            break;
         case 'Q':
            CONFIG.fx86^=1;
            upd=1;
            break;
         case 'R':
            CONFIG.fNet++;
            if (CONFIG.fNet==3) CONFIG.fNet=0;
            upd=1;
            break;
         case 'S':
            strcat ((char *)CONFIG.pbTPATH,"\\");
            ask ("Enter location for temporary files : ",(char *)CONFIG.pbTPATH);
            NoLastS ((char *)CONFIG.pbTPATH);
            upd=1;
            break;
         case 'T':
            strcat ((char *)CONFIG.pcMan,"\\");
            ask ("Enter first location for manuals (*.DOC) : ",(char *)CONFIG.pcMan);
            NoLastS (CONFIG.pcMan);
            upd=1;
            break;
         case 'U':
            if (CONFIG.pcLog[3]=='*'){
               strcpy (CONFIG.pcLog,"*");
               CONFIG.pcLog[3]=0;
            } else {
               strcat ((char *)CONFIG.pcLog,"\\");
            }
            ask ("Enter location for error logfile (enter * for NO logfile) : ",(char *)CONFIG.pcLog);
            NoLastS (CONFIG.pcLog);
            upd=1;
            break;
         case 'V':
            strcat ((char *)CONFIG.pcBat,"\\");
            ask ("Enter first location for batch&script files  : ",(char *)CONFIG.pcBat);
            NoLastS (CONFIG.pcBat);
            upd=1;
            break;
         case '1':
            Default();
            clrscr();
            upd=1;
            break;
         case '2': // SPEED
            CONFIG.fOut = 4;
            CONFIG.fEMS = 1;
            CONFIG.fXMS = 1;
            CONFIG.fMul = 2; // no banners in speed mode

            CONFIG.fx86 = 1;
            if (CONFIG.bVideo>2) // video to direct mode
               CONFIG.bVideo-=2;

            CONFIG.fNet = 2;

            CONFIG.fSMSkip = 1;
            CONFIG.bRelia = 0;
            CONFIG.bDComp = 0;
            CONFIG.fVscan = 0;
            CONFIG.fInc = 1;

            CONFIG.fAutoCon = 1;

            upd=1;
            break;
         case '3': // MAX COMPRESS
            CONFIG.bRelia = 0;
            CONFIG.bDComp = 3;
            CONFIG.fInc = 0;
            CONFIG.fAutoCon = 1;
            upd=1;
            break;
         case '4': // SAFE
            CONFIG.fEMS = 0;
            CONFIG.fXMS = 0;
            CONFIG.fMul = 2; // no banners in safe mode
            CONFIG.fVscan = 1;

            CONFIG.fx86 = 0;
            if (CONFIG.bVideo<3) // bios video mode
               CONFIG.bVideo+=2;

            if (CONFIG.fNet==2) CONFIG.fNet=1;

            CONFIG.fSMSkip = 0;
            CONFIG.bRelia = 2;
            CONFIG.fInc = 1;
            CONFIG.fAutoCon = 0;
            upd=1;
            break;
         case '5': // UNDO
            CONFIG=old;
            clrscr();
            upd=1;
            break;
         default:
            FSOut (7,"\x8\ *** WRONG CHOICE ***\r");
            delay (500);
            FSOut (7,"                                                         \r");
            goto again;
      }
      // ensure BIOS mode in case of trouble
      if (CONFIG.bVideo<3)
         Mode(CONFIG.bVideo+2);
      else
         Mode(CONFIG.bVideo);
   }
end:
   if (upd) UpdateCFG();
   FSOut (7,"\n\r");
#endif
}
