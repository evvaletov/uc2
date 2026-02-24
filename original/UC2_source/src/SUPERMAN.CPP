// Copyright 1992, all rights reserved, AIP, Nico de Vries
// SUPERMAN.CPP

#include <mem.h>
#include <dos.h>
#include <stdio.h>
#include <string.h>
#include <dir.h>
#include <stdlib.h>
#include <alloc.h>

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

static VPTR dnRoot [MAX_AREA];    // root of internal tree
static BYTE fRoot [MAX_AREA];     // flag if dbRoot has been allocated
XHEAD xhead [MAX_AREA];    // XHEAD of active archive
static VPTR vpHomeDir [MAX_AREA]; // current directory
static DWORD dwDirNTX [MAX_AREA]; // next new directory index
static DWORD dwMasNTX [MAX_AREA]; // next new master index
static BYTE bReadOnly [MAX_AREA]; // archive read only ?
static BYTE bRecFile [MAX_AREA];  // ultra recover file present?
XTAIL xTail [MAX_AREA];    // archive extended tail

DWORD dwTotalSize;

extern int os2;

extern int cdecl _required;

/*** clean root of internal tree ***/

void UpdateVersion (void){
#ifndef UE2
   if (xhead[iArchArea].wVersionMadeBy<200)
      xhead[iArchArea].wVersionMadeBy = 202;
   if (xhead[iArchArea].wVersionNeededToExtract<200)
      xhead[iArchArea].wVersionNeededToExtract = 200;
   if (stricmp (getenv("UC2_PUC"),"ON")==0)
      if (xhead[iArchArea].wVersionNeededToExtract<202)
         xhead[iArchArea].wVersionNeededToExtract = 202;
#ifdef UCPROX
   if (getenv("UC2_REV"))
      xhead[iArchArea].wVersionNeededToExtract = atoi (getenv("UC2_REV"));
#endif
#endif
}

void CleanRoot (){ // A:2 OK
   if (!fRoot[iArchArea]){
      dnRoot[iArchArea] = Vmalloc (sizeof(DIRNODE));
      Vforever();
      fRoot[iArchArea] = 0;
      ((DIRNODE *)V(dnRoot[iArchArea]))->dirmeta.dwIndex = 0;
   }
   DIRNODE *dn = (DIRNODE*)V(dnRoot[iArchArea]);

   dn->vpLeft    = VNULL;
   dn->vpRight   = VNULL;
   dn->vpFiles   = VNULL;
   dn->vpChilds  = VNULL;
   dn->vpParent  = VNULL;
   dn->vpCurDir  = VNULL; // root
   dn->vpCurFile = VNULL; // none left

   vpHomeDir[iArchArea] = dnRoot[iArchArea]; // CD \

   dwDirNTX[iArchArea]=1;
   dwMasNTX[iArchArea]=1;
}

/*** archive access ***/

void SuperGet (void);

void ReadArchive (char *pcPath){
   TRACEM("Enter ReadArchive");
   bRecFile[iArchArea]=0;
   OpenNeuro();
   CleanRoot();
   TRACEM("Check-1 ReadArchive");
   if (SetArchive (pcPath, 0)){ // read-only
      ARSeek (1, sizeof(FHEAD));
      ARead ((BYTE *)&xhead[iArchArea], sizeof(XHEAD));
      SuperGet();
   }
   bReadOnly[iArchArea]=1;
   TRACEM("Leave ReadArchive");
}

void UpdateArchive (char *pcPath){
#ifndef UE2
   bRecFile[iArchArea]=0;
   OpenNeuro();
   CleanRoot();
   SetArchive (pcPath, 1);
   ARSeek (1, sizeof(FHEAD));
   ARead ((BYTE *)&xhead[iArchArea], sizeof(XHEAD));
   SuperGet();
   AWSeek (1, sizeof(XHEAD)+sizeof(FHEAD));
   bReadOnly[iArchArea]=0;
#endif
}

void CreateArchive (char *pcPath){
#ifndef UE2
   bRecFile[iArchArea]=0;
   OpenNeuro();
   CleanRoot();
   SetArchive (pcPath, 3);
   xhead[iArchArea].fBusy=0;
   AWSeek (1, sizeof(FHEAD));
   AWrite ((BYTE *)&xhead[iArchArea], sizeof(XHEAD));
   AWSeek (1, sizeof(XHEAD)+sizeof(FHEAD));
   bReadOnly[iArchArea]=0;
#endif
}

extern int iArchOutHandle[MAX_AREA];

DWORD AWLen (void);
extern char pcRecPath[260];
extern char pcArchivePath[260];

#pragma argsused
void Recover (char *old, char *rec, char *nw){
#ifndef UE2
   if (0!=strcmp(old,nw)) CopyFile (old, nw);

   BYTE *pbBuf = xmalloc (16384, STMP);

   int a=Open (nw, MUST|RW|NOC);
   int r=Open (rec, MUST|RO|NOC);

   Seek (r, 0);
   Read (pbBuf, r, 4);
   pbBuf[4]=0;
   if (strcmp((char*)pbBuf,"UR2!")!=0){
      if (0!=strcmp(old,nw)) Delete(nw);
      FatalError (195,"%s is not an UltraRecover II file, please remove it",pcRecPath);
   }

   // restore file header
   Seek (a, 0);
   Read (pbBuf, r, sizeof(FHEAD)+sizeof(XHEAD));
   Write (pbBuf, a, sizeof(FHEAD)+sizeof(XHEAD));

   // find out restoration meta information
   DWORD from;
   DWORD len;
   Read ((BYTE*)&from,r,4);
   Read ((BYTE*)&len,r,4);

   // restore file tail
   SetFileSize (a, from+1);
   Seek (a, from);
   DWORD mov;
   while (len){
      mov=16384;
      if (mov>len) mov=len;
      Read (pbBuf, r, (WORD)mov);
      Write (pbBuf, a, (WORD)mov);
      len-=mov;
   }

   Close (a);
   Close (r);
   xfree (pbBuf, STMP);
#endif
}

extern int convertmode;
int recinst=0;
int recneeded=0;
extern int cerro;

void RecIt (void){
#ifndef UE2
   if (!convertmode){
      if (recneeded){
	 if (cerro){
	    Out (7,"\x8\A UltraRecover file is present\n\r");
	    Out (7,"\x8\Reconstruct the archive with 'UC T'\n\r");
	 } else {
	    Recover (pcArchivePath,pcRecPath,pcArchivePath);
	    Delete (pcRecPath);
	 }
      }
   } else {
      if (recneeded & !cerro)
	 Delete (pcRecPath);
   }
#endif
}
int fRecIt=0;

#pragma argsused
void IncUpdateArchive (char *pcPath){
#ifndef UE2
   if (!recinst) {
      fRecIt=1;
      recinst=1;
   }
   bRecFile[iArchArea]=0;
   OpenNeuro();
   CleanRoot();
   if (SetArchive (pcPath, 2)){ // incremental update
      char drive[MAXDRIVE];
      char dir[MAXDIR];
      char file[MAXFILE];
      char ext[MAXEXT];
      fnsplit (pcPath,drive,dir,file,ext);
      fnmerge (pcRecPath,drive,dir,file,".UR2");
      bRecFile[iArchArea]=1;
      ARSeek (1, sizeof(FHEAD));
      ARead ((BYTE *)&xhead[iArchArea], sizeof(XHEAD));
      xhead[iArchArea].fBusy=0;
      AWSeek (1, sizeof(FHEAD));
      AWrite ((BYTE *)&xhead[iArchArea], sizeof(XHEAD));
#ifdef UCPROX
      if (debug) Out (7,"<%ld:%ld>",xhead[iArchArea].locCdir.dwVolume,
			xhead[iArchArea].locCdir.dwOffset);
#endif
      SuperGet();
#ifdef UCPROX
      if (debug){
	 Out (7,"<%ld:%ld>",xhead[iArchArea].locCdir.dwVolume,
			xhead[iArchArea].locCdir.dwOffset);
	 Out (7,"<%ld>", Tell (iArchOutHandle[iArchArea]));
      }
#endif

      DWORD from=xhead[iArchArea].locCdir.dwOffset;
      DWORD len=AWLen()-from;
      if (strncmp(pcRecPath,"U$~",3)==0)
	 Out (3,"\x7\Creating internal crash recover file ");
      else
         Out (3,"\x7\Creating crash recover file %s ",pcRecPath);
      StartProgress (-1, 3);
      BYTE *pbBuf = xmalloc (16384, STMP);
      int o=Open(pcRecPath, MUST|CR|CWR);

      // header
      Write ((BYTE *)"UR2!",o,4);

      // copy of basic file header
      ARSeek (xhead[iArchArea].locCdir.dwVolume,0);
      ARead (pbBuf, sizeof(FHEAD)+sizeof(XHEAD));
      Write (pbBuf, o, sizeof(FHEAD)+sizeof(XHEAD));

      // length info
      Write ((BYTE*)&from,o,4);
      Write ((BYTE*)&len,o,4);

      // 'old' tail of file
      ARSeek (xhead[iArchArea].locCdir.dwVolume,
	      xhead[iArchArea].locCdir.dwOffset);
      DWORD mov;
      while (len){
	 mov=16384;
	 if (mov>len) mov=len;
	 ARead (pbBuf, (WORD)mov);
	 Write (pbBuf, o, (WORD)mov);
	 len-=mov;
	 Hint();
      }
      Close (o);
      Out (3|8,"\n\r");
      DFlush("secure storage of UltraRecover file");
      xfree (pbBuf, STMP);
      recneeded=1;


      AWSeek (xhead[iArchArea].locCdir.dwVolume,   // overwrite "OLD" cdir
	      xhead[iArchArea].locCdir.dwOffset);  // QQQ ???
#ifdef UCPROX
      if (debug) Out (7,"<%ld>", Tell (iArchOutHandle[iArchArea]));
#endif
      AWCut (); // cut off ALL extra information (e.g. dampro recs)
#ifdef UCPROX
      if (debug) Out (7,"<%ld>", Tell (iArchOutHandle[iArchArea]));
#endif
   } else { // archive is NEW, default create methods
      xhead[iArchArea].fBusy=0;
      AWSeek (1, sizeof(FHEAD));
      AWrite ((BYTE *)&xhead[iArchArea], sizeof(XHEAD));
//    AWSeek (1, sizeof(XHEAD)+sizeof(FHEAD));
   }
   bReadOnly[iArchArea]=0;
#endif
}

void CleanDir (VPTR dir);
void SuperDump (void);

void CloseArchive (){
   if (!bReadOnly[iArchArea]){
      SuperDump();
      xhead[iArchArea].fBusy=0;
      AWSeek (1,sizeof(FHEAD));
      AWrite ((BYTE *)&xhead[iArchArea], sizeof (XHEAD));
   }
   CloseArchiveFile();
   CleanDir (VNULL);
   CloseNeuro();
   if (bRecFile[iArchArea]){
      Delete (pcRecPath);
      recneeded=0;
   }
}

/*** tree cleanup ***/
// clean functions clean all children of a passed node
// xclean functions clean the node itsself as well

#pragma argsused
void XCleanTags (VPTR extnode){
   // QQQ to be build later
}

extern int turbo;

void XCleanRevs (VPTR revs){ //A:* OK
#ifndef UE2
   if (turbo) return;
   while (!IS_VNULL(revs)){
      XCleanTags (((REVNODE*)V(revs))->extnode);
      REVNODE *rn = (REVNODE*)V(revs);
      VPTR old = revs;
      revs = rn->vpOlder;
      Vfree (old);
   }
#endif
}


void CleanFileNN (VPTR filenn){ //A:1 OK
#ifndef UE2
   if (turbo) return;
   FILENAMENODE *fnn = (FILENAMENODE*)V(filenn);
   XCleanRevs (fnn->vpRevs);
#endif
}

// node is not changed at all but all children are removed
void CleanDir (VPTR dir){ // A:1 OK
#ifndef UE2
   if (turbo) return;
   TCD (dir);
   VPTR tmp, walk = TFirstDir ();
   while (!IS_VNULL(walk)){
      BrkQ();
      VPTR curr = TWD();
      CleanDir (walk);
      TCD(curr);
      tmp = walk;
      walk = TNextDir();
      Vfree (tmp);
   }
   walk = TFirstFileNN ();
   while (!IS_VNULL(walk)){
      BrkQ();
      CleanFileNN (walk);
      tmp = walk;
      walk = TNextFileNN();
      Vfree (tmp);
   }
#endif
}

/*** directory access ***/

int WalkUp (VPTR me){ // A:1 OK
   if (IS_VNULL(me))
      return 0;
   else
      return 1+WalkUp(((DIRNODE *)V(me))->vpParent);
}

void TCD (VPTR dir){
   if (IS_VNULL(dir)) dir = dnRoot[iArchArea];
   vpHomeDir[iArchArea] = dir;
}

static int TryDir (DWORD dwNumber){ // A:1 OK
   if (((DIRNODE*)V(vpHomeDir[iArchArea]))->dirmeta.dwIndex!=dwNumber){
      VPTR walk = TFirstDir();
      while (!IS_VNULL(walk)){
	 BrkQ();
	 TCD(walk);
	 if (TryDir(dwNumber)) return 1;
	 TUP();
	 walk = TNextDir();
      }
      return 0;
   } else
      return 1;
}

int TCDN (DWORD dwNumber){ // A:1 OK
   // do this FAST if there is no change of current directory!
   if (((DIRNODE*)V(vpHomeDir[iArchArea]))->dirmeta.dwIndex!=dwNumber){
      TCD (VNULL); // scan entier tree starting at its root
      return TryDir (dwNumber);
   }
   return 1;
}

