// Copyright 1992, all rights reserved, AIP, Nico de Vries
// NEUROMAN.CPP

#include <mem.h>
#include <alloc.h>
#include <stdlib.h>
#include <ctype.h>
#include <io.h>
#include <string.h>
#include <stdio.h>
#include <dir.h>
#include <dos.h>
#include <share.h>
#include "main.h"
#include "video.h"
#include "vmem.h"
#include "archio.h"
#include "superman.h"
#include "neuroman.h"
#include "test.h"
#include "llio.h"
#include "diverse.h"
#include "comoterp.h"
#include "compint.h"
#include "mem.h"
#include "fletch.h"
#include "dirman.h"

static WORD max_mastitem;
static WORD max_masttot;
void TuneNeuro (WORD wItem, WORD wTotal){
   max_mastitem = wItem;
   max_masttot = wTotal;
}

#define TOX_SIZE 64

VPTR masroot[MAX_AREA];
VPTR qsntx[MAX_AREA][TOX_SIZE];
VPTR qskey[MAX_AREA][TOX_SIZE];

static DWORD newindex[MAX_AREA];
static int iMHandle[MAX_AREA];
static char iHPath[MAX_AREA][120];
static DWORD dwPos[MAX_AREA];

BYTE TOX (DWORD p){
   return (BYTE)(((p&0xFFL) ^
		  ((p&0xFF00L)>>8) ^
		  ((p&0xFF0000L)>>16) ^
		  ((p&0xFF000000L)>>24)) & (TOX_SIZE-1));
//   return p & (TOX_SIZE-1);
}

void sexp (char *str, int len){
   while (strlen(str)<len) strcat (str," ");
}

extern int fhypermode;

DWORD ToKey (char *pcFileName){
   DWORD ret;
//   Out (7,"[[%s->",pcFileName);
   char name[9];
   char ext[5];
   setmem (name, 9, 0);
   setmem (ext, 5, 0);
   fnsplit (pcFileName, NULL, NULL, name, ext);
//   if (0==strcmp (ext,".C")) strcpy (ext, ".C+H");
//   if (0==strcmp (ext,".H")) strcpy (ext, ".C+H");
//   if (0==strcmp (ext,".CXX")) strcpy (ext, ".C+H");
//   if (0==strcmp (ext,".HXX")) strcpy (ext, ".C+H");
//   if (0==strcmp (ext,".CPP")) strcpy (ext, ".C+H");
//   if (0==strcmp (ext,".HPP")) strcpy (ext, ".C+H");
   if (!fhypermode){ /* file type bundling */
      for (int i=0;i<8;i++){
	 if (isdigit(name[i])) name[i]='#';
      }
      for (i=1;i<4;i++){
	 if (isdigit(ext[i])) ext[i]='#';
      }
      if (ext[0]!=0){
	 sexp (ext,3);
	 ret = (1L<<24) + ((DWORD)ext[1]<<16) + ((DWORD)ext[2]<<8) + ext[3];
      } else {
	 sexp (name,8);
	 ret = (2L<<24) + ((DWORD)name[0]<<16) + ((DWORD)name[1]<<8) + name[2];
      }
   } else { /* file name bundling 'intra-version' */
      ret = (7L<<24) + ((DWORD)name[0]<<16) + ((DWORD)name[1]<<8) + name[2];
      ret ^= ((DWORD)name[3]<<16) + ((DWORD)name[4]<<8) + name[5];
      ret ^= ((DWORD)ext[1]<<17) + ((DWORD)ext[2]<<10) + ((DWORD)ext[3]<<2);
      ret ^= ((DWORD)name[6]<<16) + ((DWORD)name[7]<<9);
      ret ^= (DWORD)name[0]*13;
      ret ^= (DWORD)name[1]*317;
      ret ^= (DWORD)name[2]*46513L;
      ret ^= (DWORD)name[3]*9361;
      ret ^= (DWORD)name[4]*3;
      ret ^= (DWORD)name[5]*17513;
      ret ^= (DWORD)name[6]*32513;
      ret ^= (DWORD)name[7]*7517;
      ret ^= (DWORD)ext[1]*129;
      ret ^= (DWORD)ext[2]*64327L;
      ret ^= (DWORD)ext[3]*3541;
   }
   //   Out (7,"%lX]]",ret);
   return ret;
}

