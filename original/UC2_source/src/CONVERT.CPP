#include <dir.h>
#include <alloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dos.h>

#include "main.h"
#include "llio.h"
#include "vmem.h"
#include "superman.h"
#include "video.h"
#include "dirman.h"
#include "ultracmp.h"
#include "archio.h"
#include "comoterp.h"
#include "neuroman.h"
#include "handle.h"
#include "tagman.h"
#include "diverse.h"

static DWORD maxrev;

static BYTE pbCLabel[11];
extern XTAIL xTail [MAX_AREA];    // archive extended tail

extern int scan1;

static void ListFiles (){
   VPTR walk = TFirstFileNN();
   while (!IS_VNULL(walk)){
      if (TRevs()>maxrev) maxrev=TRevs();
      DWORD i=TRevs()-1;
again:
      ((REVNODE*)V(TRev(i)))->bStatus=FST_OLD;
      if (i!=0){
         i--;
         goto again;
      }
      walk = TNextFileNN();
   }
}

static void WalkDirs (){
   VPTR walk = TFirstDir();
   while (!IS_VNULL(walk)){
      TCD (walk);
      ListFiles ();
      WalkDirs();
      TUP ();
      walk = TNextDir();
   }
}

#ifndef UE2
static DWORD MaxRev (){
   maxrev=0;
   TCD (VNULL);
   ListFiles ();
   WalkDirs();
   return maxrev;
}
#endif

char* NewExt (char *filename){
   static char tmp[MAXPATH];
   char drive[MAXDRIVE];
   char dir[MAXDIR];
   char file[MAXFILE];
   char ext[MAXEXT];
   fnsplit (filename,drive,dir,file,ext);
   if (stricmp (getenv("UC2_PUC"),"ON")==0)
      fnmerge (tmp,drive,dir,file,".PU2");
   else
      fnmerge (tmp,drive,dir,file,".UC2");
   return tmp;
}

#define UE2_LABEL 0x324555L  // "UE2" (notice Intel ordering)

int TType (char *filename, char *aext, int mustexist){
   char drive[MAXDRIVE];
   char dir[MAXDIR];
   char file[MAXFILE];
   char ext[MAXEXT];
   char *expp;
   char tmp[260];
   fnsplit (filename,drive,dir,file,ext);
   if (aext!=NULL){
      if (ext[0]!=0)
	 strcpy (aext, ext+1);
      else
	 aext[0]=0;
      expp = aext;
   } else {
      if (ext[0]!=0)
	 expp=ext+1;
      else
	 expp=ext;
   }
   if (!Exists(filename)) return 0;
   int i;
   if (mustexist)
      i=Open (filename, MUST|RO|NOC);
   else
      i=Open (filename, TRY|RO|NOC);
   if (i==-1) return 0; // non existent, or turned down
   DWORD l;
   Read ((BYTE *)&l, i, 4);
   Close (i);
   if (l==UC2_LABEL) return 1; // UC2
   if ((l&0xFFFFFFL)==UE2_LABEL) return 4; // UE2
   sprintf (tmp,"U2_EX%s.BAT",expp);
   if (LocateF (tmp,1)) return 2; // known type
   return 3; // unknown type
}

extern char pcEXEPath [260];
extern BYTE bExtractMode;
void Anal (char *s, int fSelect);
extern struct MODE MODE;
void RClearMask (void);
extern VPTR Mpath;
extern VPTR Uqueue;
extern VPTR Mpath;

int spstat=0;
char *spdel;

extern BYTE bPDamp[MAX_AREA];
extern BYTE bDamp[MAX_AREA];
void spdodel (void){
   if (spstat==1) Delete (spdel);
}

int fhypermode=0;
int fbackward=0;
int fsecretskipmode=1;
int fquickdel=0;