void TUP (){ // A:1 OK
   if (IS_VNULL(vpHomeDir[iArchArea])) return;
   vpHomeDir[iArchArea] = ((DIRNODE *)V(vpHomeDir[iArchArea]))->vpParent;
}

VPTR TWD (){
   return vpHomeDir[iArchArea];
}

int  TDeep (){
   return WalkUp (vpHomeDir[iArchArea])-1;
}

VPTR TMLD (BYTE *pbName){ // A:* OK
   VPTR prev = VNULL;
   VPTR walk = TFirstDir();
   while (!IS_VNULL(walk)){
      BrkQ();
      if (0==memcmp (pbName,((DIRNODE*)V(walk))->osmeta.pbName,11))
	 return walk;
      prev = walk;
      walk = TNextDir();
   }
   VPTR tmp = Vmalloc (sizeof(DIRNODE));
   DIRNODE *pdnT = (DIRNODE *)V(tmp);
   pdnT->extnode   = VNULL;
   pdnT->vpLeft    = prev;
   pdnT->vpRight   = VNULL;
   pdnT->vpFiles   = VNULL;
   pdnT->vpChilds  = VNULL;
   pdnT->vpParent  = vpHomeDir[iArchArea];
   pdnT->vpCurDir  = VNULL; // none
   pdnT->vpCurFile = VNULL; // none
   pdnT->dirmeta.dwIndex = dwDirNTX[iArchArea]++;
   pdnT->osmeta.dwParent =
      ((DIRNODE *)V(vpHomeDir[iArchArea]))->dirmeta.dwIndex;

   pdnT->osmeta.wTime=0;
   pdnT->osmeta.wDate=0;
   pdnT->osmeta.bAttrib=0;

   memcpy (((DIRNODE*)V(tmp))->osmeta.pbName,pbName,11);
   if (IS_VNULL(prev)){
      ((DIRNODE *)V(vpHomeDir[iArchArea]))->vpChilds = tmp;
   } else {
      ((DIRNODE *)V(prev))->vpRight = tmp;
   }
   return tmp;
}

VPTR TMLDF (BYTE *pbName){ // A:* OK
   VPTR tmp = Vmalloc (sizeof(DIRNODE));
   DIRNODE *pdnT = (DIRNODE *)V(tmp);

   DIRNODE *pdn = (DIRNODE *)V(vpHomeDir[iArchArea]);
   pdnT->vpRight   = pdn->vpChilds;
   pdnT->vpLeft    = VNULL;
   pdn->vpChilds   = tmp;

   pdnT->vpFiles   = VNULL;
   pdnT->vpChilds  = VNULL;
   pdnT->vpParent  = vpHomeDir[iArchArea];
   pdnT->vpCurDir  = VNULL; // none
   pdnT->vpCurFile = VNULL; // none
   pdnT->dirmeta.dwIndex = dwDirNTX[iArchArea]++;
   pdnT->osmeta.dwParent =
      ((DIRNODE *)V(vpHomeDir[iArchArea]))->dirmeta.dwIndex;

   memcpy (((DIRNODE*)V(tmp))->osmeta.pbName,pbName,11);
   return tmp;
}

extern int fbackward;

static VPTR AddRev (VPTR loc, BYTE *pbName){ // A:4 OK
   VPTR rev = Vmalloc (sizeof(REVNODE));
   REVNODE *rn = (REVNODE *)V(rev);
   memcpy (rn->osmeta.pbName, pbName, 11);

   rn->extnode=VNULL;
   rn->bSpecial=0;
   rn->vpDir = TWD();
   rn->vpFNN = loc;
   rn->vpDLink = VNULL;
   rn->osmeta.dwParent =
      ((DIRNODE *)V(vpHomeDir[iArchArea]))->dirmeta.dwIndex;
   rn->location.dwOffset = 0x01020304L;
   rn->compress.dwCompressedLength = 0x090a0b0cL;
   FILENAMENODE *fn = (FILENAMENODE *)V(loc);
   if (!fbackward){
single:
      rn->vpOlder = fn->vpRevs;
      fn->vpRevs = rev;
   } else {
      if (IS_VNULL(fn->vpRevs)) goto single;
      rn->vpOlder = VNULL;
      REVNODE *walk = (REVNODE *)V(fn->vpRevs);
      while (!IS_VNULL(walk->vpOlder)){
	 walk = (REVNODE*)V(walk->vpOlder);
      }
      walk->vpOlder = rev;
   }
   return rev;
}

static VPTR AddFNN (VPTR prev){
   VPTR tmp = Vmalloc (sizeof(FILENAMENODE));
   FILENAMENODE *pfn = (FILENAMENODE *)V(tmp);
   pfn->vpLeft = prev;
   pfn->vpRight = VNULL;
   pfn->vpRevs = VNULL;
   if (IS_VNULL(prev)){
      ((DIRNODE *)V(vpHomeDir[iArchArea]))->vpFiles = tmp;
   } else {
      ((FILENAMENODE *)V(prev))->vpRight = tmp;
   }
   return tmp;
}


static VPTR pvLastNode;

VPTR TMF (BYTE *pbName){ // A:* OK
   VPTR prev=VNULL;
   VPTR loc;
   VPTR walk = TFirstFileNN();
   while (!IS_VNULL(walk)){
      if (memcmp (pbName, ((REVNODE *)V(((FILENAMENODE*)V(walk))->vpRevs))->osmeta.pbName,11)==0){
	 loc = walk;
	 goto root;
      }
      prev = walk;
      walk = TNextFileNN();
   }
   loc = AddFNN (prev);
root:
   pvLastNode = loc;
   return AddRev (loc, pbName);
}

VPTR LocFNN (BYTE *pbName){ // A:* OK
   VPTR loc = VNULL;
   VPTR walk = TFirstFileNN();
   while (!IS_VNULL(walk)){
      if (memcmp (pbName, ((REVNODE *)V(((FILENAMENODE*)V(walk))->vpRevs))->osmeta.pbName,11)==0){
	 loc = walk;
	 goto root;
      }
      walk = TNextFileNN();
   }
root:
   return loc;
}

// same FILENAMENODE as previous call
VPTR TMFagain (BYTE *pbName){
   return AddRev (pvLastNode, pbName);
}

// same directory, next (new) FILENAMENODE to be created
VPTR TMFrapid (BYTE *pbName){
   pvLastNode = AddFNN (pvLastNode);
   return AddRev (pvLastNode, pbName);
}

VPTR TFirstDir (){ // A:1 OK
   DIRNODE *pdn = (DIRNODE *)V(vpHomeDir[iArchArea]);
   pdn->vpCurDir = pdn->vpChilds;
   return pdn->vpCurDir;
}

VPTR TNextDir (){ // A:2 OK
   DIRNODE *pdn1 = (DIRNODE *)V(vpHomeDir[iArchArea]);
   if (IS_VNULL(pdn1->vpCurDir)) return VNULL;
   DIRNODE *pdn2 = (DIRNODE *)V(pdn1->vpCurDir);
   return pdn1->vpCurDir = pdn2->vpRight;
}

VPTR TFirstFileNN (){ // A:1 OK
   DIRNODE *pdn = (DIRNODE *)V(vpHomeDir[iArchArea]);
   pdn->vpCurFile = pdn->vpFiles;
   return pdn->vpCurFile;
}

VPTR TSpecificFileNN (VPTR fnn){ // A:1 OK
   DIRNODE *pdn = (DIRNODE *)V(vpHomeDir[iArchArea]);
   pdn->vpCurFile = fnn;
   return pdn->vpCurFile;
}

VPTR TNextFileNN (){ // A:2 OK
   DIRNODE *pdn = (DIRNODE *)V(vpHomeDir[iArchArea]);
   if (IS_VNULL(pdn->vpCurFile)) return VNULL;
   FILENAMENODE *pfn = (FILENAMENODE *)V(pdn->vpCurFile);
   return pdn->vpCurFile = pfn->vpRight;
}

// return number of revisions
DWORD TRevs (){
   DWORD ctr=0;
   DIRNODE *pdn = (DIRNODE *)V(vpHomeDir[iArchArea]);
   VPTR filenamenode = pdn->vpCurFile;
   VPTR walk = ((FILENAMENODE*)V(filenamenode))->vpRevs;
   while (!IS_VNULL(walk)){
      if (((REVNODE*)V(walk))->bStatus!=FST_DEL) ctr++;
      walk = ((REVNODE *)V(walk))->vpOlder;
   }
   return ctr;
}

#define DELREV 4000000000L

// get access to specific revision (0..n-1)
VPTR TRev (DWORD dwIndex){ // A:* OK
#ifdef UCPROX
   DWORD dwi=dwIndex;
#endif
   DIRNODE *pdn = (DIRNODE *)V(vpHomeDir[iArchArea]);
   VPTR filenamenode = pdn->vpCurFile;
   VPTR walk = ((FILENAMENODE*)V(filenamenode))->vpRevs;
   if (IS_VNULL(walk)) IE();
   if (((REVNODE*)V(walk))->bStatus==FST_DEL) goto again;
match:
   if ((dwIndex--)==0) goto end;
again:
   if (IS_VNULL(walk)) IE();
   walk = ((REVNODE *)V(walk))->vpOlder;
   if (((REVNODE*)V(walk))->bStatus==FST_DEL) goto again;
   goto match;
end:
#ifdef UCPROX
   if (heavy) if (TRevNo(walk)!=dwi) IE();
#endif
   return walk;
}

DWORD TRevNo (VPTR rev){
   if (IS_VNULL(rev)) IE();
   VPTR fnn = ((REVNODE*)V(rev))->vpFNN;
   VPTR walk = ((FILENAMENODE*)V(fnn))->vpRevs;
   DWORD dwIndex=0;
   if (IS_VNULL(walk)) IE();
   if (((REVNODE*)V(walk))->bStatus==FST_DEL){
      if (CMP_VPTR(rev,walk)) return DELREV;
      goto again;
   }
match:
   if (CMP_VPTR(rev,walk)) goto end;
   dwIndex++;
again:
   walk = ((REVNODE *)V(walk))->vpOlder;
   if (IS_VNULL(walk)) IE();
   if (((REVNODE*)V(walk))->bStatus==FST_DEL){
      if (CMP_VPTR(rev,walk)) return DELREV;
      goto again;
   }
   goto match;
end:
   return dwIndex;
}

/*** path management ***/

static char pcGPath[256];
char * GetPath (VPTR here, int levels){ // A:2 OK
   if ((levels==0) || (IS_VNULL(((DIRNODE*)V(here))->vpParent))){
      strcpy (pcGPath,"");
   } else {
      GetPath (((DIRNODE*)V(here))->vpParent, levels-1);
      strcat (pcGPath,Rep2Name(((DIRNODE*)V(here))->osmeta.pbName));
      strcat (pcGPath,"\\");
   }
   return pcGPath;
}

/*** scanners ***/

int iLastNoQ;

// file, mask, exact?
int Fit (BYTE *b, BYTE *a, int exact){

   if (a[0]=='U' && a[1]=='$' && a[2]=='~')
        iLastNoQ=1;
   else
        iLastNoQ=0;

   int prob=0;

   exact=1; // always use exact matching, even when NOT in subdir

   if (!exact){ // exact spec only in 'home' directory
      prob=1;
   }
   for (int i=0;i<11;i++){
      if (a[i]=='?'){
	 prob=0;
	 iLastNoQ=0;
      } else if (a[i]!=b[i]) {
	 return 0;
      }
   }
   return !prob;
}

extern VPTR Uqueue; // negative selections (!*.bak)

void NoR (char *);

void DeleteLast (void);

DWORD smskip;

static char EAline[300];
static char EAtpt[260];

extern int scan1;

long GetOS2ExtAttrSize (char far *filename){
#ifndef UE2
   unsigned long ret;
   static unsigned char dump[26];
   union REGS regs; struct SREGS sregs;

   /* Prepare the registers the same way DIR /N seems to do it. */
   regs.x.ax = 0x5702;
   regs.x.bx = 0xFFFF;
   regs.x.cx = 0x001a;
   regs.x.dx = 0x0002;
   regs.x.si = FP_OFF(filename);
   sregs.ds = FP_SEG(filename);
   regs.x.di = FP_OFF(dump);
   sregs.es = FP_SEG(dump);
   intdosx (&regs, &regs, &sregs);

   /* Success? */
   if (regs.h.al==0x00){
      /* Get EA size */
      ret = dump[24]*256L*256L+dump[23]*256L+dump[22];

      /* 4 seems to determine 'No EA' */
      if (ret<5) ret=0;
   } else
      ret=-1;
   return ret;
#else
   return 0;
#endif
}

/* Function to read extended atttributes from an OS/2 DOS box.
 */

void GetOS2ExtAttr (char *filename, BYTE *buffer, unsigned maxlen){
#ifndef UE2
   union REGS regs; struct SREGS sregs;
   struct EAOP {
      long dummy1;
      BYTE *buf;
      long dummy2;
   } eaop;

   eaop.dummy1 = 0;
   eaop.buf = buffer;
   eaop.dummy2 = 0;
   *((unsigned long *)eaop.buf) = maxlen;
   regs.x.ax = 0x5702;
   regs.x.bx = 0xFFFF;
   regs.x.cx = 0x000c;
   regs.x.dx = 0x0004;
   regs.x.si = FP_OFF(filename);
   sregs.ds = FP_SEG(filename);
   regs.x.di = FP_OFF(&eaop);
   sregs.es = FP_SEG(&eaop);
   intdosx (&regs, &regs, &sregs);
#endif
}