DWORD ToHKey (char *pcFileName){
   int oldhyp = fhypermode;
   fhypermode=1;
   DWORD ret = ToKey (pcFileName);
   fhypermode = oldhyp;
   return ret;
}

char *ToDes (MASREC *mr){
   DWORD dwIdx = mr->masmeta.dwKey;
   static char tmp[30];
   if ((dwIdx>>24)==1L){
      strcpy (tmp,"*.???");
      tmp[2]=(dwIdx&0xFF0000L)>>16;
      if (tmp[2]<=' ') tmp[2]='\0';
      tmp[3]=(dwIdx&0xFF00)>>8;
      if (tmp[3]<=' ') tmp[3]='\0';
      tmp[4]=(dwIdx&0xFF);
      if (tmp[4]<=' ') tmp[4]='\0';
   } else if ((dwIdx>>24)==2L){
      strcpy (tmp,"???*.*");
      tmp[0]=(dwIdx&0xFF0000L)>>16;
      if (tmp[0]<=' ') tmp[0]='\0';
      tmp[1]=(dwIdx&0xFF00)>>8;
      if (tmp[1]<=' ') tmp[1]='\0';
      tmp[2]=(dwIdx&0xFF);
      if (tmp[2]<=' ') tmp[2]='\0';
   } else {
      if (mr->szName[0]){
	 sprintf (tmp,"%s;*",mr->szName);
      } else {
	 sprintf (tmp,"<FILE TYPE %08lX>",dwIdx);
      }
   }
//   Out (7,"[[%lX->%s]]",dwIdx,tmp);
   return tmp;
}

int neuact=0;

void neuman (void){
   if (neuact==1) CloseNeuro();
}

DWORD dwLM1=4000000000L;

int fneuman=0;

int delolddat=0;

extern int tfirst;

extern CONF SPARE;
extern char pcEXEPath[260]; // full path of executable (for config info)


extern void GKeep2 (void);

void OpenNeuro (){
   GKeep2();
   if (!TstPath((char *)CONFIG.pbTPATH)){
      FatalError (110,"the location for temporary files does not exist (see 'UC -!' S)");
   }

   dwLM1 = 4000000000L;
   if (neuact==0) fneuman=1;
   masroot [iArchArea] = VNULL;
   for (int i=0;i<TOX_SIZE;i++){
      qsntx[iArchArea][i]=VNULL;
      qskey[iArchArea][i]=VNULL;
   }
   newindex[iArchArea] = FIRSTMASTER;
   strcpy (iHPath[iArchArea], TmpFile((char *)CONFIG.pbTPATH,1,".TMP"));
   if (-1==(iMHandle[iArchArea] = Open (iHPath[iArchArea], CR|MAY|NOC))){
      strcpy (iHPath[iArchArea], TmpFile((char *)CONFIG.pbTPATH,1,".TMP"));
      if (-1==(iMHandle[iArchArea] = Open (iHPath[iArchArea], CR|MAY|NOC))){
	 strcpy (iHPath[iArchArea], TmpFile((char *)CONFIG.pbTPATH,1,".TMP"));
	 iMHandle[iArchArea] = Open (iHPath[iArchArea], CR|MUST|NOC);
      }
   }
   dwPos[iArchArea] = 0;
   neuact=1;
}

extern int ex;

void CloseNeuro (){
   if (!ex){
      VPTR walk = masroot[iArchArea];
      while (!IS_VNULL(walk)){
	 VPTR tmp = walk;
	 if (((MASREC*)V(tmp))->bCache){
	    for (WORD i=0;i<((MASREC*)V(tmp))->cnum;i++)
	       free16(3,((MASREC*)V(tmp))->ctab[i]);
	 }
	 walk = ((MASREC *)V(walk))->vpNext;
	 Vfree (tmp);
      }
      delolddat = 0;
   }
   Close (iMHandle[iArchArea]);
   Delete (iHPath[iArchArea]);
   neuact=2;
}

VPTR Mal (void){
//   return Vmalloc (sizeof(MASREC));
   static int ahead=0;
   static VPTR buf[20];
   if (!ahead){
      for (int i=0;i<20;i++) buf[i] = Vmalloc (sizeof (MASREC));
      ahead=20;
   }
   return buf[--ahead];
}

