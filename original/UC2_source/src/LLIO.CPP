// Copyright 1992, all rights reserved, AIP, Nico de Vries
// LLIO.CPP

#include <sys\stat.h>
#include <stdio.h>
#include <io.h>
#include <dos.h>
#include <sys\stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <mem.h>
#include <string.h>
#include <share.h>
#include <dir.h>
#include <alloc.h>
#include "main.h"
#include "video.h"
#include "diverse.h"
#include "llio.h"
#include "handle.h"
#include "mem.h"
#include "vmem.h"

#define UNKN 0xFFFFFFFFL

#define DEFCACHETAB 300 // bigger than max cache buffers in MEM suport

#define NONE 0   // no cache
#define WRIT 1   // deferred write cache
#define READ 2   // read cache

struct AFD {
// low-level
   char pcFileName[120];
   int iIntHandle;
   DWORD dwPos;  // correct or UNKN, pos of first byte = 0
   DWORD dwLen;  // correct or UNKN
// cached level
   VPTR cachet;  // table of cache blocks
   int maxentry;   // number of entries in cachet-1
   WORD cmode;     // cache operation mode
   DWORD offset;   // offset of cached partition (0==start of file)
   DWORD size;     // size of cached partition
   DWORD vfptr;    // virtual file pointer 0==first byte (only valid if cmode!=0)
   // if cmode==WRIT then offset+size==vfptr
};

AFD* afdl[10]; // initially all 0

AFD afdlst[10];

void diskfull (char *);

static void MacAcc (char *pcPath){ // internal function to get access to file
   int attr;
   int fail=0;
   CSRA;
      fail=1;
   CSS;
      if (!fail) attr = _chmod (pcPath, 0);
   CSE;
   if (!fail && attr!=-1){
      attr &= 0xFFFF ^ FA_RDONLY ^ FA_HIDDEN ^ FA_SYSTEM;
      CSRA;
         fail=1;
      CSS;
         if (!fail) _chmod (pcPath, 1, attr);
      CSE;
   }
}

/*** START OF INTERNAL I/O FUNCTIONS ***
 *
 *   Functions to replace DOS ones with smart-seek and advanced error handling.
 */

int Han (int iHan){
   return afdl[iHan]->iIntHandle;
}

static int opro; // for exists

void PreFab (void);

static char *hap[21] = {
   "NUL",
   "CON",
   "AUX",
   "PRN",
   "COM1",
   "COM2",
   "COM3",
   "COM4",
   "LPT1",
   "LPT2",
   "LPT3",
   "LPT4",
   "EMMXXXX0",
   "XMSXXXX0",
   "CACHE$$$",
   "386LOAD$",
   "MS$MOUSE",
   "CLOCK$",
   "DBLSSYS$",
   "MVPROAS",
   "386MAX$"
};
#define NO 21