/* Function to write extended atttributes from an OS/2 DOS box.
 */

void SetOS2ExtAttr (char *filename, BYTE *buffer)
{
   union REGS regs; struct SREGS sregs;
   struct EAOP {
      long dummy1;
      BYTE *buf;
      long dummy2;
   } eaop;

   eaop.dummy1 = 0;
   eaop.buf = buffer;
   eaop.dummy2 = 0;
   regs.x.ax = 0x5703;
   regs.x.bx = 0xFFFF;
   regs.x.cx = 0x000c;
   regs.x.dx = 0x0002;
   regs.x.si = FP_OFF(filename);
   sregs.ds = FP_SEG(filename);
   regs.x.di = FP_OFF(&eaop);
   sregs.es = FP_SEG(&eaop);
   intdosx (&regs, &regs, &sregs);
}

char noextrea=0;

#ifdef UCPROX

void HeapDump (void){
   struct farheapinfo hi;
   hi.ptr = NULL;
   int ctr=0;
   Out (7, "\n\rIDX ADDRESS     Size   Status\n\r" );
   Out (7, "-------------   ----   ------\n\r" );
   while( farheapwalk( &hi ) == _HEAPOK )
      Out (7, "%03d %lp %7lu    %s\n\r", ctr++, hi.ptr, hi.size, hi.in_use ? "used" : "free" );
   Out (7, "-------------   ----   ------\n\r" );
   Out (7, "FCL=%ld\n\r\n\r",farcoreleft());
}

void HeapScan (void){
   heapfillfree(123);
   farheapfillfree(123);
   HeapDump();
}

void HeapTest (void){
   if (!heapcheckfree(123)) IE();
   if (!farheapcheckfree(123)) IE();
   if (!heapcheck()) IE();
   if (!farheapcheck()) IE();
   HeapDump();
}

#endif

extern char tmppath[260];

void RepTmp (char *tmp){
   if (strstr(tmp,tmppath)){
      strcpy (tmp, tmp+strlen(tmppath)-5);
      strcpy (tmp, "(tmp");
      tmp[4]=')';
   }
}

extern int movemode;
void MoveAdd (char *file); // diverse.cpp
void RabAdd (char* file); // diverse.cpp

int SearchFile (char *szFileName);

#pragma argsused
void ScanAddR (VPTR dir, VPTR Mpath, int parents, int rapid){
#ifndef UE2
   static int level=0;
   level++;
   if (rapid==2){
      if (IS_VNULL(((DIRNODE *)V(dnRoot[iArchArea]))->vpChilds) &&
	  IS_VNULL(((DIRNODE *)V(dnRoot[iArchArea]))->vpFiles)) rapid=1;
      else
	 rapid=0;
   }
   DWORD obj=0;
   DWORD sel=0;
   VPTR home = TWD();
   TCD (dir);
   static char where[260];
   static char mask[260];
//   strcpy (where, "\x7");
   strcpy  (where, ((MPATH*)V(Mpath))->pcTPath);
//   strcat (where, "\x7");
   strcat (where, GetPath(dir,parents));
   VPTR buf = Vmalloc (270);
#ifdef UCPROX
   Vforever_do();
#endif
   strcpy ((char *)V(buf), where);
   NoR (where);
   strcpy (mask, where);

   VPTR ppos = ((MPATH *)V(Mpath))->vpMasks;
   if (!MODE.fSubDirs && !IS_VNULL(ppos) && IS_VNULL(((MMASK *)V(ppos))->vpNext)){
      char msk[20];
      strncpy (msk, (char*)((MMASK *)V(ppos))->pbName, 15);
      msk[11] = msk[10];
      msk[10] = msk[9];
      msk[9]  = msk[8];
      msk[8]  = '.';
      msk[12] = 0;
      for (int i=7; i>0; i--){
	 if (msk[i]==' ') strcpy (msk+i, msk+i+1);
      }
//      Out (7,"[%s]", msk);
      strcat (mask, msk);
   }  else
      strcat (mask, "*.*");
   struct ffblk ffblk;
   int done;
   CSB;
      done = findfirst(mask,&ffblk,0xF7);
   CSE;
   int fast=0; // after first add in this dir it MIGHT become 1
   {
      static char wh[260];
      strcpy (wh, where);
      RepTmp (wh);
      Out (1,"\x7\Scanning %s\x7 ",wh);
      StartProgress (-1, 1);
   }
   if (!scan1){
      Out (2,"\x7\Scanning\x7 ");
      scan1=1;
      StartProgress (-1, 2);
   }
   while (!done){
      BrkQ();
//      if (obj%7==0) SOut ("Scanning %s %lu (%lu)\r",where,obj,sel);
      Hint();
      obj++;
      strupr (ffblk.ff_name);
      if (strcmp(ffblk.ff_name,".")!=0)
	 if (strcmp(ffblk.ff_name,"..")!=0) {
	    if (ffblk.ff_attrib&FA_DIREC) {
	       if (MODE.fSubDirs){
		  DIRNODE *dir = (DIRNODE *)V(TMLD(Name2Rep(ffblk.ff_name)));
		  dir->osmeta.wTime = ffblk.ff_ftime;
		  dir->osmeta.wDate = ffblk.ff_fdate;
		  dir->osmeta.bAttrib = ffblk.ff_attrib;
		  dir->bStatus = DST_NEW;  // always NEW (even if already existed)
		  dir->extnode = VNULL;  // remove ALL tags

		  if (os2&&CONFIG.fEA){
		     char tmp[100];
		     sprintf (tmp,"%s%s",where,ffblk.ff_name);
		     DWORD ssiz;
		     if ((ssiz=GetOS2ExtAttrSize(tmp))!=0){
			strcpy (EAtpt, TmpFile((char *)CONFIG.pbTPATH,1,".TMP"));
			if (Exists(EAtpt)) Delete (EAtpt);
			BYTE *dat;
			WORD len;
			GetDat (&dat, &len);
			if (len<ssiz+16){
			   UnGetDat ();
			   sprintf (EAline,"EAUTIL /s /p %s%s %s",where,ffblk.ff_name,EAtpt);
			   ssystem (EAline);
			} else {
			   int handle = Open (EAtpt, CR|MUST|NOC);
			   GetOS2ExtAttr (tmp, dat, len);
			   Write (dat, handle, (WORD)ssiz);
			   Close (handle);
			   UnGetDat();
			}
			if (Exists(EAtpt)){
			   int f = Open (EAtpt,RO|MUST|CRT);
			   DWORD ctr = GetFileSize (f);
			   if (ctr!=0){
			      // create EXTNODE and fill fields
			      VPTR en = Vmalloc (sizeof(EXTNODE));
			      ((EXTNODE*)V(en))->extmeta.size = ctr;
			      strcpy (((EXTNODE*)V(en))->extmeta.tag, TAG_OS2EA);

			      // add EXTNODE to revision information
			      VPTR tmp = ((DIRNODE *)V(TMLD(Name2Rep(ffblk.ff_name))))->extnode;
			      ((EXTNODE*)V(en))->next = tmp;
			      ((DIRNODE *)V(TMLD(Name2Rep(ffblk.ff_name))))->extnode = en;

			      VPTR walk;
			      if (ctr>900)
				 walk = Vmalloc (sizeof (EXTDAT));
			      else
				 walk = Vmalloc (8+(WORD)ctr);
			      ((EXTNODE*)V(en))->extdat = walk;
			      while (ctr){
				 WORD trn = ctr<1000 ? (WORD)ctr : 1000;
				 Read (((EXTDAT*)V(walk))->dat, f, trn);
				 ctr-=trn;
				 if (ctr){
				    VPTR tmp = ((EXTDAT*)V(walk))->next = Vmalloc (sizeof (EXTDAT));
				    walk = tmp;
				 }
			      }
			   }
			   Close (f);
			   Delete (EAtpt);
			}
		     }
		  }
	       }
	    } else if (!(ffblk.ff_attrib&FA_LABEL)) {
	       REVNODE *rev;
	       int ok=0;
	       VPTR walk = ((MPATH *)V(Mpath))->vpMasks;
	       while (!IS_VNULL(walk)){ // select masks
		  if (Fit(Name2Rep(ffblk.ff_name),((MMASK *)V(walk))->pbName,level==1)){
		     ((MMASK *)V(walk))->bFlag=1;
		     ok=1;
		     if (iLastNoQ) goto nounmask;
		     // no shortcut for special file detection
		  }
		  walk = ((MMASK *)V(walk))->vpNext;
	       }
	       if (ok && !IS_VNULL(Uqueue)){ // unselect masks
                  VPTR w2 = Uqueue;
                  while (!IS_VNULL(w2)){
                      if (0==strncmp(where, ((MPATH *)V(w2))->pcTPath,
                                     strlen (((MPATH *)V(w2))->pcTPath))){
                          walk = ((MPATH *)V(w2))->vpMasks;
                          while (!IS_VNULL(walk)){ // unselect masks
                             if (Fit(Name2Rep(ffblk.ff_name),((MMASK *)V(walk))->pbName,level==1)){
                                ok=0;
                                break;
                             }
                             walk = ((MMASK *)V(walk))->vpNext;
                          }
                      }
                      w2 = ((MPATH *)V(w2))->vpNext;
                  }
	       }
nounmask:
	       if (ok){ // NEW FILES GO HERE !!!
		  sel++;
		  VPTR lmc = LocKey (ToHKey (ffblk.ff_name));
		  if (IS_VNULL(lmc))
		     lmc = LocMacKey (ToKey (ffblk.ff_name));
		  strcpy (((MASREC*)V(lmc))->szName, ffblk.ff_name);

		  VPTR rv;
		  if (fast)
		     rv=TMFrapid(Name2Rep(ffblk.ff_name));
		  else
		     rv=TMF(Name2Rep(ffblk.ff_name));
		  rev = (REVNODE*)V(rv);
		  rev->osmeta.wTime = ffblk.ff_ftime;
		  rev->osmeta.wDate = ffblk.ff_fdate;
		  rev->osmeta.bAttrib = ffblk.ff_attrib;
                  rev->filemeta.dwLength = ffblk.ff_fsize;
		  rev->vpDLink = buf;
		  rev->vpMas = lmc;
		  rev->bStatus = FST_DISK;
		  int skip=0;

		  // try to save time if not in movemode
		  if (!movemode && skip==0 && MODE.fSMSkip && !IS_VNULL(rev->vpOlder)){
		     REVNODE *older = (REVNODE *)V(rev->vpOlder);
		     if ((rev->osmeta.wTime == older->osmeta.wTime) &&
			 (rev->osmeta.wDate == older->osmeta.wDate) &&
			 (older->filemeta.dwLength == ffblk.ff_fsize)){
			goto SKIP;
		     }
		  }

		  rev = (REVNODE*)V(rv);
		  if (skip==0 && MODE.bExcludeAttr){
		     if (rev->osmeta.bAttrib & MODE.bExcludeAttr){
			skip=1;
		     }
		  }

		  if (skip==0 && MODE.fArca && !(rev->osmeta.bAttrib&FA_ARCH)){
		     skip=1;
		  }

		  rev = (REVNODE*)V(rv);
		  if (skip==0 && MODE.bELD){
		     DWORD spec1 = ((DWORD)rev->osmeta.wDate<<16L)+(DWORD)rev->osmeta.wTime;
		     DWORD spec2 = *((DWORD*)(&MODE.ftELD));
		     if (spec1<spec2){
			skip=1;
		     }
		  }

		  rev = (REVNODE*)V(rv);
		  if (skip==0 && MODE.bEED){
		     DWORD spec1 = ((DWORD)rev->osmeta.wDate<<16L)+(DWORD)rev->osmeta.wTime;
		     DWORD spec2 = *((DWORD*)(&MODE.ftEED));
		     if (spec1>spec2){
			skip=1;
		     }
		  }

		  if (skip==0 && MODE.fFreshen && IS_VNULL(rev->vpOlder)){
		     skip=1;
		  }

		  if (skip==0 && MODE.fNof && !IS_VNULL(rev->vpOlder)){
		     skip=1;
		  }

		  rev = (REVNODE*)V(rv);
		  if (skip==0 && MODE.fNewer && !IS_VNULL(rev->vpOlder)){
		     REVNODE *older = (REVNODE *)V(rev->vpOlder);
		     DWORD spec1 = ((DWORD)rev->osmeta.wDate<<16L)+(DWORD)rev->osmeta.wTime;
		     DWORD spec2 = ((DWORD)older->osmeta.wDate<<16L)+(DWORD)older->osmeta.wTime;
		     if (spec1<=spec2){
			skip=1;
		     }
		  }

		  // slowest latest
		  if (skip==0 && MODE.fContains){
//		     Out (7,"[%s ",Full(rv));
		     if (!SearchFile (Full (rv))){
			rev = (REVNODE*)V(rv);
			skip=1;
			rev->bStatus=FST_DEL;
		     } else {
			rev = (REVNODE*)V(rv);
		     }
//		     Out (7,"%d]",skip);
		  }
SKIP:
		  if (skip==0 && MODE.fSMSkip && !IS_VNULL(rev->vpOlder)){
		     REVNODE *older = (REVNODE *)V(rev->vpOlder);
		     if ((rev->osmeta.wTime == older->osmeta.wTime) &&
			 (rev->osmeta.wDate == older->osmeta.wDate) &&
			 (older->filemeta.dwLength == ffblk.ff_fsize)){
			smskip+=ffblk.ff_fsize;
			skip=1;
			if (movemode)
			   MoveAdd (Full(rv));
			if (MODE.fRab)
			   RabAdd (Full(rv));
			older->osmeta.bAttrib = rev->osmeta.bAttrib;
		     }
		  }
		  if (skip==0){
		     if (((ffblk.ff_attrib&FA_HIDDEN)||(ffblk.ff_attrib&FA_SYSTEM))){
			switch (MODE.bHid){
			   case 1:
			      Menu ("\x6Include hidden/system file %s%s ?",where,ffblk.ff_name);
			      Option ("",'Y',"es");
			      Option ("",'N',"o");
			      Option ("",'A',"lways include system/hidden files");
			      Option ("N",'e',"ver include system/hidden files");
			      switch(Choice()){
				 case 1:
				    break;
				 case 2:
				    skip=1;
				    break;
				 case 3:
				    MODE.bHid=2;
				    break;
				 case 4:
				    MODE.bHid=3;
				    skip=1;
				    break;
			      }
			      break;
			   case 3:
			      skip=1;
			      break;
			}
		      } else {
			 if (MODE.fQuery==1){
			    Menu ("\x6Include file %s%s ?",where,ffblk.ff_name);
			    Option ("",'Y',"es");
			    Option ("",'N',"o");
			    Option ("",'A',"ll files");
			    Option ("N",'o'," more files");
			    switch(Choice()){
			       case 1:
				  break;
			       case 2:
				  skip=1;
				  break;
			       case 3:
				  MODE.fQuery=0;
				  break;
			       case 4:
				  skip=1;
				  MODE.fQuery=2;
				  break;
			    }
			 } else if (MODE.fQuery==2){
			    skip=1;
			 }
		      }
		  }
		  rev = (REVNODE*)V(rv);
		  if (skip){
		     rev->bStatus = FST_DEL;
		  }
		  rev->extnode = VNULL;
		  if (!skip)
		     AddToNtx (((MASREC*)V(lmc))->masmeta.dwIndex,rv,ffblk.ff_fsize);

		  rev = (REVNODE *)V(rv);

		  rev->compress.dwMasterPrefix =
		     ((MASREC *)V(lmc))->masmeta.dwIndex;
//                  if (((MASREC*)A(lmc))->bStatus == MS_NEW1)
//                     ((MASREC*)A(lmc))->bStatus = MS_NEW2;
		  if (!skip){
		     if (((MASREC*)V(lmc))->bStatus == MS_CLR)
			((MASREC*)V(lmc))->bStatus = MS_NEW2; // ALWAYS create masters
		     if (((MASREC*)V(lmc))->bStatus == MS_OLD)
			((MASREC*)V(lmc))->bStatus = MS_OLDN;
		  }
		  fast = rapid; // if rapid then be fast
	       }
	    } // else volume label
      }
      CSB;
	 done = findnext(&ffblk);
      CSE;
   }
//   SOut ("Scanning %s %lu (%lu)\n\r",where,obj,sel);
   Out (1|8,"\n\r");

   VPTR walk = TFirstDir();
   while (!IS_VNULL(walk)){
      if (((DIRNODE *)V(walk))->bStatus==DST_NEW){
	 if (MODE.fSubDirs) ScanAddR (walk, Mpath, parents+1,rapid);
      }
      walk = TNextDir();
   }
   TCD (home);
   level--;
#endif
}