VPTR LocMacNtx (DWORD dwIndex){
   BYTE intx = TOX (dwIndex);
   if (dwIndex<FIRSTMASTER) return VNULL;
   VPTR walk = qsntx[iArchArea][intx];
   while (!IS_VNULL(walk)){
      if (((MASREC*)V(walk))->masmeta.dwIndex == dwIndex) return walk;
      walk = ((MASREC*)V(walk))->vpNextNtx;
   }
   VPTR tmp = Mal();
   MASREC* mr = ((MASREC*)V(tmp));
   mr->compress.wMethod = 0;
   mr->masmeta.dwIndex = dwIndex;
   mr->bStatus = MS_CLR;
   mr->vpNext = masroot[iArchArea];
   mr->vpChain[0] = VNULL;
   mr->vpChain[1] = VNULL;
   mr->vpChain[2] = VNULL;
   mr->bCache = 0;
   mr->szName[0] = 0;
   masroot[iArchArea] = tmp;
   mr->vpNextNtx = qsntx[iArchArea][intx];
   qsntx[iArchArea][intx] = tmp;
   if (newindex[iArchArea]<=dwIndex)
      newindex[iArchArea]=dwIndex+1;
   return tmp;
}

void RegNtxKey (DWORD dwIndex, DWORD dwKey){
   BYTE ikey = TOX (dwKey);
   VPTR tmp = LocMacNtx (dwIndex);
   ((MASREC*)V(tmp))->vpNextKey = qskey[iArchArea][ikey];
   qskey[iArchArea][ikey] = tmp;
}

VPTR LocMacKey (DWORD dwIndex){
   BYTE ikey = TOX (dwIndex);
   BYTE intx = TOX (newindex[iArchArea]);
   VPTR walk = qskey[iArchArea][ikey];
//   while (!IS_VNULL(walk)){
//      walk = ((MASREC*)A(walk))->vpNext;
//   }
   while (!IS_VNULL(walk)){
      if (((MASREC*)V(walk))->masmeta.dwKey == dwIndex) return walk;
      walk = ((MASREC*)V(walk))->vpNextKey;
   }
#ifdef UCPROX
   dd++;
#endif
   VPTR tmp = Mal();
   ((MASREC*)V(tmp))->compress.wMethod = 0;
   ((MASREC*)V(tmp))->masmeta.dwIndex = newindex[iArchArea]++;
   ((MASREC*)V(tmp))->masmeta.dwKey = dwIndex;
   ((MASREC*)V(tmp))->masmeta.dwRefLen = 0;
   ((MASREC*)V(tmp))->masmeta.dwRefCtr = 0;
   ((MASREC*)V(tmp))->vpChain[0] = VNULL;
   ((MASREC*)V(tmp))->vpChain[1] = VNULL;
   ((MASREC*)V(tmp))->vpChain[2] = VNULL;
   ((MASREC*)V(tmp))->bCache = 0;
   ((MASREC*)V(tmp))->szName[0] = 0;
   ((MASREC*)V(tmp))->bStatus = MS_CLR;
   ((MASREC*)V(tmp))->vpNext = masroot[iArchArea];
   masroot[iArchArea] = tmp;
   ((MASREC*)V(tmp))->vpNextNtx = qsntx[iArchArea][intx];
   qsntx[iArchArea][intx] = tmp;
   ((MASREC*)V(tmp))->vpNextKey = qskey[iArchArea][ikey];
   qskey[iArchArea][ikey] = tmp;
   return tmp;
}

VPTR LocKey (DWORD dwIndex){
   BYTE ikey = TOX (dwIndex);
   VPTR walk = qskey[iArchArea][ikey];
   while (!IS_VNULL(walk)){
      if (((MASREC*)V(walk))->masmeta.dwKey == dwIndex) return walk;
      walk = ((MASREC*)V(walk))->vpNextKey;
   }
   return VNULL;
}

int ValidMaster (DWORD dwIndex){
   VPTR mas = LocMacNtx(dwIndex);
   switch (((MASREC*)V(mas))->bStatus){
      case MS_NEW1:
	 return 0;
      case MS_CLR:
	 IE();
      default:
	 return 1;
   }
}

static DWORD dwMaster;
static WORD wQuota, wLen;
static BYTE *ptmp;

static BYTE bCache;
static int hctab[4];