static int I_Open (char *pcP, BYTE how){
   char pcPath[260];
   strcpy (pcPath,pcP);
   if (how!=RO){
      for (int i=0;i<NO;i++){
	 if (strncmp (pcPath, hap[i], strlen (hap[i]))==0){
	    if ((pcPath[strlen(hap[i])]==0) || (pcPath[strlen(hap[i])]=='.')){
	       movmem (pcPath, pcPath+1, 258);
	       pcPath[0]='_';
	       Warning (10,"mapped (device)name %s to %s",pcP,pcPath);
	    }
	 }
      }
   }
   TRACEM ("Enter I_Open");
   opro=0;
   int acc;
   int fail=0;
   if (!(how&7)) IE();
   switch (how&56){
      case RO:
	 acc = O_RDONLY | O_BINARY;
	 if (CONFIG.fNet) acc |= SH_DENYWR;
	 break;
      case RW:
	 MacAcc (pcPath);
	 acc = O_RDWR | O_BINARY;
	 if (CONFIG.fNet) acc |= SH_DENYRW;
	 break;
      case CR:
	 MacAcc (pcPath);
	 acc = O_RDWR | O_BINARY | O_CREAT | O_TRUNC;
	 if (CONFIG.fNet) acc |= SH_DENYRW;
	 break;
      default:
	 IE();
   }
   int han;
again:
   if (how&MUST){
      CSB;
	 han = open (pcPath, acc, S_IREAD | S_IWRITE);
      CSE;
   } else {
      CSRA;
	 if (how&TRY){
	    char *why;
	    switch (how&56){
	       case RO:
		  why = "read from";
		  break;
	       case RW:
		  why = "write to";
		  break;
	       case CR:
		  why = "create";
		  break;
	    }
	    if (!CeAskOpen (pcPath,why,1)) fail=1;
	 } else {
	    fail=1;
	 }
      CSS;
	 if (!fail) han = open (pcPath, acc, S_IREAD | S_IWRITE);
      CSE;
      if (fail){
	 opro=1; // file MIGHT exists but can't be opened
	 TRACEM("Leave I_Open");
	 return -1; // failed to open file
      }
   }
   if (han==-1){
      if ((how&TRY) && !fail){
	 PreFab();
	 char *why;
	 switch (how&56){
	    case RO:
	       why = "read from";
	       break;
	    case RW:
	       why = "write to";
	       break;
	    case CR:
	       why = "create";
	       break;
	 }
	 if (CeAskOpen (pcPath, why, 1)) goto again;
      } else if (how&MUST){
	 PreFab();
	 char *why;
	 switch (how&56){
	    case RO:
	       why = "read from";
	       break;
	    case RW:
	       why = "write to";
	       break;
	    case CR:
	       why = "create";
	       break;
	 }
	 CeAskOpen (pcPath, why, 0);
	 goto again;
      }
      TRACEM("Leave I_Open");
      return -1; // return 'failed to open' to caller
   }
   for (int i=0;i<10;i++){
      if (afdl[i]==NULL){
	 afdl[i] = &(afdlst[i]);
	 strcpy (afdl[i]->pcFileName, pcPath);
	 afdl[i]->iIntHandle = han;
	 afdl[i]->dwPos = 0;
	 afdl[i]->cmode=NONE;
	 afdl[i]->cachet = VNULL;
	 if (how&CR){
	    afdl[i]->dwLen = 0;
	 } else {
	    afdl[i]->dwLen = UNKN;
	 }
	 TRACEM("Leave I_Open");
	 return i;
      }
   }
   IE();
   TRACEM("Leave I_Open");
   return 0; // to satisfy compiler
}

static void I_Close (int i){
   if (-1==close (afdl[i]->iIntHandle)){
      Error (80,"failed to close file %s",afdl[i]->pcFileName);
   }
   if (!IS_VNULL(afdl[i]->cachet))
      Vfree (afdl[i]->cachet);
   afdl[i]=NULL;
}

static WORD I_Read (BYTE *bBuf, int iHan, WORD wSiz){
   static int nest;
   if ((FP_SEG(bBuf)>0x9FFF) && !nest && (wSiz<1100)){
      nest=1;
      BYTE buf[1101];
      WORD w = I_Read (buf, iHan, wSiz);
      if (w) RapidCopy (bBuf, buf, wSiz);
      nest=0;
      return w;
   }

#ifdef UCPROX
   if (debug) Out (7,"[R %s %u",afdl[iHan]->pcFileName,wSiz);
#endif
   WORD w;
   CSR;
      {CSB;
	 lseek (afdl[iHan]->iIntHandle, afdl[iHan]->dwPos, SEEK_SET);
      CSE;}
   CSS;
      w = _read (afdl[iHan]->iIntHandle, bBuf, wSiz);
   CSE;
   afdl[iHan]->dwPos+=w;
   if (w!=wSiz){
      afdl[iHan]->dwLen=afdl[iHan]->dwPos;
   }
#ifdef UCPROX
   if (debug) Out (7,"]");
#endif
   BrkQ();
   return w;
}

static void I_Write (BYTE *bBuf, int iHan, WORD wSiz){
#ifdef UCPROX
   if (debug) Out (7,"[W %s %u",afdl[iHan]->pcFileName,wSiz);
#endif
   int fail;
again:
   fail=0;
   CSR;
      {CSB;
	 while (-1== lseek (afdl[iHan]->iIntHandle, afdl[iHan]->dwPos, SEEK_SET)){
	    diskfull (afdl[iHan]->pcFileName);
	 }
      CSE;}
   CSS;
      if (wSiz!=_write (afdl[iHan]->iIntHandle, bBuf, wSiz)){
	 diskfull (afdl[iHan]->pcFileName);
	 fail=1;
	 {CSB;
	    while (-1== lseek (afdl[iHan]->iIntHandle, afdl[iHan]->dwPos, SEEK_SET)){
	       diskfull (afdl[iHan]->pcFileName);
            }
         CSE;}
      }
   CSE;
   if (fail) goto again;
   afdl[iHan]->dwPos+=wSiz;
   if (afdl[iHan]->dwLen<afdl[iHan]->dwPos){
      afdl[iHan]->dwLen = afdl[iHan]->dwPos;
   }
   BrkQ();
#ifdef UCPROX
   if (debug) Out (7,"]");
#endif
}

