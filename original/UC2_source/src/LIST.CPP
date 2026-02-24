// Copyright 1992, all rights reserved, AIP, Nico de Vries
// LIST.CPP

#include <mem.h>
#include <string.h>
#include <io.h>
#include <time.h>
#include <dos.h>
#include <stdio.h>
#include "main.h"
#include "video.h"
#include "vmem.h"
#include "list.h"
#include "superman.h"
#include "diverse.h"
#include "comoterp.h"
#include "llio.h"
#include "archio.h"
#include "handle.h"

extern BYTE bVerbose; // COMOTERP.CPP
extern BYTE bDump;

int iListHandle;

char month[13][4]={
   "jan",
   "feb",
   "mar",
   "apr",
   "may",
   "jun",
   "jul",
   "aug",
   "sep",
   "oct",
   "nov",
   "dec",
   "???"};

void DateTime (WORD spec1, WORD spec2){
   DWORD spec = ((DWORD)spec2<<16L)+(DWORD)spec1;
   ftime* f = (ftime*)&spec;
   if (f->ft_month==0 || f->ft_month>12) f->ft_month=13;
   Out (7,"%s-%02d-%4d  %2d:%02d:%02d",
      strupr(month[f->ft_month-1]), f->ft_day, f->ft_year+1980,
      f->ft_hour, f->ft_min, f->ft_tsec*2);
}

void DDateTime (WORD spec1, WORD spec2){
   DWORD spec = ((DWORD)spec2<<16L)+(DWORD)spec1;
   ftime* f = (ftime*)&spec;
   Out (7,"      DATE(MDY)=%02d %02d %04d\r\n"
          "      TIME(HMS)=%02d %02d %02d\r\n",
	  f->ft_month, f->ft_day, f->ft_year+1980,
	  f->ft_hour, f->ft_min, f->ft_tsec*2);
}

DWORD dwItemCtr;

void DumpTags (VPTR walk){
   VPTR next;

   while (!IS_VNULL(walk)){
      BrkQ();
      int ctr=1;
loop:
      next = ((EXTNODE*)V(walk))->next;
      if (!IS_VNULL(next)){
	 if (strcmp(TagName(((EXTNODE*)V(next))->extmeta.tag),
		    TagName(((EXTNODE*)V(walk))->extmeta.tag))==0){
	    ctr++;
	    walk=next;
	    goto loop;
	 }
      }
      if (ctr>1)
	 Out (7," %d*",ctr);
      else
	 Out (7," ");
      Out (7,"[%s]",TagName(((EXTNODE*)V(walk))->extmeta.tag));
      walk = next;
   }
}

static void ListDirs (){
   if (MODE.fGlf) return;
   VPTR walk = TFirstDir();
   while (!IS_VNULL(walk)){
      DIRNODE *dn = (DIRNODE *)V(walk);
#ifdef UCPROX
//      Out (" [[%lu %lu]] ",dn->osmeta.dwParent,dn->dirmeta.dwIndex);
#endif
      if (bDump){
         Out (7,"   DIR\r\n");
         Out (7,"      NAME=[%s]\r\n",Rep2Name(dn->osmeta.pbName));
	 DDateTime (dn->osmeta.wTime, dn->osmeta.wDate);
	 Out (7,"      ATTRIB=");
	 if (dn->osmeta.bAttrib & FA_ARCH) Out (7,"A");
	 if (dn->osmeta.bAttrib & FA_RDONLY) Out (7,"R");
	 if (dn->osmeta.bAttrib & FA_SYSTEM) Out (7,"S");
	 if (dn->osmeta.bAttrib & FA_HIDDEN) Out (7,"H");
         Out (7,"\r\n");
	 VPTR walk = dn->extnode;
	 while (!IS_VNULL(walk)){
            Out (7,"      TAG=[%s]\r\n",TagName(((EXTNODE*)V(walk))->extmeta.tag));
	    walk = ((EXTNODE*)V(walk))->next;
	 }
      } else if (bVerbose){
	 Out (7,"%s <DIR>",Rep2DName(dn->osmeta.pbName));
	 DumpTags (dn->extnode);
         Out (7,"\r\n");
      } else {
	 char tmp[20];
	 strcpy (tmp,Rep2Name(dn->osmeta.pbName));
	 strcat (tmp,"]");
	 Out (7,"[%-14s",tmp);
	 dwItemCtr++;
	 if (dwItemCtr%5==0)
            Out (7,"\r\n");
	 else
	    Out (7," ");
      }
      BrkQ();
      walk = TNextDir();
   }
}