static void CWrite (BYTE *data, WORD len){
   if (len>16384){
      CWrite (data, 16384);
      CWrite (data+16384, len-16384);
      return;
   }
   WORD block = wLen/16384;
   WORD ofs = wLen - block*16384;
   if (len<=16384-ofs){
      to16 (hctab[block],ofs,data,len);
   } else {
      to16 (hctab[block],ofs,data,16384-ofs);
      to16 (hctab[block+1],0,data+16384-ofs,len-(16384-ofs));
   }
   wLen+=len;
}

#pragma argsused
void TTmnm (VPTR dir, VPTR rev, DWORD no){
#ifndef UE2
   TRACEM("Enter TTmnm");
   REVNODE* revn = (REVNODE*)V(rev);
   if (dwMaster==revn->compress.dwMasterPrefix &&
       (revn->bStatus==FST_SPEC ||
       revn->bStatus==FST_DISK)){
      if (wQuota){
	 int h = Open (Full(rev), MAY|RO|NOC); // skip trouble files invisible
	 if (h!=-1){
	    DWORD fl = GetFileSize (h); // direct look (networking problems)
	    WORD len;
	    if (wQuota<fl){
	      len = wQuota;
	    } else {
	      len = (WORD)fl;
	    }
	    if (len>max_mastitem) len = max_mastitem;
#ifdef UCPROX
	    if (debug) Out (7,"{{master (%lu) part extracted from %s of size %u}}\n\r",
			  dwMaster,Full(rev),len);
#endif
//          Out (7,"\n\r{{ (%lu) %s (%u)",
//                        dwMaster,Full(rev),len);
	    if (len && (len==fl)){
//             Out (7," CF O=%d}}",wLen);
	       revn->bSpecial=1;
	       revn->wLen=(WORD)fl;
	       revn->wOfs=wLen;
	    } //else
	       // Out (7," }}");
	    if (len){
	       Hint(); // 1
	       Read (ptmp, h, len);
	       revn = (REVNODE*)V(rev);
	       if (revn->bSpecial){
		  FREC tmp;
		  FletchInit (&tmp);
		  FletchUpdate (&tmp, ptmp, len);
		  revn = (REVNODE*)V(rev);
		  revn->wFletch = Fletcher (&tmp);
	       }
	       if (bCache)
		  CWrite (ptmp, len);
	       else {
		  Write (ptmp, iMHandle[iArchArea], len);
		  wLen+=len;
	       }
	       wQuota-=len;
	    }
	    Close (h);
	 }
      }
   }
   TRACEM("Leave TTmnm");
#endif
}

#pragma argsused
void MakeNewMas (VPTR me){
#ifndef UE2
   bCache = 0;
   if (coreleft16(3)>=4) bCache=1; // store in cache, NOT in file
   ptmp = xmalloc (max_mastitem, TMP);
   WORD wExtra;
   dwMaster = ((MASREC*)V(me))->masmeta.dwIndex;
   Out (1,"\x7\Analyzing %s files ",ToDes(((MASREC*)V(me))));
   StartProgress (-1, 1);
   ((MASREC*)V(me))->bCache = bCache;
   if (!bCache){
      ((MASREC*)V(me))->dwVOffset = GetFileSize (iMHandle[iArchArea]);
      Seek (iMHandle[iArchArea], GetFileSize (iMHandle[iArchArea]));
   } else { // allocate cache memory
      ((MASREC*)V(me))->cnum = ((max_masttot+16383UL)/16384);
      for (WORD i=0;i<((MASREC*)V(me))->cnum;i++)
	 ((MASREC*)V(me))->ctab[i] = hctab[i] = malloc16(3);
   }
   wQuota = max_masttot;
   wLen = 0;
   ToToWalk (TTmnm, dwMaster, 1);
   if (wQuota) ToToWalk (TTmnm, dwMaster, 0);
   if (wQuota) ToToWalk (TTmnm, dwMaster, 2);
   Out (1|8,"\n\r");
   if (wLen<512){
      wExtra = 512-wLen;
   } else {
      wExtra = ((wLen+511)/512)*512 - wLen;
   }
   if (wExtra){
#ifdef UCPROX
      if (heavy){
	 if (wExtra>512) IE();
      }
#endif
      memset (ptmp, 0, wExtra);
      if (!bCache){
	 Write (ptmp, iMHandle[iArchArea], wExtra);
	 wLen+=wExtra;
      } else
	 CWrite (ptmp, wExtra);
   }
   ((MASREC*)V(me))->masmeta.wLength = wLen;
   xfree (ptmp, TMP);
#endif
}

