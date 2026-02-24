// Copyright 1992, all rights reserved, AIP, Nico de Vries
// COMOTERP.CPP

#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <dir.h>
#include <string.h>
#include <ctype.h>
#include <mem.h>
#include <alloc.h>
#include "main.h"
#include "vmem.h"
#include "comoterp.h"
#include "video.h"
#include "archio.h"
#include "superman.h"
#include "neuroman.h"
#include "ultracmp.h"
#include "list.h"
#include "diverse.h"
#include "archio.h"
#include "convert.h"
#include "llio.h"
#include "handle.h"
#include "menu.h"
#include "dirman.h"
#include "tagman.h"
#include "views.h"
#include "vol.h"

struct MODE MODE;
BYTE bExtractMode;
BYTE bDeleteMode;

int movemode=0;
void MoveClear (void);
void MoveDelete (void);
void RabClear (void);
void RabDelete (void);

VPTR Mpath = VNULL;
VPTR Uqueue = VNULL;

int turbo1=0;
int turbo=0;

void Turbo (void){
   if (turbo1 && !turbo){
      turbo=1;
   }
}

void Compressor (WORD no){
   MODE.bCompressor = no;
   switch (no){
      case 2:
      case 22:
//       TuneNeuro (62976U, 62976U);
	 TuneNeuro (32768U, 32768U);
//       TuneNeuro (16384U, 16384U);
	 break;
      case 3:
      case 23:
      case 30:
      case 31:
      case 32:
      case 33:
      case 34:
      case 35:
      case 36:
      case 37:
      case 38:
      case 39:
	 TuneNeuro (62976U, 62976U);
//       TuneNeuro (49152U, 49152U);
	 break;
      case 4:
      case 24:
	 TuneNeuro (62976U, 62976U);
	 break;
      case 5:
      case 25:
	 TuneNeuro (62976U, 62976U);
	 break;
      case 80:
	 TuneNeuro (62976U, 62976U);
	 break;
   }
}

extern CONF CONFIG, SPARE;

static void DefaultMode (){
   CONFIG=SPARE;
   switch (CONFIG.bDComp){
      case 0:
	 Compressor (2);
	 break;
      case 1:
	 Compressor (3);
	 break;
      case 2:
	 Compressor (4);
	 break;
      case 3:
	 Compressor (5);
	 break;
   }
   MODE.fForce = 0;
   MODE.fSubDirs = 0;
   MODE.fASubDirs = 0;
   MODE.fInc = CONFIG.fInc;
   MODE.bAddOpt = 1;
   MODE.bExOverOpt = 1;
   MODE.fSMSkip = CONFIG.fSMSkip;
   MODE.bMKDIR = 1;
   MODE.bDTT = 0;
   switch (CONFIG.fHID){
      case 0:
	 MODE.bHid=3;
	 break;
      case 1:
	 MODE.bHid=2;
	 break;
      case 2:
	 MODE.bHid=1;
	 break;
   }
   MODE.bDamageProof = 0; // don't care, keep old status or make not damage protected
   if (CONFIG.bRelia) MODE.bDamageProof=1;
   MODE.fNoLock=0;

   MODE.fFreshen=0;
   MODE.fNoPath=0;
   MODE.fSmart=0;
   MODE.fPrf=0;
   MODE.fContains=0;
   MODE.bExcludeAttr=0;
   MODE.fArca=0;
   MODE.bELD=0;
   MODE.bEED=0;
   MODE.fQuery=0;
   MODE.fNewer=0;
   MODE.fNew=0;
   MODE.fNof=0;
   MODE.fRab=0;
   MODE.fBak=0;
   MODE.fKdt=0;
   MODE.fVlab=0;
}

static void ForceMode (){ // DefaultMode is called as well !!!
   MODE.fForce = 1;
   MODE.bAddOpt = 2;
   MODE.bExOverOpt = 2;
   MODE.bMKDIR = 2;
//   MODE.fSMSkip = 0; // no smart skipping in force mode
   switch (CONFIG.fHID){
      case 0:
	 MODE.bHid=3;
	 break;
      case 1:
	 MODE.bHid=2;
	 break;
      case 2:
	 MODE.bHid=2;
	 break;
   }
}

static int iArgc;
static char ** ppcArgv;
static int iNext;
static char atom[300];

int specat=0;

struct ATOM {
   char atom[300];
   VPTR next;
};

static VPTR aroot=VNULL;
static VPTR aatom=VNULL;

int totalfil=0;
int items=0;
void TotP (void){
   static int i=0;
   static int j=0;
   static int cl=0;
   i++;
   if (i>25){
      if (totalfil!=j){
         if (!cl){
            Out (3|8,"Reading command line & script(s) ");
            cl=1;
            items=1;
         }
         j=totalfil;
      } else {
         if (!cl){
            Out (3|8,"Reading command line ");
            cl=1;
            items=1;
         }
      }
      Hint ();
      i-=25;
   }
}

void AddAtom (char *at){
   TotP();
   strupr (at);
//   Out (7,"[%s]",at);
   VPTR tmp = Vmalloc (sizeof(ATOM));
   strcpy (((ATOM*)V(tmp))->atom, at);
   if (IS_VNULL(aatom)){
      ((ATOM*)V(tmp))->next = aroot;
      aroot = tmp;
      aatom = tmp;
   } else {
      ((ATOM*)V(tmp))->next = ((ATOM*)V(aatom))->next;
      ((ATOM*)V(aatom))->next = tmp;
      aatom = tmp;
   }
}

int GetComlAtom (){
   if (iArgc==iNext)
      return 0;
   else
      strncpy (atom, ppcArgv[iNext++], 88);
   return 1;
}

int Expand (void){
   int invert;
#ifndef UE2
   VPTR walk=aroot;
   aatom=VNULL;
   while (!IS_VNULL(walk)){
      strcpy (atom, ((ATOM*)V(walk))->atom);
      if (atom[0]=='@' || (atom[0]=='!' && atom[1]=='@')){
	 if (atom[0]=='!'){
	    strcpy (atom, atom+1);
	    invert=1;
	 } else {
	    invert=0;
	 }
	 // remove atom
	 VPTR tmp = walk;
	 if (IS_VNULL(aatom)){
	    aroot = ((ATOM*)V(walk))->next;
	 } else {
	    ((ATOM*)V(aatom))->next = ((ATOM*)V(walk))->next;
	 }
	 Vfree (tmp);
	 int o;
	 char tmp1[260];
	 char tmp2[250];
	 strcpy (tmp1, atom+1);
	 strcat (tmp1,"_USC");
	 if (getenv(tmp1)){
	    strcpy (tmp2, getenv(tmp1));
	    int pos=0;
            if (invert){
                atom[0]='!';
                pos++;
            }
	    char c;
	    for (WORD l=0; l<strlen(tmp2); l++){
	       c = tmp2[l];
	       if (c==' ' || c=='\n' || c=='\r'){
		  if (pos){
		     if (pos>89) pos=89;
		     atom[pos] = 0;
		     AddAtom (atom);
		     pos=0;
                     if (invert){
                         atom[0]='!';
                         pos++;
                     }
		  }
	       } else {
		  atom[pos++]=c;
		  if (pos>90) pos=90;
	       }
	    }
	    if (pos){
	       if (pos>89) pos=89;
	       atom[pos] = 0;
	       AddAtom (atom);
	       pos=0;
               if (invert){
                   atom[0]='!';
                   pos++;
               }
	    }
	 } else {
            strcpy (tmp1, atom+1);
            strcat (tmp1, ".USC");
	    if (LocateF (tmp1, 1)){
	       o = Open (LocateF (tmp1, 1), MUST|RO|CRT);
	    } else {
	       if (LocateF (atom+1, 1)){
		  o = Open (LocateF (atom+1, 1), MUST|RO|CRT);
	       } else {
		  FatalError (120,"cannot find script file %s",atom+1);
	       }
	    }
	    if (totalfil++>255) FatalError(120,"more than 250 files specified with @ (cyclic reference?)");
   //       Out (7,"[[%s",atom+1);
	    int pos=0, check=0;
            if (invert){
                atom[0]='!';
                pos++;
                check=1;
            }
	    char c;
	    for (DWORD l=0; l<GetFileSize(o); l++){
	       Read ((BYTE *)&c, o, 1);
	       if (c==' ' || c=='\n' || c=='\r' || c==26){
		  if (pos!=check){
		     if (pos>89) pos=89;
		     atom[pos] = 0;
		     AddAtom (atom);
		     pos=0;
                     if (invert){
                         atom[0]='!';
                         pos++;
                     }
		  }
	       } else {
		  atom[pos++]=c;
		  if (pos>90) pos=90;
	       }
	    }
	    if (pos!=check){
	       if (pos>89) pos=89;
	       atom[pos] = 0;
	       AddAtom (atom);
	       pos=0;
               if (invert){
                    atom[0]='!';
                    pos++;
               }
	    }
	    Close (o);
	 }
//       Out (7,"]]");
	 return 1;
      } else {
	 aatom = walk;
	 walk = ((ATOM*)V(walk))->next;
      }
   }
#endif
   return 0;
}

int GetAtom (){
   static int nw=1;
   if (nw){
      while (GetComlAtom())
	 AddAtom (atom);
      nw=0;
      while (Expand());
      if (items) Out (3|8,"\n\r");
   }
   if (IS_VNULL(aroot)){
      turbo1=1;
      return 0;
   }
   strcpy (atom, ((ATOM*)V(aroot))->atom);
   VPTR tmp = aroot;
   aroot = ((ATOM*)V(aroot))->next;
   Vfree (tmp);
//   Out (7,"{%s}",atom);
   return 1;
}

int ReadAtom (){
   if (specat==1) return 0;
   if (!GetAtom()){
      return 0;
   }
   if (atom[0]=='&'){
      specat=1;
      return 0;
   }
   return 1;
}

void CheckIs (char c){
   if (c=='=') return;
   char atb[300];
   strcpy (atb, atom);
   if (!ReadAtom()) FatalError (120,"incomplete command line");
   strcat (atb,"=");
   strcat (atb, atom);
   strcpy (atom, atb);
}

#ifdef UCPROX
   #define DB_TXT(x,y) {if (debug) {Out(7,"<%s:%s>",x,y);}}
   #define DB_LF() {if (debug) {Out (7,"\n\r");}}
#else
   #define DB_TXT(x,y)
   #define DB_LF()
#endif

void CP (VPTR v){
   while (!IS_VNULL(v)){
      VPTR tmp = v;
      v = ((MMASK *)V(v))->vpNext;
      Vfree (tmp);
   }
}

void CM (VPTR v){
   while (!IS_VNULL(v)){
      VPTR tmp = v;
      CP (((MPATH *)V(v))->vpMasks);
      v = ((MPATH *)V(v))->vpNext;
      Vfree (tmp);
   }
}