// mode 1=full extract,
//      2=only if file has single revision,
//      3=only if file has NOT single revision
#pragma argsused
void CExtract (char *archive, char *path, DWORD revision, int mode){
#ifndef UE2
   char com[200];

   VPTR UQ = Uqueue;
   VPTR MP = Mpath;
   struct MODE m=MODE;
   Uqueue = VNULL;
   Mpath = VNULL;

   fsecretskipmode = mode;

   InvHashC(); // Needed for each archive being processed!
   ReadArchive (archive);
   memcpy (pbCLabel, xTail[iArchArea].pbLabel, 11);
   RClearMask();

   ChPath (path);

   bExtractMode=1;
   MODE.fSubDirs=1;
   MODE.bExOverOpt = 2;
   MODE.bMKDIR = 2;
   MODE.fSMSkip=0;
   MODE.bHid=2;

   InvHashC(); // Needed for each archive being processed!
   SetArea (0);
   sprintf (com,"*.*;%ld",revision);
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
   RClearMask();

   CloseArchive();

   fsecretskipmode = 1;

   Mpath = MP;
   Uqueue = UQ;
   MODE=m;
   InvHashC();
#endif
}

extern int scan1;

#pragma argsused
void CAdd (char *archive, char *path, int hypermode, int backward){
#ifndef UE2
   char com[200];

   scan1 = 0;
   VPTR UQ = Uqueue;
   VPTR MP = Mpath;
   struct MODE m=MODE;
   Uqueue = VNULL;
   Mpath = VNULL;

   fhypermode = hypermode;

   MODE.fInc=1;
   MODE.bAddOpt = 2;
   MODE.fSubDirs=1;
   MODE.fSMSkip=0;
   MODE.bHid=2;
   IncUpdateArchive (archive);

   memcpy (xTail[iArchArea].pbLabel, pbCLabel, 11);

   RClearMask();
   sprintf (com,"%s\\*.*",path);
   Anal (com, 1);
   MODE.fSubDirs=1;

   BoosterOn();
   VPTR walk = Mpath;
   fbackward = backward;
   scan1=0;
   while (!IS_VNULL(walk)){
      ScanAdd (VNULL, walk, 0);
      walk = ((MPATH *)V(walk))->vpNext;
   }
   if (scan1) Out (2|8,"\n\r");
   fbackward = 0;
   BoosterOff();
   RClearMask();

   AddAccess (MODE.fInc); // read & create needed masters
   AddFiles (); // add new files
   CloseArchive ();

   fhypermode = 0;

   Mpath = MP;
   Uqueue = UQ;
   MODE = m;
   InvHashC();
#endif
}


int fspdodel=0;

extern int noopt, nounp;

int fconv=0;
static char ConvDel [MAXPATH];
static char TagsDel [MAXPATH];

void exitc (void){
   if (Exists (ConvDel)) Delete (ConvDel);
   if (Exists (TagsDel)) Delete (TagsDel);
   fconv=0;
}

int Test (char *pcArchivePath);

int convertmode=0;

int conto=0;

extern char fContinue;

extern char noextrea;
extern char noskip;

extern int convback;