static DWORD I_Tell (int iHan){
   if (UNKN==afdl[iHan]->dwPos){
      CSB;
         afdl[iHan]->dwPos = (DWORD)tell (afdl[iHan]->iIntHandle);
      CSE;
   }
   return afdl[iHan]->dwPos;
}

static void I_Seek (int iHan, DWORD dwPos){
   long r=0;
   I_Tell (afdl[iHan]->iIntHandle);
   if (afdl[iHan]->dwPos!=dwPos){
again:
//      Out (7,"[SK %s %ld > %ld]",afdl[iHan]->pcFileName,afdl[iHan]->dwPos,dwPos);
      CSB;
	 r = lseek (afdl[iHan]->iIntHandle, (long)dwPos, SEEK_SET);
      CSE;
   }
   if (r==-1){
      diskfull (afdl[iHan]->pcFileName);
      goto again;
   } else {
      afdl[iHan]->dwPos=dwPos;
   }
}

static DWORD I_GetLen (int iHan){
   if (UNKN==afdl[iHan]->dwLen){
      CSB;
         afdl[iHan]->dwLen = (DWORD)filelength (afdl[iHan]->iIntHandle);
      CSE;
   }
   return afdl[iHan]->dwLen;
}

static void I_SetLen (int iHan, DWORD dwLen){
   if (dwLen==afdl[iHan]->dwLen) return;
   int r;
again:
   CSB;
      r = chsize (afdl[iHan]->iIntHandle, dwLen);
   CSE;
   if (r==-1){
      diskfull (afdl[iHan]->pcFileName);
      goto again;
   }
   afdl[iHan]->dwLen = dwLen;
}

/*
 *
 *** END OF INTERNAL I/O FUNCTIONS ***/

/*** START OF CACHE MANAGER
 *
 */

#define MIN(a,b) ( ((a)<(b)) ? (a) : (b) )

void CacheTabSize (int han){
   if (IS_VNULL(afdl[han]->cachet)){
      afdl[han]->cachet = Vmalloc (DEFCACHETAB*sizeof(int));
      afdl[han]->maxentry = DEFCACHETAB-1;
   }
}

//void q (void){
//   Out (7,"{%d}",((int*)V(afdl[2]->cachet))[1]);
//}

void Plop (char *line1, char*line2);
void PlopQ (void);
void UnPlop (void);

void Flush (int han){
//   Out (7,"[FLUSH %s]",afdl[han]->pcFileName);
   if (afdl[han]->cmode==WRIT){ // write caching
      if (afdl[han]->size){
	 BYTE *dat;
	 WORD len;
//       Out (7,"[CW=%ld]",afdl[han]->size);
	 I_Seek (han, afdl[han]->offset);
	 GetDat (&dat, &len);
//       Out (7,"FL:GetDat\n\r");
	 WORD pos=0;
	 WORD loff=0;
	 WORD fre=len;
	 BYTE *ofs=dat;
	 DWORD siz=afdl[han]->size;
	 Plop ("Secure UltraCache", "Writing from CACHE to DISK");
	 while (siz){
	    WORD trans = (WORD)MIN(MIN(fre,siz),16384-loff);
//	    q();
//	    Out (7,"[%d %d<%d]",han,((int*)V(afdl[han]->cachet))[pos], pos);
	    from16 (ofs, ((int*)V(afdl[han]->cachet))[pos], loff, trans);
	    siz-=trans;
	    loff+=trans;
	    if (loff==16384){
	       pos++;
	       loff=0;
	    }
	    fre-=trans;
	    ofs+=trans;
	    if (fre==0){
	       PlopQ();
	       I_Write (dat, han, len);
	       ofs = dat;
	       fre=len;
	    }
	 }
	 if (len-fre){
	    PlopQ();
	    I_Write (dat, han, len-fre);
	 }
//	 UnPlop();
	 UnGetDat();
//       Out (7,"FL:UnGetDat\n\r");
      }
   }
   if (afdl[han]->cmode!=NONE){ // READ & WRIT
      for (int i=0;i<((afdl[han]->size+16383)/16384);i++)
	 free16(4,((int*)V(afdl[han]->cachet))[i]);
      afdl[han]->size=0;
      afdl[han]->offset=afdl[han]->vfptr;
   }
}