static void far pascal Writer (BYTE *buf, WORD wSize){
   Hint();
   if (bCache)
      CWrite (buf, wSize);
   else {
      Write (buf, iMHandle[iArchArea], wSize);
      wLen+=wSize;
   }
}

void ReadOldMas (VPTR me){
   ARSeek ((((MASREC*)V(me))->location.dwVolume),
           (((MASREC*)V(me))->location.dwOffset));
   bCache = 0;
   if (coreleft16(3)>=4) bCache=1; // store in cache, NOT in file
   ((MASREC*)V(me))->bCache = bCache;
   if (bCache){
      WORD masttot = ((MASREC*)V(me))->masmeta.wLength;
      ((MASREC*)V(me))->cnum = ((masttot+16383UL)/16384);
      for (WORD i=0;i<((MASREC*)V(me))->cnum;i++)
	 ((MASREC*)V(me))->ctab[i] = hctab[i] = malloc16(3);
   } else {
      ((MASREC*)V(me))->dwVOffset = GetFileSize (iMHandle[iArchArea]);
      Seek (iMHandle[iArchArea], GetFileSize (iMHandle[iArchArea]));
   }
   wLen=0;
   Out (1,"\x7\Loading %s statistics ",ToDes(((MASREC*)V(me))));
   StartProgress (3, 1);
   Hint();
   if (((MASREC*)V(me))->compress.dwMasterPrefix==0xDEDEDEDEL)
      ((MASREC*)V(me))->compress.dwMasterPrefix = SUPERMASTER;
   Decompressor (((MASREC*)V(me))->compress.wMethod, ARead, Writer,
		 ((MASREC*)V(me))->compress.dwMasterPrefix,
		 63000L); // QQQ other masters too ???
   EndProgress();
   Out (1,"\n\r");
}

void AddAccess (BYTE fInc){
   Out (2,"\x7\Analyzing ");
   StartProgress (-1, 2);
   VPTR walk = masroot[iArchArea];
   while (!IS_VNULL(walk)){
      Hint ();
      switch (((MASREC*)V(walk))->bStatus){
	 case MS_NEWC:
         case MS_OLDC:
            break;
         case MS_NEW1: // RRR now Always make masters
         case MS_NEW2:
	    if (fInc) AWEnd();
	    MakeNewMas(walk);
            ((MASREC*)V(walk))->bStatus = MS_NEWC;
            break;
         case MS_OLDN:
         case MS_NEED:
            ReadOldMas(walk);
            ((MASREC*)V(walk))->bStatus = MS_OLDC;
            break;
         default: // no access needed, MS_NEW1 remains MS_NEW1 !!!
            break;
      }
      walk = ((MASREC*)V(walk))->vpNext;
   }
   if (fInc) AWEnd();
   EndProgress ();
   Out (2,"\n\r");
}

static char smCache=0;
static int smCT[3];

static char smStore=0;
static char smTmpP[120];
static int smHan;

void SmKillTmp (void){
   if (smStore){
      Close (smHan);
      Delete (smTmpP);
   }
}

static FILE *exfp;

extern char pcEXEPath[260];

static WORD far pascal Cread (BYTE *buf, WORD len){
   return fread (buf, 1, len, exfp);
}

static huge BYTE *adr;
static unsigned ctr;

void far *norm (void far *adr){
   unsigned seg = FP_SEG (adr);
   unsigned off = FP_OFF (adr);
   seg+=off/16;
   off%=16;
   return MK_FP(seg,off);
}

static void far pascal Cwrite (BYTE *buf, WORD len){
   RapidCopy (norm(adr), buf, len);
   adr = (BYTE huge *)norm (adr);
   adr+=len;
   adr = (BYTE huge *)norm (adr);
   ctr+=len;
//   printf ("[%u]",ctr);
}

static BYTE bMasCas=0;
static WORD wMasLen;
static mcbuf[4];

void MMinit (void){
   if (!bMasCas&&coreleft16(6)>=3){
      bMasCas=1;
      mcbuf[0]=malloc16(6);
      mcbuf[1]=malloc16(6);
      mcbuf[2]=malloc16(6);
      mcbuf[3]=malloc16(6);
   }
}

void SecureSmast (void){
   static int nest=0;
   if (!smCache && !nest){
      nest=1;
      BYTE *b = xmalloc (50000L, TMP);
      WORD dum;
      Transfer (b, &dum, SUPERMASTER);
      xfree (b, TMP);
   }
}