void Anal (char *s, int fSelect);

void RClearMask (void){
   CM (Mpath);
   Mpath = VNULL;
   CM (Uqueue);
   Uqueue = VNULL;
}

void ClearMask(char *pcArchName){
   char drive[MAXDRIVE];
   char dir[MAXDIR];
   char file[MAXFILE];
   char ext[MAXEXT];
   char pcTmpName[MAXPATH];
   if (pcArchName){
      fnsplit (pcArchName,drive,dir,file,ext);
      fnmerge (pcTmpName,NULL,NULL,file,".UR2");
      Anal (pcTmpName,0);
      fnmerge (pcTmpName,NULL,NULL,file,ext);
      Anal (pcTmpName,0);
   } else {
      RClearMask();
      Anal (TMPPREF"?????.???",0); // no AIP temp files
   }
}

void DumpProbs (void){
   VPTR walk=Mpath;
   while (!IS_VNULL(walk)){
      VPTR wlk = ((MPATH *)V(walk))->vpMasks;
      while (!IS_VNULL(wlk)){
	 if (!(((MMASK *)V(wlk))->bFlag)){
	    Warning (20,"no file found matching %s",((MMASK*)V(wlk))->pcOrig);
	 }
	 wlk = ((MMASK *)V(wlk))->vpNext;
      }
      walk = ((MPATH *)V(walk))->vpNext;
   }
}


int rs (char *s, char c){
   for (int i=strlen(s)-1;i>=0;i--)
      if (s[i]==c) return i;
   return -1;
}

void Exp (char *pc, int i){
   int mode=0;
   pc[i]=0;
   for (int j=0;j<i;j++){
      if (mode==0 && pc[j]==0) mode = 2;
      if (pc[j]=='*') mode = 1;
      if (mode==1) pc[j]='?';
      if (mode==2) pc[j]=' ';
   }
}

VPTR LocMac (int f, char *pcTPath){
   VPTR walk = Mpath;
   if (f) walk = Uqueue;
   while (!IS_VNULL(walk)){
      if (strcmp (((MPATH *)V(walk))->pcTPath, pcTPath)==0) return walk;
      walk = ((MPATH *)V(walk))->vpNext;
   }
   VPTR n = Vmalloc (sizeof (MPATH));
   strcpy (((MPATH *)V(n))->pcTPath, pcTPath);
   ((MPATH *)V(n))->vpMasks = VNULL;
   if (f){
       ((MPATH *)V(n))->vpNext = Uqueue;
       Uqueue = n;
   } else {
       ((MPATH *)V(n))->vpNext = Mpath;
       Mpath = n;
   }
   return n;
}

BYTE mark=0;

int specialanal=0;

void Anal (char *spi, int fSelect){
   char s[200];
   strcpy (s,spi);
   if (s[strlen(s)-1]=='\\') strcat (s,"*.*");
   char pcTPath[200];
   VPTR mm = Vmalloc (sizeof (MMASK));
   MMASK *m = (MMASK *)V(mm);
   m->bFlag=mark;
   strcpy (m->pcOrig, spi);
   char drive[MAXDRIVE];
   char dir[MAXDIR];
   char file[MAXFILE];
   char ext[MAXEXT];
   if (-1!=rs(s,';')){
      if (rs(s,'*') > rs(s,';')){
	 m->fRevs=0;
      } else {
	 m->fRevs=1;
	 m->dwRevision=atol(s+rs(s,';')+1);
      }
      s[rs(s,';')]='\0';
   } else {
      m->fRevs=1;
      m->dwRevision=0;
      if (specialanal) m->fRevs=0;
   }
   fnsplit (s, drive, dir, file, ext);
   if (ext[0]=='.') strcpy (ext, ext+1);
   Exp(file,8);
   Exp(ext,3);
   fnmerge (pcTPath, drive, dir, NULL, NULL);
   memcpy (m->pbName, file, 8);
   memcpy (m->pbName+8, ext, 3);
   VPTR mp;
   if (fSelect){
      mp = LocMac (0, pcTPath);
   } else {
      if (IS_VNULL(Uqueue)){
	  Uqueue = Vmalloc (sizeof (MPATH));
          strcpy (((MPATH *)V(Uqueue))->pcTPath, "");
	  ((MPATH *)V(Uqueue))->vpMasks = VNULL;
          ((MPATH *)V(Uqueue))->vpNext = VNULL;
      }
//      if (pcTPath[0]==0) {
//	 mp = Uqueue;
//      } else {
         mp = LocMac (1, pcTPath);
//       Out (7, "(%s)", pcTPath);
//	 FatalError (120,"definition of files to exclude cannot contain a path");
//      }
   }
   m = (MMASK *)V(mm);
   if (fSelect){
      m->vpNext = ((MPATH *)V(mp))->vpMasks;
      ((MPATH *)V(mp))->vpMasks = mm;
   } else {
      m->vpNext = ((MPATH *)V(mp))->vpMasks;
      ((MPATH *)V(mp))->vpMasks = mm;
   }
#ifdef UCPROX
   if (debug){
      char pcName[12];
      memcpy (pcName,m->pbName,11);
      pcName[11]=0;
      if (m->fRevs){
	 Out(7,"(\"%s\" \"%11s\" %lu)",pcTPath,pcName,m->dwRevision);
      } else {
	 Out(7,"(\"%s\" \"%11s\" *)",pcTPath,pcName);
      }
   }
#endif
}

char szDestPath[260];
void SetDest (char *dp){
   if (strlen(szDestPath)) FatalError (120,"only one destination path (#...) can be defined");
   strcpy (szDestPath, dp);
   if ((strlen(szDestPath)==2)&&(szDestPath[1]==':'))
      strcat (szDestPath, ".");
   if (strlen(szDestPath) && szDestPath[strlen(szDestPath)-1]!='\\')
      strcat (szDestPath,"\\");
//   Out (7,"[dest=%s]",szDestPath);
}

int delscan=0;

int iDrvBack;

extern int iDrv;

int GetMask (){
   int ret=0;
   int once=1;
   iDrv = iDrvBack;
again:
   switch (atom[0]){
      case '#':
         if (iDrv==0 && isalpha(atom[1]) && atom[2]==':')
         {
            iDrv = atom[1]-'A'+1;
//            Out (7,"[%d]",iDrv);
         }
	 SetDest (atom+1);
	 if (ReadAtom()) goto again;
	 break;
      case '!':
	 DB_TXT("negsel",atom+1);
	 Anal(atom+1, 0);
	 if (ReadAtom()) goto again;
	 break;
      default:
         if (once && iDrv==0 && isalpha(atom[0]) && atom[1]==':')
         {
            iDrv = atom[0]-'A'+1;
//            Out (7,"[%d]",iDrv);
         }
         once=0;
	 DB_TXT("sel",atom);
	 ret=1;
	 Anal(atom, 1);
	 if (ReadAtom()) goto again;
   }
ex:
   if ((ret==0) && delscan){
        FatalError (120, "to delete all files *.* is needed");
   }
   if (ret==0) Anal("*.*", 1); // only negative selects or none at all
   return 1;
}

char pcArchivePath[200];
char pcRecPath[200];
char pcLckPath[200];

struct AP {
   char pcAP[200];
   VPTR next;
};

VPTR artap=VNULL;

BYTE bDump;

extern int scan1;

extern int skcount;

int Arch (char *r){
   skcount=0;
   scan1=0;
   static int first=1;
   if (IS_VNULL(artap)){
      return 0;
   } else {
      VPTR tmp=artap;
      strcpy (pcArchivePath, ((AP*)V(tmp))->pcAP);
      artap = ((AP*)V(tmp))->next;
      if (IS_VNULL(artap)) Turbo();
      Vfree (tmp);
      if (!bDump){
	 if (first){
	    first=0;
	 } else
	    Out (7|8,"\n\r");
	 if (strlen(szDestPath)){
	    if (szDestPath[0]=='#'){
	       if (strlen(szDestPath+2)){
		  Out (3,"\x5%s %s (destination path %s+<sourcepath>)\n\r",r,pcArchivePath,szDestPath+1);
	       } else {
		  Out (3,"\x5%s %s (destination path <sourcepath>)\n\r",r,pcArchivePath);
	       }
	    } else {
	       Out (3,"\x5%s %s (destination path %s)\n\r",r,pcArchivePath,szDestPath);
	    }
	 } else {
	    Out (3,"\x5%s %s\n\r",r,pcArchivePath);
	 }
	 Out (4,"\x5%s\n\r",pcArchivePath);
      }
      char drive[MAXDRIVE];
      char dir[MAXDIR];
      char file[MAXFILE];
      char ext[MAXEXT];

      fnsplit (pcArchivePath,drive,dir,file,ext);
      fnmerge (pcRecPath,drive,dir,file,".UR2");
      fnmerge (pcLckPath,drive,dir,file,".ULC");

      return 1;
   }
}

static int specmode=0;

void AddArch (char *pth){
   if (specmode) ClearMask (pth);
   VPTR tmp=Vmalloc (sizeof(AP));
   strcpy (((AP*)V(tmp))->pcAP,pth);
   ((AP*)V(tmp))->next = artap;
   artap=tmp;
}

int convert=0;


typedef struct S_NEXTMASK
{
    char szPath[MAXPATH];
    VPTR vpNext;
} NEXTMASK;

