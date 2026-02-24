#include <mem.h>
#include <dos.h>
#include <stdio.h>
#include <string.h>
#include <dir.h>
#include <stdlib.h>

#include "main.h"
#include "archio.h"
#include "vmem.h"
#include "superman.h"
#include "video.h"
#include "diverse.h"
#include "test.h"
#include "compint.h"
#include "comoterp.h"
#include "neuroman.h"
#include "llio.h"
#include "handle.h"
#include "dirman.h"
#include "fletch.h"
#include "menu.h"


static void DFiles (){
   VPTR walk = TFirstFileNN();
   while (!IS_VNULL(walk)){
      if (TRevs()){
         BrkQ();
	 VPTR rwlk = TRev(0);
	 DWORD ctr = 0;
ok:
	 // action
	 Out (7,"%s\n",WFull (rwlk, ctr));
next:
	 rwlk = ((REVNODE*)V(rwlk))->vpOlder;
	 if (!IS_VNULL(rwlk)){
	    if (((REVNODE*)V(rwlk))->bStatus!=FST_DEL){
	       ctr++;
	       goto ok;
	    } else
	       goto next;
	 }
      }
      walk = TNextFileNN();
   }
}

static void DDirs (){
   VPTR walk = TFirstDir();
   while (!IS_VNULL(walk)){
      BrkQ();
      // action


      if (((DIRNODE*)V(walk))->bStatus==DST_DEL) goto skip;
      TCD (walk);
      DFiles (ttf);
      DDirs(ttf);
      TUP ();
skip:
      walk = TNextDir();
   }
}

static void DumpAll (){ // allows inter revision delete!
   VPTR cur = TWD();
   TCD(VNULL);
   DFiles ();
   DDirs ();
   TCD(cur);
}

void DumpTags (char *pcArchPath, char *pcDumpName){
   SetArea (0);
   TTP (pcArchPath, 1);
   ReadArchive (pcArchPath);
   DumpAll ();
   CloseArchive();
}
