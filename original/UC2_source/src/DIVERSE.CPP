// Copyright 1992, all rights reserved, AIP, Nico de Vries
// DIVERSE.CPP

#pragma inline // use Turbo Assembler!

#include <stdio.h>
#include <time.h>
#include <dos.h>
#include <stdlib.h>
#include <dir.h>
#include <alloc.h>
#include <mem.h>
#include <string.h>
#include <io.h>
#include "main.h"
#include "video.h"
#include "diverse.h"
#include "mem.h"
#include "vmem.h"
#include "llio.h"
#include "handle.h"

#define min(a,b) (((a)<(b))?(a):(b))

/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       File name management.
*/

void EatSpace (char *name){
   int i=strlen(name)-1;
   while (name[i]==' ' && i>=0){
      name[i]='\0';
      i--;
   }
}

BYTE *Name2Rep (char *pcName){
   static BYTE ret[11];
   char name[9];
   char ext[4];
   fnsplit(pcName, NULL, NULL, name, ext);
   memset (ret,32,11);
   memcpy (ret, name, strlen(name));
   if (strlen(ext)>1)
      memcpy (ret+8, ext+1, strlen(ext)-1);
   return ret;
}

char *Rep2Name (BYTE *pbRep){
   static char ret[14];
   char name[9]="        ";
   char ext[4]="   ";
   memcpy (name, pbRep, 8);
   EatSpace (name);
   memcpy (ext, pbRep+8, 3);
   EatSpace (ext);
   if (strlen(ext))
      fnmerge (ret, NULL, NULL, name, ext);
   else
      fnmerge (ret, NULL, NULL, name, NULL);
   return ret;
}

char *Rep2DName (BYTE *pbRep){
   static char ret[14];
   memcpy (ret, pbRep, 8);
   memcpy (ret+9, pbRep+8, 3);
   ret[8] = ' ';
   ret[13]='\0';
   return ret;
}


/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       Determine if output has been redirected.
*/

int StdOutType (void){
   unsigned dx;
   dx = ioctl (fileno(stdout), 0, 0, 0);
   if (!(dx&0x80)) return D_FILE;  // output redirected to file
   if (dx&0x3) return D_CON;       // ouput not redirected (CON:)
   if (dx&0x4) return D_NUL;       // output redirected to NUL:
   return D_DEV;                   // output redirected to other device
}

#undef farmalloc
#undef farfree

#ifdef TRACE
   DWORD mallox=0;
   DWORD last=0;
   int ina=1;

void Set (void){
   ina=0;
   last = farcoreleft();
}

void Check (void){
   ina=1;
//   if (last && last!=farcoreleft())
//      Out (7,"\n\r[***]\n\r");
}

#endif

#ifdef UCPROX
   DWORD minimal=100000000L;
   DWORD maximal=0;
   DWORD tmin;
#endif

#ifdef UCPROX
//static int tctr[5]; // type-counter
#endif