int GetArch (){
#ifndef UE2
   char drive[MAXDRIVE];
   char dir[MAXDIR];
   char mfile[MAXFILE];
   char mext[MAXEXT];
   char mask[MAXPATH];
   char tmp[MAXPATH];
   char nmask[MAXPATH];
   char file[MAXFILE];
   char ext[MAXEXT];
   struct ffblk ffblk;
   int done;
   int once=0;
   int found=0;

   VPTR nm=VNULL;

   if (!once && MODE.fASubDirs)
   {
      Out (3,"\x7Scanning for archives ");
      StartProgress (-1, 3);
      once=1;
   }

   DB_TXT("arch",atom);
again:
   fnsplit (atom,drive,dir,mfile,mext);
   if (strcmp (mext,".UR2")==0){
      FatalError (120,"the .UR2 extention is reserved for UltraRecover files");
   }
   if (atom[strlen(atom)-1]!='.'){
      if (mext[0]==0 || mext[1]==0) {
	 if (convert){
	    strcpy (mext,".*");
	    fnmerge (mask,drive,dir,mfile,".*");
	 } else {
	    if (stricmp (getenv("UC2_PUC"),"ON")==0)
	       fnmerge (mask,drive,dir,mfile,".PU2");
	    else
	       fnmerge (mask,drive,dir,mfile,".UC2");
	 }
      } else
	 fnmerge (mask,drive,dir,mfile,mext);
   } else {
      atom[strlen(atom)-1]=0;
      strcpy (mask, atom);
   }
   if ((strstr(mfile,"?")==NULL)&&
       (strstr(mfile,"*")==NULL)&&
       (strstr(mext,"?")==NULL)&&
       (strstr(mext,"*")==NULL)){
       AddArch (mask);
       found=1;
   } else {
      if (!once && MODE.fASubDirs)
      {
         Out (3,"\x7Scanning for archives ");
         StartProgress (-1, 3);
         once=1;
      }
      CSB;
	 done = findfirst (mask, &ffblk, 0xF7);
      CSE;
      while (!done){
         Hint();
         BrkQ();
	 if (!(ffblk.ff_attrib&FA_LABEL) && !(ffblk.ff_attrib&FA_DIREC)){
	    fnsplit (ffblk.ff_name, NULL, NULL, file, ext);
	    fnmerge (pcArchivePath, drive, dir, file, ext);
	    AddArch (pcArchivePath);
            found=1;
	 }
	 CSB;
	    done = findnext(&ffblk);
	 CSE;
      }
   }
   if (MODE.fASubDirs)
   {
      fnmerge (mask,drive,dir,"*",".*");
      CSB;
         done = findfirst (mask, &ffblk, 0xF7);
      CSE;
      while (!done){
         Hint();
         BrkQ();
         if (ffblk.ff_attrib&FA_DIREC)
            if (strcmp(ffblk.ff_name,".")!=0)
      	    if (strcmp(ffblk.ff_name,"..")!=0) {
                  fnsplit (ffblk.ff_name, NULL, NULL, file, ext);
                  fnmerge (tmp, NULL, dir, file, ext);
                  fnmerge (nmask, drive, tmp, mfile, mext);
                  VPTR nw = Vmalloc (sizeof (NEXTMASK));
                  ((NEXTMASK*)V(nw))->vpNext = nm;
//                  Out (7,"[%s]",nmask);
                  strcpy (((NEXTMASK*)V(nw))->szPath, nmask);
                  nm = nw;
               }
         CSB;
   	 done = findnext(&ffblk);
         CSE;
      }
      if (!IS_VNULL(nm))
      {
          strcpy (atom, ((NEXTMASK*)V(nm))->szPath);
          VPTR tmp = nm;
          nm = ((NEXTMASK*)V(nm))->vpNext;
          Vfree (tmp);
          goto again;
      }
   }
#ifdef UCPROX
   if (debug) Out (7,"(%s)",pcArchivePath);
#endif
#endif
   EndProgress();
   if (once) Out (3,"\n\r\n\r");
   if (!found) Warning (20,"no archive found");
   return 1;
}

int getdig (char* pc, int f, int t){
   int ret=0;
   for (int i=f;i<t+1;i++){
      if (!isdigit(pc[i])) FatalError(120,"you can only use digits in a date, not '%c'",pc[i]);
      ret = 10*ret + pc[i] - '0';
   }
   return ret;
}

extern char month[12][4];

int dayim[12] = {31,29,31,30,31,30,31,31,30,31,30,31};

int iFilterMode=0;
int iVlab=0, iDrv;
int iTsn=0;
char pcFilter[MAXPATH];

#pragma argsused
void ScanDT (char *pc, ftime* pft, int mode){
#ifndef UE2
   int tmp;
   int l = strlen(pc);
   if (l>7) {
      tmp = getdig (pc,4,7);
      if ((tmp<1980) || (tmp>2107)) FatalError (120,"year should be between 1980 and 2107, not %d",tmp);
      pft->ft_year = tmp - 1980;
   } else {
      if (mode)
	 pft->ft_year = 127;
      else
	 pft->ft_year = 0;
   }
   if (l>9){
      tmp = getdig (pc,8,9);
      if ((tmp<1)||(tmp>12)) FatalError(120,"month should be between 1 and 12, not %d",tmp);
      pft->ft_month = tmp;
   } else {
      if (mode)
	 pft->ft_month = 12;
      else
	 pft->ft_month = 1;
   }
   if (l>11){
      tmp = getdig (pc,10,11);
      int lim = dayim[pft->ft_month-1];
      if (tmp>lim) FatalError(120,"day should be between 1 and %d, not %d",lim,tmp);
      if (tmp<1) FatalError(120,"day should be between 1 and %d, not %d",lim,tmp);
      pft->ft_day = tmp;
   } else {
      if (mode){
	 pft->ft_day = 31; // always used as upper limit
      } else {
	 pft->ft_day = 1;
      }
   }
   if (l>13){
      tmp = getdig (pc,12,13);
      if (tmp>23) FatalError(120,"hour should be 23 or smaller, not %d",tmp);
      pft->ft_hour = tmp;
   } else {
      if (mode)
	 pft->ft_hour = 23;
      else
	 pft->ft_hour = 0;
   }
   if (l>15){
      tmp = getdig (pc,14,15);
      if (tmp>59) FatalError(120,"minute should be 59 or smaller, not %d",tmp);
      pft->ft_min = tmp;
   } else {
      if (mode)
	 pft->ft_min = 59;
      else
	 pft->ft_min = 0;
   }
   if (l>17){
      tmp = getdig (pc,16,17)/2;
      if (tmp>29) FatalError(120,"second should be 59 or smaller, not %d",getdig (pc,16,17));
      pft->ft_tsec = tmp;
   } else {
      if (mode)
	 pft->ft_tsec = 29;
      else
	 pft->ft_tsec = 0;
   }
#endif
}


#pragma argsused
void ProOpt (char *pc){
#ifndef UE2
again:
   for (int i=0;i<strlen(pc);i++){
      if (pc[i]=='-'||pc[i]==':'||pc[i]=='/'){
	 strcpy (pc+i,pc+i+1);
	 goto again;
      }
   }
   if (strncmp(pc,"FILTER",6)==0){
      CheckIs (pc[6]);
      strcpy (pcFilter, pc+7);
      iFilterMode = 1;
   } else
   if (strncmp(pc,"TSN",3)==0){
      iTsn=1;
   } else
   if (strncmp(pc,"VLAB",4)==0){
      iVlab = 1;
      if (pc[4]&&pc[5]) {
	 iDrvBack = iDrv = pc[5]-'A'+1;
      } else {
	 iDrvBack = iDrv = 0;
      }
   } else
   if (strncmp(pc,"ARCA",4)==0){
      MODE.fArca=1;
   } else
   if (strncmp(pc,"NEWER",5)==0){
      MODE.fNewer=1;
   } else
   if (strncmp(pc,"NOF",3)==0){
      MODE.fNof=1;
   } else
   if (strncmp(pc,"BAK",3)==0){
      MODE.fBak=1;
   } else
   if (strncmp(pc,"RAB",3)==0){
      MODE.fRab=1;
   } else
   if (strncmp(pc,"ASUB",4)==0){
      MODE.fASubDirs=1;
   } else
   if (strncmp(pc,"QUERY",5)==0){
      MODE.fQuery=1;
   } else
   if (strncmp(pc,"EFA",3)==0){
      CheckIs (pc[3]);
      if (strstr(pc+4,"H")) MODE.bExcludeAttr |= FA_HIDDEN;
      if (strstr(pc+4,"A")) MODE.bExcludeAttr |= FA_ARCH;
      if (strstr(pc+4,"S")) MODE.bExcludeAttr |= FA_SYSTEM;
      if (strstr(pc+4,"R")) MODE.bExcludeAttr |= FA_RDONLY;
   } else {
      if (!strstr(pc,"="))
	 CheckIs (pc[strlen(pc)]);
      if (strncmp(pc,"CONTAINS=",9)==0){
	 strcpy (MODE.szContains, pc+9);
	 if (!strlen(MODE.szContains)){
	    if (!ReadAtom() || strlen(atom)==0) FatalError (120,"contains string is too short");
	    strcpy (MODE.szContains,atom);
	 }
	 if (strlen(pc)>50) FatalError (120,"contains string is too long");
	 if (MODE.szContains[0]=='"') strcpy (MODE.szContains, MODE.szContains+1);
	 if (MODE.szContains[strlen(MODE.szContains)-1]=='"')
	    MODE.szContains[strlen(MODE.szContains)-1]=0;
//       Out (7,"[Search=\"%s\"]",MODE.szContains);
	 MODE.fContains=1;
      } else
      if (strncmp(pc,"RELIA=D",7)==0){
	 CONFIG.bRelia = 0;
      } else
      if (strncmp(pc,"RELIA=P",7)==0){
	 CONFIG.bRelia = 1;
	 MODE.bDamageProof=1;
      } else
      if (strncmp(pc,"RELIA=E",7)==0){
	 CONFIG.bRelia = 2;
	 MODE.bDamageProof=1;
      } else
      if (strncmp(pc,"ARCON=ON",8)==0){
	 CONFIG.fAutoCon = 1;
      } else
      if (strncmp(pc,"ARCON=OF",8)==0){
	 CONFIG.fAutoCon = 0;
      } else
      if (strncmp(pc,"SMSKIP=ON",9)==0){
	 CONFIG.fSMSkip = 1;
	 MODE.fSMSkip = 1;
      } else
      if (strncmp(pc,"SMSKIP=OF",9)==0){
	 CONFIG.fSMSkip = 0;
	 MODE.fSMSkip = 0;
      } else
      if (strncmp(pc,"BAN=AS",6)==0){
	 CONFIG.fMul = 0;
      } else
      if (strncmp(pc,"BAN=AL",6)==0){
	 CONFIG.fMul = 1;
      } else
      if (strncmp(pc,"BAN=ON",6)==0){
	 CONFIG.fMul = 1;
      } else
      if (strncmp(pc,"BAN=N",5)==0){
	 CONFIG.fMul = 2;
      } else
      if (strncmp(pc,"BAN=OF",5)==0){
	 CONFIG.fMul = 2;
      } else
      if (strncmp(pc,"VSCAN=ON",8)==0){
	 CONFIG.fVscan = 1;
      } else
      if (strncmp(pc,"VSCAN=OF",8)==0){
	 CONFIG.fVscan = 0;
      } else
      if (strncmp(pc,"SOS2EA=ON",9)==0){
	 CONFIG.fEA = 1;
      } else
      if (strncmp(pc,"SOS2EA=OF",9)==0){
	 CONFIG.fEA = 0;
      } else
      if (strncmp(pc,"SYSHID=AS",9)==0){
	 CONFIG.fHID = 2;
	 MODE.bHid=1;
      } else
      if (strncmp(pc,"SYSHID=AL",9)==0){
	 CONFIG.fHID = 1;
	 MODE.bHid=2;
      } else
      if (strncmp(pc,"SYSHID=ON",9)==0){
	 CONFIG.fHID = 1;
	 MODE.bHid=2;
      } else
      if (strncmp(pc,"SYSHID=N",8)==0){
	 CONFIG.fHID = 0;
	 MODE.bHid=3;
      } else
      if (strncmp(pc,"SYSHID=OF",8)==0){
	 CONFIG.fHID = 0;
	 MODE.bHid=3;
      } else
      if (strncmp(pc,"NET=ON",6)==0){
	 CONFIG.fNet = 1;
      } else
      if (strncmp(pc,"NET=OF",6)==0){
	 CONFIG.fNet = 0;
      } else
      if (strncmp(pc,"NET=A",5)==0){
	 CONFIG.fNet = 2;
      } else
      if (strncmp(pc,"NOLOCK",6)==0){
	 MODE.fNoLock = 1;
      } else
      if (strncmp(pc,"ELD=",4)==0){
	 MODE.bELD=1;
	 ScanDT (pc, &MODE.ftELD, 0);
   //      ftime *f=&MODE.ftELD;
   //      Out (7,"[ELD=%s-%02d-%4d %d:%02d:%02d]",
   //	     strupr(month[f->ft_month-1]), f->ft_day, f->ft_year+1980,
   //	     f->ft_hour, f->ft_min, f->ft_tsec*2);
      } else
      if (strncmp(pc,"EED=",4)==0){
	 MODE.bEED=1;
	 ScanDT (pc, &MODE.ftEED, 1);
   //      ftime *f=&MODE.ftEED;
   //      Out (7,"[EED=%s-%02d-%4d %d:%02d:%02d]",
   //	     strupr(month[f->ft_month-1]), f->ft_day, f->ft_year+1980,
   //	     f->ft_hour, f->ft_min, f->ft_tsec*2);
      } else
      if (strncmp(pc,"DTT=",4)==0){ // YYYYMMDDHHMMSS
	 MODE.bDTT=1;
	 ScanDT (pc, &MODE.ftDTT, 1);
	 ftime *f=&MODE.ftDTT;
	 Out (7,"\5\Dynamic Time Travel to "
		"%s-%02d-%4d %d:%02d:%02d \n\r",
		strupr(month[f->ft_month-1]), f->ft_day, f->ft_year+1980,
		f->ft_hour, f->ft_min, f->ft_tsec*2);
      } else
	 FatalError(120,"unknown option !%s",pc);
   }
#endif
}