int fSmKillTmp=0;

void Transfer (BYTE *pbAddress, WORD *pwLen, DWORD dwIndex){
   int flag=0;
   MMinit();
   if (pbAddress && bMasCas && (dwLM1==dwIndex)){
//      IOut ("[R:%ld]",dwIndex);
      *pwLen = wMasLen;
      WORD w=*pwLen;
      WORD j=0;
      while (w){
         WORD q=16384;
         if (q>w) q=w;
         from16(pbAddress+(16384U*j),mcbuf[j],0,q);
         w-=q;
         j++;
      }
      return;
   } else {
      dwLM1=4000000000L;
   }
#ifdef UCPROX
   if (debug && pbAddress) Out (7,"{Master transfer}");
#endif
   if (dwIndex==SUPERMASTER){
      if (pbAddress){
	 if (smCache){
	       from16 (pbAddress,smCT[0],0,16384);
	       from16 (pbAddress+16384,smCT[1],0,16384);
	       from16 (pbAddress+32768U,smCT[2],0,16384);
	 } else {
	    if (smStore){
	       Seek (smHan, 0);
	       Read (pbAddress, smHan, 49152U);
	    } else {
   #ifdef UCPROX
	       if (debug) Out (7,"{(Super)master diskread}");
   #endif
	       exfp = _fsopen (pcEXEPath,"rb",SH_DENYWR);
	       if (!exfp){
		  Doing ("reading data from UC2 executable");
		  FatalError (105,"cannot read %s\n\r",pcEXEPath);
	       }
	       fseek (exfp, CONFIG.dwSoffset,SEEK_SET);
	       adr = pbAddress;
	       Decompressor (4, Cread, Cwrite, NOMASTER,49152L);
//	       Out (7,"[FLETCH: %04X]",Fletcher(&Fout));
	       if (Fletcher(&Fout)!=0x1E55)
#ifdef UE2
		  FatalError (105,"UE.EXE is damaged");
#else
		  FatalError (105,"UC.EXE is damaged");
#endif
	       fclose (exfp);
	       flag=1;

	       if (coreleft16(1)>=3){
		  smCache = 1;
   #ifdef UCPROX
		  if (debug) Out (7,"{Supermaster cached}");
   #endif
		  smCT[0] = malloc16(1);
		  smCT[1] = malloc16(1);
		  smCT[2] = malloc16(1);
		  to16 (smCT[0],0,pbAddress,16384);
		  to16 (smCT[1],0,pbAddress+16384,16384);
		  to16 (smCT[2],0,pbAddress+32768U,16384);
		  flag=0;
	       } else {
		  strcpy (smTmpP, TmpFile((char *)CONFIG.pbTPATH,1,".TMP"));
		  if (-1==(smHan = Open (smTmpP, CR|MAY|NOC))){
		     strcpy (smTmpP, TmpFile((char *)CONFIG.pbTPATH,1,".TMP"));
		     if (-1==(smHan = Open (smTmpP, CR|MAY|NOC))){
			strcpy (smTmpP, TmpFile((char *)CONFIG.pbTPATH,1,".TMP"));
			smHan = Open (smTmpP, CR|MUST|NOC);
		     }
		  }
		  fSmKillTmp=1;
		  Write (pbAddress, smHan, 49152U);
		  smStore=1;
	       }
	    }
	 }
      }
      *pwLen = 49152U;
   } else if (dwIndex==NOMASTER){
      if (pbAddress){
         memset (pbAddress, 0, 512);
      }
      *pwLen = 512;
   } else {
      VPTR mas = LocMacNtx (dwIndex);
      *pwLen = ((MASREC*)V(mas))->masmeta.wLength;
      if (pbAddress){
         if (((MASREC*)V(mas))->bCache){
	    int ct=0;
            WORD len = *pwLen;
again:
            WORD trans = (len>16384) ? 16384 : len;
	    from16 (pbAddress,((MASREC*)V(mas))->ctab[ct],0,trans);
            len-=trans;
            if (len){
               ct++;
               pbAddress+=16384;
               goto again;
            }
         } else {
#ifdef UCPROX
            if (debug) Out (7,"{Master diskread}");
#endif
            Seek (iMHandle[iArchArea],((MASREC*)V(mas))->dwVOffset);
            Read (pbAddress, iMHandle[iArchArea], *pwLen);
            flag=1;
         }
      }
   }
   if (pbAddress && bMasCas && flag){
//      IOut ("[S:%ld]",dwIndex);
      WORD w=*pwLen;
      WORD j=0;
      while (w){
         WORD q=16384;
         if (q>w) q=w;
         to16(mcbuf[j],0,pbAddress+(16384U*j),q);
         w-=q;
         j++;
      }
      wMasLen = *pwLen;
      dwLM1 = dwIndex;
   }
}