extern DWORD dwTotalSize; // superman.h

DWORD dwFil,dwTot;

char *WFull (VPTR rev, DWORD hint);

static void ListFiles (){
   int flag=0;
   Out (7,"\x7");
   DWORD ctr=0, rctr=0;
   VPTR walk = TFirstFileNN();
   while (!IS_VNULL(walk)){
      for (DWORD idx=0;idx<TRevs();idx++){
	 REVNODE *fn = (REVNODE *)V(TRev(idx));
	 if (idx==0) flag=1;
	 if (fn->bStatus==FST_MARK){
	    Out (7,"\x7");
	    if (flag){
	       ctr++;
	       flag=0;
	    }
	    rctr++;
	    dwFil++;
	    if (MODE.fGlf){
	       char pth[260];
	       strcpy (pth, WFull (TRev(idx),idx));
               strcat (pth,"\r\n");
	       Write ((BYTE *)pth, iListHandle, strlen (pth));
	    } else if (bDump){
	       BrkQ();
	       REVNODE *fn = (REVNODE *)V(TRev(idx));
               Out (7,"   FILE\r\n");
               Out (7,"      NAME=[%s]\r\n",Rep2Name(fn->osmeta.pbName));
               Out (7,"      VERSION=%ld\r\n",idx);
               Out (7,"      SIZE=%ld\r\n",fn->filemeta.dwLength);
	       WORD w=fn->filemeta.wFletch;
	       DWORD dw=fn->filemeta.dwLength;
	       dw^=0x26F79FA4L;
	       dw^=(DWORD)w|((DWORD)w<<16);
               Out (7,"      CHECK=%04X%08lX\r\n",w,dw);
	       DDateTime (fn->osmeta.wTime, fn->osmeta.wDate);
	       Out (7,"      ATTRIB=");
	       if (fn->osmeta.bAttrib & FA_ARCH) Out (7,"A");
	       if (fn->osmeta.bAttrib & FA_RDONLY) Out (7,"R");
	       if (fn->osmeta.bAttrib & FA_SYSTEM) Out (7,"S");
	       if (fn->osmeta.bAttrib & FA_HIDDEN) Out (7,"H");
               Out (7,"\r\n");
	       VPTR walk = fn->extnode;
	       while (!IS_VNULL(walk)){
                  Out (7,"      TAG=[%s]\r\n",TagName(((EXTNODE*)V(walk))->extmeta.tag));
		  walk = ((EXTNODE*)V(walk))->next;
	       }
	    } else if (bVerbose){
	       if (idx>0){
		  BrkQ();
		  REVNODE *fn = (REVNODE *)V(TRev(idx));
		  Out (7,"%s;%-2ld %9lu  ",Rep2DName(fn->osmeta.pbName),
		     idx,
		     fn->filemeta.dwLength);
		  dwTot+=fn->filemeta.dwLength;
		  DateTime (fn->osmeta.wTime, fn->osmeta.wDate);
		  Out (7," ");
		  if (fn->osmeta.bAttrib & FA_ARCH) Out (7," Arch");
		  if (fn->osmeta.bAttrib & FA_RDONLY) Out (7," R/O");
		  if (fn->osmeta.bAttrib & FA_SYSTEM) Out (7," Sys");
		  if (fn->osmeta.bAttrib & FA_HIDDEN) Out (7," Hid");
		  DumpTags (fn->extnode);
                  Out (7,"\r\n");
	       } else {
		  Out (7,"%s    %9lu  ",Rep2DName(fn->osmeta.pbName),
		     fn->filemeta.dwLength);
		  dwTot+=fn->filemeta.dwLength;
		  DateTime (fn->osmeta.wTime, fn->osmeta.wDate);
   //             Out (" %04X ",fn->filemeta.wFletch);
		  Out (7," ");
		  if (fn->osmeta.bAttrib & FA_ARCH) Out (7," Arch");
		  if (fn->osmeta.bAttrib & FA_RDONLY) Out (7," R/O");
		  if (fn->osmeta.bAttrib & FA_SYSTEM) Out (7," Sys");
		  if (fn->osmeta.bAttrib & FA_HIDDEN) Out (7," Hid");
		  DumpTags (fn->extnode);
                  Out (7,"\r\n");
	       }
	    } else {
	       if (idx==0){
		  Out (7,"%-15s",Rep2Name(fn->osmeta.pbName));
		  dwTot+=fn->filemeta.dwLength;
	       } else {
		  char ttmp[30];
		  sprintf (ttmp,"%s;%ld",Rep2Name(fn->osmeta.pbName),idx);
		  Out (7,"%-15s",ttmp);
		  dwTot+=fn->filemeta.dwLength;
	       }
	       dwItemCtr++;
	       if (dwItemCtr%5==0)
                  Out (7,"\r\n");
	       else
		  Out (7," ");
	    }
	 }
      }
      BrkQ();
      walk = TNextFileNN();
   }
   if (bDump || MODE.fGlf){
   } else if (!bVerbose){
      if (dwItemCtr%5!=0) Out (7,"\r\n");
   } else {
      if (rctr>ctr) {
	 if (ctr==1)
            Out (7,"      1 matching file (%lu file revisions)\r\n",rctr);
	 else
            Out (7,"%7lu matching files (%lu file revisions)\r\n",ctr,rctr);
      } else if (ctr>1)
         Out (7,"%7lu matching files\r\n",ctr);
      else if (ctr==0)
         Out (7,"     no matching files\r\n");
      else
         Out (7,"     1 matching file\r\n");
   }
}