#pragma argsused
int GetOptions(int p=1){
#ifndef UE2
   if (p==-1) goto reread;
again:
   switch (atom[p]){
      case '#':
	 SetDest (atom+p+1);
	 // now get a new atom!
      case 0:
reread:
	 if (!ReadAtom()) return 0;
	 if (atom[0]=='#'){
	    SetDest (atom+1);
	    if (!ReadAtom()) return 0;
	 }
	 if (atom[0]=='-' || atom[0]=='/'){
	    p=1;
	    goto again;
	 }
	 if (atom[0]=='!'){
	    ProOpt (atom+1);
	    goto reread;
	 }
	 break; // end of options
      case '!':
	 ProOpt (atom+p+1);
	 goto reread;
      case 'M':
	 movemode=1;
	 p++;
	 goto again;
      case 'T':
	 switch (atom[p+1]){
	    case 'F':
	       DB_TXT("opt","TF");
	       Compressor(2);
	       break;
	    case 'N':
	       DB_TXT("opt","TN");
	       Compressor(3);
	       break;
	    case 'T':
	       DB_TXT("opt","TT");
	       Compressor(4);
	       break;
	    case 'S':
	       if (atom[p+2]=='T'){
		  DB_TXT("opt","TST");
		  Compressor(5);
		  p++;
#ifdef UCPROX
	       } else if (atom[p+2]=='F'){
		  DB_TXT("opt","TSF");
		  Compressor(80);
		  p++;
#endif
	       } else goto err;
	       break;
	    case 'M':
	       if (atom[p+2]=='1'){
		  DB_TXT("opt","TM1");
		  Compressor(30);
		  p++;
	       } else if (atom[p+2]=='2'){
		  DB_TXT("opt","TM2");
		  Compressor(31);
		  p++;
	       } else if (atom[p+2]=='3'){
		  DB_TXT("opt","TM3");
		  Compressor(32);
		  p++;
	       } else if (atom[p+2]=='4'){
		  DB_TXT("opt","TM4");
		  Compressor(33);
		  p++;
	       } else if (atom[p+2]=='5'){
		  DB_TXT("opt","TM5");
		  Compressor(34);
		  p++;
	       } else if (atom[p+2]=='6'){
		  DB_TXT("opt","TM6");
		  Compressor(35);
		  p++;
	       } else if (atom[p+2]=='7'){
		  DB_TXT("opt","TM7");
		  Compressor(36);
		  p++;
	       } else if (atom[p+2]=='8'){
		  DB_TXT("opt","TM8");
		  Compressor(37);
		  p++;
	       } else {
		  DB_TXT("opt","TM");
		  Compressor(24);
	       }
	       break;
	    default:
err:
	       FatalError(120,"unknown option -T%c",atom[p+1]);
	 }
	 p+=2;
	 goto again;
      case 'S':
	 DB_TXT("opt","S");
	 MODE.fSubDirs=1;
	 p++;
	 goto again;
      case 'F':
	 DB_TXT("opt","F");
	 ForceMode();
	 p++;
	 goto again;
//    case 'M':
//       DB_TXT("opt","M");
//       MODE.fMoveMode=1;
//       p++;
//       goto again;
      case 'I':
	 DB_TXT("opt","I");
	 MODE.fInc = 1;
	 p++;
	 goto again;
      case 'B':
	 DB_TXT("opt","B");
	 MODE.fInc = 0;
	 p++;
	 goto again;
      case 'P': // damage protection as option
	 MODE.bDamageProof = 1; // set
	 DB_TXT("opt","P");
	 p++;
	 goto again;
      case 'U': // damage protection as option
	 if (CONFIG.bRelia) FatalError (123,"configuration does not allow removal of damage protection (UC -! C)");
	 MODE.bDamageProof = 2; // unset
	 DB_TXT("opt","U");
	 p++;
	 goto again;
      default:
	 FatalError(120,"unknown option -%c",atom[p]);
   }
#endif
   return 1;
}

BYTE bVerbose;

int VerifyDP (void); // DAMPRO.CPP
extern BYTE bPDamp[MAX_AREA]; // ARCHIO.CPP
extern int RepairDP (char *);
int iVerifyMode;
int probeo;
extern int problemos;

extern int iArchInHandle[MAX_AREA];

int DetectUseal (void){
   BYTE magic[8]={0xdb, 0x3a, 0x0f, 0x20, 0x02, 0x20, 0x13, 0x45};
   BYTE buf[1024];
   memset (buf, 0, 1024);
   int iHan = iArchInHandle[iArchArea];
   DWORD siz = GetFileSize (iHan);
   if (siz<1025){
     Seek (iHan, 0);
     if (siz>1000) siz=1000;
   } else {
     Seek (iHan, siz-1024);
     siz=1000;
   }
   Read (buf, iHan, (WORD)siz);
   for (int i=0;i<1000;i++){
      if (buf[i]==0xdb){
	 for (int j=1;j<8;j++){
	    if (buf[i+j]!=magic[j]) goto next;
	 }
	 return 1;
      }
next:
   }
   return 0;
}

static int typ;
void TTP (char *pcArchivePath, int mustexist){
   typ = TType (pcArchivePath, NULL, mustexist);
//   IE();
   if (typ==2){
#ifdef UE2
      FatalError (205,"%s is not in UC2 format",pcArchivePath);
#else
      if (CONFIG.fAutoCon || MODE.fForce){
	 Convert (pcArchivePath,0);
	 strcpy (pcArchivePath, NewExt (pcArchivePath));
      } else {
	 Menu ("\x6\Upgrade %s to UC2 format?",pcArchivePath);
	 Option ("",'Y',"es");
	 Option ("",'N',"o");
	 switch(Choice()){
	    case 1:
	       Convert (pcArchivePath,0);
	       strcpy (pcArchivePath, NewExt (pcArchivePath));
	       typ=1;
	       goto leave;
	    case 2:
	       FatalError (100,"%s is not in UC2 format, conversion not allowed",pcArchivePath);
	 }
      }
      scan1=0;
#endif
   } else if (typ==3){
      FatalError (120,"%s has a completely unknown file format",pcArchivePath);
   } else if (typ==4){
      FatalError (125,"%s is encrypted with UltraCrypt",pcArchivePath);
   } else if (mustexist && typ==0){
      if (Exists(pcArchivePath))
	 FatalError (130,"failed to access archive %s",pcArchivePath);
      else
	 FatalError (130,"%s does not exist",pcArchivePath);
   }
leave:
}

extern DWORD smskip;

extern char fContinue;
extern int iArchInHandle[MAX_AREA];

#pragma argsused
int Test (char *pcArchivePath){
#ifndef UE2
   int ret=1;
   if (!Exists(pcArchivePath)) return 0;
   VPTR UQ = Uqueue;
   VPTR MP = Mpath;
   BYTE b=bDeleteMode;
   struct MODE m=MODE;
   Uqueue = VNULL;
   Mpath = VNULL;
   Out (3,"\x5\Testing archive integrity\n\r");
   InvHashC(); // Needed for each archive being processed!
   SetArea (1);
   iVerifyMode=1;
   MODE.fSubDirs=1;
   bDeleteMode=0;
   TTP (pcArchivePath, 1);
   probeo=0;
   ReadArchive (pcArchivePath);
   CacheRT (iArchInHandle[iArchArea]);
   bExtractMode=1;
   if (bPDamp[iArchArea]!=0){
      if (!VerifyDP()){
	 Error (90,"archive is damaged");
	 ret=0;
      }
   }
   if (ret){
      RClearMask ();
      Anal("*.*;*",1);
      BoosterOn();
      VPTR walk = Mpath;
      while (!IS_VNULL(walk)){
	 MarkUp (".\\",VNULL, walk, 0);
	 walk = ((MPATH *)V(walk))->vpNext;
      }
      BoosterOff();
      AddAccess(0); // read needed masters
      ExtractFiles();
      if (probeo) ret=0;
   }
   ClearMask(NULL);
   CloseArchive();
   Mpath = MP;
   Uqueue = UQ;
   MODE=m;
   InvHashC();
   SetArea (0);
   bDeleteMode=b;
   iVerifyMode=0;
   return ret;
#else
   return 1;
#endif
}

extern DWORD dwTSize;
DWORD dwMyCtr;

int DirExists (char *path);
VPTR StrToDir (char *path);

void Recover (char *old, char *rec, char *nw);

static char* Fix = "FIX_0000.UC2";
void NewFix (void){
   for (int i=1;i<10000;i++){
      sprintf (Fix,"FIX_%04d.UC2",i);
      if (!Exists(Fix)) return;
   }
   FatalError (150,"cannot create a FIX_????.UC2 file");
}