static WORD limit;
static WORD wCOffset;

static WORD far pascal Reader (BYTE *buf, WORD len){
   if (len>16384) {
      WORD ret = Reader (buf,16384);
      if (ret) ret += Reader (buf+16384, len-16384);
      return ret;
   }
   if (len>limit) len=limit;
   limit-=len;
   if (len) {
       if (bCache){
          WORD block = wCOffset/16384;
          WORD ofs = wCOffset - block*16384;
          if (len<=16384-ofs){
	     from16 (buf,hctab[block],ofs,len);
          } else {
	     from16 (buf,hctab[block],ofs,16384-ofs);
	     from16 (buf+16384-ofs,hctab[block+1],0,len-(16384-ofs));
          }
          wCOffset+=len;
          return len;
       } else
	  return Read (buf, iMHandle[iArchArea], len);
   } else
       return 0;
}

void NoSpec ();

extern BYTE bCBar;

#pragma argsused
void ListMast (PIPE &p){
#ifndef UE2
   OHEAD ohead;
   ohead.bType = BO_MAST;
   VPTR walk = masroot[iArchArea];
   int flag=0;
   while (!IS_VNULL(walk)){

//      IOut ("[[%ld]]",((MASREC*)A(walk))->masmeta.dwRefLen);
      if (((MASREC*)V(walk))->masmeta.dwRefCtr){ // master used at all?
//	    Out (1,"[%s %d]",
//		  ToDes(((MASREC*)V(walk))), (WORD)((MASREC*)V(walk))->bStatus);
	    switch (((MASREC*)V(walk))->bStatus){
	    case MS_NEWC:
	       // compress master
	       if (!flag){
		  Out (2,"\x7\Optimizing compression ");
                  StartProgress (-1, 2);
		  flag=1;
	       }
	       Out (1,"\x7\Optimizing %s compression ",
		     ToDes(((MASREC*)V(walk))));
	       AWTell (&(((MASREC*)V(walk))->location.dwVolume),
		       &(((MASREC*)V(walk))->location.dwOffset));
	       if (((MASREC*)V(walk))->compress.wMethod==0)
		  ((MASREC*)V(walk))->compress.wMethod = MODE.bCompressor;
	       bCache = ((MASREC*)V(walk))->bCache;
	       if (bCache){
		  wCOffset=0;
		  for (int i=0;i<4;i++)
		     hctab[i] = ((MASREC*)V(walk))->ctab[i];
	       } else {
		  Seek (iMHandle[iArchArea],((MASREC*)V(walk))->dwVOffset);
	       }
	       limit = ((MASREC*)V(walk))->masmeta.wLength;
               StartProgress (limit/512, 1);
#ifdef UCPROX
	       if (debug) Out (7,"{{master index %lu size %u}}\n\r",
				 ((MASREC*)V(walk))->masmeta.dwIndex,
				 ((MASREC*)V(walk))->masmeta.wLength);
#endif
	       ResetOutCtr();
	       if (CONFIG.fOut!=4) bCBar=1;
	       ((MASREC*)V(walk))->compress.dwMasterPrefix = SUPERMASTER;
	       if (stricmp (getenv("UC2_PUC"),"ON")==0)
		  ((MASREC*)V(walk))->compress.dwMasterPrefix = NOMASTER;
	       Compressor (((MASREC*)V(walk))->compress.wMethod, Reader, AWrite,
			   ((MASREC*)V(walk))->compress.dwMasterPrefix);
	       NoSpec ();

	       bCBar=0;
	       Out (1|8,"\n\r");
#ifdef UCPROX
               if (debug) Out (7,"{{compressed master size %lu}}\n\r",GetOutCtr());
#endif
               ((MASREC*)V(walk))->compress.dwCompressedLength = GetOutCtr();
	       WritePipe (p, (BYTE *)&ohead, sizeof (OHEAD));
	       WritePipe (p, (BYTE *)&(((MASREC*)V(walk))->masmeta), sizeof (MASMETA));
	       WritePipe (p, (BYTE *)&(((MASREC*)V(walk))->compress), sizeof (COMPRESS));
	       WritePipe (p, (BYTE *)&(((MASREC*)V(walk))->location), sizeof (LOCATION));
#ifdef UCPROX
               bb+=((MASREC*)V(walk))->masmeta.dwRefLen;
#endif
	       break;
            case MS_OLD:
            case MS_OLDN:
	    case MS_OLDC:
	    case MS_NEED:
               // copy from old archive if not incremental
               if (!MODE.fInc){
		  ARSeek ((((MASREC*)V(walk))->location.dwVolume),
			  (((MASREC*)V(walk))->location.dwOffset));
		  AWTell (&(((MASREC*)V(walk))->location.dwVolume),
			  &(((MASREC*)V(walk))->location.dwOffset));
		  Old2New (((MASREC*)V(walk))->compress.dwCompressedLength);
	       }
	       WritePipe (p, (BYTE *)&ohead, sizeof (OHEAD));
	       WritePipe (p, (BYTE *)&(((MASREC*)V(walk))->masmeta), sizeof (MASMETA));
	       WritePipe (p, (BYTE *)&(((MASREC*)V(walk))->compress), sizeof (COMPRESS));
	       WritePipe (p, (BYTE *)&(((MASREC*)V(walk))->location), sizeof (LOCATION));
#ifdef UCPROX
	       bb+=((MASREC*)V(walk))->masmeta.dwRefLen;
#endif
	       break;
	    case MS_WRT:
	       // nothing to do
	       WritePipe (p, (BYTE *)&ohead, sizeof (OHEAD));
	       WritePipe (p, (BYTE *)&(((MASREC*)V(walk))->masmeta), sizeof (MASMETA));
	       WritePipe (p, (BYTE *)&(((MASREC*)V(walk))->compress), sizeof (COMPRESS));
	       WritePipe (p, (BYTE *)&(((MASREC*)V(walk))->location), sizeof (LOCATION));
#ifdef UCPROX
	       bb+=((MASREC*)V(walk))->masmeta.dwRefLen;
#endif
	       break;
	    case MS_NEW1: // failed attempt to create master
	       break;
	    default:
	       IE();
	       break;
	 }
	 ((MASREC*)V(walk))->bStatus = MS_WRT;
      }
      walk = ((MASREC*)V(walk))->vpNext;
   }
   if (flag) Out (2|8,"\n\r");
#endif
}