static int bListableFiles (){
   int ret=0;
   VPTR walk = TFirstFileNN();
   while (!IS_VNULL(walk)){
      for (DWORD idx=0;idx<TRevs();idx++){
	 REVNODE *fn = (REVNODE *)V(TRev(idx));
	 if (fn->bStatus==FST_MARK)
	    ret=1;
      }
      BrkQ();
      walk = TNextFileNN();
   }
   return ret;
}

static void WalkDirs (){
   VPTR walk = TFirstDir();
   Out (7,"\x7");
   while (!IS_VNULL(walk)){
      TCD (walk);
      if (bVerbose)
         Out (7,"\x4\----------------------------------------------------------------------------\r\n");
      if (MODE.fGlf){
      } else if (bDump){
         Out (7, "LIST [\\%s]\r\n",GetPath(walk,TDeep()));
      } else {
	 if (bVerbose || bListableFiles())
            Out(7,"\x4\--> Directory of \\%s\r\n\x7",GetPath(walk,TDeep()));
      }
      if (!bDump && !MODE.fGlf && bVerbose) Out (7,"\r\n");
      dwItemCtr=0;
      if (bVerbose || bDump || bListableFiles())
	 ListDirs ();
      ListFiles ();
      WalkDirs();
      TUP ();
      walk = TNextDir();
   }
}

extern char pcArchivePath[260]; // comoterp.cpp
extern int iArchInHandle[MAX_AREA]; // archio.cpp

void ListArchI (){
   if (MODE.fGlf){
   } else if (bDump){
      Out (7, "LIST [\\]\r\n");
   } else {
      Out(7,"\x4\--> Directory of \\%s\r\n\x7",GetPath(TWD(),TDeep()));
      if (bVerbose) Out (7,"\r\n");
   }
   dwItemCtr=0;
   ListDirs ();
   ListFiles ();
   if (MODE.fSubDirs) WalkDirs();
}

extern int TCW(VPTR Mpath);

extern DWORD dwFiles, dwDirs;

extern int factor;

extern XHEAD xhead [MAX_AREA];    // XHEAD of active archive

extern DWORD dwArchSerial;

