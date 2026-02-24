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
#include "tagman.h"
#include "ultracmp.h"

//#define TEST

void TTP (char *pcArchivePath, int mustexist);
char * GetPath (VPTR here, int levels);
char *WFull (VPTR rev, DWORD hint);

int gHan;

static char tmp[MAXPATH];

extern PIPE pipe;

#define Write(data, dummy, len) WritePipe (pipe, data, len)
#define Read(data, dummy, len) ReadPipe (pipe, data, len)

static void DumpT (VPTR extnode){
   BYTE flag;
   if (!IS_VNULL(extnode)){
      flag = TM_DELETE_ALL; // start from scratch
      Write (&flag, gHan, 1);
   }
   while (!IS_VNULL(extnode)){
//      Out (7,".");
#ifdef TEST
      Out (7,"[%s]\n\r",((EXTNODE*)V(extnode))->extmeta.tag);
#endif
//      Out (1,"[%s] ",((EXTNODE*)V(extnode))->extmeta.tag);
      flag = TM_ADD;
      Write (&flag, gHan, 1); // ADD command
      VPTR walk=((EXTNODE*)V(extnode))->extdat;
      DWORD ctr=((EXTNODE*)V(extnode))->extmeta.size;
      char tag[20];
      strcpy (tag,((EXTNODE*)V(extnode))->extmeta.tag);
      Write ((BYTE *)tag, gHan, TAG_SIZE);
      Write ((BYTE *)&ctr, gHan, 4); // tagdata size
      while (ctr){
	 WORD trn = ctr<1024 ? (WORD)ctr : 1024;
	 Write (((EXTDAT*)V(walk))->dat,gHan,trn); //tag data
	 ctr-=trn;
	 walk = ((EXTDAT*)V(walk))->next;
      }

      extnode = ((EXTNODE*)V(extnode))->next;
   }
   flag = TM_NONE; // signal end of information
   Write (&flag, gHan, 1);
}

