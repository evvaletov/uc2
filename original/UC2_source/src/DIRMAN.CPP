#include <sys\stat.h>
#include <dos.h>
#include <string.h>
#include <direct.h>
#include <io.h>
#include <stdio.h>
#include <dir.h>
#include <stdlib.h>
#include "main.h"
#include "vmem.h"
#include "comoterp.h"
#include "dirman.h"
#include "video.h"
#include "handle.h"
#include "llio.h"
#include "handle.h"
#include "diverse.h"
#include "menu.h"

struct lloo {
   char bDisk;
   char pcPath[260];
   VPTR next;
};

VPTR root=VNULL;

unsigned char gbDisk=2; // default c:
char gpcPath[260];

static unsigned char sbDisk;
static char spcPath[260];

static unsigned char sbBDisk;
static char spcBPath[260];

static unsigned char sbEDisk;
static char spcEPath[260];

void Keep (void){
   unsigned char bDisk;
   char pcPath[260];
   CSB;
      bDisk = getdisk();
      getcwd (pcPath,99);
   CSE;
   VPTR tmp = Vmalloc (sizeof (lloo));
   ((lloo*)V(tmp))->bDisk = bDisk;
   strcpy (((lloo*)V(tmp))->pcPath, pcPath);
   ((lloo*)V(tmp))->next = root;
   root = tmp;
}

void Back (void){
   if (IS_VNULL(root)) IE();
   unsigned char bDisk;
   char pcPath[260];
   bDisk=((lloo*)V(root))->bDisk;
   strcpy (pcPath,((lloo*)V(root))->pcPath);
   CSB;
      setdisk (bDisk);
      chdir (pcPath);
   CSE;
   VPTR tmp = root;
   root = ((lloo*)V(root))->next;
   Vfree (tmp);
}

char szTPath[260];

void GKeep (void){
   CSB;
      gbDisk = getdisk();
      getcwd (gpcPath,199);
      strcpy (szTPath, gpcPath);
   CSE;
}

void GKeep2 (void){
   static int first=1;
   if (!first) return;
   first=0;
   CSB;
      _getdcwd (CONFIG.pbTPATH[0]-'A'+1, szTPath, 199);
   CSE;
}

void GBack (void){
   CSB;
      setdisk (gbDisk);
      chdir (gpcPath);
      chdir (szTPath);
   CSE;
}

void SKeep (void){
   CSB;
      sbDisk = getdisk();
      getcwd (spcPath,199);
   CSE;
}

void SBack (void){
   CSB;
      setdisk (sbDisk);
      chdir (spcPath);
   CSE;
}

void BKeep (void){
   CSB;
      sbBDisk = getdisk();
      getcwd (spcBPath,199);
   CSE;
}

void BBack (void){
   CSB;
      setdisk (sbBDisk);
      chdir (spcBPath);
   CSE;
}

void EKeep (void){
   CSB;
      sbEDisk = getdisk();
      getcwd (spcEPath,199);
   CSE;
}

void EBack (void){
   CSB;
      setdisk (sbEDisk);
      chdir (spcEPath);
   CSE;
}

void ChPath (char *path){
   strupr (path);
   if (path[1]==':'){
      CSB;
	 setdisk (path[0]-'A');
      CSE;
      path+=2;
   }
   int ret;
   CSB;
      ret = chdir (path);
   CSE;
   if (ret!=0)
      FatalError (185,"failed to change directory into %s",path);
}

int TstPath (char *path){
   if (path[0]==0) return 1; // ""
   if ((path[0]=='\\')&&(path[1]==0)) return 1; // "\"
   if ((path[0]=='.')&&(path[1]==0)) return 1; // "."
   if ((path[0]=='\\')&&(path[1]=='.')&&(path[2]==0)) return 1; // "\."
   if ((path[0]=='.')&&(path[1]=='\\')&&(path[2]==0)) return 1; // ".\"
   if (path[1]==':'){
       if (path[2]==0) return 1; // "c:"
       if ((path[2]=='\\')&&(path[3]==0)) return 1; // "c:\"
       if ((path[2]=='.') && (path[3]==0)) return 1; // "c:."
       if ((path[2]=='\\') && (path[3]=='.') && (path[4]==0)) return 1; // "c:\."
       if ((path[2]=='.') && (path[3]=='\\') && (path[4]==0)) return 1; // "c:.\"
   }

   int ret=1;
   struct ffblk ffblk;
   CSB;
      if (findfirst (path, &ffblk, 0xF7)==0){
	 if (!(ffblk.ff_attrib&FA_DIREC)) ret=0; // not a dir
      } else
	 ret=0;
   CSE;
   return ret;
}