#pragma argsused
#ifdef TRACE
BYTE *xxmalloc (char *file, long line, DWORD len, BYTE type){
#else
BYTE *yxmalloc (DWORD len){
#endif
#ifdef TRACE
   Check();
   if (beta && (farcoreleft()>maximal)){
      maximal = farcoreleft();
   }
//   tctr[type]++;
//   if (type==PERM && tctr[TMP]+tctr[STMP]+tctr[NTMP]) IE();
   if (heavy){
      if( heapcheck() == _HEAPCORRUPT) IE();
   }
   if (debug & heavy)
      Out (7,"\n\r[xmalloc %ld ",farcoreleft());
#endif
   BYTE *ret;
//   if (len>4191){
//      ret = (BYTE *)exmalloc (len);
//      if (!ret) ret = (BYTE *)farmalloc (len);
//   } else {
      ret = (BYTE *)farmalloc (len);
//   }
#ifdef TRACE
   if (debug & heavy){
      Out (7,"(%ld)",len);
      Out (7," %ld] %s %ld %ld",farcoreleft(),file,line,mallox);
   }
   mallox++;
#endif
   if (!ret){
      FatalError (160,"Not enough memory");
   }
#ifdef UCPROX
   if (beta && farcoreleft()<minimal){
      minimal=farcoreleft();
   }
   Set();
#endif
   return ret;
}

#ifdef TRACE
#pragma argsused
void xxfree (char *file, long line, BYTE *address, BYTE type){
#else
void yxfree (BYTE *address){
#endif
#ifdef TRACE
   Check();
//   tctr[type]--;
   if (heavy){
      if( heapcheck() == _HEAPCORRUPT) IE();
   }
   if (debug & heavy)
      Out (7,"\n\r[xfree %ld ",farcoreleft());
#endif
   if (extest (address)){
      exfree (address);
   } else {
      farfree (address);
   }
#ifdef TRACE
   if (debug & heavy){
      Out (7," %ld] %s %ld %ld",farcoreleft(),file,line,mallox);
   }
   mallox--;
   Set();
#endif
}

#ifdef TRACE
void checkx (void){
   if (beta && debug){
//      Out (7,"TMP%d PERM%d STMP%d NTMP%d ",tctr[1],tctr[2],tctr[3],tctr[4]);
   }
}
#endif

BYTE *normalize (BYTE *base){
   if (FP_OFF(base)>65520U) IE();
   return (BYTE *)MK_FP(FP_SEG(base)+(FP_OFF(base)+15)/16,0);
}

static BYTE *regad;
static WORD reglen=0;

int lflag=0;

void RegDat (BYTE *dat, WORD len){
   if (lflag) IE();
   lflag=1;
   regad=dat;
   reglen=len;
   if (reglen>65024U) reglen=65024U;
}

void UnRegDat (void){
   lflag=0;
   reglen=0;
}

static int mode;
static int ddt[4];
static BYTE *freeme;
static WORD ttlen;

int fflag=0;
extern int ex;

void GetDat (BYTE **dat, WORD *len){
   if (!ex && fflag) IE();
   fflag=1;
#ifdef UCPROX
   tmin = minimal;
#endif
//   Out (7,"[[%d %d]]",reglen, coreleft16(7));
   if (reglen && coreleft16(7)){
      *dat = regad;
      WORD tlen = reglen, pos=0;
      if (16384L*coreleft16(7)<tlen) tlen=16384*coreleft16(7);
      ttlen=tlen;
      while (tlen){
	 ddt[pos] = malloc16(7);
	 to16 (ddt[pos], 0, regad+16384U*pos,min(16384,tlen));
	 tlen-=min(16384,tlen);
	 pos++;
      }
      *len = ttlen;
      mode=2;
   } else {
      BYTE *ret=NULL;
      if (farcoreleft()>66000L)
	 ret = (BYTE *)xmalloc (65024U, STMP);
      if (ret){
	 *dat = ret;
	 *len = 65024U;
	 mode = 1;
      } else {
	 if (farcoreleft()>16400L)
	    ret = xmalloc (16384, STMP);
	 if (ret){
	    *dat = ret;
	    *len = 16384;
	    mode = 1;
	 } else {
	    if (farcoreleft()>5100L)
	       ret = xmalloc (4096, STMP);
	    if (ret){
	       *dat = ret;
	       *len = 4096;
	       mode = 1;
	    } else {
	       ret = xmalloc (512, STMP);
	       *dat = ret;
	       *len = 512;
	       mode = 1;
	    }
	 }
      }
      freeme=ret;
   }
}

void UnGetDat (void){
   /*
   if (mode==2) memcpy (regad, freeme, reglen);
   xfree (freeme);
   return;
   */

   fflag=0;
   switch (mode){
      case 1:
	 xfree (freeme, STMP);
	 break;
      case 2:
	 WORD tlen = ttlen, pos=0;
	 while (tlen){
	    from16 (regad+16384U*pos, ddt[pos], 0, min(16384,tlen));
	    tlen-=min(16384,tlen);
	    free16 (7, ddt[pos]);
	    pos++;
	 }
	 break;
   }
#ifdef UCPROX
   minimal = tmin;
#endif
}


BYTE probits (void){
   asm   pushf
   asm   xor   ax,ax
   asm   push  ax
   asm   popf
   asm   pushf
   asm   pop   ax
   asm   and   ax,0f000h
   asm   cmp   ax,0f000h
   asm   je    bit8
   asm   mov   ax,07000h
   asm   push  ax
   asm   popf
   asm   pushf
   asm   pop   ax
   asm   and   ax,07000h
   asm   je    bit16
   asm   popf
   return (BYTE)32;
bit16:
   asm   popf
   return (BYTE)16;
bit8:
   asm   popf
   return (BYTE)8;
}

void far RapidCopy (void far *dst, void far *src, WORD wLen){
   int first, rest;
   asm cld
   if ((wLen<32U) || !m386){ // no too small moves
      if (wLen>0){
	 asm     mov     dx,ds           /* save ds */
	 asm     les     di, dst
	 asm     lds     si, src
	 asm     mov     cx,wLen
	 asm     shr     cx,1
	 asm     cld
	 asm     rep     movsw
	 asm     jnc     cpy_end
	 asm     movsb
	 cpy_end:
	 asm     mov     ds,dx
      }
   } else {
      // make sure second block is dword alligned
      first = 4-FP_OFF(src)%4; // first miniblock
      wLen -= first;

      // resolve uneven/quad block problems
      rest = wLen % 4 + 4;  // last miniblock
      wLen -= rest; // middle block (multiple of 4)

      wLen >>= 2;         // convert to dwords
      asm push ds         // save DS
      asm les  di, dst    // define destination in ES:DI
      asm lds  si, src    // define source in DS:SI
      asm mov  cx, first  // define first bytes to move in CX
      asm rep  movsb      // repeat move byte (8bit)
      asm mov  cx, wLen   // define dwords to move in CX
      asm .386            // allow 386 instructions
      asm rep  movsd      // 386 repeat move dword (32bit)
      asm .8086
      asm mov  cx, rest   // define bytes to move in CX
      asm rep  movsb
      asm pop  ds         // restore DS
   }
}


#ifdef UCPROX
void DDump (char *pcFilename, BYTE *pbData, WORD wSize){
   if (debug & heavy){
      FILE *pf = fopen (pcFilename,"wb");
      if (!pf) IE();
      fwrite (pbData, 1, wSize, pf);
      fclose (pf);
   }
}
#endif

int pulsar (void){
   static clock_t ct;
   if (labs(clock()-ct)){
      ct = clock();
      return 1;
   }
   return 0;
}

// XSPAWN library
extern "C" {
   int xsystem (...);
}

void ssystem (char *line){
   flushall();
   if (CONFIG.fSwap)
      xsystem (line);
   else
      system (line);
}

static int deep=0;

extern int win;

void Critical (void){
   if (!win) return;

   if (!deep++){
      union REGS regs;

      regs.x.ax = 0x1681;
//      Out (7,"[LOCK]");
      int86(0x2F, &regs, &regs);
   }
}

void Normal (void){
   if (!win) return;

   if (!--deep){
      union REGS regs;

      regs.x.ax = 0x1682;
//      Out (7,"[UNLOCK]");
      int86(0x2F, &regs, &regs);
   }
}

void Reset (void){
   while (deep) Normal();
}

VPTR rot=VNULL;

void MoveClear (void){
   while (!IS_VNULL(rot)){
      VPTR tmp=rot;
      rot = *((VPTR*)V(rot));
      Vfree (tmp);
   }
}

void MoveDelete (void){
   Out (3,"\x7\n\rMoving files ");
   StartProgress (-1, 3);
   while (!IS_VNULL(rot)){
      Hint();
      VPTR tmp=rot;
      rot = *((VPTR*)V(rot));
      if (Exists ((char*)V(tmp)+sizeof(VPTR))) // allow over-specification
	 Delete ((char*)V(tmp)+sizeof(VPTR));
      Vfree (tmp);
   }
   EndProgress();
   Out (3,"\n\r");
}

void MoveAdd (char *file){
   VPTR tmp = Vmalloc (sizeof(VPTR)+strlen(file)+1);
   *((VPTR*)V(tmp)) = rot;
   strcpy ((char*)V(tmp)+sizeof(VPTR), file);
   rot = tmp;
}

VPTR qrot=VNULL;

void RabClear (void){
   while (!IS_VNULL(qrot)){
      VPTR tmp=qrot;
      qrot = *((VPTR*)V(qrot));
      Vfree (tmp);
   }
}

void RabDelete (void){
   Out (3,"\x7\n\rResetting archive bits ");
   StartProgress (-1, 3);
   while (!IS_VNULL(qrot)){
      Hint();
      VPTR tmp=qrot;
      qrot = *((VPTR*)V(qrot));
      if (Exists ((char*)V(tmp)+sizeof(VPTR))){ // allow over-specification
	 WORD wAttrib;
	 CSB;
	    _dos_getfileattr ((char*)V(tmp)+sizeof(VPTR),&wAttrib);
	 CSE;
	 wAttrib&=(0xFF-FA_ARCH);
	 CSB;
	    _dos_setfileattr ((char*)V(tmp)+sizeof(VPTR),wAttrib);
	 CSE;
      }
      Vfree (tmp);
   }
   EndProgress();
   Out(3,"\n\r");
}

void RabAdd (char *file){
   VPTR tmp = Vmalloc (sizeof(VPTR)+strlen(file)+1);
   *((VPTR*)V(tmp)) = qrot;
   strcpy ((char*)V(tmp)+sizeof(VPTR), file);
   qrot = tmp;
}

void strrep (char *base, char *from, char *onto){
   char *p;
   while (p=strstr(base,from)){
      movmem (p+strlen(from), p, strlen(p)+1);
      movmem (p, p+strlen(onto), strlen(p)+1);
      for (int i=0;i<strlen(onto);i++)
         p[i] = onto[i];
   }
}

extern char pcManPath[260];

char* LocateF (char *name, int batch){
    static char ret[260];
    if (batch){
       if (CONFIG.pcBat[0]=='*') goto aut;
       strcpy (ret,CONFIG.pcBat);
    } else {
       if (CONFIG.pcMan[0]=='*') goto aut;
       strcpy (ret,CONFIG.pcMan);
    }
    strcat (ret,"\\");
    strcat (ret,name);
    if (Exists (ret)) return ret;

aut:
    strcpy (ret, pcManPath);
    strcat (ret,"\\");
    strcat (ret,name);
    if (Exists (ret)) return ret;

    if (searchpath(name)){
       strcpy (ret, searchpath(name));
       return ret;
    }

    if (batch==2) FatalError (120,"cannot locate %s",name);

    return NULL;
}