static void DFiles (){
//   Out (7,"(");
   VPTR walk = TFirstFileNN();
   while (!IS_VNULL(walk)){
      if (TRevs()){
         BrkQ();
	 VPTR rwlk = TRev(0);
	 DWORD ctr = 0;
ok:
	 // action
	 strcpy (tmp, GetPath(TWD(),1000));
	 strcat (tmp, WFull (rwlk, ctr));
	 WORD wLen = strlen(tmp)+1+32768U;
	 Write ((BYTE *)&wLen, gHan, 2);
	 Write ((BYTE *)tmp, gHan, strlen(tmp)+1);
#ifdef TEST
	 Out (7,"%s\n\r",tmp);
#endif
	 if (!IS_VNULL(((REVNODE*)V(rwlk))->extnode)){
//	    Out (1,"%s  ",tmp);
	 }
//	 Out (7,"<");
	 DumpT (((REVNODE*)V(rwlk))->extnode);
//	 Out (7,">");
	 if (!IS_VNULL(((REVNODE*)V(rwlk))->extnode)){
//	    Out (1,"\n\r");
	 }
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
//   Out (7,")");
}

static void DDirs (){
   VPTR walk = TFirstDir();
   while (!IS_VNULL(walk)){
      BrkQ();
      if (((DIRNODE*)V(walk))->bStatus==DST_DEL) goto skip;
      TCD (walk);

      // action
      strcpy (tmp, GetPath(TWD(),1000));
      {
	 WORD wLen = strlen(tmp)+1;
	 Write ((BYTE *)&wLen, gHan, 2);
	 Write ((BYTE *)tmp, gHan, strlen(tmp)+1);
#ifdef TEST
	 Out (7,"%s\n\r",tmp);
#endif
	 DumpT (((DIRNODE*)V(walk))->extnode);
      }

      DFiles ();
      DDirs();
      TUP ();
skip:
      walk = TNextDir();
   }
}

static void DumpAll (){
//   Out (7,"{");
   VPTR cur = TWD();
   TCD(VNULL);
//   BoosterOn();
   DFiles ();
   DDirs ();
//   BoosterOff();
   TCD(cur);
//   Out (7,"}");
}

VPTR AccessRev (char *fullpath);

void XCleanTags (VPTR extnode);

static void ReadAll (){
   BYTE flag;
   WORD wLen;
   VPTR rev;
   for (;;){
      Read ((BYTE *)&wLen, gHan, 2);
      if (!wLen) return;
      wLen&=0x7FFF;
      Read ((BYTE *)tmp, gHan, wLen);
#ifdef TEST
      Out (7,"%s\n\r",tmp);
#endif
      Read (&flag, gHan, 1);
      if (flag!=TM_NONE){
	 rev = AccessRev (tmp);
      }
//      int crlf=0;
      if (flag!=TM_NONE){
	 // Out (1,"%s  ",tmp);
	 // crlf=1;
      }
      while (flag!=TM_NONE){
         BrkQ();
	 switch (flag){
	    case TM_DELETE_ALL:
	       XCleanTags (((REVNODE*)V(rev))->extnode);
	       ((REVNODE*)V(rev))->extnode = VNULL;
	       break;
	    case TM_ADD:
	       // create EXTNODE
	       VPTR en = Vmalloc (sizeof(EXTNODE));

	       // read tag description
	       char tag[20];
	       Read ((BYTE *)tag, gHan, TAG_SIZE);
	       if (strlen(tag)>(TAG_SIZE-1)) IE();
	       strcpy (((EXTNODE*)V(en))->extmeta.tag, tag);

	       // detemine tagdata size
	       DWORD ctr;
	       Read ((BYTE *)&ctr, gHan, 4);
	       ((EXTNODE*)V(en))->extmeta.size = ctr;

#ifdef TEST
	       static int cct;
	       if (cct==495){
		  Out (7,"[]\n\r");
	       }
	       Out (7,"[%s] %ld %d %d:%d %d:%d\n\r", tag, ctr, cct++, en, rev);
#endif
	       //Out (1,"[%s] ", tag);

	       // add EXTNODE to revision information
	       ((EXTNODE*)V(en))->next = ((REVNODE*)V(rev))->extnode;
	       ((REVNODE*)V(rev))->extnode = en;

	       VPTR walk;
	       if (ctr>900)
		  walk = Vmalloc (sizeof (EXTDAT));
	       else
		  walk = Vmalloc (8+(WORD)ctr);
	       ((EXTNODE*)V(en))->extdat = walk;
	       while (ctr){
		  WORD trn = ctr<1024 ? (WORD)ctr : 1024;
                  char tmp[1000];
		  Read ((BYTE *)tmp, gHan, trn);
                  memcpy (((EXTDAT*)V(walk))->dat, tmp, trn);
		  ctr-=trn;
		  if (ctr){
		     VPTR tmp = ((EXTDAT*)V(walk))->next = Vmalloc (sizeof (EXTDAT));
		     walk = tmp;
		  }
	       }
	       break;
	    default:
	       IE();
	       break;
	 }
	 Read (&flag, gHan, 1);
      }
//      if (crlf) Out (1,"\n\r");
   }
}

void DumpTags (char *pcArchPath, char *pcDumpName){
   TRACEM("Enter DumpTags");
   SetArea (0);
   TTP (pcArchPath, 1);
   InvHashC(); // Needed for each archive being processed!
   ReadArchive (pcArchPath);

   BoosterOn();
   MakePipe (pipe);
   DumpAll ();
   WORD wLen=0;
   Write ((BYTE *)&wLen, gHan, 2);

#undef Write

   BoosterOff();
   gHan = Open (pcDumpName, CR|MUST|CWR);
   BYTE tmp[500];
   WORD siz= ReadPipe (pipe, tmp, 500);
   while (siz){
	Write (tmp, gHan, siz);
	siz= ReadPipe (pipe, tmp, 500);
   }

   Close (gHan);
   ClosePipe (pipe);

   CloseArchive();
   TRACEM("Leave DumpTags");
}

#undef Read

void ReadTags (char *pcArchPath, char *pcDumpName){
   SetArea (0);
   TTP (pcArchPath, 1);
   MODE.fInc=1;
   InvHashC(); // Needed for each archive being processed!
   IncUpdateArchive (pcArchPath);
   MakePipe (pipe);
   gHan = Open (pcDumpName, RO|MUST|CRT);
   BYTE tmp[500];
   WORD siz= Read(tmp, gHan, 500);
   while (siz){
	WritePipe (pipe, tmp, siz);
	siz = Read (tmp, gHan, 500);
   }
   Close (gHan);
   BoosterOn();
   ReadAll ();
   ClosePipe (pipe);
   BoosterOff();
   CloseArchive();
}

static void PFiles (){
   VPTR walk = TFirstFileNN();
   while (!IS_VNULL(walk)){
      if (TRevs()){
	 for (long l=TRevs()-1;l>=0;l--){
	    VPTR rwlk = TRev(l);
	    switch (((REVNODE*)V(rwlk))->bStatus){
	       case FST_MARK:
		  ((REVNODE*)V(rwlk))->bStatus=FST_OLD;
		  break;
	       case FST_OLD:
		  ((REVNODE*)V(rwlk))->bStatus=FST_DEL;
		  break;

	    }
	 }
      }
      walk = TNextFileNN();
   }
}

static void PDirs (){
   VPTR walk = TFirstDir();
   while (!IS_VNULL(walk)){
      if (((DIRNODE*)V(walk))->bStatus==DST_DEL) goto skip;
      TCD (walk);
      PFiles ();
      PDirs();
      TUP ();
skip:
      walk = TNextDir();
   }
}

static void PAll (){
   VPTR cur = TWD();
   TCD(VNULL);
   PFiles ();
   PDirs ();
   TCD(cur);
}

void ApplyFilter (char *pcFilter){
   WORD wLen;
   gHan = Open (pcFilter, RO|MUST|CRT);
   MakePipe (pipe);
   BYTE tmm[500];
   WORD siz= Read(tmm, gHan, 500);
   while (siz){
	WritePipe (pipe, tmm, siz);
	siz = Read (tmm, gHan, 500);
   }
   Close (gHan);
   BoosterOn();
   for (;;){
      ReadPipe (pipe, (BYTE *)&wLen, 2);
      if (!wLen) break;
      ReadPipe (pipe, (BYTE *)tmp, wLen);
      VPTR rev = AccessRev (tmp);
      ((REVNODE*)V(rev))->bStatus=FST_MARK;
   }
   ClosePipe (pipe);
   PAll();
   BoosterOff();
}