void ScanAdd (VPTR dir, VPTR Mpath, int parents){
   ScanAddR (dir,Mpath,parents,2);
}

// change to (command line determinded) directory, extract
int TCWI(char *pp, int f){
   char p[128];
   char rp[12];
   char *pt = p, *pt2, *ptn;
   TCD (VNULL); // goto root
   strcpy (pt, pp);
   strupr (pt);
   while (*pt){
      if ((*pt=='\\')||(*pt=='/')||(*pt==' ')) {
	 pt++;
	 continue;
      };
      pt2=pt+1;
      while ((*pt2!='\\')&&(*pt2!='/')&&(*pt2!=' ')&&(*pt2!='\0')) pt2++;
      if (pt2!='\0') ptn = pt2+1; else ptn = pt2;
      *pt2='\0';
      memcpy (rp, Name2Rep (pt), 11);
      VPTR walk = TFirstDir();
      while (!IS_VNULL(walk)){
	 if (0==memcmp (rp,((DIRNODE*)V(walk))->osmeta.pbName,11)){
	    TCD (walk);
	    goto deeper;
	 }
	 walk = TNextDir();
      }
      if (f){
	 TCD(TMLD ((BYTE*)rp));
      }else
	 return 0;
deeper:
      pt=ptn;
   }
   return 1;
}

int TCW(VPTR Mpath){
   return TCWI (((MPATH *)V(Mpath))->pcTPath,0);
}

int DirExists (char *path){
   VPTR now=TWD();
   int ret = TCWI (path,0);
   TCD (now);
   return ret;
}

VPTR StrToDir (char *path){
   VPTR now=TWD();
   TCWI (path,1);
   VPTR ret = TWD();
   TCD (now);
   return ret;
}

VPTR AccessRev (char *fullpath){
   char fpath[MAXPATH+10];
   char fname[20];

   char drive[MAXDRIVE];
   char dir[MAXDIR];
   char file[MAXFILE];
   char ext[MAXEXT];

   DWORD ver=0;

   strcpy (fpath, fullpath);
   for(int i=strlen(fpath);i>0;i--){
      if (fpath[i]==';'){
	 ver = atol (fpath+i+1);
	 fpath[i] = 0;
	 break;
      }
   }

   fnsplit (fpath,drive,dir,file,ext);
   fnmerge (fpath,drive,dir,NULL,NULL);
   fnmerge (fname,NULL,NULL,file,ext);

   if (!TCWI (fpath, 0)) IE(); // change to correct directory
   TSpecificFileNN (LocFNN(Name2Rep(fname))); // make file with specific name current

   DIRNODE *pdn = (DIRNODE *)V(vpHomeDir[iArchArea]);
   if (IS_VNULL(((FILENAMENODE*)V(pdn->vpCurFile))->vpRevs)) IE();

   VPTR ret = TRev (ver); // access specific revision
   if (IS_VNULL (ret)) IE();
   return ret;
}

extern BYTE bExtractMode; // COMOTERP.CPP
extern BYTE bDeleteMode;  // COMOTERP.CPP
extern int iVerifyMode;
extern int probeo;

extern DWORD dwMyCtr;

#define UNK 4000000000L
char *WFull (VPTR rev, DWORD hint);

char fContinue=0;

extern int fsecretskipmode;

// mark (all) selected files & directories
#pragma argsused
void MarkUp (char *rdir, VPTR dir, VPTR Mpath, int parents){
   static int level=0;
   VPTR home = TWD();

   // if root of path to scan then goto directory (failing at that->return)
   if (level) {
      TCD(dir); // dir now contains dir to scan
   } else {
      if(!TCW(Mpath)) // goto root of tree to scan
	 return;
   }

   level++;

   // determine location on disk (extract path)
   VPTR buf = Vmalloc (200);
#ifdef UCPROX
   Vforever_do();
#endif
   strcpy ((char *)V(buf), rdir);
   if (MODE.fNoPath){
      strcat ((char *)V(buf), GetPath(dir,0));
   } else {
      strcat ((char *)V(buf), GetPath(dir,parents));
   }

   static char ttmp[130];
   strcpy (ttmp, ((char *)V(buf)));
   ttmp[strlen(ttmp)-1]=0; // remove last '\\'
   if (!iVerifyMode && !MODE.fPrf && bExtractMode && !TstPath(ttmp)){
      if (!MkPath (ttmp)){
	 // failed to create (needed) path
	 TCD (home);
	 level--;
	 return;
      } else {
	 if (os2 && !IS_VNULL(dir)){
	    VPTR wlk = ((DIRNODE*)V(dir))->extnode;
	    while (!IS_VNULL(wlk)){
	       if (0==strcmp(((EXTNODE*)V(wlk))->extmeta.tag,TAG_OS2EA) && !noextrea){
		  strcpy (EAtpt, TmpFile((char *)CONFIG.pbTPATH,1,".TMP"));
		  int f = Open (EAtpt,CR|MUST|CWR);
		  VPTR walk=((EXTNODE*)V(wlk))->extdat;
		  DWORD ctr=((EXTNODE*)V(wlk))->extmeta.size;
		  DWORD ssiz=((EXTNODE*)V(wlk))->extmeta.size;
		  while (ctr){
		     WORD trn = ctr<1000 ? (WORD)ctr : 1000;
		     Write (((EXTDAT*)V(walk))->dat,f,trn);
		     ctr-=trn;
		     walk = ((EXTDAT*)V(walk))->next;
		  }
		  Close (f);

		  BYTE *dat;
		  WORD len;
		  GetDat (&dat, &len);
		  if (len<ssiz+16){
		     UnGetDat();
		     sprintf (EAline,"EAUTIL /o /j %s %s",ttmp,EAtpt);
		     ssystem (EAline);
		  } else {
		     int handle = Open (EAtpt, RO|MUST|NOC);
		     Read (dat, handle, (WORD)ssiz);
		     Close (handle);
		     SetOS2ExtAttr (ttmp, dat);
		     UnGetDat();
		     Delete (EAtpt);
		  }

		  if (Exists(EAtpt)){
		     Error (75,"failed to set OS/2 extended attribute for %s",ttmp);
		     Delete (EAtpt);
		  }

		  break; // max 1 EA
	       }
	       wlk = ((EXTNODE*)V(wlk))->next;
	    }
	    // aaa execute extended attributes
	 }
      }
   }

   VPTR walkf = TFirstFileNN();
   while (!IS_VNULL(walkf)){ // for each file in archive dir
      BrkQ();

      // process revisions from old to new to process changing revisions during delete
      DWORD i = TRevs()-1;
loopo:
      if (!TRevs()) goto endo;
      if (i>TRevs()-1) i = TRevs()-1;
      { // for all revisions of file
	 BrkQ();
	 VPTR rev = TRev (i);
	 int ok=0;
	 VPTR walk = ((MPATH *)V(Mpath))->vpMasks;
	 while (!IS_VNULL(walk)){ // for all select masks
	    if (Fit(((REVNODE*)V(rev))->osmeta.pbName,((MMASK *)V(walk))->pbName,level==1)){
	       if (((MMASK*)V(walk))->fRevs)
		  if (((MMASK*)V(walk))->dwRevision!=i) // correct revision ?
		     goto nono;
	       ((MMASK *)V(walk))->bFlag=1;
	       ok=1;
	       if (iLastNoQ && ((MMASK*)V(walk))->fRevs) goto noq; // exact match, skip negate masks
	    }
nono:
	    walk = ((MMASK *)V(walk))->vpNext;
	 }
okq:
	 if (ok && !IS_VNULL(Uqueue)){ // for all unselect masks
	    VPTR walk = ((MPATH *)V(Uqueue))->vpMasks;
	    while (!IS_VNULL(walk)){ // unselect masks
	       if (Fit(((REVNODE*)V(rev))->osmeta.pbName,((MMASK *)V(walk))->pbName,level==1)){
		  if (((MMASK*)V(walk))->fRevs)
		     if (((MMASK*)V(walk))->dwRevision!=i) // correct revision ?
			goto ddn;
		  ok=0;
		  goto noq;
	       }
ddn:
	       walk = ((MMASK *)V(walk))->vpNext;
	    }
	 }
	 if (ok && (fsecretskipmode == 2) && (TRevs() > 1)) ok = 0;
	 if (ok && (fsecretskipmode == 3) && (TRevs() == 1)) ok = 0;
	 if (ok && (MODE.bExcludeAttr & ((REVNODE*)V(rev))->osmeta.bAttrib))
	    ok=0;
	 if (ok && MODE.fArca && !(FA_ARCH & ((REVNODE*)V(rev))->osmeta.bAttrib))
	    ok=0;

	 if (ok && MODE.bELD){
	    REVNODE* rv = (REVNODE*)V(rev);
	    DWORD spec1 = ((DWORD)rv->osmeta.wDate<<16L)+(DWORD)rv->osmeta.wTime;
	    DWORD spec2 = *((DWORD*)(&MODE.ftELD));
	    if (spec1<spec2){
	       ok=0;
	    }
	 }

	 if (ok && MODE.bEED){
	    REVNODE* rv = (REVNODE*)V(rev);
	    DWORD spec1 = ((DWORD)rv->osmeta.wDate<<16L)+(DWORD)rv->osmeta.wTime;
	    DWORD spec2 = *((DWORD*)(&MODE.ftEED));
	    if (spec1>spec2){
	       ok=0;
	    }
	 }
noq:
	 if (ok){
	    if (IS_VNULL(((REVNODE*)V(rev))->vpDLink) || bDeleteMode){
	       ((REVNODE*)V(rev))->vpDLink = buf; // where should it go
	    } else {
	       FatalError (120,"double reference to single file, please simplify command line");
	    }
	    if (!bDeleteMode){
	       ((REVNODE*)V(rev))->bStatus = FST_MARK; // mark file for extract
	       dwMyCtr += ((REVNODE*)V(rev))->filemeta.dwLength;
	    } else {
	       if (!MODE.fContains && !MODE.fQuery){
		  Out (3,"\x7\Deleting %s\n\r",WFull (rev,i));
		  Out (4,"\x7\Delete %s\n\r",WFull (rev,i));
		  ((REVNODE*)V(rev))->bStatus = FST_DEL; // mark file for delete
	       } else {
		  ((REVNODE*)V(rev))->bStatus = FST_MARK; // mark file for delete
	       }
	    }
//	    if (!bDeleteMode){
	       VPTR mas = LocMacNtx (((REVNODE*)V(rev))->compress.dwMasterPrefix);
	       if (!IS_VNULL(mas)) ((MASREC*)V(mas))->bStatus = MS_NEED;
//	    }
#ifdef UCPROX
	    if (debug)
	    Out (7,"{{to be extracted file '%s' path '%s' master index %lu internal revision %ld offset %ld}}\n\r",
			     Rep2Name(((REVNODE*)V(rev))->osmeta.pbName),
			     (char *)V(buf),
			     ((REVNODE*)V(rev))->compress.dwMasterPrefix,
			     i,
			     ((REVNODE*)V(rev))->location.dwOffset);
#endif
	 }
      }
      if (i!=0){
         i--;
	 goto loopo;
      }
endo:
      walkf = TNextFileNN();
   }

   VPTR walk = TFirstDir();
   if (MODE.fSubDirs)
      while (!IS_VNULL(walk)){
	 MarkUp (rdir, walk, Mpath, parents+1);
         walk = TNextDir();
      }
   TCD (home);
   level--;
}