extern BYTE bPDamp[MAX_AREA]; // ARCHIO.CPP
extern XTAIL xTail [MAX_AREA];    // archive extended tail

void ListArch (VPTR Mpath){
   dwFil=0;
   dwTot=0;
   VPTR walk = Mpath;
   factor=3;
   if (MODE.fGlf){
      iListHandle=Open (MODE.szListFile, MUST|CR|CWR);
   }
   while (!IS_VNULL(walk)){
      if (TCW(walk)){
	 ListArchI();
      }
      walk = ((MPATH *)V(walk))->vpNext;
   }
   factor=1;
   if (MODE.fGlf){
      Close (iListHandle);
   } else if (bDump){
      Out (7,"END\r\n");
      Out (7,"SPEC-SECTION\r\n");
      Out (7,"    DAMPRO = %d\r\n", bPDamp[iArchArea]);
      if (xTail[iArchArea].pbLabel[0])
	 Out (7,"    VOLLABEL = \"%.11s\"\r\n", xTail[iArchArea].pbLabel);
      Out (7,"    SERIAL = %s\r\n", neat(dwArchSerial));
      int no = xhead[iArchArea].wVersionMadeBy%100;
      if (no==0) no=1;
      Out (7,"    UC2-REVISION = %d\r\n", no);
      Out (7,"END\r\n");
   } else if (bVerbose){
      DWORD gf;
      Out (7,"\r\n\4");
      if (bPDamp[iArchArea])
	 Out (7,"Archive is damage protected, ");
      else
	 Out (7,"Archive is NOT damage protected, ");
      if (xTail[iArchArea].pbLabel[0])
	 Out (7,"archive volume label is \"%.11s\"\n\r", xTail[iArchArea].pbLabel);
      else
	 Out (7,"archive has no volume label\n\r");
      int no = xhead[iArchArea].wVersionMadeBy%100;
      if (no==0) no=1;
      Out (7,"\5""Archive created by UltraCompressor II revision %d  ", no);
      if (dwArchSerial==0)
	 Out (7,"");
      else if (dwArchSerial==1)
	 Out (7,"[anonymous version]");
      else
	 Out (7,"[%s]",neat(dwArchSerial));
      Out (7,"\r\n\4");
      Out (7,"files listed           = %-8s",neat(dwFil));
      Out (7,"total length listed files = %s bytes\r\n",neat(dwTot));
      Out (7,"files in archive       = %-8s",neat(dwFiles));
      Out (7,"total length all files    = %s bytes\r\n",neat(dwTotalSize));
      Out (7,"directories in archive = %-8s",neat(dwDirs));
      Out (7,"archive length            = %s bytes\r\n",neat((DWORD)(gf=(long)GetFileSize(iArchInHandle[iArchArea]))));
      long ts = (long)dwTotalSize;
      while (gf>1000000L || ts>1000000L){
	 gf/=10;
	 ts/=10;
      }
      if (gf!=0 && ts!=0){
	 long rat = (ts*100)/gf;
	 long per = (gf*1000)/ts;
	 long aper = 1000-per;
	 if (rat>100)
	    Out (7,"\5\compression ratio      = 1:%ld.%02ld  (%ld.%1ld%% reduction, %ld.%1ld%% left)\r\n",rat/100,rat%100,aper/10,aper%10,per/10,per%10);
      }
   } else {
      Out (7,"\4""files listed = %s", neat(dwFil));
      Out (7," (%s bytes)\r\n", neat(dwTot));
      long gf;
      gf=(long)GetFileSize(iArchInHandle[iArchArea]);
      long ts = (long)dwTotalSize;
      while (gf>1000000L || ts>1000000L){
	 gf/=10;
	 ts/=10;
      }
      if (gf!=0 && ts!=0){
	 long rat = (ts*100)/gf;
	 long per = (gf*1000)/ts;
	 long aper = 1000-per;
	 if (rat>100)
            Out (7,"\x5\compression ratio = 1:%ld.%02ld  (%ld.%1ld%% reduction, %ld.%1ld%% left)\r\n",rat/100,rat%100,aper/10,aper%10,per/10,per%10);
      }
   }
}