#pragma argsused
void Convert (char *filename, int type){
#ifndef UE2
   char tmp[260];
   char pcTmpFile [MAXPATH];
   char pcTagFile [MAXPATH];
   noopt=0;

   if (spstat==0) fspdodel=1;
   spstat=2;
   VPTR UQ = Uqueue;
   VPTR MP = Mpath;
   struct MODE m=MODE;

   MODE.bAddOpt = 2;
   MODE.fSubDirs=1;
   MODE.fSMSkip=0;
   MODE.bHid=2;
   MODE.bMKDIR = 2;

   Uqueue = VNULL;
   Mpath = VNULL;

   char ext[5];
   char tfile[260];
   char com[400];
   int t;
//   Out (7,"%ld ",farcoreleft());
   if (type==2){
      t=1;
   } else
      t=TType (filename, ext, 0);
//   Out (7,"%ld ",farcoreleft());
//   IE();
   switch (t){
      case 4:
	 FatalError (125,"file %s is encrypted with UltraCrypt",filename);
	 break;
      case 0:
	 FatalError (130,"file %s does not exist",filename);
	 break;
      case 1: // already UC2
	 sprintf (tmp,"U2_EX%s.BAT",ext);
	 if (LocateF (tmp,1)) goto anyway; // known convertable type after all
	 else {
	    noskip=1;
	    BYTE oldea = CONFIG.fEA;
	    CONFIG.fEA=0;
	    noextrea=1;
	    if (type==0){
	       if (conto){
		  Error (65,"file %s is already in UC2 format",filename);
		  return;
	       } else
		  FatalError (205,"file %s is already in UC2 format",filename);
	    }
	    DWORD i, revs;
	    SetArea (0);
	    if (CONFIG.bRelia==2){
	       if (!Test (filename)){
		  FatalError (200,"you should repair this archive with 'UC T'");
	       }
	    }

	    // create temp file
	    strcpy (tfile, TmpFile ((char *)CONFIG.pbTPATH,1,".TMP"));
	    int f = Open (tfile, CR|MAY|NOC);
	    if (f==-1){
	       strcpy (tfile, TmpFile ((char *)CONFIG.pbTPATH,1,".TMP"));
	       f = Open (tfile, CR|MAY|NOC);
	       if (f==-1){
		  strcpy (tfile, TmpFile ((char *)CONFIG.pbTPATH,1,".TMP"));
		  f = Open (tfile, CR|MUST|NOC);
	       }
	    }
	    Close (f);
	    spdel=tfile;
	    CopyFile (filename, tfile);
	    int shield = Open (filename, RO|MUST|NOC); // allow no updates
	    spstat=1; // mark tmp file for future deletion

	    convertmode=1;

	    // Analyze archive to be converted
	    Out (3,"\x4\Analyzing archive depth\n\r");
	    RClearMask();
	    InvHashC(); // Needed for each archive being processed!
	    bExtractMode=0; // do NOT create directories (yet)
	    ReadArchive (tfile);
	    int pdampo = bPDamp[iArchArea];
	    if (noopt) FatalError (140,"archive is protected against optimizing");
	    if ((MODE.bDamageProof==2) && nounp){
	       FatalError (140,"archive is protected against removal of damage protection");
	    }
	    revs = MaxRev();
	    RClearMask();
	    CloseArchive();
	    if (revs){
	       Out (3,"\x4Highest revision number is %ld\n\r",revs-1);
	    } else {
	       Out (3, "\x4\x41rchive contains no revisions\n\r");
	    }

	    // Create new empty archive with correct damage protection mode
	    strcpy (pcTmpFile,TmpFile(filename,0,".TMP"));
	    strcpy (ConvDel, pcTmpFile);
	    strcpy (pcTagFile,TmpFile(filename,0,".TMP"));
	    strcpy (TagsDel, pcTagFile);
	    Out (3,"\x4\Saving archive tags\n\r");
	    DumpTags (tfile, pcTagFile);

	    fconv=1;
	    Out (3,"\x4\Creating empty target archive\n\r");
	    GBack();
	    InvHashC(); // Needed for each archive being processed!
	    MODE.fInc = 0;
	    CreateArchive (pcTmpFile);
	    bDamp[iArchArea]=pdampo;
	    if (MODE.bDamageProof!=0){ // no damage protection specs available
	       if (MODE.bDamageProof==1){
		  bDamp[iArchArea] = 1; // specific protect command
	       } else {
		  bDamp[iArchArea] = 0; // specific unprotect command
	       }
	    }
	    pdampo = bDamp[iArchArea];
	    CloseArchive();

	    // create temp path
	    fquickdel = 1;
	    char *tp = MkTmpPath();

	    if (revs-1) for (i=0;i<revs;i++){
	       Out (3,"\x4\Processing revision %ld files \n\r",i);

	       CExtract (tfile, tp, i, 3);
	       GBack();
	       CAdd (pcTmpFile, tp, 1, 1);
	    }

	    Out (3,"\x4\Processing revision free files \n\r");

	    CExtract (tfile, tp, 0, 2);
	    GBack();
	    CAdd (pcTmpFile, tp, 0, 0);

	    ChPath ("\\"); // ensure tdir isn't active, OS/2 2.x
	    Out (3,"\x7\Removing temporary files/directories ");
            StartProgress (-1, 3);
	    KillTmpPath();
	    GBack();
	    EndProgress();
            Out (3,"\n\r");

	    Out (3,"\x4\Restoring archive tags\n\r");
	    ReadTags (pcTmpFile, pcTagFile);
	    Delete (pcTagFile);

	    DWORD olds = GetFileSize (shield);
	    Close (shield);

	    int tmp = Open (pcTmpFile, RO|MUST|NOC);
	    DWORD news = GetFileSize (tmp);
	    Close (tmp);

	    if (news>=olds && !fContinue
	        && (stricmp (getenv("UC2_PUC"),"ON")!=0)){
	       Warning (15,"archive size has not changed");
	       Delete (pcTmpFile);
	    } else {
	       DFlush("secure storage of tmp archive");
	       Delete (filename);
	       Rename (pcTmpFile, filename);
	       DFlush("secure storage of result archive");
	       if (!fContinue && (news<olds)){
		  Out (3,"\x5Old = %s bytes, ", neat(olds));
		  Out (3,"new = %s bytes ", neat(news));
		  long per = (news*1000)/olds;
		  long aper = 1000-per;
		  Out (3,"(%ld.%1ld%% reduction, %ld.%1ld%% left)\r\n",aper/10,aper%10,per/10,per%10);
	       }
	    }

	    fconv=0;
	    fquickdel = 0;

	    GBack();
	    Delete (tfile);
	    spstat=2;
	    convertmode=0;
	    CONFIG.fEA=oldea;
	    noextrea=0;
	    noskip=1;
	 }
	 break;
      case 2: // non UC2
anyway:
	 if (Exists(NewExt(filename))){
	    if (conto){
	       Error (65,"cannot convert %s to %s, %s already exists",filename,NewExt(filename),NewExt(filename));
	       return;
	    } else
	       FatalError (205,"cannot convert %s to %s, %s already exists",filename,NewExt(filename),NewExt(filename));
	 }
	 char *tp = MkTmpPath();
	 char useext[10];
	 sprintf (useext, ".%s",ext);
	 strcpy (tfile, TmpFile ((char *)CONFIG.pbTPATH,1,useext));
	 int f = Open (tfile, CR|MAY|NOC);
	 if (f==-1){
	    strcpy (tfile, TmpFile ((char *)CONFIG.pbTPATH,1,useext));
	    f = Open (tfile, CR|MAY|NOC);
	    if (f==-1){
	       strcpy (tfile, TmpFile ((char *)CONFIG.pbTPATH,1,useext));
	       f = Open (tfile, CR|MUST|NOC);
	    }
	 }
	 Close (f);
	 CopyFile (filename, tfile);
	 spstat=1;
	 spdel=tfile;
	 ChPath (tp);
	 char tmp[20];
	 sprintf (tmp,"U2_EX%s.BAT",ext);
	 sprintf (com,"%s %s %s",LocateF(tmp,1),tfile,_argv[0]);
	 Out (3,"\x4\Expanding archive %s\n\r",filename);
	 Out (7,"\x7   \n\r");
	 ssystem (com);
	 ssystem (LocateF("U2_XTRA.BAT",2));
	 if (!Exists(TMPPREF"chk1") || !Exists(TMPPREF"chk2")){
	    if (Exists(TMPPREF"chk1")) Delete (TMPPREF"chk1");
	    if (Exists(TMPPREF"chk2")) Delete (TMPPREF"chk2");
	    if (conto)
	       Error (65,"something went wrong during decompression of %s", filename);
	    else
	       FatalError (205,"something went wrong during decompression of %s", filename);
errr:
	    Delete (tfile);
	    spstat=2;
	    GBack();
	    Out (3,"\x7\Removing temporary files/directories ");
            StartProgress (-1, 3);
	    KillTmpPath();
	    EndProgress();
            Out (3,"\n\r");
	    goto error;
	 }
	 Out (3,"\x5\Archive %s has been successfully expanded\n\r",filename);
	 Delete (TMPPREF"chk1");
	 Delete (TMPPREF"chk2");
	 if (CONFIG.fVscan){
	    sprintf (com,"%s",LocateF("U2_CHECK.BAT",2));
	    Out (3,"\x4\Testing files (virus checking etc.)\n\r");
	    Out (7,"\x7   \n\r");
	    ssystem (com);
	    if (!Exists(TMPPREF"chk3")){
	       if (conto)
		  Error (95,"something went wrong during testing of files (virus checking etc.)");
	       else
		  FatalError (190,"something went wrong during testing of files (virus checking etc.)");
	       goto errr;
	    }
	    Out (3,"\x5\U2_CHECK did not report problems.\n\r");
	    Delete (TMPPREF"chk3");
	 }
	 GBack();
	 InvHashC(); // Needed for each archive being processed!
	 SetArea (0);
	 MODE.fInc = 0;

	 CreateArchive (NewExt(filename));
	 if (MODE.bDamageProof!=0){ // no damage protection specs available
	    if (MODE.bDamageProof==1){
	       bDamp[iArchArea] = 1; // specific protect command
	    } else {
	       bDamp[iArchArea] = 0; // specific unprotect command
	    }
	 }

	 RClearMask();
	 sprintf (com,"%s\\*.*",tp);
	 Anal (com, 1);
	 MODE.fSubDirs=1;

	 BoosterOn();
	 VPTR walk = Mpath;
	 scan1=0;
	 while (!IS_VNULL(walk)){
	    ScanAdd (VNULL, walk, 0);
	    walk = ((MPATH *)V(walk))->vpNext;
	 }
	 if (scan1) Out (2|8,"\n\r");
	 BoosterOff();
	 RClearMask();

	 AddAccess(MODE.fInc); // read & create needed masters
	 AddFiles(); // add new files
	 CopyFiles(); // copy (old) files
	 CloseArchive();

	 ChPath ("\\"); // ensure tdir isn't active, OS/2 2.0
	 Out (3,"\x7\Removing temporary files/directories ");
         StartProgress (-1, 3);
	 KillTmpPath();
	 EndProgress();
         Out (3,"\n\r");
	 GBack();
	 Delete (tfile);
	 spstat=2;
	 DFlush("secure storage of result archive");
	 int tm = Open (filename, RO|MUST|NOC);
	 DWORD olds = GetFileSize (tm);
	 Close (tm);
	 tm = Open (NewExt(filename), RO|MUST|NOC);
	 DWORD news = GetFileSize (tm);
	 Close (tm);

	 Out (3,"\x5Old = %s bytes, ", neat(olds));
	 Out (3,"new = %s bytes ", neat(news));
	 if (news<olds){
	    long per = (news*1000)/olds;
	    long aper = 1000-per;
	    Out (3,"(%ld.%1ld%% reduction, %ld.%1ld%% left)\r\n",aper/10,aper%10,per/10,per%10);
	 } else {
            Out (3,"\r\n");
         }
         if (!convback)
	    Delete (filename); // everything went perfect, delete old archive
	 break;
      case 3:
	 Error (65,"file %s has an unknown format",filename);
	 break;
   }
error:
   Mpath = MP;
   Uqueue = UQ;
   MODE=m;
   InvHashC();
#endif
}