/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       Archive status I/O.
*/

PIPE pipe;

WORD pascal far PRead (BYTE *pbBuf, WORD wLen){
   return ReadPipe (pipe, pbBuf, wLen);
}

void pascal far PWrite (BYTE *pbBuf, WORD wLen){
   WritePipe (pipe, pbBuf, wLen);
}

extern BYTE bDamp[MAX_AREA]; // archio.h
extern BYTE bPDamp[MAX_AREA]; // archio.h

char *neat (DWORD val){
   DWORD t;
   int m=0;
   static char ret[20], tmp[10];
   strcpy (ret,"");
   if (val>999999999L){
      t = val/1000000000L;
      val-=t*1000000000L;
      sprintf (ret,"%ld",t);
      strcat (ret,",");
      m=1;
   }
   if (val>999999L || m){
      t = val/1000000L;
      val-=t*1000000L;
      if (!m)
	 sprintf (tmp,"%ld",t);
      else
	 sprintf (tmp,"%03ld",t);
      strcat (ret,tmp);
      strcat (ret,",");
      m=1;
   }
   if (val>999L || m){
      t = val/1000L;
      val-=t*1000L;
      if (!m)
	 sprintf (tmp,"%ld",t);
      else
	 sprintf (tmp,"%03ld",t);
      strcat (ret,tmp);
      strcat (ret,",");
      m=1;
   }
   if (!m)
      sprintf (tmp,"%ld",val);
   else
      sprintf (tmp,"%03ld",val);
   strcat (ret,tmp);
   return ret;
}

static void fs (DWORD files, DWORD dirs, DWORD bytes){
   if (files==0 && dirs==0){
      COut (3|8,"is empty");
   } else {
      COut (3|8,"contains ");
      if (files!=0){
	 if (files==1)
	    COut (3|8,"1 file ");
	 else
	    COut (3|8,"%s files ",neat(files));
	 if (bytes==1)
	    COut (3|8,"(1 byte) ",bytes);
	 else
	    COut (3|8,"(%s bytes) ",neat(bytes));
      }
      if (files!=0 && dirs!=0) COut (3,"and ");
      if (dirs!=0){
	 if (dirs==1)
	    COut (3|8,"1 directory");
	 else
	    COut (3|8,"%s directories ",neat(dirs));
      }
   }
}

DWORD dwFiles, dwDirs;
DWORD dwTSize;
static void NotifyOld (void){
   if (CONFIG.fOut==1){
      COut (3,"\x7\Archive ");
      fs (dwFiles, dwDirs, dwTSize);
      COut (3|8," \r");
   }
}
static void NotifyNew (void){
   COut (3,"\x7Updated archive ");
   fs (dwFiles, dwDirs, dwTSize);
   COut (3|8," \r");
}

static void DDump (VPTR extnode){
#ifndef UE2
   while (!IS_VNULL (extnode)){
      VPTR next = ((EXTNODE*)V(extnode))->next;
      if (IS_VNULL(next))
	 ((EXTNODE*)V(extnode))->extmeta.next=0;
      else
	 ((EXTNODE*)V(extnode))->extmeta.next=1;
      WritePipe (pipe, (BYTE*)&(((EXTNODE*)V(extnode))->extmeta), sizeof (EXTMETA));
//      Out (7,"[%s]\n\r",((EXTNODE*)V(extnode))->extmeta.tag);
      VPTR walk=((EXTNODE*)V(extnode))->extdat;
      DWORD ctr=((EXTNODE*)V(extnode))->extmeta.size;
      while (ctr){
	 WORD trn = (ctr<1000) ? (WORD)ctr : 1000;
	 WritePipe (pipe,((EXTDAT*)V(walk))->dat,trn);
	 ctr-=trn;
	 walk = ((EXTDAT*)V(walk))->next;
      }
      extnode=next;
   }
#endif
}

static VPTR DGet (){
   VPTR ret = Vmalloc (sizeof (EXTNODE));
   ((EXTNODE*)V(ret))->next=VNULL;
next:
   ReadPipe (pipe,(BYTE*)&((EXTNODE*)V(ret))->extmeta, sizeof (EXTMETA));
//   Out (7,"[%s]\n\r",((EXTNODE*)V(ret))->extmeta.tag);
   DWORD ctr=((EXTNODE*)V(ret))->extmeta.size;
   if (ctr>1000000L){
      Error (90,"fatal damage in central directory");
      if (!fContinue) FatalError (200,"you should repair this archive with 'UC T'");
      ctr=((EXTNODE*)V(ret))->extmeta.size=0;
      ((EXTNODE*)V(ret))->extmeta.next=0;
      return ret;
   }
   VPTR walk;
   if (ctr>900)
      walk = Vmalloc (sizeof (EXTDAT));
   else
      walk = Vmalloc (8+(WORD)ctr);
   ((EXTNODE*)V(ret))->extdat = walk;
   while (ctr){
      WORD trn = (ctr<1000) ? (WORD)ctr : 1000;
      ReadPipe (pipe, ((EXTDAT*)V(walk))->dat, trn);
      ctr-=trn;
      if (ctr){
	 VPTR tmp =  Vmalloc (sizeof (EXTDAT));
	 ((EXTDAT*)V(walk))->next = tmp;
	 walk = tmp;
      }
   }
   ((EXTDAT*)V(walk))->next = VNULL;
   if (((EXTNODE*)V(ret))->extmeta.next){
      VPTR tmp = Vmalloc (sizeof (EXTNODE));
      ((EXTNODE*)V(tmp))->next = ret;
      ret=tmp;
      goto next;
   }
   return ret;
}

static void ListDirs (){ // A:* OK
#ifndef UE2
   OHEAD ohead;
   ohead.bType = BO_DIR;
   VPTR walk = TFirstDir();
   while (!IS_VNULL(walk)){
      BrkQ();
      if (((DIRNODE*)V(walk))->bStatus!=DST_DEL){
	 if (pulsar()) NotifyNew();
	 dwDirs++;
	 DIRNODE *dn = (DIRNODE *)V(walk);
	 WritePipe (pipe, (BYTE *)&ohead, sizeof(OHEAD));
	 if (IS_VNULL(dn->extnode))
	    dn->osmeta.tag=0;
	 else
	    dn->osmeta.tag=1;
	 WritePipe (pipe, (BYTE *)&(dn->osmeta), sizeof(OSMETA));
	 WritePipe (pipe, (BYTE *)&(dn->dirmeta), sizeof(DIRMETA));
	 DDump (dn->extnode);
      }
      walk = TNextDir();
   }
#endif
}

static void ListFiles (){ // A:* OK
#ifndef UE2
   OHEAD ohead;
   ohead.bType = BO_FILE;
   VPTR walk = TFirstFileNN();
   while (!IS_VNULL(walk)){
      if (TRevs()){ // might be 0 !!!
	 for (long i=TRevs()-1;i>=0;i--){
	    BrkQ();
	    dwFiles++;
	    REVNODE *rn = (REVNODE *)V(TRev(i));
#ifdef UCPROX
	    if (debug){
	       Out (7,"{{file to CDIR '%s' master index %lu internal revision %ld offset %ld REVREC %lp}}\n\r",
		     Rep2Name(rn->osmeta.pbName),
		     rn->compress.dwMasterPrefix,
		     i,
		     rn->location.dwOffset,
		     rn);
	    }
#endif
	    WritePipe (pipe, (BYTE *)&ohead, sizeof(OHEAD));
	    if (IS_VNULL(rn->extnode))
	       rn->osmeta.tag=0;
	    else
	       rn->osmeta.tag=1;
	    WritePipe (pipe, (BYTE *)&(rn->osmeta), sizeof(OSMETA));
	    WritePipe (pipe, (BYTE *)&(rn->filemeta), sizeof(FILEMETA));
	    dwTSize+=rn->filemeta.dwLength;
	    if (pulsar()) NotifyNew();
	    WritePipe (pipe, (BYTE *)&(rn->compress), sizeof(COMPRESS));
	    WritePipe (pipe, (BYTE *)&(rn->location), sizeof(LOCATION));
	    DDump (rn->extnode);
	    rn = (REVNODE *)V(TRev(i));
	    if (rn->compress.dwMasterPrefix >= FIRSTMASTER){
	       ((MASREC*)V(LocMacNtx(rn->compress.dwMasterPrefix)))->masmeta.dwRefLen+=
		  ((REVNODE *)V(TRev(i)))->filemeta.dwLength;
	       REVNODE *rn = (REVNODE *)V(TRev(i));
	       ((MASREC*)V(LocMacNtx(rn->compress.dwMasterPrefix)))->masmeta.dwRefCtr++;
	    }
#ifdef UCPROX
	    else
	       cc+=((REVNODE *)V(TRev(i)))->filemeta.dwLength;
	    aa+=((REVNODE *)V(TRev(i)))->filemeta.dwLength;
#endif
	 }
      }
      walk = TNextFileNN();
   }
#endif
}

static int CleanDirs (){ // 1->deleted 0->failed
   static int depth;
   depth++;
   int ret=1;
   VPTR walk = TFirstDir();
   while (!IS_VNULL(walk)){
      TCD (walk);
      if (!CleanDirs ()) ret=0;
      TUP ();
      walk = TNextDir();
   }
   if (!IS_VNULL(TFirstFileNN())) ret=0;
   if (!IS_VNULL(TWD()))
      if (ret){
//	 Out (7,"[%d]",depth);
	 ((DIRNODE*)V(TWD()))->bStatus=DST_DEL;
      }
   depth--;
   return ret;
}

void CleanEmptyDirs (void){
   TCD (VNULL);
   CleanDirs ();
}

static void WalkDirs (){ // A:* OK
   VPTR walk = TFirstDir();
   while (!IS_VNULL(walk)){
      if (((DIRNODE*)V(walk))->bStatus==DST_DEL) goto skip;
      TCD (walk);
      ListDirs ();
      ListFiles ();
      WalkDirs();
      TUP ();
skip:
      walk = TNextDir();
   }
}

extern BYTE bCBar; // ULTRACMP.CPP
extern int dumpo; //  VIDEO.CPP

void NoSpec ();

void SuperDump (void){ // A:* OK
#ifndef UE2
//   Out (7,"[SUPER-DUMP]\n");
   CleanNtx (); // clean up speed chains
   dwFiles=0;
   dwTSize=0;
   dwDirs=0;
   OHEAD ohead;
   COMPRESS compress;
   MakePipe (pipe);
   BoosterOn();
   ClearMasRefLen();
   TCD (VNULL);
//   Out (7,"[SUPER-DUMP 2]\n");
   ListDirs ();
//   Out (7,"[SUPER-DUMP 3]\n");
   ListFiles ();
//   Out (7,"[SUPER-DUMP 4]\n");
   WalkDirs();
//   Out (7,"[SUPER-DUMP 5]\n");
   BoosterOff();
   dumpo=1;
   NotifyNew();
   COut(3|8,"\n\r");
   dumpo=0;
   ListMast (pipe); // totals are OK now, masters are written right NOW
//   Out (7,"[SUPER-DUMP 6]\n");
   BoosterOn();
   ohead.bType = BO_EOL;
   WritePipe (pipe, (BYTE *)&ohead, sizeof (OHEAD));
   WritePipe (pipe, (BYTE *)&(xTail[iArchArea]), sizeof (XTAIL));
   compress.dwCompressedLength = 0; // not important
   compress.wMethod = MODE.bCompressor;
   if (compress.wMethod>9) compress.wMethod=4; // nono multimedia
   UpdateVersion();
   compress.dwMasterPrefix = NOMASTER;
   // record start of CDIR
   AWTell (&(xhead[iArchArea].locCdir.dwVolume),
	  &(xhead[iArchArea].locCdir.dwOffset));
   BoosterOff();
   if (CONFIG.dSerial && getenv("UC2_ANONYMOUS"))
      CONFIG.dSerial = 1;
   WritePipe (pipe, (BYTE *)&(CONFIG.dSerial), 4);
//   Out (7,"[SUPER-DUMP 7]\n");
   Out (1,"\x7\Compressing central directory ");
   StartProgress (-1, 1);
   if (compress.wMethod>=20 && compress.wMethod<30){
      compress.wMethod-=20;
   } // never use DELTA on central directory
   AWrite ((BYTE *)&compress, sizeof (COMPRESS));
   if (CONFIG.fOut==1) bCBar=1;
//   Out (7,"[SUPER-DUMP 8]\n");
   Compressor (compress.wMethod, PRead, AWrite, NOMASTER);
//   Out (7,"[SUPER-DUMP 9]\n");
   NoSpec ();
   bCBar=0;
   Out (1|8,"\n\r");
   xhead[iArchArea].wFletch=Fletcher(&Fin); // keep check of CDIR
   ClosePipe(pipe);
//   Out (7,"[SUPER-DUMP 10]\n");
#endif
}