void UnCache (int han){
//   Out (7,"[UNC %s]",afdl[han]->pcFileName);
   if (afdl[han]->cmode!=NONE){
      Flush (han);
      afdl[han]->cmode=NONE;
      I_Seek (han, afdl[han]->vfptr);
   }
}

void KillCache (int han){
//   Out (7,"[UNC %s]",afdl[han]->pcFileName);
   if (afdl[han]->cmode!=NONE){
      afdl[han]->cmode=NONE;
   }
}

void CacheR (int han){ // smart and easy read caching
   TRACEM("Enter CacheR");
//   Out (7,"[CAR %s]\n\r",afdl[han]->pcFileName);
   if (afdl[han]->cmode==READ){
      TRACEM("Leave CacheRT");
      return;
   }
   UnCache (han);
   afdl[han]->size=0;
   afdl[han]->cmode=READ;
   afdl[han]->offset = afdl[han]->vfptr=I_Tell (han);
   TRACEM("Leave CacheR");
}

void CacheRT (int han){ // attempt a total-cache
   TRACEM("Enter CacheRT");
//   Out (7,"[CART %s]\n\r",afdl[han]->pcFileName);
   if (afdl[han]->cmode==READ){
      if (afdl[han]->size==I_GetLen(han)){
	 TRACEM("Leave CacheRT");
	 return;
      }
   }
   UnCache (han);
   CacheR (han);
   if ((1+(I_GetLen(han)/16384L))<=coreleft16(4)){
      BYTE *dat;
      WORD len;
      GetDat (&dat, &len);
//      Out (7,"CT:GetDat\n\r");
      DWORD todo=I_GetLen(han);
      if (!todo){
	 UnGetDat();
//       Out (7,"CT1:UnGetDat\n\r");
	 TRACEM("Leave CacheRT");
	 return;
      }
      Plop ("Secure UltraCache", "Reading from DISK to CACHE");
      I_Seek (han, 0);
      afdl[han]->offset=0;
      afdl[han]->size=todo;
      WORD pos=0;
      WORD ofs=0;
      CacheTabSize (han);
      ((int*)V(afdl[han]->cachet))[pos]=malloc16(4);
      while (todo){
	 WORD red = (WORD)(MIN (len, todo));
	 PlopQ();
	 I_Read (dat, han, red);
	 BYTE *ddt=dat;
	 todo-=red;
	 while (red){
	    WORD trn=MIN (16384-ofs, red);
	    to16 (((int*)V(afdl[han]->cachet))[pos], ofs, ddt, trn);
	    ofs+=trn;
	    ddt+=trn;
	    if (ofs==16384){
	       pos++;
	       ((int*)V(afdl[han]->cachet))[pos]=malloc16(4);
	       ofs=0;
	    }
	    red-=trn;
	 }
      }
      UnGetDat();
//      UnPlop();
//      Out (7,"CT2:UnGetDat\n\r");
      if (ofs==0){
	 free16 (4, ((int*)V(afdl[han]->cachet))[pos]);
      }
   }
   TRACEM("Leave CacheRT");
}

void CacheW (int han){ // write cacheing
   TRACEM("Enter CacheW");
//   Out (7,"[CAW %s]\n\r",afdl[han]->pcFileName);
   if (afdl[han]->cmode==WRIT){
      TRACEM("Leave CacheW");
      return;
   }
   UnCache (han);
   afdl[han]->size=0;
   afdl[han]->cmode=WRIT;
   afdl[han]->offset = afdl[han]->vfptr = I_Tell(han);
   TRACEM("Leave CacheW");
}

/*
 *
 *** END OF CACHE MANAGER ***/