void ClearMasRefLen(){
   VPTR walk = masroot[iArchArea];
   while (!IS_VNULL(walk)){
      ((MASREC*)V(walk))->masmeta.dwRefLen = 0;
      ((MASREC*)V(walk))->masmeta.dwRefCtr = 0;
      walk = ((MASREC*)V(walk))->vpNext;
   }
}


static MASREC supe [MAX_AREA];
static MASREC noma [MAX_AREA];

void AddToNtx (DWORD ntx, VPTR rev, DWORD size){
   WORD idx;
   if (size<2000){
      idx=0;
   } else if (size<10000){
      idx=1;
   } else
      idx=2;

   MASREC *mn;
   if (ntx==NOMASTER){
      mn = &noma[iArchArea];
   } else if (ntx==SUPERMASTER){
      mn = &supe[iArchArea];
   } else {
      VPTR nt = LocMacNtx (ntx);
      mn = (MASREC*)V(nt);
   }
   ((REVNODE*)V(rev))->vpSis = mn->vpChain[idx];
   mn->vpChain[idx] = rev;
}

VPTR GetFirstNtx (DWORD ntx, WORD index){
   MASREC *mn;
   if (ntx==NOMASTER){
      mn = &noma[iArchArea];
   } else if (ntx==SUPERMASTER){
      mn = &supe[iArchArea];
   } else {
      VPTR nt = LocMacNtx (ntx);
      mn = (MASREC*)V(nt);
   }
   return mn->vpChain[index];
}

void CleanNtx (void){
   supe[iArchArea].vpChain[0] = VNULL;
   supe[iArchArea].vpChain[1] = VNULL;
   supe[iArchArea].vpChain[2] = VNULL;
   noma[iArchArea].vpChain[0] = VNULL;
   noma[iArchArea].vpChain[1] = VNULL;
   noma[iArchArea].vpChain[2] = VNULL;
}