extern int somban;
extern int nobas, nodel, noadd, noopt, nounp, norev;

extern WORD maxtime, maxdate;

DWORD dwArchSerial;

void SuperGet (void){ // A:*
   somban=0;
   CleanNtx (); // clean up speed chains
   dwTotalSize=0;
   DWORD dwPrevDir=4100000000L;
   BYTE fast;
   BYTE pbPrev[11];
   dwFiles=dwDirs=dwTSize=0;
   COMPRESS compress;
   OHEAD ohead;
   OSMETA osmeta;
   FILEMETA filemeta;
   LOCATION location;
   MASMETA masmeta;
   if (xhead[iArchArea].wVersionNeededToExtract > 202){
      FatalError (145,"you need at least UltraCompressor %d revision %d for this archive",
		  xhead[iArchArea].wVersionNeededToExtract/100,
		  xhead[iArchArea].wVersionNeededToExtract%100);
   }
   ARSeek (xhead[iArchArea].locCdir.dwVolume,
	   xhead[iArchArea].locCdir.dwOffset);
   ARead ((BYTE *)&compress, sizeof (COMPRESS));
   MakePipe (pipe);
   Decompressor (compress.wMethod, ARead, PWrite, NOMASTER,100000000L);
   if (xhead[iArchArea].wFletch!=Fletcher(&Fout)){
      Error (90,"central directory is damaged");
      if (!fContinue) FatalError (200,"you should repair this archive with 'UC T'");
      probeo=1;
   }
   BoosterOn();
   ReadPipe (pipe, (BYTE *)&ohead, sizeof (OHEAD));
   while (ohead.bType!=BO_EOL){
      switch (ohead.bType){
	 case BO_MAST:
	    ReadPipe (pipe, (BYTE *)&masmeta, sizeof (MASMETA));
	    VPTR mas = LocMacNtx (masmeta.dwIndex);
	    ReadPipe (pipe, (BYTE *)&(((MASREC*)V(mas))->compress), sizeof(COMPRESS));
	    ReadPipe (pipe, (BYTE *)&(((MASREC*)V(mas))->location), sizeof(LOCATION));
	    ((MASREC*)V(mas))->masmeta = masmeta;
	    ((MASREC*)V(mas))->bStatus = MS_OLD;
	    RegNtxKey (masmeta.dwIndex, masmeta.dwKey);
	    break;
	 case BO_FILE:
	    dwFiles++;
	    ReadPipe (pipe, (BYTE *)&osmeta, sizeof (OSMETA));
	    if ((osmeta.wDate>maxdate) || ((osmeta.wDate==maxdate)&&(osmeta.wTime>maxtime))){
	       maxdate=osmeta.wDate;
	       maxtime=osmeta.wTime;
	    }
	    ReadPipe (pipe, (BYTE *)&filemeta, sizeof (FILEMETA));
	    ReadPipe (pipe, (BYTE *)&compress, sizeof(COMPRESS));
	    ReadPipe (pipe, (BYTE *)&location, sizeof(LOCATION));
	    dwTotalSize+=filemeta.dwLength;
	    if (MODE.bDTT){
	       DWORD spec1 = ((DWORD)osmeta.wDate<<16L)+(DWORD)osmeta.wTime;
	       DWORD spec2 = *((DWORD*)(&MODE.ftDTT));
	       if (spec1>spec2) goto skkip;
	    }
	    fast=0;
	    if (osmeta.dwParent==dwPrevDir) fast=1;
	    dwPrevDir = osmeta.dwParent;
	    if (!fast && !TCDN(osmeta.dwParent)){
	       Error (90,"fatal damage in central directory");
	       if (!fContinue) FatalError (200,"you should repair this archive with 'UC T'");
	       probeo=1;
	       goto leave;
	    }
	    VPTR rev;
	    if (fast && (0==memcmp(osmeta.pbName,pbPrev,11)))
	       rev = TMFagain (osmeta.pbName);
	    else {
	       if (fast)
		  rev = TMFrapid (osmeta.pbName);
	       else
		  rev = TMF (osmeta.pbName);
	    }
	    REVNODE *rn = (REVNODE *)V(rev);
	    memcpy (&(rn->osmeta), &(osmeta), sizeof(OSMETA));
	    if (osmeta.tag) {
	       VPTR tmp=DGet();
	       rn = (REVNODE *)V(rev);
	       rn->extnode=tmp;
	    } else
	       rn->extnode=VNULL;
	    memcpy ((BYTE *)&(rn->filemeta), &filemeta, sizeof(FILEMETA));
	    dwTSize+=rn->filemeta.dwLength;
	    if (pulsar()) NotifyOld();
	    memcpy ((BYTE *)&(rn->compress), &compress, sizeof(COMPRESS));
	    memcpy ((BYTE *)&(rn->location), &location, sizeof(LOCATION));
	    memcpy (pbPrev, osmeta.pbName, 11);

	    if (osmeta.pbName[0]=='U'){
	       if (osmeta.pbName[1]=='$'){
		  if (0==memcmp (osmeta.pbName, "U$~BAN  GIF", 11)) somban=1;
		  if (0==memcmp (osmeta.pbName, "U$~BAN  JPG", 11)) somban=1;
		  if (0==memcmp (osmeta.pbName, "U$~BAN  TXT", 11)) somban=1;
		  if (0==memcmp (osmeta.pbName, "U$~BAN  MOD", 11)) somban=1;
		  if (0==memcmp (osmeta.pbName, "U$~BAN  ASK", 11)) somban=1;

		  if (0==memcmp (osmeta.pbName, "U$~NOBASLCK", 11)) nobas=1;
		  if (0==memcmp (osmeta.pbName, "U$~NODELLCK", 11)) nodel=1;
		  if (0==memcmp (osmeta.pbName, "U$~NOADDLCK", 11)) noadd=1;
		  if (0==memcmp (osmeta.pbName, "U$~NOOPTLCK", 11)) noopt=1;
		  if (0==memcmp (osmeta.pbName, "U$~NOUNPLCK", 11)) nounp=1;
		  if (0==memcmp (osmeta.pbName, "U$~NOREVLCK", 11)) norev=1;
	       }
	    }

	    rn->bStatus = FST_OLD;
	    ((REVNODE *)V(rev))->vpMas = LocMacNtx (rn->compress.dwMasterPrefix);
	    rn = (REVNODE *)V(rev);
	    strcpy (((MASREC*)V(rn->vpMas))->szName, Rep2Name (rn->osmeta.pbName));
	    AddToNtx (rn->compress.dwMasterPrefix, rev, rn->filemeta.dwLength);
	    break;
	 case BO_DIR:
	    dwDirs++;
	    if (pulsar()) NotifyOld();
	    ReadPipe (pipe, (BYTE *)&osmeta, sizeof (OSMETA));
	    if (!TCDN(osmeta.dwParent)){
	       probeo=1;
	       Error (90,"fatal damage in central directory");
	       if (!fContinue) FatalError (200,"you should repair this archive with 'UC T'");
	       goto leave;
	    }
	    VPTR dir = TMLDF (osmeta.pbName);
	    DIRNODE *dn = (DIRNODE *)V(dir);
	    dn->bStatus = DST_OLD;
	    memcpy (&(dn->osmeta), &(osmeta), sizeof(OSMETA));

	    ReadPipe (pipe, (BYTE *)&(dn->dirmeta), sizeof(DIRMETA));
	    if (osmeta.tag) {
	       VPTR tmp=DGet();
	       DIRNODE *dn = (DIRNODE *)V(dir);
	       dn->extnode=tmp;
	    } else
	       dn->extnode=VNULL;
	    if (dn->dirmeta.dwIndex >= dwDirNTX[iArchArea]){
	       dwDirNTX[iArchArea] = dn->dirmeta.dwIndex+1;
	       // notice that TMLDF alters the index as well
	       // this is just a precaution to avoid problems with
	       // "unnatural" archives
	    }
	    break;
	 default:
	    probeo=1;
	    Error (90,"fatal damage in central directory");
	    if (!fContinue) FatalError (200,"you should repair this archive with 'UC T'");
	    goto leave;
      }
skkip:
      ReadPipe (pipe, (BYTE *)&ohead, sizeof (OHEAD));
   }
   ReadPipe (pipe, (BYTE *)&(xTail[iArchArea]), sizeof (XTAIL));
leave:
   ReadPipe (pipe, (BYTE *)&dwArchSerial, 4);
//   Out (7,"[%s]\n\r",neat(dwArchSerial));
   ClosePipe (pipe);
   BoosterOff();
   dumpo=1;
   NotifyOld();
   COut (1|8,"\n\r");
   dumpo=0;
}

/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
*/

static void DOIT (ToToFun ttf, VPTR rev){
   VPTR dir = TWD();
   TCD (((REVNODE*)V(rev))->vpDir);
   DWORD rn = TRevNo (rev);
   if (rn!=DELREV) (*ttf)(TWD(), rev, rn);
   TCD (dir);
}