int Open (char *pcPath, int mode){
   TRACEM("Enter Open");
//   Out (7,"[OPEN %s]",pcPath);
   int r=I_Open (pcPath, mode);
   if (r!=-1){
      if (mode&NOC){
	 TRACEM("Leave Open");
	 return r; // no cache!
      }
      if (mode&CRI) {
	 CacheR (r);
	 TRACEM("Leave Open");
	 return r;
      }
      if (mode&CRT) {
	 CacheRT (r);
	 TRACEM("Leave Open");
	 return r;
      }
      if (mode&CWR) {
	 CacheW (r);
	 TRACEM("Leave Open");
	 return r;
      }
      IE();
   }
   TRACEM("Leave Open");
   return r;
}

extern int ex;

void Close (int iHandle){
   TRACEM("Enter Close");
   if (afdl[iHandle]==NULL)
      if (ex){
	 TRACEM("Leave Close");
	 return;
      } else IE();
   UnCache (iHandle);
   I_Close (iHandle);
   TRACEM ("Leave Close");
}

WORD Read (BYTE *bBuf, int iHandle, WORD wSize){
   TRACEM("Enter Read");
   BrkQ();
   if (afdl[iHandle]->cmode==READ){ // read cache
fit:
      if ((afdl[iHandle]->vfptr>=afdl[iHandle]->offset) &&
	  (afdl[iHandle]->vfptr+wSize<=afdl[iHandle]->offset+afdl[iHandle]->size)){
	 // data to read is all in cache
	 WORD pos = (WORD)((afdl[iHandle]->vfptr-afdl[iHandle]->offset)/16384L);
	 WORD ofs = (WORD)(afdl[iHandle]->vfptr-afdl[iHandle]->offset-pos*16384L);
	 WORD ret = wSize;
	 while (wSize){
	    WORD trn = MIN (16384-ofs,wSize);
	    from16 (bBuf, ((int*)V(afdl[iHandle]->cachet))[pos], ofs, trn);
	    wSize-=trn;
	    ofs+=trn;
	    if (ofs==16384){
	       pos++;
	       ofs=0;
	    }
	    bBuf+=trn;
	 }
	 afdl[iHandle]->vfptr+=ret;
	 TRACEM ("Leave Read");
	 return ret;
      }
      // NO FIT
      if (I_GetLen (iHandle)<(afdl[iHandle]->vfptr+wSize)){
	 if (afdl[iHandle]->vfptr>I_GetLen (iHandle)){
	    TRACEM ("Leave Read");
	    return 0;
	 }
	 wSize=(WORD)(I_GetLen (iHandle) - afdl[iHandle]->vfptr);
	 if (wSize==0){
	    TRACEM ("Leave Read");
	    return 0;
	 }
	 goto fit;
      }
      // NO FIT & NOT TOO HUNGRY
      if (wSize>16384){
	 // large block to read don't waste time on caching it
noca:
	 I_Seek (iHandle,afdl[iHandle]->vfptr);
	 WORD ret=I_Read (bBuf, iHandle, wSize);
	 afdl[iHandle]->vfptr = I_Tell (iHandle);
	 TRACEM ("Leave Read");
	 return ret;
      }
      // && NOT LARGER THAN 16384
      Flush (iHandle);
      if (!coreleft16(4)) goto noca; // no enough memory for caching
      BYTE *dat;
      WORD len;
      GetDat (&dat, &len);
//      Out (7,"RD:GetDat\n\r");
      if (len<16384){
	 UnGetDat();
//         Out (7,"RD1:UnGetDat\n\r");
	 goto noca; // caching is waste of time here
      }
      // && len>=16384
      I_Seek (iHandle, afdl[iHandle]->vfptr);
      afdl[iHandle]->offset = afdl[iHandle]->vfptr;
      if (wSize<500)
	 afdl[iHandle]->size = I_Read (dat, iHandle, 2048);
      else
	 afdl[iHandle]->size = I_Read (dat, iHandle, 16384);

      CacheTabSize (iHandle);
      ((int*)V(afdl[iHandle]->cachet))[0]=malloc16(4);
      to16 (((int*)V(afdl[iHandle]->cachet))[0], 0, dat, (WORD)afdl[iHandle]->size);
      UnGetDat();
//      Out (7,"RD2:UnGetDat\n\r");
      goto fit;
   } else if (afdl[iHandle]->cmode==WRIT){ // write cache
      UnCache (iHandle);
      WORD ret = I_Read (bBuf, iHandle, wSize);
      CacheW (iHandle);
      TRACEM ("Leave Read");
      return ret;
   } else { // no cache
      TRACEM ("Leave Read");
      return I_Read (bBuf, iHandle, wSize);
   }
}

