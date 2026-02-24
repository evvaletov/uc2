// Copyright 1992, all rights reserved, AIP, Nico de Vries
// ARCHIO.CPP

#include <dir.h>
#include <stdlib.h>
#include <stdio.h>
#include <dos.h>
#include <string.h>
#include "main.h"
#include "llio.h"
#include "archio.h"
#include "vmem.h"
#include "video.h"
#include "superman.h"
#include "diverse.h"
#include "comoterp.h"
#include "neuroman.h"
#include "handle.h"

int iArchLockHandle[MAX_AREA];
int iArchInHandle[MAX_AREA];
int iArchOutHandle[MAX_AREA];
static char pcTmpFile[MAX_AREA][MAXPATH];
static char pcArchFile[MAX_AREA][MAXPATH];
static int iMode[MAX_AREA];
BYTE bDamp[MAX_AREA];
BYTE bPDamp[MAX_AREA];
DWORD dwPLen [MAX_AREA];

int iArchArea=0;

void SetArea (int a){
   iArchArea = a;
}

void CacheIt (int);

extern void KillCache (int han);

static int ei=0, di=0, oo=1;
void exiti (void){
   if (di){
      if (oo){
	 KillCache (iArchOutHandle[iArchArea]);
	 Close (iArchOutHandle[iArchArea]);
      }
      if (Exists (pcTmpFile[iArchArea]))
	 Delete (pcTmpFile[iArchArea]);
   }
}

extern char fContinue;
extern int probeo;
extern int iVerifyMode;

#define AMAG 0x01B2C3D4L

int fexiti=0;

void Backup (char *pcName){
   static char path[MAXPATH];
   char drive[MAXDRIVE];
   char dir[MAXDIR];
   char file[MAXFILE];
   char ext[MAXEXT];
   fnsplit (pcName,drive,dir,file,ext);
   if (strcmp(ext,".BAK"))
      fnmerge (path,drive,dir,file,".BAK");
   else
      fnmerge (path,drive,dir,file,".BAC");
   Out (3,"\x7\Creating backup of archive (%s)\n\r",path);
   CopyFile (pcName, path);
}