static void TTFiles (ToToFun ttf){
   VPTR walk = TFirstFileNN();
   while (!IS_VNULL(walk)){
      if (TRevs()){
	 VPTR rwlk = TRev(0);
	 DWORD ctr = 0;
ok:
//         (*ttf)(TWD(), rwlk, ctr);
	 DOIT (ttf, rwlk);
#ifdef UCPROX
	 if (heavy){
	     if (!CMP_VPTR(TWD(),((REVNODE*)V(rwlk))->vpDir)) IE();
//	     if (ctr!=TRevNo(rwlk)) IE();
	 }
#endif
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

static void TTDirs (ToToFun ttf){
   VPTR walk = TFirstDir();
   while (!IS_VNULL(walk)){
      if (((DIRNODE*)V(walk))->bStatus==DST_DEL) goto skip;
      TCD (walk);
      TTFiles (ttf);
      TTDirs(ttf);
      TUP ();
skip:
      walk = TNextDir();
   }
}

void ToToWalk (ToToFun ttf){ // allows inter revision delete!
   VPTR cur = TWD();
   TCD(VNULL);
   TTFiles (ttf);
   TTDirs (ttf);
   TCD(cur);
}

void ToToWalk (ToToFun ttf, DWORD dwIndex, WORD idx){
   VPTR cur = TWD();
   VPTR walk = GetFirstNtx (dwIndex, idx);
   while (!IS_VNULL(walk)){
      if (((REVNODE*)V(walk))->bStatus!=FST_DEL) DOIT (ttf, walk);
      walk = ((REVNODE*)V(walk))->vpSis;
   }
   TCD(cur);
}

static int iHan;

static WORD far pascal Reader (BYTE *buf, WORD len){
   WORD ret = Read (buf, iHan, len);
   Hint();
#ifdef UCPROX
   if (debug) Out (7,"{{Reader call %u, returns %u}}\n\r",len,ret);
#endif
   return ret;
}

static void far pascal Writer (BYTE *buf, WORD len){
   Write (buf, iHan, len);
}

#pragma argsused
static void far pascal VWriter (BYTE *buf, WORD len){
}

char *Full (VPTR rev){
   static char ret[120];
   strcpy (ret, (char *)V(((REVNODE*)V(rev))->vpDLink));
   strcat (ret, Rep2Name(((REVNODE*)V(rev))->osmeta.pbName));
   NoR (ret);
   if (ret[0]=='.' && ret[1]=='\\'){
      return ret+2;
   }
   return ret;
}

char *WFull (VPTR rev, DWORD hint){
   static char ret[220];
   static char tmp[15];
   if (!IS_VNULL(((REVNODE*)V(rev))->vpDLink))
      strcpy (ret, (char *)V(((REVNODE*)V(rev))->vpDLink));
   else
      strcpy (ret, "");
   strcat (ret, Rep2Name(((REVNODE*)V(rev))->osmeta.pbName));
   DWORD r;
   if (hint==UNK)
      r = TRevNo(rev);
   else
      r = hint;
   if (r==DELREV) r=0;
   if (r) strcat (ret, ";");
   if (r) strcat (ret, ultoa(r, tmp, 10));
   RepTmp (ret);
   if (ret[0]=='.' && ret[1]=='\\'){
      return ret+2;
   }
   return ret;
}

extern DWORD dwInCtr; // COMPINT.CPP

static int filter;
static DWORD ntx;

extern WORD hhint, llen, ddst;

extern int fquickdel;

WORD Analyze (char *file, WORD comp, int handle, DWORD master);

extern int factor;

extern int skipstat;

int skcount=0;

#pragma argsused
// add a single file
void TTafl (VPTR dir, VPTR rev, DWORD no){
#ifndef UE2
   REVNODE* revn = (REVNODE*)V(rev);
   if (revn->bStatus==FST_SPEC||
       revn->bStatus==FST_DISK){
	 if (filter&&((REVNODE*)V(rev))->compress.dwMasterPrefix!=ntx)
	    return;
	 if (((REVNODE*)V(rev))->bSpecial){ // && (MODE.bCompressor<10)){
	    WORD wLen;
	    Transfer (NULL, &wLen, ((REVNODE*)V(rev))->compress.dwMasterPrefix); // determine length
	    hhint = 1;
	    llen = ((REVNODE*)V(rev))->wLen;
	    ddst = wLen-((REVNODE*)V(rev))->wOfs;
	 }
	 if (hhint && ((MODE.bCompressor==4)||(MODE.bCompressor==5)) &&
	    (((MASREC*)V(LocMacNtx(((REVNODE*)V(rev))->compress.dwMasterPrefix)))
	       ->compress.wMethod==0)){
	    hhint=0;
	 }
	 if (!hhint){
	    if (skipstat==2) goto skipit;
	    iHan = Open (Full(rev), RO|TRY|CRT);
	 }

	 if (hhint || (iHan!=-1)){
	    AWTell (&(((REVNODE*)V(rev))->location.dwVolume),
		    &(((REVNODE*)V(rev))->location.dwOffset));
	    if (!ValidMaster(((REVNODE*)V(rev))->compress.dwMasterPrefix)){
	       ((REVNODE*)V(rev))->compress.dwMasterPrefix = SUPERMASTER;
	       if (stricmp(getenv("UC2_PUC"),"ON")==0)
		  ((REVNODE*)V(rev))->compress.dwMasterPrefix = NOMASTER;
	    }
            Out (3,"\x7\Compressing %s ",WFull(rev,no));
            StartProgress ((int)(((REVNODE*)V(rev))->filemeta.dwLength/256), 3);
	    if (movemode) MoveAdd (Full(rev));
	    if (MODE.fRab) RabAdd (Full(rev));

	    if (!hhint){
	       WORD meth = ((REVNODE*)V(rev))->compress.wMethod =
		   Analyze ("", MODE.bCompressor, iHan,
			    ((REVNODE*)V(rev))->compress.dwMasterPrefix);
	       if (((MODE.bCompressor==4)||(MODE.bCompressor==5)) &&
		  (((MASREC*)V(LocMacNtx(((REVNODE*)V(rev))->compress.dwMasterPrefix)))
		     ->compress.wMethod==0)){
		  ((MASREC*)V(LocMacNtx(((REVNODE*)V(rev))->compress.dwMasterPrefix)))
		     ->compress.wMethod=meth;
//		  Out (7,"[M%d]",meth);
	       }
	    } else {
	       if (MODE.bCompressor>10){
		  ((REVNODE*)V(rev))->compress.wMethod = 3;
	       } else {
		  ((REVNODE*)V(rev))->compress.wMethod = MODE.bCompressor;	       }
//	       if ((MODE.bCompressor==4)||(MODE.bCompressor==5)){
//		  Out (3,"\7þ");
//	       }
	    }
	    UpdateVersion();
	    ResetOutCtr();
#ifdef UCPROX
	    if (debug) Out (7,"{{master %lu, size of file %lu}}\n\r",
		     ((REVNODE*)V(rev))->compress.dwMasterPrefix,
		     ((REVNODE*)V(rev))->filemeta.dwLength);
#endif
	    Out (4,"\x7");
            Out (4|8,"Add %s ",WFull(rev,no));
	    dwInCtr=0;
	    bCBar=1;
	    Compressor (((REVNODE*)V(rev))->compress.wMethod, Reader, AWrite, ((REVNODE*)V(rev))->compress.dwMasterPrefix);
	    bCBar=0;
	    if (hhint){
	       ((REVNODE*)V(rev))->filemeta.dwLength = llen;
	       ((REVNODE*)V(rev))->filemeta.wFletch = ((REVNODE*)V(rev))->wFletch;
	       ((REVNODE*)V(rev))->compress.dwCompressedLength = GetOutCtr();
	    } else {
	       ((REVNODE*)V(rev))->filemeta.dwLength = dwInCtr;
	       ((REVNODE*)V(rev))->filemeta.wFletch = Fletcher(&Fin);
	       ((REVNODE*)V(rev))->compress.dwCompressedLength = GetOutCtr();
	    }
#ifdef UCPROX
	    if (debug) Out (7,"{{compressed file size %lu  fletch %d}}\n\r",GetOutCtr(),
			    ((REVNODE*)V(rev))->filemeta.wFletch);
#endif
	    if (!hhint) Close (iHan);
	    Out (3,"\x5""DONE\x7");
	    if ((MODE.bCompressor==4)||(MODE.bCompressor==5)){
	       WORD tmp=((REVNODE*)V(rev))->compress.wMethod;
	       if (hhint){
		  tmp = ((MASREC*)V(LocMacNtx(((REVNODE*)V(rev))->compress.dwMasterPrefix)))
		     ->compress.wMethod;
		  if (!tmp) tmp=((REVNODE*)V(rev))->compress.wMethod;
	       }
	       switch (tmp){
		  case 30:
		  case 40:
		     Out (3|8," (8 bit)");
		     break;
		  case 31:
		  case 41:
		     Out (3|8," (16 bit)");
		     break;
		  case 32:
		  case 42:
		     Out (3|8," (24 bit)");
		     break;
		  case 33:
		  case 43:
		     Out (3|8," (32 bit)");
		     break;
		  default:
		     Out (3|8,"");
		     break;
	       }
	    }

	    if(os2&&CONFIG.fEA){
	       DWORD ssiz;
	       if ((ssiz=GetOS2ExtAttrSize (Full(rev)))!=0){
		  strcpy (EAtpt, TmpFile((char *)CONFIG.pbTPATH,1,".TMP"));
		  if (Exists(EAtpt)) Delete (EAtpt);

		  BYTE *dat;
		  WORD len;
		  GetDat (&dat, &len);
		  if (len<ssiz+16){
		     UnGetDat ();
		     sprintf (EAline,"EAUTIL /s /p %s %s",Full(rev),EAtpt);
		     ssystem (EAline);
		  } else {
		     int handle = Open (EAtpt, CR|MUST|NOC);
		     GetOS2ExtAttr (Full(rev), dat, len);
		     Write (dat, handle, (WORD)ssiz);
		     Close (handle);
		     UnGetDat();
		  }

		  if (Exists(EAtpt)){
		     int f = Open (EAtpt,RO|MUST|CRT);
		     DWORD ctr = GetFileSize (f);
		     if (ctr!=0){
			// create EXTNODE and fill fields
			VPTR en = Vmalloc (sizeof(EXTNODE));
			((EXTNODE*)V(en))->extmeta.size = ctr;
			strcpy (((EXTNODE*)V(en))->extmeta.tag, TAG_OS2EA);

			// add EXTNODE to revision informtion
			((EXTNODE*)V(en))->next = ((REVNODE*)V(rev))->extnode;
			((REVNODE*)V(rev))->extnode = en;

			VPTR walk;
			if (ctr>900)
			   walk = Vmalloc (sizeof (EXTDAT));
			else
			   walk = Vmalloc (8+(WORD)ctr);
			((EXTNODE*)V(en))->extdat = walk;
			while (ctr){
			   WORD trn = (ctr<1000) ? (WORD)ctr : 1000;
			   Read (((EXTDAT*)V(walk))->dat, f, trn);
			   ctr-=trn;
			   if (ctr){
			      VPTR tmp = ((EXTDAT*)V(walk))->next = Vmalloc (sizeof (EXTDAT));
			      walk = tmp;
			   }
			}
			Out (3,"\x7  [OS/2 EA stored]");
		     }
		     Close (f);
		     Delete (EAtpt);
		  }

		  // aaa record filetags from external filesystem
	       }
	    }

	    if (fquickdel) Delete(Full(rev));

	    hhint=0;
	    ((REVNODE*)V(rev))->bStatus = FST_NEW;
	    Out (7|8,"\n\r");
	    if (!MODE.fInc){ // none incremental mode, get rid of prev version
	       VPTR walk = ((REVNODE*)V(rev))->vpOlder;
	       while (!IS_VNULL(walk)){
//		  if (((REVNODE*)V(walk))->bStatus==FST_SPEC){
//		     ((REVNODE*)V(walk))->bStatus=FST_DEL;
//		     break;
//		  }
//		  if (((REVNODE*)V(walk))->bStatus==FST_DISK){
//		     ((REVNODE*)V(walk))->bStatus=FST_DEL;
//		     break;
//		  }
		  if (((REVNODE*)V(walk))->bStatus==FST_OLD){
		     ((REVNODE*)V(walk))->bStatus=FST_DEL;
		     break; // only delete single revision
		  }
//		  if (((REVNODE*)V(walk))->bStatus==FST_NEW){
//		     ((REVNODE*)V(walk))->bStatus=FST_DEL;
//		     break;
//		  }
		  break;
		  VPTR walk = ((REVNODE*)V(walk))->vpOlder;
	       }
	    }
	 } else {
skipit:
	    ((REVNODE*)V(rev))->bStatus = FST_DEL;
	    ((REVNODE*)V(rev))->filemeta.dwLength= 1000000000L;
            if (++skcount>15) {
               if (skcount==16)
                  Warning (30,"skipping more than 15 files");
            } else
               Warning (30,"skipped file %s",WFull(rev,no));
	 }
   }
#endif
}

extern VPTR masroot[];

void NoSpec ();

void AddFiles (){
#ifndef UE2
   skipstat=1;
   VPTR walk = masroot[iArchArea];
   while (!IS_VNULL(walk)){
      if (((MASREC*)V(walk))->bStatus != MS_NEW1){
	 filter = 1;
	 ntx = ((MASREC*)V(walk))->masmeta.dwIndex;
	 if (((MASREC*)V(walk))->bStatus!=MS_OLD){
	    ToToWalk (TTafl,ntx,1);
	    ToToWalk (TTafl,ntx,2);
	    ToToWalk (TTafl,ntx,0);
	 }
	 NoSpec();
      }
      walk = ((MASREC*)V(walk))->vpNext;
   }
   filter=0;
   ToToWalk (TTafl, SUPERMASTER,1);
   ToToWalk (TTafl, SUPERMASTER,2);
   ToToWalk (TTafl, SUPERMASTER,0);
   ToToWalk (TTafl, NOMASTER,1);
   ToToWalk (TTafl, NOMASTER,2);
   ToToWalk (TTafl, NOMASTER,0);
   skipstat=0;
#endif
}

static void NotifyTrn (void){
   COut (1,"\x7Transfered %lu files (%s bytes) from old to new archive\r",dwFiles,neat(dwTSize));
}


#pragma argsused
void TTcfl (VPTR dir, VPTR rev, DWORD no){
   REVNODE* revn = (REVNODE*)V(rev);
   if (revn->bStatus==FST_OLD){
      ((REVNODE*)V(rev))->bStatus = FST_NEW;
      if (!MODE.fInc){
	 ARSeek ((((REVNODE*)V(rev))->location.dwVolume),
		 (((REVNODE*)V(rev))->location.dwOffset));
	 AWTell (&(((REVNODE*)V(rev))->location.dwVolume),
		 &(((REVNODE*)V(rev))->location.dwOffset));
	 Old2New (((REVNODE*)V(rev))->compress.dwCompressedLength);
      }
      dwFiles++;
      dwTSize+=((REVNODE*)V(rev))->filemeta.dwLength;
      if (dwFiles && pulsar()) NotifyTrn();
   }
}

void CopyFiles (){
   dwFiles = dwTSize = 0;
   ToToWalk (TTcfl); // qqq sort on masters here
   if (dwFiles!=0){
      dumpo=1;
      NotifyTrn();
      COut (1|8,"\n\r");
      dumpo=0;
   }
}

#pragma argsused
void TTpfl (VPTR dir, VPTR rev, DWORD no){
   REVNODE* revn = (REVNODE*)V(rev);
   if (revn->bStatus==FST_MARK){
      revn->bStatus=FST_DEL;
   }
}

void ProcessFiles (){
   ToToWalk (TTpfl); // qqq sort on masters here
}

extern BYTE bDBar; // ULTRACMP.CPP

void CacheIt (int);

extern int closeme;

#pragma argsused
// extract a single file
void TTefl (VPTR dir, VPTR rev, DWORD no){
   struct ffblk ffblk;
   REVNODE* revn = (REVNODE*)V(rev);
   BrkQ();
   if (revn->bStatus==FST_MARK){
      if (filter&&((REVNODE*)V(rev))->compress.dwMasterPrefix!=ntx)
	 return;
      if (!movemode) revn->bStatus = FST_OLD;
      if (MODE.fPrf){
#ifndef UE2
	 if (MODE.fQuery==2) goto skipit;
	 if (MODE.fQuery){
	    Menu ("\x6\Print file %s ?",WFull(rev,no));
	    Option ("",'Y',"es");
	    Option ("",'N',"o");
	    Option ("",'A',"ll files");
	    Option ("N",'o'," more files");
	    switch(Choice()){
	       case 1:
		  break;
	       case 2:
		  goto skipit;
	       case 3:
		  MODE.fQuery=0;
		  break;
	       case 4:
		  MODE.fQuery=2;
		  goto skipit;
	    }
	 }
	 Out (3,"\x7\Printing file %s ",WFull(rev,no));
         Out (4,"\x7\Print %s ",WFull(rev,no));
         StartProgress (-1, 3);
	 ARSeek ((((REVNODE*)V(rev))->location.dwVolume),
		 (((REVNODE*)V(rev))->location.dwOffset));
	 bDBar=1;
	 iHan = Open ("U$~PRF.TMP", MUST|CR|CWR);
	 Decompressor (((REVNODE*)V(rev))->compress.wMethod, ARead, Writer,
		       ((REVNODE*)V(rev))->compress.dwMasterPrefix,
		       ((REVNODE*)V(rev))->filemeta.dwLength);
	 Close (iHan);
	 bDBar=0;
	 ((REVNODE*)V(rev))->bStatus = FST_OLD; // mark as extracted
	 if (((REVNODE*)V(rev))->filemeta.wFletch==Fletcher(&Fout)){
	    char com[400];
	    sprintf (com, "%s U$~PRF.TMP %s %s", LocateF("U2_PRINT.BAT ",2), pcArchivePath, WFull(rev,no));
	    ssystem (com);
	    Out (3,"\x5\OK\n\r");
	 } else {
	    Doing ("printing file %s",WFull(rev,no));
	    Error (90,"file is damaged");
	    probeo=1;
	 }
	 Delete ("U$~PRF.TMP");
      } else if (iVerifyMode){
	 Out (3,"\x7\Verifying %s ",WFull(rev,no));
         StartProgress ((int)(((REVNODE*)V(rev))->filemeta.dwLength/32768L), 3);
	 ARSeek ((((REVNODE*)V(rev))->location.dwVolume),
		 (((REVNODE*)V(rev))->location.dwOffset));
	 bDBar=1;
	 Decompressor (((REVNODE*)V(rev))->compress.wMethod, ARead, VWriter,
		       ((REVNODE*)V(rev))->compress.dwMasterPrefix,
		       ((REVNODE*)V(rev))->filemeta.dwLength);
	 bDBar=0;
	 ((REVNODE*)V(rev))->bStatus = FST_OLD; // mark as extracted
	 if (((REVNODE*)V(rev))->filemeta.wFletch==Fletcher(&Fout)){
	    Out (3,"\x5\OK\n\r");
	 } else {
	    Doing ("verifying file %s",WFull(rev,no));
	    Error (90,"file is damaged");
	    probeo=1;
	 }
#endif
      } else {
	 smskip=0;

	 int ask;

	 // user chose to skip the rest
	 if (MODE.fQuery==2) goto skipit;

	 // check for smart skipping
	 if (MODE.fSMSkip && (findfirst (Full(rev), &ffblk, 0xF7))==0){
	    REVNODE *rv = (REVNODE*)V(rev);
	    if ((rv->osmeta.wTime == ffblk.ff_ftime) &&
		(rv->osmeta.wDate == ffblk.ff_fdate) &&
		(rv->filemeta.dwLength == ffblk.ff_fsize)){
	       smskip=1;
	    }
	 }

	 // check for !NOF and !NEWER
	 if ((findfirst (Full(rev), &ffblk, 0xF7))==0){
	    if (MODE.fNof) goto skipit;
	    REVNODE *rv = (REVNODE*)V(rev);
	    DWORD arch = ((DWORD)rv->osmeta.wDate<<16L)+(DWORD)rv->osmeta.wTime;
	    DWORD disk = ((DWORD)ffblk.ff_fdate<<16L)+(DWORD)ffblk.ff_ftime;
	    if (MODE.fNewer && (arch<=disk)) goto skipit;
	 }

	 // ask user
	 if (!smskip){
	    ask=1;
	    if (Exists (Full (rev))){
	       if (!(MODE.bExOverOpt==2)){ // not always
		  if (MODE.bExOverOpt!=3){ // not NEVER
		     ask=0;
		     Menu ("\x6\Overwrite file %s ?",WFull(rev,DELREV));
		     Option ("",'Y',"es");
		     Option ("",'N',"o");
		     Option ("",'A',"lways overwrite files");
		     Option ("N",'e',"ver overwrite files");
		     switch(Choice()){
			case 1:
			   break;
			case 2:
skipit:
			   ((REVNODE*)V(rev))->bStatus=FST_OLD;
			   goto skipfile;
			case 3:
			   MODE.bExOverOpt = 2;
			   break;
			case 4:
			   MODE.bExOverOpt = 3;
			   goto skipit;
		     }
		  } else {
		     Out (3,"\x7\Skipping %s (already exists)\n\r",WFull(rev,no));
		     goto skipit;
		  }
	       }
	    }
#ifndef UE2
	    if (MODE.fQuery && ask){
	       Menu ("\x6\Extract file %s ?",WFull(rev,DELREV));
	       Option ("",'Y',"es");
	       Option ("",'N',"o");
	       Option ("",'A',"ll files");
	       Option ("N",'o'," more files");
	       switch(Choice()){
		  case 1:
		     break;
		  case 2:
		     goto skipit;
		  case 3:
		     MODE.fQuery=0;
		     break;
		  case 4:
		     MODE.fQuery=2;
		     goto skipit;
	       }
	    }
#endif
	 }

	 if (!smskip && Exists(Full(rev))) Delete (Full(rev));

doit:
	 if (!smskip) closeme = iHan = Open (Full(rev), TRY|CR|CWR);
	 if (smskip || (iHan!=-1)){
	    if (smskip){
	       Out (3,"\x7\Smart skipping %s ",WFull(rev,no));
	    } else {
	       Out (3,"\x7\Decompressing %s ",WFull(rev,no));
               StartProgress ((int)(((REVNODE*)V(rev))->filemeta.dwLength/32768L), 3);
	       Out (4,"\x7\Extract %s",WFull(rev,no));
	       ARSeek ((((REVNODE*)V(rev))->location.dwVolume),
		       (((REVNODE*)V(rev))->location.dwOffset));
	       bDBar=1;
	       Decompressor (((REVNODE*)V(rev))->compress.wMethod, ARead, Writer,
			     ((REVNODE*)V(rev))->compress.dwMasterPrefix,
			     ((REVNODE*)V(rev))->filemeta.dwLength);
	       bDBar=0;
	       Close (iHan);
	       closeme=-1;
	    }
	    iHan = Open (Full(rev), MUST|RW|NOC);
	    CSB;
	       _dos_setftime (Han(iHan),((REVNODE*)V(rev))->osmeta.wDate,
				((REVNODE*)V(rev))->osmeta.wTime);
	    CSE;
	    Close (iHan);
	    CSB;
	       _dos_setfileattr (Full(rev),((REVNODE*)V(rev))->osmeta.bAttrib);
	    CSE;

//	    ((REVNODE*)V(rev))->bStatus = FST_OLD; // mark as extracted
	    int err=0;
	    if (smskip || (((REVNODE*)V(rev))->filemeta.wFletch==Fletcher(&Fout))){
	       if (os2){
		  VPTR wlk = ((REVNODE*)V(rev))->extnode;
		  while (!IS_VNULL(wlk)){
		     if (0==strcmp(((EXTNODE*)V(wlk))->extmeta.tag,TAG_OS2EA) && !noextrea){
			strcpy (EAtpt, TmpFile((char *)CONFIG.pbTPATH,1,".TMP"));
			int f = Open (EAtpt,CR|MUST|CWR);
			VPTR walk=((EXTNODE*)V(wlk))->extdat;
			DWORD ctr=((EXTNODE*)V(wlk))->extmeta.size;
			DWORD ssiz=((EXTNODE*)V(wlk))->extmeta.size;
			while (ctr){
			   WORD trn = ctr<1000 ? (WORD)ctr : 1000;
			   Write (((EXTDAT*)V(walk))->dat,f,trn);
			   ctr-=trn;
			   walk = ((EXTDAT*)V(walk))->next;
			}
			Close (f);

			BYTE *dat;
			WORD len;
			GetDat (&dat, &len);
			if (len<ssiz+16){
			   UnGetDat();
			   sprintf (EAline,"EAUTIL /o /j %s %s",Full(rev),EAtpt);
			   ssystem (EAline);
			} else {
			   int handle = Open (EAtpt, RO|MUST|NOC);
			   Read (dat, handle, (WORD)ssiz);
			   Close (handle);
			   SetOS2ExtAttr (Full(rev), dat);
			   UnGetDat();
			   Delete (EAtpt);
			}

			if (Exists(EAtpt)){
			   Error (75,"failed to set OS/2 extended attribute");
			   Delete (EAtpt);
			   err=1;
			} else {
			   err=2;
			}

			break; // max 1 EA
		     }
		     wlk = ((EXTNODE*)V(wlk))->next;
		  }
	       }

	       if (err!=1) Out (3,"\x5\OK");
	       if (err==2) Out (3,"\x7  [OS/2 EA extracted]");
	       Out (7,"\n\r");
	    } else {
	       Doing ("decompressing %s",WFull(rev,no));
	       Error (90,"file is damaged");
	       if (!fContinue) FatalError (200,"you should repair this archive with 'UC T'");
	    }
	 } else {
	    Error (80,"cannot write to %s, file skipped",Full(rev));
	 }
skipfile:;
      }
   }
}

void ExtractFiles (){
   VPTR walk = masroot[iArchArea];
   // process files sorted on master
   while (!IS_VNULL(walk)){
      filter = 1;
      ntx = ((MASREC*)V(walk))->masmeta.dwIndex;
      if (((MASREC*)V(walk))->bStatus!=MS_OLD){
	 ToToWalk (TTefl, ntx, 1);
	 ToToWalk (TTefl, ntx, 2);
	 ToToWalk (TTefl, ntx, 0);
      }
      walk = ((MASREC*)V(walk))->vpNext;
   }
   filter=0;
   // process files with no master (=supermaster)
   ToToWalk (TTefl, NOMASTER,1);
   ToToWalk (TTefl, NOMASTER,2);
   ToToWalk (TTefl, NOMASTER,0);
   ToToWalk (TTefl, SUPERMASTER,1);
   ToToWalk (TTefl, SUPERMASTER,2);
   ToToWalk (TTefl, SUPERMASTER,0);
}

void StartSearch (void);
void Investigate (BYTE *data, WORD len);
long Found (void);

static void far pascal COWriter (BYTE *buf, WORD len){
   Investigate (buf, len);
}

int doquery;
char *qres;

#pragma argsused
// extract a single file
void TTchk (VPTR dir, VPTR rev, DWORD no){
#ifndef UE2
   REVNODE* revn = (REVNODE*)V(rev);
   if (revn->bStatus==FST_MARK || revn->bStatus==FST_DEL){
      if (filter&&((REVNODE*)V(rev))->compress.dwMasterPrefix!=ntx)
	 return;
      if (!doquery){
	 Out (3,"\x7\Searching file %s ",WFull(rev,no));
         StartProgress (-1, 3);
	 ARSeek ((((REVNODE*)V(rev))->location.dwVolume),
		 (((REVNODE*)V(rev))->location.dwOffset));
	 bDBar=1;
	 StartSearch();
	 Decompressor (((REVNODE*)V(rev))->compress.wMethod, ARead, COWriter,
		       ((REVNODE*)V(rev))->compress.dwMasterPrefix,
		       ((REVNODE*)V(rev))->filemeta.dwLength);
	 if (!Found()){
	    revn->bStatus=FST_OLD;
	    Out (3," NO MATCHES\n\r");
	 } else {
	    if (Found()==1)
	       Out (3,"\x5 1 MATCH\n\r\x7");
	    else
	       Out (3,"\x5 %ld MATCHES\n\r\x7",Found());
	    if (bDeleteMode && !MODE.fQuery){
	       Out (3,"\x7\Deleting %s\n\r",WFull (rev,no));
	       Out (4,"\x7\Delete %s\n\r",WFull (rev,no));
	    }
	 }
	 bDBar=0;
	 if (((REVNODE*)V(rev))->filemeta.wFletch!=Fletcher(&Fout)){
	    Doing ("decompressing %s",WFull(rev,no));
	    Error (90,"file is damaged");
	    FatalError (200,"you should repair this archive with 'UC T'");
	 }
      } else {
	 if ((MODE.fQuery!=0) && (MODE.fQuery!=3)){
	    if (MODE.fQuery==2) goto okk;
	    Menu ("\x6%s file %s ?", qres, WFull(rev,no));
	    Option ("",'Y',"es");
	    Option ("",'N',"o");
	    Option ("",'A',"ll files");
	    Option ("N",'o'," more files");
	    switch(Choice()){
	       case 1:
		  break;
	       case 2:
okk:
		  revn->bStatus=FST_OLD; // undo delete
		  break;
	       case 3:
		  MODE.fQuery=3;
		  break;
	       case 4:
		  MODE.fQuery=2;
		  goto okk;
	    }
	 }
	 if (bDeleteMode && revn->bStatus!=FST_OLD){
	    Out (3,"\x7\Deleting %s\n\r",WFull (rev,no));
	    Out (4,"\x7\Delete %s\n\r",WFull (rev,no));
	 }

      }
   }
#endif
}

void CheckFiles (){
   VPTR walk = masroot[iArchArea];
   // process files sorted on master
   while (!IS_VNULL(walk)){
      filter = 1;
      ntx = ((MASREC*)V(walk))->masmeta.dwIndex;
      if (((MASREC*)V(walk))->bStatus!=MS_OLD){
	 ToToWalk (TTchk, ntx, 1);
	 ToToWalk (TTchk, ntx, 2);
	 ToToWalk (TTchk, ntx, 0);
      }
      walk = ((MASREC*)V(walk))->vpNext;
   }
   filter=0;
   // process files with no master (=supermaster)
   ToToWalk (TTchk, NOMASTER,1);
   ToToWalk (TTchk, NOMASTER,2);
   ToToWalk (TTchk, NOMASTER,0);
   ToToWalk (TTchk, SUPERMASTER,1);
   ToToWalk (TTchk, SUPERMASTER,2);
   ToToWalk (TTchk, SUPERMASTER,0);
   Out (3,"\n\r");
}

void QueryFiles (char *res){
   qres=res;
   doquery=1;
   CheckFiles();
   doquery=0;
}

