// EXIT.CPP

#include "main.h"
#include "diverse.h"
#include "llio.h"
#include <stdlib.h>
#include <dos.h>

#ifdef TRACE
   extern void checkx (void);
#endif


extern void RestoreCursor (void);

extern void exito (void);

extern void neuman (void);
extern int fneuman;

extern void SmKillTmp (void);
extern int fSmKillTmp;

extern void ktmp (void);
extern int fktmp;

extern void ExitMem (void);
extern int fExitMem;

extern void RecIt (void);
extern int fRecIt;

extern void spdodel (void);
extern int fspdodel;

extern void exiti (void);
extern int fexiti;

extern void CloseVmem (void);
extern int fCloseVmem;

extern void exitc (void);
extern int fconv;

extern void exitcopy (void);
extern int fcopy;

int ex=0;

static int sdig=0;

void Level (int s){
  if (s>sdig) sdig=s;
}

extern int factor;

int closeme=-1;

extern void CloseAll (void);

void masterexit (void){
   factor=1;

   static int skips;

   ex++; // flag call to exit

   switch (skips){
      case 1: goto l10;
      case 2: goto l2;
      case 3: goto l3;
      case 4: goto l4;
      case 5: goto l5;
      case 6: goto l6;
      case 7: goto l7;
      case 8: goto l8;
      case 9: goto l9;
      case 10: goto l11;
      case 11: goto l1;
   }

#ifdef TRACE
   checkx();
#endif

   skips=1;
   if (fexiti) exiti(); // del archio tmp files

l10:
   skips=10;
   if (fconv) exitc();

l11:
   skips=11;
   if (fcopy) exitcopy();

l1:
   skips=2;
   if (fRecIt) RecIt(); // tell user he has to recover with UR2

l2:
   skips=3;
   if (fSmKillTmp) SmKillTmp(); // kill supermaster tmpfile

l3:
   skips=4;
   if (fspdodel) spdodel(); // del tmp arch during convert
l4:

   skips=5;
   if (fneuman) neuman(); // neuromanager, CloseNeuro
l5:

   skips=6;
   if (closeme!=-1) Close (closeme);
   if (fktmp) ktmp(); // remove temporarely files
l6:

   skips=7;
   if (fCloseVmem) CloseVmem(); // del VMEM tmp files
l7:

   skips=8;
   if (fExitMem) ExitMem(); // release EMS, XMS etc
l8:

   skips=9;
   exito(); // exit of main.cpp
l9:
   Reset();
//  RestoreCursor();
   exit (sdig);
}

void doexit (int num){
   Level (num);
   masterexit();
}