// mode 0=read only 1=update 2=incremental update 3=create
int SetArchive (char *pcPath, BYTE mode){
   if (MODE.fBak){
      Backup (pcPath);
   }
   TRACEM("Enter SetArchive");
   if (ei==0) {
      fexiti = 1;
      ei=1;
   }
   CleanNtx();
   TRACEM("Check-1 SetArchive");
   int ret = Exists (pcPath);
   iMode[iArchArea]=mode;
   TRACEM("Check-2 SetArchive");
   if (mode!=2 && mode!=3){ // RO & upd
      if (mode==1) // update
	 iArchInHandle[iArchArea] = Open (pcPath, RW|MUST|CRT);
      else // "just" read
	 iArchInHandle[iArchArea] = Open (pcPath, RO|MUST|CRI);
      FHEAD g;
      ARSeek (1,0);
      ARead((BYTE *)&g, sizeof(FHEAD));
      if (g.dwComponentLength != (g.dwComponentLength2-AMAG)){
	 if (fContinue) {
	    Error (90,"file header is damaged");
	    probeo=1;
	    FHEAD q;
	    ARSeek (1, GetFileSize (iArchInHandle[iArchArea])-sizeof(FHEAD));
	    ARead ((BYTE *)&q, sizeof(FHEAD));
	    if (q.dwComponentLength != (q.dwComponentLength2-AMAG)){
	       Error (90,"spare copy of file header is damaged, unable to test for damage protection");
	       g.fDamageProtected = 0;
	    } else {
	       Out (7,"\x5\File header has been reconstructed with spare copy of file header\n\r");
	       g=q;
	    }
	 } else {
	    FatalError (200,"file header is damaged, repair this archive with 'UC T'");
	 }
      } else if (iVerifyMode){
	 FHEAD q;
	 ARSeek (1, GetFileSize (iArchInHandle[iArchArea])-sizeof(FHEAD));
	 ARead ((BYTE *)&q, sizeof(FHEAD));
	 if (memcmp(&g,&q,sizeof(g))!=0){
	    Error (90,"file header and spare file header are different");
	    probeo=1;
	 }
      }
      bPDamp[iArchArea] = g.fDamageProtected;
      dwPLen[iArchArea] = g.dwComponentLength + sizeof(FHEAD);
      ARSeek (1,sizeof(FHEAD));
   }
   TRACEM("Check-3 SetArchive");
   if (mode==1){ // update
      strcpy (pcTmpFile[iArchArea],TmpFile(pcPath,0,".TMP"));
      di=1;
      strcpy (pcArchFile[iArchArea],pcPath);
      if (-1==(iArchOutHandle[iArchArea] = Open (pcTmpFile[iArchArea], CR|MAY|CWR))){
	 strcpy (pcTmpFile[iArchArea],TmpFile(pcPath,0,".TMP"));
	 if (-1==(iArchOutHandle[iArchArea] = Open (pcTmpFile[iArchArea], CR|MAY|CWR))){
	    strcpy (pcTmpFile[iArchArea],TmpFile(pcPath,0,".TMP"));
	    iArchOutHandle[iArchArea] = Open (pcTmpFile[iArchArea], CR|MUST|CWR);
	 }
      }
      oo=1;
      FHEAD g;
      ARSeek (1,0);
      ARead((BYTE *)&g, sizeof(FHEAD));
      ARSeek (1,0);
      bPDamp[iArchArea] = g.fDamageProtected;
      FHEAD f;
      f.dwHead = UC2_LABEL;
      f.dwComponentLength = 0;
      f.dwComponentLength2 = AMAG;
      if (MODE.bDamageProof==0){ // no damage protection specs available
	 if (ret){ // keep damage protection from old archive
	    f.fDamageProtected = g.fDamageProtected;
	 } else
	    f.fDamageProtected = 0; // default NO damage protection
      } else if (MODE.bDamageProof==1){
	 f.fDamageProtected = 1; // specific protect command
      } else
	 f.fDamageProtected = 0; // specific unprotect command
      AWrite ((BYTE *)&f, sizeof(FHEAD));
      AWSeek (1,0);
      bDamp [iArchArea]= f.fDamageProtected;
   }
   TRACEM("Check-4 SetArchive");
   if (mode==3){ // create
      strcpy (pcTmpFile[iArchArea],TmpFile(pcPath,0,".TMP"));
      di=1;
      strcpy (pcArchFile[iArchArea],pcPath);
      if (-1==(iArchOutHandle[iArchArea] = Open (pcTmpFile[iArchArea], CR|MAY|CWR))){
	 strcpy (pcTmpFile[iArchArea],TmpFile(pcPath,0,".TMP"));
	 if (-1==(iArchOutHandle[iArchArea] = Open (pcTmpFile[iArchArea], CR|MAY|CWR))){
	    strcpy (pcTmpFile[iArchArea],TmpFile(pcPath,0,".TMP"));
	    iArchOutHandle[iArchArea] = Open (pcTmpFile[iArchArea], CR|MUST|CWR);
	 }
      }
      oo=1;
      FHEAD f;
      f.dwHead = UC2_LABEL;
      f.dwComponentLength = 0;
      f.dwComponentLength2=AMAG;
      if (MODE.bDamageProof==1){
	 f.fDamageProtected = 1; // specific protect command
      } else
	 f.fDamageProtected = 0; // specific unprotect command
      AWrite ((BYTE *)&f, sizeof(FHEAD));
      AWSeek (1,0);
      bDamp [iArchArea]= f.fDamageProtected;
   }
   TRACEM("Check-5 SetArchive");
   if (mode==2){ // inc update
      strcpy (pcArchFile[iArchArea],pcPath);
      iArchInHandle[iArchArea]
	 = iArchOutHandle[iArchArea]
	 = Open (pcPath, RW|MUST|CRI);
      di=1;
      oo=1;
      FHEAD g;
      ARSeek (1,0);
      ARead((BYTE *)&g, sizeof(FHEAD));
      if (g.dwComponentLength != (g.dwComponentLength2-AMAG)){
	 FatalError (200,"file header is damaged, repair this archive with 'UC T'");
      }
      ARSeek (1,0);
      bPDamp[iArchArea] = g.fDamageProtected;
      if (MODE.bDamageProof==0) bDamp[iArchArea]=g.fDamageProtected; // keep old status
      if (MODE.bDamageProof==1) bDamp[iArchArea]=1; // force to on
      if (MODE.bDamageProof==2) bDamp[iArchArea]=0; // force to off
   }
   TRACEM("Leave SetArchive");
   return ret;
}

int NewArch (){
   return iArchInHandle[iArchArea]==-1;
}

void DamPro (int iHandle); // dampro.cpp

int Test (char *);