void Write (BYTE *bBuf, int iHandle, WORD wSize){
   TRACEM ("Enter Write");
   BrkQ();
//   Out (7,"[CL %u]",coreleft16(4));
   if (afdl[iHandle]->cmode==NONE){
      I_Write (bBuf, iHandle, wSize);
   } else if (afdl[iHandle]->cmode==WRIT){
      if (coreleft16(4)<((wSize/16384)+2)){
	 Flush (iHandle);
	 if (coreleft16(4)<((wSize/16384)+2)){
	    I_Seek (iHandle, afdl[iHandle]->vfptr);
	    I_Write (bBuf, iHandle, wSize);
	    afdl[iHandle]->vfptr+=wSize;
	    afdl[iHandle]->offset=afdl[iHandle]->vfptr;
	 } else goto toca;
      } else {
toca:
	 WORD pos = (WORD)((afdl[iHandle]->vfptr-afdl[iHandle]->offset)/16384L);
	 WORD ofs = (WORD)(afdl[iHandle]->vfptr-afdl[iHandle]->offset-pos*16384L);
	 afdl[iHandle]->size+=wSize;
	 afdl[iHandle]->vfptr+=wSize;
	 CacheTabSize (iHandle);
	 if (ofs==0){
	    ((int*)V(afdl[iHandle]->cachet))[pos] = malloc16(4);
//	    q();
//	    Out (7,"[%d %d<-%d]",iHandle, ((int*)V(afdl[iHandle]->cachet))[pos], pos);
	 }
	 while (wSize){
	    WORD trn = MIN (16384-ofs, wSize);
	    to16 (((int*)V(afdl[iHandle]->cachet))[pos],ofs,bBuf,trn);
	    bBuf+=trn;
	    wSize-=trn;
	    ofs+=trn;
	    if (ofs==16384){
	       pos++;
	       ofs=0;
	       ((int*)V(afdl[iHandle]->cachet))[pos] = malloc16(4);
//	       q();
//	       Out (7,"[%d %d<-%d]",iHandle, ((int*)V(afdl[iHandle]->cachet))[pos], pos);
	    }
	 }
	 if (ofs==0){
	    free16 (4, ((int*)V(afdl[iHandle]->cachet))[pos]);
	 }
      }
   } else {
      UnCache (iHandle);
      I_Write (bBuf, iHandle, wSize);
      CacheR (iHandle);
   }
   TRACEM ("Leave Write");
}

void Seek (int iHandle, DWORD dwPos){
   if (afdl[iHandle]->cmode==NONE){
      I_Seek (iHandle, dwPos);
   } else {
      if (afdl[iHandle]->vfptr==dwPos) return;
      if (afdl[iHandle]->cmode==WRIT) {
         Flush (iHandle);
         afdl[iHandle]->offset = dwPos;
      }
      afdl[iHandle]->vfptr = dwPos;
   }
}

DWORD Tell (int iHandle){
   if (afdl[iHandle]->cmode==NONE){
      return I_Tell (iHandle);
   } else {
      return afdl[iHandle]->vfptr;
   }
}

DWORD GetFileSize (int iHandle){
   if (afdl[iHandle]->cmode!=WRIT){ // NONE or READ
      return I_GetLen (iHandle);
   } else { // WRIT
      UnCache (iHandle);
      DWORD ret = I_GetLen (iHandle);
      CacheW (iHandle);
      return ret;
   }
}

void SetFileSize (int iHandle, DWORD dwNewSize){
   if (afdl[iHandle]->cmode==NONE){
      I_SetLen (iHandle, dwNewSize);
   } else { // WRIT (no READ possible)
      UnCache (iHandle);
      I_SetLen (iHandle, dwNewSize);
      CacheW (iHandle);
   }
}