int somban=0;

int scan1;

int nobas, nodel, noadd, noopt, nounp, norev;

extern int conto;

void SKillPath (char *p);

DWORD ser2int (DWORD n);
DWORD int2ser (DWORD n);
WORD ser2pin (DWORD n);

void UpdateCFG ();

static void near CHK (void){
   if (!MODE.fNoLock && Exists(pcLckPath)){
      FatalError (135,"archive lock file %s found",pcLckPath);
   }
   if (Exists(pcRecPath)){
      FatalError (200,"found crash recover file %s (repair archive with 'UC T')",pcRecPath);
   }
}

void ProcessFiles (void);

void ApplyFilter (char *pcFilter);

void CleanEmptyDirs (void);

extern char *sspec; // views/cpp

WORD maxdate, maxtime;

void CheckFiles (void);
void ProcessFiles (void);
void QueryFiles (char *res);

extern XTAIL xTail [MAX_AREA];    // archive extended tail

int convback=0;
extern char* NewExt (char *filename);

void ComoTerp (int argc, char **argv){
   char p[260], q[260];
   char tmp[130];
   iArgc = argc;
   ppcArgv = argv;
   iNext = 1;
again:
   nobas=0;
   nodel=0;
   noadd=0;
   noopt=0;
   nounp=0;

   scan1=0;
   DefaultMode();
   iFilterMode = 0;
   iVlab=0;
   iTsn=0;
   iVerifyMode=0;
   specat=0;
   strcpy (szDestPath,"");
   if (!ReadAtom()) return;
   InvHashC(); // Needed for each archive being processed!
   bExtractMode=0;
   bDeleteMode=0;
#ifdef UE2
   movemode=0;
   MoveClear();
   if (atom[0]=='~') goto normal;
   if ((atom[0]=='L') && (atom[1]==0)){
      if (ReadAtom())
	 goto ue2_list;
      else
	 strcpy (atom,"L");
   }
   if (((atom[0]=='-')||(atom[0]=='/')) && (atom[1]=='L')){
      if (!ReadAtom()) FatalError (120,"no archive specified");
      goto ue2_list;
   } else {
      goto ue2_extract;
   }
#else
   if ((atom[0]=='-')||(atom[0]=='/')){
      movmem (atom+1, atom, strlen(atom)+1);
   }
   movemode=0;
   MoveClear();
#endif
normal:
   switch (atom[0]){
#ifndef UE2
      case '$':
	 int r;
	 if (strcmp(atom,"$TSN")==0){
	    r = GetOptions(4);
TSN:
	    if (!r) FatalError(120,"no archive specified");
            if (MODE.fSubDirs)
            {
               MODE.fASubDirs=1;
            }
	    if (MODE.bDTT) FatalError (120,"Dynamic Time Travel is not possible for 'time_stamp_newest'");
	    if (movemode) FatalError (120,"Move mode is not possible for 'time_stamp_newest'");
	    if (!GetArch()) FatalError(120,"no archive specified");
	    while (ReadAtom()){
	       GetArch();
	    }
	    DB_LF();
	    while (Arch("Time stamping")){
	       CHK();
	       InvHashC(); // Needed for each archive being processed!
	       SetArea (0);
	       TTP (pcArchivePath, 1);
	       maxdate=0;
	       maxtime=0;
	       ReadArchive (pcArchivePath);
	       CloseArchive();
	       int iHan = Open (pcArchivePath, MUST|RW|NOC);
	       CSB;
	       if (maxdate)
		  _dos_setftime (Han(iHan),maxdate,maxtime);
	       CSE;
	       Close (iHan);
	    }
	    ClearMask(NULL);
	 } else if (strcmp(atom,"$RED")==0){
	    r = GetOptions(4);
RED:
	    bDeleteMode=1;
	    if (!r) FatalError(120,"no archive specified");
	    if (movemode) FatalError (120,"Move mode is not possible for 'remove empty dirs'");
	    if (MODE.bDTT) FatalError (120,"Dynamic Time Travel is not possible for 'remove empty dirs'");
	    if (!GetArch()) FatalError(120,"no archive specified");
	    ClearMask(NULL);
	    DB_LF();
	    MODE.fInc=0;
	    while (Arch("Removing empty directories from")){
	       nobas=0;
	       nodel=0;
	       noadd=0;
	       noopt=0;
	       nounp=0;

	       CHK();
	       if (CONFIG.bRelia==2){
		  if (!Test (pcArchivePath)){
		     FatalError (200,"you should repair this archive with 'UC T'");
		  }
	       }
	       InvHashC(); // Needed for each archive being processed!
	       SetArea (0);
	       TTP (pcArchivePath, 1);
	       UpdateArchive (pcArchivePath);
	       if (nodel){
		  FatalError (140,"archive is protected against deletion");
	       }
	       if ((MODE.bDamageProof==2) && nounp){
		  FatalError (140,"archive is protected against the removal of damage protection");
	       }
	       CleanEmptyDirs();
	       CopyFiles(); // copy (old) files
	       CloseArchive();
	    }
	    ClearMask(NULL);
//	 } else if (strcmp(atom,"$AWP")==0){
//	    MODE.fNoPath=1;
//	    goto ADD;
	 } else if (strcmp(atom,"$DOS")==0){
	    strcpy (p,"");
	    while (ReadAtom()){
	       strcat (p,atom);
	       strcat (p," ");
	    }
	    Out (7,"\x7");
	    Out (3,"\x5\Executing DOS command: %s\x7\n\r", p);
	    ssystem (p);
	    GKeep();
	 } else if (strcmp(atom,"$EWP")==0){
	    MODE.fSubDirs=1;
	    MODE.fNoPath=1;
	    goto EXTRACT;
	 } else if (strcmp(atom,"$PRF")==0){
	    MODE.fPrf=1;
	    goto EXTRACT;
	 } else if (strcmp(atom,"$FRE")==0){
	    MODE.fFreshen=1;
	    goto ADD;
	 } else if (strcmp(atom,"$GLF")==0){
	    MODE.fGlf=1;
	    if (!ReadAtom()) FatalError (120,"no listfile specified");
	    strcpy (MODE.szListFile, atom);
	    goto LIST;
	 } else {
	    FatalError(120,"unknown command %s",atom);
	 }
	 break;
#endif
      case '~':
	 switch (atom[1]){
#ifdef UCPROX
	    case '!':
	       switch (atom[2]){
		  case 'S':
		     Out (7,"%lu",ser2int((DWORD)atol((char *)&(atom[3]))));
		     break;
		  case 'I':
		     Out (7,"%lu %u",int2ser((DWORD)atol((char *)&(atom[3]))), ser2pin (int2ser((DWORD)atol((char *)&(atom[3])))));
		     break;
	       }
	       break;
#endif
	    case '~':
	       CONFIG.dSerial = atol (atom+2);
	       UpdateCFG();
	       break;
	    case 'K':
	       CONFIG.fOut=4;
	       bDump=1;
	       if (!ReadAtom()) FatalError (120,"no path specified");
	       SKillPath (atom);
	       break;
	    case 'X':
	       CONFIG.fOut=4;
	       bDump=1;
	       if (!ReadAtom()) FatalError (120,"no archive specified");
	       strcpy (tmp, atom);
	       if (!ReadAtom()) FatalError (120,"no dumpfile specified");
//	       BoosterOn();
	       DumpTags (tmp, atom);
//	       BoosterOff();
	       break;
#ifndef UE2
	    case 'V':
	       CONFIG.fOut=4;
	       bDump=1;
	       if (!ReadAtom()) FatalError (120,"no file specified");
	       strcpy (tmp, atom);
	       sspec=tmp;
	       ViewFile (-1);
	       sspec=NULL;
	       break;
#endif
	    case 'M':
	       CONFIG.fOut=4;
	       bDump=1;
	       char c = GetKey();
	       switch (c){
		  case '0':
		  case '1':
		  case '2':
		  case '3':
		  case '4':
		  case '5':
		  case '6':
		  case '7':
		  case '8':
		  case '9':
		     doexit (c-'0');
		     break;
		  default:
		     doexit (255);
		     break;
	       }
	       break;
#ifndef UE2
	    case 'R':
	       CONFIG.fOut=4;
	       bDump=1;
	       if (!ReadAtom()) FatalError (120,"no archive specified");
	       strcpy (tmp, atom);
	       if (!ReadAtom()) FatalError (120,"no dumpfile specified");
//	       BoosterOn();
	       ReadTags (tmp, atom);
//	       BoosterOff();
	       break;
#endif
	    case 'D':
	       MODE.fSubDirs=1;
	       bDump=1;
	       CONFIG.fOut=4;
	       ClearMask (NULL);
	       if (!GetOptions(2)) FatalError(120,"no archive specified");
	       if (!GetArch()) FatalError(120,"no archive specified");
	       ClearMask(NULL);
	       if (ReadAtom()){
		  GetMask();
	       } else {
		  RClearMask ();
		  Anal ("*.*;*",1);
	       }
	       DB_LF();
	       while (Arch("")){
		  CHK();
		  InvHashC(); // Needed for each archive being processed!
		  SetArea (0);
		  TTP (pcArchivePath, 1);
		  ReadArchive (pcArchivePath);
		  BoosterOn();
		  VPTR walk = Mpath;
		  while (!IS_VNULL(walk)){
		     MarkUp (".\\",VNULL, walk, 0);
		     walk = ((MPATH *)V(walk))->vpNext;
		  }
		  BoosterOff();
		  ListArch(Mpath);
		  CloseArchive();
	       }
	       ClearMask(NULL);
	       break;
	    default:
	       FatalError(120,"unkown command ~%c",atom[1]);
	 }
	 break;
#ifndef UE2
      case 'M':
	 movemode=1;
         goto ADD;
      case 'F':
         MODE.fFreshen=1;
         if (!GetOptions()) FatalError(120,"no archive specified");
         goto AADD;
      case 'A':
ADD:
	 DB_TXT ("com","A");
	 if (MODE.fFreshen){
	    if (!GetOptions(4)) FatalError(120,"no archive specified");
	 } else {
	    if (!GetOptions()) FatalError(120,"no archive specified");
	 }
AADD:
	 if (MODE.bDTT) FatalError (120,"Dynamic Time Travel is not possible for 'add'");
	 ClearMask(NULL);
	 specmode=1;
	 if (!GetArch()) FatalError(120,"no archive specified");
	 specmode=0;
	 if (ReadAtom()){
	    GetMask();
	 }else
	    Anal("*.*",1);
	 DB_LF();
	 while (Arch("Adding files to")){
	    nobas=0;
	    nodel=0;
	    noadd=0;
	    noopt=0;
	    nounp=0;

	    CHK();
	    if (Exists(pcArchivePath))
	       TTP (pcArchivePath,1);
	    else
	       typ=0;
	    if ((typ!=0) && (CONFIG.bRelia==2)){
	       if (!Test (pcArchivePath)){
		  FatalError (200,"you should repair this archive with 'UC T'");
	       }
	    }
	    InvHashC(); // Needed for each archive being processed!
	    SetArea (0);
	    int f1=MODE.fInc;
	    int f2=f1;
	    if (MODE.fInc && typ==1)
	       IncUpdateArchive (pcArchivePath);
	    else {
	       f2 = 0;
	       MODE.fInc = 0;
	       if (typ==0)
		  CreateArchive (pcArchivePath);
	       else {
		  UpdateArchive (pcArchivePath);
		  if (nobas){
		     FatalError (140,"archive is protected against BASIC updates (use -I)");
		  }
	       }
	    }
	    if (noadd){
	       FatalError (140,"archive is protected against the addition of files");
	    }
	    if ((MODE.bDamageProof==2) && nounp){
	       FatalError (140,"archive is protected against the removal of damage protection");
	    }
	    BoosterOn();
	    VPTR walk = Mpath;
	    smskip=0;
	    VPTR ddst=VNULL;
	    if (szDestPath[0]=='#'){
	       if (strlen(szDestPath+1)){
		  if ((MODE.bMKDIR!=2) && !DirExists(szDestPath+1)){
		     Menu ("\x6\Create %s in %s ?",szDestPath+1,pcArchivePath);
		     Option ("",'Y',"es");
		     Option ("",'N',"o");
		     switch(Choice()){
			case 1:
			   break; // let StrToDir handle it
			case 2:
			   goto nonono;
		     }
		  }
	       }
	       while (!IS_VNULL(walk)){
		  strcpy (q,szDestPath+1);
		  strcpy (p,((MPATH*)V(walk))->pcTPath);
		  if (p[1]==':') strcpy (p,p+2);
		  if (p[0]=='\\') strcpy (p,p+1);
		  strcat (q,p);
		  strrep (q, "..\\", "\\");
		  strrep (q, ".\\", "\\");
		  strrep (q, "\\\\", "\\");
		  ddst=StrToDir (q);
		  MODE.fInc = f1;
		  ScanAdd (ddst, walk, 0);
		  walk = ((MPATH *)V(walk))->vpNext;
	       }
	    } else {
	       if (strlen(szDestPath)){
		  if ((MODE.bMKDIR!=2) && !DirExists(szDestPath)){
		     Menu ("\x6\Create %s in %s ?",szDestPath,pcArchivePath);
		     Option ("",'Y',"es");
		     Option ("",'N',"o");
		     switch(Choice()){
			case 1:
			   break; // let StrToDir handle it
			case 2:
			   goto nonono;
		     }
		  }
		  ddst = StrToDir (szDestPath);
	       }
	       while (!IS_VNULL(walk)){
		  MODE.fInc = f1;
		  ScanAdd (ddst, walk, 0);
		  walk = ((MPATH *)V(walk))->vpNext;
	       }
	    }

	    if (scan1) Out (2|8,"\n\r");
	    if (smskip) Out (3,"\x5\Smart skipping %s bytes\n\r",neat(smskip));
nonono:
	    MODE.fInc=f2;
	    BoosterOff();
	    AddAccess(MODE.fInc); // read & create needed masters
	    MODE.fInc=f1;
	    AddFiles(); // add new files
	    MODE.fInc=f2;
	    if (!MODE.fInc){
	       CopyFiles(); // copy (old) files
	    }
	    MODE.fInc=f1;
	    if (iVlab){
               BYTE tmp[20];
	       memcpy (tmp, getvol (iDrv), 11);
               if (tmp[0]){
                  memcpy (xTail[iArchArea].pbLabel, tmp, 11);
	          if (iDrv) {
		    Out (3,"\x7Reading volume label \"%.11s\" from drive %c:\n\r",xTail[iArchArea].pbLabel,iDrv+'A'-1);
	          } else {
		    Out (3,"\x7Reading volume label \"%.11s\" from current drive\n\r",xTail[iArchArea].pbLabel);
                  }
               } else {
	          if (iDrv) {
		    Out (3,"\x7""Drive %c: does not contain a volume label\n\r",iDrv+'A'-1);
	          } else {
		    Out (3,"\x7""The current drive does not contain a volume label\n\r");
                  }
               }

	    }
	    CloseArchive();
	    if (iTsn){
	       maxdate=0;
	       maxtime=0;
	       ReadArchive (pcArchivePath);
	       CloseArchive();
	       int iHan = Open (pcArchivePath, MUST|RW|NOC);
	       CSB;
	       if (maxdate)
		  _dos_setftime (Han(iHan),maxdate,maxtime);
	       CSE;
	       Close (iHan);
	    }
	    DumpProbs();

	 }
	 if (MODE.fRab)
	    RabDelete();
	 else
	    RabClear();
	 if (movemode)
	    MoveDelete();
	 else
	    MoveClear();
	 ClearMask(NULL);
	 break;
      case 'D':
         delscan=1;
	 bDeleteMode=1;
	 if (!GetOptions()) FatalError(120,"no archive specified");
	 if (movemode) FatalError (120,"Move mode is not possible for 'delete'");
	 if (MODE.bDTT) FatalError (120,"Dynamic Time Travel is not possible for 'delete'");
	 if (!GetArch()) FatalError(120,"no archive specified");
	 ClearMask(NULL);
	 if (ReadAtom()){
	    GetMask();
	 } else {
            FatalError (120, "to delete all files *.* is needed");
         }
         delscan=0;
	 DB_LF();
	 MODE.fInc=0;
	 while (Arch("Deleting files from")){
	    nobas=0;
	    nodel=0;
	    noadd=0;
	    noopt=0;
	    nounp=0;

	    CHK();
	    if (CONFIG.bRelia==2){
	       if (!Test (pcArchivePath)){
		  FatalError (200,"you should repair this archive with 'UC T'");
	       }
	    }
	    InvHashC(); // Needed for each archive being processed!
	    SetArea (0);
	    TTP (pcArchivePath, 1);
	    UpdateArchive (pcArchivePath);
	    if (nodel){
	       FatalError (140,"archive is protected against file deletion");
	    }
	    if ((MODE.bDamageProof==2) && nounp){
	       FatalError (140,"archive is protected against the removal of damage protection");
	    }
	    BoosterOn();
	    VPTR walk = Mpath;
	    while (!IS_VNULL(walk)){
	       MarkUp (".\\",VNULL, walk, 0);
	       walk = ((MPATH *)V(walk))->vpNext;
	    }
	    BoosterOff();
	    if (MODE.fContains) AddAccess(0); // read needed masters
	    if (MODE.fContains) CheckFiles();
	    if (MODE.fQuery) QueryFiles ("Delete");
	    if (MODE.fContains || MODE.fQuery) ProcessFiles();
	    CopyFiles(); // copy (old) files
	    CloseArchive();
	    if (iTsn){
	       maxdate=0;
	       maxtime=0;
	       ReadArchive (pcArchivePath);
	       CloseArchive();
	       int iHan = Open (pcArchivePath, MUST|RW|NOC);
	       CSB;
	       if (maxdate)
		  _dos_setftime (Han(iHan),maxdate,maxtime);
	       CSE;
	       Close (iHan);
	    }
	    DumpProbs();
	 }
	 ClearMask(NULL);
	 break;
#endif
      case 'X':
      case 'E':
EXTRACT:
ue2_extract:
	 bExtractMode=1;
	 DB_TXT ("com","X");
#ifndef UE2
	 if (MODE.fNoPath || MODE.fPrf){
	    if (!GetOptions(4)) FatalError(120,"no archive specified");
	 } else {
	    if (!GetOptions()) FatalError(120,"no archive specified");
	 }
	 if (movemode && MODE.bDTT) FatalError (120,"Dynamic Time Travel and Move mode cannot be used together");
	 if (!GetArch()) FatalError(120,"no archive specified");
	 ClearMask(NULL);
	 if (ReadAtom()){
	    GetMask();
	 }else
	    Anal("*.*",1);
	 DB_LF();
#else
	 MODE.fSubDirs=1;
	 ClearMask(NULL);
	 Anal("*.*",1);
#endif
#ifdef UE2
	 {
	    char drive[MAXDRIVE];
	    char dir[MAXDIR];
	    char file[MAXFILE];
	    char ext[MAXEXT];
	    if (atom[strlen(atom)-1]!='.'){
	       fnsplit (atom,drive,dir,file,ext);
	       if (ext[0]==0 || ext[1]==0) {
                  if (stricmp (getenv("UC2_PUC"),"ON")==0)
		     fnmerge (atom,drive,dir,file,".PU2");
                  else
		     fnmerge (atom,drive,dir,file,".UC2");
	       }
	    }
	    strcpy (pcArchivePath, atom);
#else
	 while (Arch("Extracting files from")){
            nodel=0;
#endif
	    CHK();
	    InvHashC(); // Needed for each archive being processed!
	    SetArea (0);
	    TTP (pcArchivePath, 1);
	    if (movemode){
	       UpdateArchive (pcArchivePath);
               if (nodel){
	           FatalError (140,"archive is protected against file deletion");
	       }
	    } else {
	       ReadArchive (pcArchivePath);
	    }
#ifndef UE2
	    if (somban){
	       switch (CONFIG.fMul){
		  case 0:
		     if (MODE.fForce) goto leave;
		     Menu ("\x6Show/play multimedia banners?");
		     Option ("",'Y',"es");
		     Option ("",'N',"o");
		     switch(Choice()){
			case 1:
			   goto leave;
			case 2:
			   somban=0;
			   break;
		     }
		     break;
		  case 1:
		     break;
		  case 2:
		     somban=0;
		     break;
	       }
	    }
leave:
	    if (somban && !iFilterMode){
	       if (Exists("U$~BAN.GIF") ||
		   Exists("U$~BAN.JPG") ||
		   Exists("U$~BAN.ASK") ||
		   Exists("U$~BAN.TXT") ||
		   Exists("U$~BAN.MOD"))
	       {
		  FatalError (155,"cannot show banners if U$~BAN.* files are present in the current directory");
	       }
	       VPTR s1=Mpath, s2=Uqueue;
	       Mpath=VNULL;
	       Uqueue=VNULL;

	       // clean destination path (if any)
	       char c=szDestPath[0];
	       szDestPath[0]=0;

	       char fqb = MODE.fQuery;
	       MODE.fQuery = 0;

	       Anal ("U$~BAN.GIF",1);
	       Anal ("U$~BAN.MOD",1);
	       Anal ("U$~BAN.ASK",1);
	       Anal ("U$~BAN.JPG",1);
	       Anal ("U$~BAN.TXT",1);

	       BoosterOn();
	       VPTR walk = Mpath;
	       dwMyCtr=0;
	       while (!IS_VNULL(walk)){
		  MarkUp (".\\",VNULL, walk, 0);
		  walk = ((MPATH *)V(walk))->vpNext;
	       }

	       BoosterOff();

	       if (Exists("U$~BAN.GIF")) Delete ("U$~BAN.GIF");
	       if (Exists("U$~BAN.JPG")) Delete ("U$~BAN.JPG");
	       if (Exists("U$~BAN.ASK")) Delete ("U$~BAN.ASK");
	       if (Exists("U$~BAN.TXT")) Delete ("U$~BAN.TXT");
	       if (Exists("U$~BAN.MOD")) Delete ("U$~BAN.MOD");

	       AddAccess(0); // read needed masters

	       int t=movemode;
	       movemode=0;
	       ExtractFiles();
	       movemode=t;

	       MODE.fQuery = fqb;

	       // restore destination path
	       szDestPath[0]=c;

	       Out (7,"\x7\ษอออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออป\n\r");
	       ssystem (LocateF("U2_SHOW.BAT",2));
	       Out (7,"\x7\ศอออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออผ\n\r");

	       if (Exists("U$~BAN.GIF")) Delete ("U$~BAN.GIF");
	       if (Exists("U$~BAN.JPG")) Delete ("U$~BAN.JPG");
	       if (Exists("U$~BAN.TXT")) Delete ("U$~BAN.TXT");
	       if (Exists("U$~BAN.MOD")) Delete ("U$~BAN.MOD");
	       if (Exists("U$~BAN.ASK")){
		   Delete ("U$~BAN.ASK");
		   if (!MODE.fForce){
		      Menu ("\x6\Continue?");
		      Option ("",'Y',"es");
		      Option ("",'N',"o");
		      switch(Choice()){
			 case 1:
			    break;
			 case 2:
			    FatalError (100,"user discontinued command");
			    break;
		      }
		   }
	       }
	       RClearMask ();
	       Mpath=s1;
	       Uqueue=s2;
	    }
#endif

	    BoosterOn();

	    if (iFilterMode){
	       ApplyFilter (pcFilter);
	    }

	    VPTR walk = Mpath;
	    dwMyCtr=0;
	    while (!IS_VNULL(walk)){
	       if (strlen(szDestPath)){
		  if (szDestPath[0]=='#'){
		     strcpy (p, szDestPath+1);
		     if (szDestPath[2]==0)
			p[0]=0;
		     if ((((MPATH*)V(walk))->pcTPath[0])=='\\')
			strcat (p, ((MPATH*)V(walk))->pcTPath+1);
		     else
			strcat (p, ((MPATH*)V(walk))->pcTPath);
		     Out (7,"[%s]",p); // qqq
		     MarkUp (p, VNULL, walk, 0);
		  } else {
		     MarkUp (szDestPath, VNULL, walk, 0);
		  }
	       } else
		  MarkUp (".\\",VNULL, walk, 0);
	       walk = ((MPATH *)V(walk))->vpNext;
	    }
	    BoosterOff();
	    if (dwTSize<(dwMyCtr*2)) CacheRT (iArchInHandle[iArchArea]);
	    AddAccess(0); // read needed masters
	    if (MODE.fContains) CheckFiles();
	    ExtractFiles();
	    if (movemode){
	       Out (3,"\x7Moving files ");
	       StartProgress (-1, 3);
	       ProcessFiles ();
	       EndProgress ();
               Out (3,"\n\r");
	       CopyFiles ();
	    }
	    if (!movemode && DetectUseal()){
	       Out (7,"\n\r\x5\This archive contains UltraSeals which can be checked with USAFE\n\r");
	    }
	    if (iVlab){
	       if (!xTail[iArchArea].pbLabel[0]){
		  Out (3,"\x7The archive does not contain a volume label\n\r");
	       } else {
                  if (iDrv)
                     Out (3,"\x7Writing volume label \"%.11s\" to drive %c:\n\r",xTail[iArchArea].pbLabel,iDrv+'A'-1);
                  else
                     Out (3,"\x7Writing volume label \"%.11s\" to current drive\n\r",xTail[iArchArea].pbLabel);
		  setvol (iDrv, (char *)xTail[iArchArea].pbLabel);
	          if (memcmp (xTail[iArchArea].pbLabel, getvol (iDrv), 11)!=0)
		     Error (70,"failed to set volume label to \"%.11s\"", xTail[iArchArea].pbLabel);
               }
	    }
	    CloseArchive();
	    DumpProbs();
	 }
	 ClearMask(NULL);
	 break;
      case 'V':
	 DB_TXT ("com","V");
	 bVerbose=1;
	 bDump=0;
	 specialanal=1;
	 goto listing;
      case 'L':
ue2_list:
LIST:
	 DB_TXT ("com","L");
	 bVerbose=0;
listing:
	 ClearMask(NULL);
#ifndef UE2
	 if (MODE.fGlf){
	    if (!GetOptions(-1)) FatalError(120,"no archive specified");
//	    Out (7,"[LISTFILE= %s]",MODE.szListFile);
	 } else {
	    if (!GetOptions()) FatalError(120,"no archive specified");
	 }
	 if (movemode) FatalError (120,"Move mode is not possible for 'list'");
	 if (!GetArch()) FatalError(120,"no archive specified");
	 mark=1;
	 ClearMask(NULL);
	 if (ReadAtom()){
	    GetMask();
	 }else{
	    Anal("*.*",1);
	 }
	 specialanal=0;
	 mark=0;
#else
	 MODE.fSubDirs=1;
	 mark=1;
	 ClearMask(NULL);
	 Anal("*.*",1);
	 specialanal=0;
	 mark=0;
#endif
	 DB_LF();
#ifdef UE2
	 {
	    char drive[MAXDRIVE];
	    char dir[MAXDIR];
	    char file[MAXFILE];
	    char ext[MAXEXT];
	    if (atom[strlen(atom)-1]!='.'){
	       fnsplit (atom,drive,dir,file,ext);
	       if (ext[0]==0 || ext[1]==0) {
                  if (stricmp (getenv("UC2_PUC"),"ON")==0)
   		     fnmerge (atom,drive,dir,file,".PU2");
                  else
		     fnmerge (atom,drive,dir,file,".UC2");
	       }
	    }
	    strcpy (pcArchivePath, atom);
#else
	 while (Arch("Listing files from")){
#endif
	    CHK();
	    InvHashC(); // Needed for each archive being processed!
	    SetArea (0);
	    TTP (pcArchivePath, 1);
	    ReadArchive (pcArchivePath);
	    BoosterOn();

	    if (iFilterMode){
	       ApplyFilter (pcFilter);
	    }

	    VPTR walk = Mpath;
	    while (!IS_VNULL(walk)){
	       MarkUp (".\\",VNULL, walk, 0);
	       walk = ((MPATH *)V(walk))->vpNext;
	    }
	    BoosterOff();
	    if (MODE.fContains) AddAccess(0); // read needed masters
	    if (MODE.fContains) CheckFiles();
	    if (MODE.fQuery) QueryFiles("Include");
	    ListArch(Mpath);
	    if (DetectUseal()){
	       Out (7,"\n\r\x5\This archive contains UltraSeals which can be checked with USAFE\n\r");
	    }
	    CloseArchive();
	    DumpProbs();
	 }
	 ClearMask(NULL);
	 break;
#ifndef UE2
      case 'P':
	 MODE.fInc=1; // incremental mode
	 if (!GetOptions()) FatalError(120,"no archive specified");
	 if (MODE.bDTT) FatalError(120,"Dynamic Time Travel is not possible for 'protect'");
	 if (movemode) FatalError (120,"Move mode is not possible for 'protect'");
         if (MODE.fSubDirs)
            {
               MODE.fASubDirs=1;
            }
	 MODE.bDamageProof=1; // damage protect file
	 if (!GetArch()) FatalError(120,"no archive specified");
	 while (ReadAtom()){
	    GetArch();
	 }
	 DB_LF();
	 while (Arch("Damage protecting")){
	    CHK();
	    if (CONFIG.bRelia==2){
	       if (!Test (pcArchivePath)){
		  FatalError (200,"you should repair this archive with 'UC T'");
	       }
	    }
	    InvHashC(); // Needed for each archive being processed!
	    SetArea (0);
	    TTP (pcArchivePath, 1);
	    IncUpdateArchive (pcArchivePath);
	    CloseArchive();
	 }
	 ClearMask(NULL);
	 break;
      case 'U':
	 MODE.fInc=1; // incremental mode
	 if (!GetOptions()) FatalError(120,"no archive specified");
	 if (CONFIG.bRelia) FatalError (123,"configuration does not allow removal of damage protection (UC -! C)");
	 if (MODE.bDTT) FatalError (120,"Dynamic Time Travel is not possible for 'unprotect'");
	 if (movemode) FatalError (120,"Move mode is not possible for 'unprotect'");
	 MODE.bDamageProof=2; // damage unprotect file
         if (MODE.fSubDirs)
            {
               MODE.fASubDirs=1;
            }
	 if (!GetArch()) FatalError(120,"no archive specified");
	 while (ReadAtom()){
	    GetArch();
	 }
	 DB_LF();
	 while (Arch("Removing damage protection from")){
	    nobas=0;
	    nodel=0;
	    noadd=0;
	    noopt=0;
	    nounp=0;

	    CHK();
	    if (CONFIG.bRelia==2){ // for consistency, is never called
	       if (!Test (pcArchivePath)){
		  FatalError (200,"you should repair this archive with 'UC T'");
	       }
	    }
	    InvHashC(); // Needed for each archive being processed!
	    SetArea (0);
	    TTP (pcArchivePath, 1);
	    IncUpdateArchive (pcArchivePath);
	    if (nounp){
	       FatalError (140,"archive is protected against the removal of damage protection");
	    }
	    CloseArchive();
	 }
	 ClearMask(NULL);
	 break;
      case 'R':
	 if (!GetOptions()) FatalError(120,"no archive specified");
	 if (MODE.bDTT) FatalError (120,"Dynamic Time Travel is not possible for 'revise'");
	 if (movemode) FatalError (120,"Move mode is not possible for 'revise'");
	 MODE.fInc=0;
         if (MODE.fSubDirs)
            {
               MODE.fASubDirs=1;
            }
	 if (!GetArch()) FatalError(120,"no archive specified");
	 while (ReadAtom()){
	    GetArch();
	 }
	 DB_LF();
	 while (Arch("Revise comment from")){
	    nobas=0;
	    nodel=0;
	    noadd=0;
	    noopt=0;
	    nounp=0;
	    norev=0;

	    CHK();
	    if (CONFIG.bRelia==2){
	       if (!Test (pcArchivePath)){
		  FatalError (200,"you should repair this archive with 'UC T'");
	       }
	    }
	    if (Exists ("U$~COMM.TXT")){
	       FatalError (157,"cannot revise comment if U$~COMM.TXT is present in the current directory");
	    }
	    bExtractMode=1;
	    MODE.fSubDirs=0;
	    MODE.bAddOpt = 2;
	    MODE.bExOverOpt = 2;
	    MODE.bMKDIR = 2;
	    SetArea (0);
	    TTP (pcArchivePath, 1);

	    ReadArchive (pcArchivePath);
	    if ((MODE.bDamageProof==2) && nounp){
	       FatalError (140,"archive is protected against the removal of damage protection");
	    }
	    RClearMask();
	    InvHashC(); // Needed for each archive being processed!
	    SetArea (0);
	    char com[260];
	    sprintf (com,"U$~COMM.TXT");
	    Anal (com, 1);
	    BoosterOn();
	    VPTR walk = Mpath;
	    while (!IS_VNULL(walk)){
	       MarkUp (".\\",VNULL, walk, 0);
	       walk = ((MPATH *)V(walk))->vpNext;
	    }
	    BoosterOff();
	    AddAccess(0); // read needed masters
	    ExtractFiles();
	    CloseArchive();
	    if (!Exists("U$~COMM.TXT")){
	       int i=Open ("U$~COMM.TXT", MUST|CR|NOC);
	       Close (i);
	    }
	    sprintf (com,"%s U$~COMM.TXT",LocateF("U2_EDIT.BAT",2));
//	    Out (7,"\x7\ษอออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออป\n\r");
	    ssystem (com);
//	    Out (7,"\x7\ศอออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออผ\n\r");

	    if (norev){
	       Warning (7,"archive is protected against changes in comment");
	    } else {
	       RClearMask();
	       Anal ("U$~COMM.TXT", 1);

	       CHK();
	       TTP (pcArchivePath,1);
	       if (CONFIG.bRelia==2){
		  if (!Test (pcArchivePath)){
		     FatalError (200,"you should repair this archive with 'UC T'");
		  }
	       }
	       InvHashC(); // Needed for each archive being processed!
	       SetArea (0);

	       UpdateArchive (pcArchivePath);
	       BoosterOn();
	       walk = Mpath;
	       VPTR ddst=VNULL;
	       while (!IS_VNULL(walk)){
		  ScanAdd (ddst, walk, 0);
		  walk = ((MPATH *)V(walk))->vpNext;
	       }
	       if (scan1) Out (2|8,"\n\r");
	       BoosterOff();
	       AddAccess(MODE.fInc); // read & create needed masters
	       AddFiles(); // add new files
	       if (!MODE.fInc){
		  CopyFiles(); // copy (old) files
	       }
	       CloseArchive();
	    }

	    Delete ("U$~COMM.TXT");
	    if (Exists ("U$~COMM.BAK")) Delete ("U$~COMM.BAK");
	    if (Exists ("U$~COMM.BK!")) Delete ("U$~COMM.BK!");
	    if (Exists ("U$~COMM.~AK")) Delete ("U$~COMM.~AK");
	    if (Exists ("U$~COMM.~BA")) Delete ("U$~COMM.~BA");
	    if (Exists ("U$~COMM.BA~")) Delete ("U$~COMM.BA~");
	 }
	 ClearMask(NULL);
	 break;
      case 'T':
	 fContinue=1; // continue if errors are found
	 if (!GetOptions()) FatalError(120,"no archive specified");
         if (MODE.fSubDirs)
         {
            MODE.fASubDirs=1;
         }
	 if (MODE.bDTT) FatalError (120,"Dynamic Time Travel is not possible for 'test&repair'");
	 if (movemode) FatalError (120,"Move mode is not possible for 'test&repair'");
	 MODE.bDamageProof = 1;
	 if (!GetArch()) FatalError(120,"no archive specified");
	 while (ReadAtom()){
	    GetArch();
	 }
	 DB_LF();
	 while (Arch("Testing")){
	    NewFix();
	    if (!MODE.fNoLock && Exists(pcLckPath)){
	       FatalError (135,"archive lock file %s found",pcLckPath);
	    }
	    if (Exists(pcRecPath)){
	       if (MODE.fForce) goto repairit;
	       Out (7,"\x8\A damage recover file has been found\n\r");
	       Menu ("\x6Repair archive?");
	       Option ("",'Y',"es");
	       Option ("",'N',"o");
	       switch(Choice()){
		  case 1:
repairit:
		     Out (7,"\x5\Archive will be repaired with the crash recover file %s\n\r",pcRecPath);
		     Out (7,"\x7\Creating archive %s\n\r",Fix);
		     Recover (pcArchivePath, pcRecPath, Fix);
		     break;
		  case 2:
		     FatalError (100,"user did not allow archive repair");
		     break;
	       }
	    } else {
	       int fixlevel=0;
	       InvHashC(); // Needed for each archive being processed!
	       SetArea (0);
	       iVerifyMode=1;
	       MODE.fSubDirs=1;
	       probeo=0;
	       switch (TType (pcArchivePath,NULL,0)){
		  case 4:
		     FatalError (125,"file is encrypted with UltraCrypt");
		     break;
		  case 2:
		  case 3:
		     Error (90,"file does not have a valid UC2 header");
		     probeo=1;
		     break;
		  case 0:
		     TTP (pcArchivePath,1);
		     break;
	       }
	       ReadArchive (pcArchivePath);
	       if (bPDamp[iArchArea]!=0){
		  if (!VerifyDP()){
		     Error (90,"archive is damaged");
		     if (!MODE.fForce){
			Menu ("\x6Repair archive?");
			Option ("",'Y',"es");
			Option ("",'N',"o");
			switch(Choice()){
			   case 1:
			      break;
			   case 2:
			      FatalError (100,"user did not allow archive repair");
			      break;
			}
		     }
		     fixlevel=1;
		     Out (7,"\x7\Creating archive %s\n\r",Fix);
		     if (RepairDP (Fix)){
			Out (7,"\x5\Archive has been repaired (using damage protection)");
		     } else {
			Error (90,"failed to repair archive (using damage protection)");
		     }
		     CloseArchive ();
		     InvHashC(); // Needed for each archive being processed!
		     Out (3,"\n\r\x7\Testing/repairing %s\n\r",Fix);
		     probeo=0;
		     ReadArchive (Fix);
		  }
	       } else {
		  Out (3,"\x7\Archive is not damage protected\n\r");
	       }
	       ClearMask (NULL);
	       bExtractMode=1;
	       Anal("*.*;*",1);
	       BoosterOn();
	       {
		  VPTR walk = Mpath;
		  while (!IS_VNULL(walk)){
		     MarkUp (".\\",VNULL, walk, 0);
		     walk = ((MPATH *)V(walk))->vpNext;
		  }
	       }
	       BoosterOff();
	       AddAccess(0); // read needed masters
	       ExtractFiles();
	       CloseArchive();
	       if (probeo){
		  iVerifyMode=0;
		  if (fixlevel){
		     Convert (Fix, 2);
		     scan1=0;
		     Out (7,"\x8\ MESSAGE: some files might still be damaged\n\r");
		     ErrorLog (" MESSAGE: some files might still be damaged");
		  } else {
		     if (!MODE.fForce){
			Menu ("\x6Repair archive?");
			Option ("",'Y',"es");
			Option ("",'N',"o");
			switch(Choice()){
			   case 1:
			      break;
			   case 2:
			      FatalError (100,"user did not allow archive repair");
			      break;
			}
		     }
		     CopyFile (pcArchivePath, Fix);
		     Convert (Fix, 2);
		     scan1=0;
		     Out (7,"\x8\ MESSAGE: some files might be damaged\n\r");
		     ErrorLog (" MESSAGE: some files might be damaged");
		  }
	       } else {
		  if (fixlevel){
		     Out (7,"\x5\ MESSAGE: all files have been restored 100%%\n\r");
		     Out (7,"          (All found (and recovered) errors are reported in UC2_ERR.LOG)\n\r");
		     ErrorLog (" MESSAGE: all files have been restored 100%% (into %s)",Fix);
		  }
	       }
	       if (fixlevel || probeo){
		  Out (7,"\x8 MESSAGE: original archive is not touched (beware of bad sectors!)\n\r");
		  Out (7,"          (%s contains a repaired 'copy' of %s)\n\r",Fix,pcArchivePath);
		  ErrorLog (" MESSAGE: original archive is not touched (beware of bad sectors!)");
		  ErrorLog ("          (%s contains a repaired 'copy' of %s)",Fix,pcArchivePath);
	       }
	    }
	 }
	 ClearMask(NULL);
	 fContinue=0;
	 break;
      case 'C':
	 if (!GetOptions()) FatalError(120,"no archive specified");
         if (MODE.fSubDirs)
         {
            MODE.fASubDirs=1;
         }
	 if (MODE.bDTT) FatalError (120,"Dynamic Time Travel is not possible for 'convert'");
	 if (movemode) FatalError (120,"Move mode is not possible for 'convert'");
	 convert=1;
	 if (!GetArch()) FatalError(120,"no archive specified");
	 convert=0;
	 while (ReadAtom()){
	    GetArch();
	 }
	 DB_LF();
         if (MODE.fBak){
            convback=1;
            MODE.fBak=0;
         } else
            convback=0;
	 while (Arch("Converting")){
	    CHK();
	    conto=1;
	    Convert (pcArchivePath, 0);
	    conto=0;
	    if (iTsn){
	       maxdate=0;
	       maxtime=0;
	       ReadArchive (NewExt(pcArchivePath));
	       CloseArchive();
	       int iHan = Open (NewExt(pcArchivePath), MUST|RW|NOC);
	       CSB;
	       if (maxdate)
		  _dos_setftime (Han(iHan),maxdate,maxtime);
	       CSE;
	       Close (iHan);
	    }
	 }
	 ClearMask(NULL);
	 break;
      case 'O':
	 MODE.bHid=2;
	 Compressor (4); // default TIGHT
	 if (!GetOptions()) FatalError(120,"no archive specified");
	 if (MODE.bDTT) FatalError (120,"Dynamic Time Travel is not possible for 'optimize'");
	 if (movemode) FatalError (120,"Move mode is not possible for 'optimize'");
         if (MODE.fSubDirs)
            {
               MODE.fASubDirs=1;
            }
	 if (!GetArch()) FatalError(120,"no archive specified");
	 while (ReadAtom()){
	    GetArch();
	 }
	 DB_LF();
	 while (Arch("Optimizing")){
	    CHK();
	    Convert (pcArchivePath, 1);
	 }
	 ClearMask(NULL);
	 break;
#endif
      default:
	 FatalError(120,"unknown command %c",atom[0]);
   }
#ifndef UE2
   goto again;
#endif
}