void CloseArchiveFile (void){ // ALWAYS creates new component head of Out
   if (iMode[iArchArea]==0){ // read only
      Close (iArchInHandle[iArchArea]);
   } else if (iMode[iArchArea]==1){ // plain update
#ifndef UE2
      if (iArchInHandle[iArchArea]!=-1) // any (read) file open?
	 Close (iArchInHandle[iArchArea]);
      FHEAD f;
      f.dwHead = UC2_LABEL;
      f.dwComponentLength = GetFileSize (iArchOutHandle[iArchArea]) - sizeof(FHEAD);
      f.dwComponentLength2 = f.dwComponentLength+AMAG;
      f.fDamageProtected = bDamp[iArchArea];
      AWSeek (1,0);
      AWrite ((BYTE *)&f, sizeof(FHEAD));
      if (bDamp[iArchArea])
	 DamPro (iArchOutHandle[iArchArea]);
      AWSeek (1, GetFileSize (iArchOutHandle[iArchArea]));
      AWrite ((BYTE *)&f, sizeof(FHEAD));
      Close (iArchOutHandle[iArchArea]);
      oo=0;
#endif
   } else if (iMode[iArchArea]==3){ // plain create
#ifndef UE2
      FHEAD f;
      f.dwHead = UC2_LABEL;
      f.dwComponentLength = GetFileSize (iArchOutHandle[iArchArea]) - sizeof(FHEAD);
      f.dwComponentLength2 = f.dwComponentLength+AMAG;
      f.fDamageProtected = bDamp[iArchArea];
      AWSeek (1,0);
      AWrite ((BYTE *)&f, sizeof(FHEAD));
      if (bDamp[iArchArea])
	 DamPro (iArchOutHandle[iArchArea]);
      AWSeek (1, GetFileSize (iArchOutHandle[iArchArea]));
      AWrite ((BYTE *)&f, sizeof(FHEAD));
      Close (iArchOutHandle[iArchArea]);
      oo=0;
#endif
   } else if (iMode[iArchArea]==2){ // incremental update (In=Out)
#ifndef UE2
      FHEAD f;
      f.dwHead = UC2_LABEL;
      f.dwComponentLength = GetFileSize (iArchOutHandle[iArchArea]) - sizeof(FHEAD);
      f.dwComponentLength2 = f.dwComponentLength+AMAG;
      f.fDamageProtected = bDamp[iArchArea];
      AWSeek (1,0);
      AWrite ((BYTE *)&f, sizeof(FHEAD));
      if (bDamp[iArchArea])
	 DamPro (iArchOutHandle[iArchArea]);
      AWSeek (1, GetFileSize (iArchOutHandle[iArchArea]));
      AWrite ((BYTE *)&f, sizeof(FHEAD));
      Close (iArchOutHandle[iArchArea]);
      DFlush("secure storage of result archive");
      oo=0;
      if (CONFIG.bRelia==2){
	 if (!Test (pcArchFile[iArchArea])){
	    FatalError (210,"update failed");
	 }
      }
#endif
   }
   if (iMode[iArchArea]==1 || iMode[iArchArea]==3){ // update, remove "backup" and place new version
#ifndef UE2
      if (CONFIG.bRelia==2){
	 if (!Test (pcTmpFile[iArchArea])){
	    Delete (pcTmpFile[iArchArea]);
	    FatalError (210,"operation failed (archive has been restored to original state)");
	 }
      }
      if (Exists(pcArchFile[iArchArea])){
	 DFlush("secure storage of tmp archive");
	 Delete (pcArchFile[iArchArea]);
      }
      Rename (pcTmpFile[iArchArea],pcArchFile[iArchArea]);
      DFlush("secure storage of result archive");
#endif
   }
   di=0;
}

void AWSeek (DWORD dwVolume, DWORD dwOffset){
   if (dwVolume==1){
      Seek (iArchOutHandle[iArchArea], dwOffset);
   }
}

void AWCut (void){
   SetFileSize (iArchOutHandle[iArchArea], Tell (iArchOutHandle[iArchArea]));
}

void AWEnd (void){
   Seek (iArchOutHandle[iArchArea], GetFileSize (iArchOutHandle[iArchArea]));
}

void ARSeek (DWORD dwVolume, DWORD dwOffset){
   if (dwVolume==1){
      Seek (iArchInHandle[iArchArea], dwOffset);
   }
}

int AWTell (DWORD *pdwVolume, DWORD *pdwOffset){
   *pdwVolume = 1;
   *pdwOffset = Tell (iArchOutHandle[iArchArea]);
   return 0;
}

DWORD AWLen (void){
   return GetFileSize (iArchOutHandle[iArchArea]);
}

int ARTell (DWORD *pdwVolume, DWORD *pdwOffset){
   *pdwVolume = 1;
   *pdwOffset = Tell (iArchInHandle[iArchArea]);
   return 0;
}

static DWORD dwOutCtr;

void far pascal AWrite (BYTE *bBuf, WORD wSize){
#ifdef UCPROX
   if (debug) Out (7,"<<%ld>>", Tell (iArchOutHandle[iArchArea]));
#endif
   dwOutCtr += wSize;
   Write (bBuf, iArchOutHandle[iArchArea], wSize);
}

void ResetOutCtr (){
   dwOutCtr = 0;
}

DWORD GetOutCtr (){
   return dwOutCtr;
}

WORD far pascal ARead (BYTE *bBuf, WORD wSize){
   return Read (bBuf, iArchInHandle[iArchArea], wSize);
}

void Old2New (DWORD dwLen){
#ifndef UE2
   BYTE *buf = xmalloc (16*1024L, STMP);
   while (dwLen){
      WORD wLen = 16*1024;
      if (wLen>dwLen) wLen = (WORD) dwLen;
      ARead (buf, wLen);
      AWrite (buf, wLen);
      dwLen -= wLen;
   }
   xfree (buf, STMP);
#endif
}