char *TmpFile (char *pcLocat, int pure, char *useext){
   static int first=1;
   register char   *cp;
   int             len;
   int f;
   char locat[260];
   static char path[MAXPATH];
   char drive[MAXDRIVE];
   char dir[MAXDIR];
   char file[MAXFILE];
   char ext[MAXEXT];
   strcpy (locat,pcLocat);
   if (pure){ // add file spec
      if (locat[strlen(locat)-1]=='\\'){
	 strcat (locat,"X");
      } else {
	 strcat (locat,"\\X");
      }
   }
   fnsplit (locat,drive,dir,file,ext);
   fnmerge (path,drive,dir,TMPPREF"XXXXX",useext);

   len = strlen(path);
   cp = path + len - (9 + strlen (useext) - 4);
   for (;;){
      if (first){
         randomize ();
         first=0;
      }
      cp[0] = 'A'+random(26);
      cp[1] = 'A'+random(26);
      cp[2] = 'A'+random(26);
      cp[3] = 'A'+random(26);
      cp[4] = 'A'+random(26);
      CSB;
         f = Exists (path);
      CSE;
      if (!f) goto done;
   }
done:
   return path;
}

int Exists (char *pcP){
   char pcPath[260];
   if (pcP[1]==':'){
      if (strstr(pcP,"\\")) goto normal;
      strcpy (pcPath, pcP);
      strcpy (pcPath+2,".\\");
      strcat (pcPath, pcP+2);
   } else {
      if (strstr(pcP,"\\"))
normal:
	 strcpy (pcPath, pcP);
      else {
	 strcpy (pcPath, ".\\");
	 strcat (pcPath, pcP);
      }
   }
   int ret=1;
   struct ffblk ffblk;
   CSB;
      if (findfirst (pcPath, &ffblk, 0xF7)==0){
	 if ((ffblk.ff_attrib&FA_DIREC)) ret=0; // not a file
	 if ((ffblk.ff_attrib&FA_LABEL)) ret=0; // not a file
	 if ((ffblk.ff_attrib&0x40)) ret=0; // not a file (but a device)
      } else
	 ret=0;
   CSE;
   return ret;
}

void Rename (char *pcOld, char *pcNew){
   int ret;
   if (Exists (pcNew)) FatalError (175,"failed to rename %s to %s",pcOld,pcNew);
   CSB;
      ret = rename (pcOld, pcNew);
   CSE;
   if (ret!=0) FatalError (175,"failed to rename %s to %s",pcOld,pcNew);
   if (Exists (pcOld)) FatalError (175,"failed to rename %s to %s",pcOld,pcNew);
}

void Delete (char *pcPath){
#ifdef UCPROX
   if (debug) Out (7,"{DELETE %s}\n\r",pcPath);
#endif
   int ret;
   MacAcc (pcPath);
   CSB;
      ret = unlink (pcPath);
   CSE;
   if ((ret!=0) && Exists(pcPath)) Error (55,"failed to delete %s",pcPath);
}

int fcopy=0;
static char *ddel;
static int fo, fi;

void exitcopy (void){
   Close (fo);
   Close (fi);
   Delete (ddel);
   fcopy=0;
}

void CopyFile (char *from, char *onto){
   BYTE *pbBuf = xmalloc (16384, STMP);
#ifdef UCPROX
   if (debug) Out (7,"{COPY %s->%s}\n\r",from,onto);
#endif
   int f=Open(from, MUST|RO|CRT);
   int o=Open(onto, MUST|CR|CWR);
   fo = o;
   fi = f;
   ddel = onto;
   fcopy = 1;
   DWORD mov,len=GetFileSize(f);
   while (len){
      mov=16384;
      if (mov>len) mov=len;
      Read (pbBuf, f, (WORD)mov);
      Write (pbBuf, o, (WORD)mov);
      len-=mov;
   }
   Close (f);
   Close (o);
   fcopy = 0;
   xfree (pbBuf, STMP);
}

void DFlush (char *why){
   if (CONFIG.bRelia==2){
      Out (3,"\x4\Flush: %s\n\r",why);
      Out (4,"\x4\Flush\n\r");
      flushall();
      if (CONFIG.fOut==4){
	 char tmp[260];
	 sprintf (tmp, "command /c%s > NUL",LocateF("U2_FLUSH.BAT",2));
	 ssystem (tmp);
      } else
	 ssystem (LocateF("U2_FLUSH.BAT",2));
   }
}