int RCMD (char *path){
   if (0==strcmp(path,".")) return 1;
   int ret;
   CSB;
      ret = mkdir(path)==0;
   CSE;
   if (!ret){
      for (int i=strlen(path);i>0;i--){
	 if (path[i]=='\\' && path[i-1]!=':'){
	    path[i]=0;
	    if (!TstPath(path)){
		if (RCMD (path)){
		   path[i]='\\';
                   if (0==strcmp(path,".")) return 1;
		   CSB;
		      ret = mkdir(path)==0;
		   CSE;
		   if (ret)
		      return 1;
		   else {
		      goto giveup;
		   }
		} else {
		   path[i]='\\';
		   goto giveup;
		}
	    }
	 }
      }
giveup:
      if (0==strcmp(path,".")) return 1;
      Error (85,"cannot create directory %s",path);
   }
   return ret;
}

int MkPath (char *path){
   if (MODE.bMKDIR!=3){ // not NEVER
      if (MODE.bMKDIR!=2){ // not ALWAYS
	 Menu ("\x6\Create directory %s ?",path);
	 Option ("",'Y',"es");
	 Option ("",'N',"o");
	 Option ("",'A',"lways create directories");
	 Option ("N",'e',"ver create directories");
	 switch(Choice()){
	    case 1:
	       goto doit;
	    case 2:
	       return 0;
	    case 3:
	       MODE.bMKDIR = 2;
	       goto doit;
	    case 4:
	       MODE.bMKDIR = 3;
	       return 0;
	 }
      }
doit:
      return RCMD (path);
   } else
      return 0;
}

char tmppath[260]="*************************";

int tmpp=0;
void ktmp (void){
   if (tmpp==1){
      Out (1,"\x7\Removing temporary files/directories ");
      StartProgress (-1, 1);
      KillTmpPath();
      EndProgress();
      Out (1,"\n\r");
   }
}

int fktmp=0;

char* MkTmpPath (void){
   if (tmpp==0) fktmp=1;
   tmpp=1;
   strcpy (tmppath, (char *)CONFIG.pbTPATH);
   strcpy (tmppath, TmpFile (tmppath,1,".TMP"));
   int f = Open (tmppath, CR|MAY|NOC);
   if (f==-1){
      strcpy (tmppath, TmpFile ((char *)CONFIG.pbTPATH,1,".TMP"));
      f = Open (tmppath, CR|MAY|NOC);
      if (f==-1){
	 strcpy (tmppath, TmpFile ((char *)CONFIG.pbTPATH,1,".TMP"));
	 f = Open (tmppath, CR|MUST|NOC);
      }
   }
   Close (f);
   Delete (tmppath);
   CSB;
      mkdir (tmppath);
   CSE;
   return tmppath;
}

static int ctr;

void blk (void){
   Hint();
}

static int special;
static int mmask;

void KillPath (char *p){
   int sp=special;
   special=0;
   tmpp=2;
   struct ffblk ffblk;
   int ret;
   char nam[15];
   Keep();
   ChPath (p);
   int done;
again:
   CSB;
      done = findfirst ("*.*",&ffblk,0xF7);
   CSE;
sppwb:
   CSB;
sppw:
      if (!done && strcmp(ffblk.ff_name,".")==0)
	 done = findnext (&ffblk);
      if (!done && strcmp(ffblk.ff_name,"..")==0){
	 done = findnext (&ffblk);
	 goto sppw;
      }
      if (!done && mmask && (strstr(ffblk.ff_name, ".U~K")!=NULL)){
	 strupr (ffblk.ff_name);
	 done = findnext (&ffblk);
	 goto sppw;
      }
   CSE;
   if (!done){
      BrkQ();
      blk();
      strcpy (nam, ffblk.ff_name);
      if (ffblk.ff_attrib&FA_DIREC){
	 KillPath (nam);
      } else {
	 Delete (nam);
	 done = findnext (&ffblk);
	 goto sppwb;
      }
      goto again;
   }
delfail:
   if (sp) return;
   ChPath ("..");
   CSB;
      ret = rmdir (p);
   CSE;
   Back();
   if (ret!=0){
      if (!TstPath(p)) return; // some glitch in OS/2 2.x ???
      Error (60,"failed to delete directory %s",p);
      static int derr=0;
      derr++;
      if (derr==100) FatalError (180,"attempt to delete directory failed too often");
   }
}

void SKillPath (char *p){
   mmask=1;
   special=1;
   KillPath (p);
   mmask=0;
   special=0;
}

void KillTmpPath (void){
   ctr=0;
   KillPath (tmppath);
}
