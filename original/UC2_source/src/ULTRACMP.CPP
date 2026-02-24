// Copyright 1992, all rights reserved, AIP, Nico de Vries
// ULTRACMP.CPP

#pragma inline // enforce use of TASM (386 support etc)

/**********************************************************************/
/*$ INCLUDES */

#include <stdlib.h>
#include <stdio.h>
#include <dos.h>
#include <string.h>
#include <mem.h>
#include "main.h"
#include "video.h"
#include "diverse.h"
#include "compint.h"
#include "bitio.h"
#include "test.h"
#include "ultracmp.h"
#include "vmem.h"
#include "superman.h"
#include "neuroman.h"
#include "handle.h"
#include "mem.h"
#include "fletch.h"
#include "llio.h"
#include "comoterp.h"

FREC Fin, Fout;

void RPutLenDst (WORD len, WORD dst);


/**********************************************************************/
/*$ DEFENITION BLOCK  // classes,types,global data,constants etc
    PURPOSE OF DEFENITION BLOCK:
       Global data for ANA and REC.
*/

// decompression always lightspeed

//#define HYPT  // hyper-trace

static WORD wMd=1400; // normal max search depth
static WORD wLd=900;  // lazy max search depth
static WORD wLl=200;  // limit for lazy evaluation
static WORD wGu=180;  // give up chain search length

void TuneComp (WORD wMaxSearch, WORD wMaxLazySearch,
	       WORD wLimitLazy, WORD wLimitSearch){
   wMd = wMaxSearch;
   wLd = wMaxLazySearch;
   wLl = wLimitLazy;
   wGu = wLimitSearch;
}

// compression characteristics
#define EOB_MARK   125*512+1U // copy from ULTRACMP !!!
#define MAX_LEN    200        // maximal length of direct detected match
#define MAX_XLEN   32760U     // maximal length of indirect detected match
unsigned max_dist = 125*512U;
#define MAX_DIST   max_dist   // max match distance
#define READ_SIZE  512U       // size of blocks read
#define MAX_DEPTH  wMd
#define LAZY_DEPTH wLd
#define LAZY_LIMIT wLl
#define GIVE_UP    wGu

#define BUF_SIZE (MAX_DIST+2*READ_SIZE)

// hashing
#define HASH_SIZE 8192

static BYTE *mal_pwPrev;
static BYTE *mal_pwHash;
static BYTE *mal_pwLen;
static BYTE *mal_pbDataC;
static BYTE *mal_pbDataD;

static WORD *pwPrevL; // previous element in hash chain
static WORD *pwPrevH;
static WORD *pwHash;  // hash table
static WORD *pwLen;   // length per entry
BYTE *pbDataC;  // data buffer
BYTE *pbDataD;  // data buffer

static WORD wLH;   // last byte refered to in hash chains
static WORD wLR;   // last byte read
static WORD wLP;   // last byte to be processed in current batch
WORD wTOE;  // next byte to be encoded


/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       Initialization end exit code for ANA and REC.
*/

BYTE specStat=0;
BYTE *spp1;
BYTE *spp2;

// pwPrevL is used as temporary storage for the master
#pragma argsused
void InitPrevL (){
#ifndef UE2
   // the PREV array (BUF_SIZE WORD elements)
   if (specStat)
      mal_pwPrev = spp1;
   else
      mal_pwPrev = xmalloc (32768L*2+(BUF_SIZE-32768L)*2+15L,TMP);
   pwPrevL = (WORD *)normalize(mal_pwPrev);
   if (!specStat){
      RegDat ((BYTE *)pwPrevL,65534U); // double use memory for boosting
//      Out (7,"UC:RegDat\n\r");
   }
#endif
}

#pragma argsused
void InitAlloc1 (){ // approx 250 k (256000 bytes)
#ifndef UE2
   if (specStat)
      mal_pbDataC = spp2;
   else {
      mal_pbDataC = (BYTE *)exmalloc (BUF_SIZE+15L);
      if (!mal_pbDataC){
	 mal_pbDataC = xmalloc (BUF_SIZE+15L,TMP);
      }
   }
   pbDataC = normalize (mal_pbDataC);
#endif
}

#pragma argsused
void InitAlloc2 (){ // approx 250 k (256000 bytes)
#ifndef UE2
//#if BUF_SIZE > 32768L // unusual situations only
      pwPrevH = (WORD *)MK_FP(FP_SEG(pwPrevL)+4096,0);
//#endif

   mal_pwHash = xmalloc (HASH_SIZE*2L+15L,TMP);
   pwHash = (WORD *)normalize(mal_pwHash);

   mal_pwLen = xmalloc (HASH_SIZE*2L+15L,TMP);
   pwLen = (WORD *)normalize(mal_pwLen);
#endif
}

#pragma argsused
void ExitAlloc (){
#ifndef UE2
   specStat=1;
   spp1=mal_pwPrev;
   spp2=mal_pbDataC;

   xfree (mal_pwHash,TMP);
   xfree (mal_pwLen,TMP);
#endif
}

#pragma argsused
void NoSpec (){
#ifndef UE2
   InvHashC();
   if (specStat){
      xfree (spp1,TMP);
      xfree (spp2,TMP);
      specStat=0;
      UnRegDat();
//      Out (7,"UC:UnRegDat\n\r");
   }
#endif
}


/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       Hash chain management.
*/

//#define NO_ASM_HASH

#define HL1 3
#define HL2 6

#define HASH(p) ((WORD)(((pbDataC[p])^          \
			((pbDataC[p+1])<<HL1)^      \
			((0x7F&(pbDataC[p+2]))<<HL2))))   // 13 bits hash key

#define PutPrev(wIndex,wValue) {\
   if (wIndex<32768U){\
	 pwPrevL[wIndex]=wValue;\
   } else {\
      pwPrevH[wIndex^0x8000]=wValue;\
   }\
}

#define GetPrev(wIndex) ((wIndex<32768U)?pwPrevL[wIndex]:pwPrevH[wIndex^0x8000])

#define Enter(wPos) {\
   WORD hash = HASH(wPos);\
   WORD tmp = pwHash[hash];\
   pwHash[hash] = wPos;\
   PutPrev (wPos, tmp);\
   pwLen[hash]++;\
}

#pragma argsused
static void near pascal EnterMuch386 (WORD wFrom, WORD wOnto){
#ifndef UE2
#ifdef NO_ASM_HASH
   for (WORD i=wFrom; i<=wOnto; i++) Enter (i);
#else
   // assure only low OR high range usage
   if (wFrom<32768U && wOnto>=32768U){
      EnterMuch386 (wFrom, 32767);
      wFrom=32768U;
   }
   WORD segPrevL = FP_SEG(pwPrevL);
   WORD segPrevH = FP_SEG(pwPrevH);
   WORD segHash = FP_SEG(pwHash);
   WORD segLen = FP_SEG(pwLen);

   if (wFrom<32768U)
      _ES = segPrevL;
   else
      _ES = segPrevH;

   asm .386
   asm push ds
   _DS = FP_SEG(pbDataC);

   asm mov fs, segLen
   asm mov gs, segHash
   asm mov cl, HL2
   _DX = wOnto;
   for (_SI=wFrom; _SI<=_DX; _SI++){
      asm mov bl, [si]
      asm xor bh, bh
      asm mov al, [si+1]
      asm xor ah, ah
      asm shl ax, HL1
      asm xor bx, ax
      asm mov al, [si+2]
      asm and ax, 0x7F
      asm shl ax, cl
      asm xor bx, ax

      asm shl bx, 1
      asm inc word ptr fs:[bx]
      asm mov di, gs:[bx]
      asm mov gs:[bx], si

      asm mov bx, si
      asm shl bx, 1
      asm mov es:[bx], di
   }

   asm pop ds

   asm .8086
#endif
#endif
}

#pragma argsused
static void near pascal EnterMuch86 (WORD wFrom, WORD wOnto){
#ifndef UE2
#ifdef NO_ASM_HASH
   for (WORD i=wFrom; i<=wOnto; i++) Enter (i);
#else
   WORD segPrevL = FP_SEG(pwPrevL);
   WORD segPrevH = FP_SEG(pwPrevH);
   WORD segHash = FP_SEG(pwHash);
   WORD segLen = FP_SEG(pwLen);
   asm push ds
   _DS = FP_SEG(pbDataC);

   asm mov cl, HL2
   _DX = wOnto;
   for (_SI=wFrom; _SI<=_DX; _SI++){
      asm mov bl, [si]
      asm xor bh, bh
      asm mov al, [si+1]
      asm xor ah, ah
      asm shl ax, HL1
      asm xor bx, ax
      asm mov al, [si+2]
      asm and ax, 0x7F
      asm shl ax, cl
      asm xor bx, ax

      asm shl bx, 1
      _ES = segLen;
      asm inc word ptr es:[bx]
      _ES = segHash;
      asm mov DI, ES:[BX]
      asm mov ES:[BX], SI

      asm mov bx, si
      asm shl bx, 1
      asm jc high
      _ES = segPrevL;
      asm mov es:[bx], di
      continue;
high:
      _ES = segPrevH;
      asm mov es:[bx], di
   }

   asm pop ds
#endif
#endif
}

#define EnterMuch(f,o) if(m386)EnterMuch386(f,o);else EnterMuch86(f,o);

#define Remove(wPos) pwLen[HASH (wPos)]--

#pragma argsused
static void near pascal RemoveMuch (WORD wFrom, WORD wOnto){
#ifndef UE2
   InvHashC();
#ifdef NO_ASM_HASH
   for (WORD i=wFrom; i<=wOnto; i++) Remove (i);
#else
   asm push DS
   _ES = FP_SEG(pwLen);
   _DS = FP_SEG(pbDataC);
   asm mov cl, HL2
   for (_SI=wFrom; _SI<=wOnto; _SI++){
      asm mov bl, [si]
      asm xor bh, bh
      asm mov al, [si+1]
      asm xor ah, ah
      asm shl ax, HL1
      asm xor bx, ax
      asm mov al, [si+2]
      asm and ax, 0x7F
      asm shl ax, cl
      asm xor bx, ax
      asm shl bx, 1
      asm dec word ptr es:[bx]
   }
   asm pop DS
#endif
#endif
}



/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       Smart file input management.
*/

BYTE bCBar=0;
static BYTE bCtr;

#pragma argsused
WORD ReadBlock (void){
#ifndef UE2
   if (bCBar){
      Hint();
   } else
      bCtr=0;
   WORD ret;
   WORD backup = wLR;
   BrkQ();
   if (++wLR==MAX_DIST+READ_SIZE) wLR=0;

   ret = CReader (pbDataC+wLR, READ_SIZE);

#ifdef UCPROX
   if (heavy && ret>200){
      FletchUpdate (&Fin,pbDataC+wLR, ret-123);
      FletchUpdate (&Fin,pbDataC+wLR+ret-123, 123);
   } else {
      FletchUpdate (&Fin,pbDataC+wLR, ret);
   }
   if (debug&heavy){
      Out (7,"{CI:%u}",ret);
      Out (7,"[%u %lu %lu %lu]\n\r",ret,aa,bb,aa+bb);
   }
#else
   FletchUpdate (&Fin,pbDataC+wLR, ret);
#endif
   if (wLR==0)
      RapidCopy (pbDataC+MAX_DIST+READ_SIZE, pbDataC, READ_SIZE);
   if (ret==0){
      wLR = backup;
   } else {
      wLR += ret-1;
   }
   return ret;
#else
   return 0;
#endif
}

static char fCache=0;
static char fCheck=0;
static DWORD dwLasMas=4000000000L;
DWORD dwLM=4000000000L;
static int scbuf[10];

#pragma argsused
void OptInit (void){
#ifndef UE2
   if (!fCheck){
      if (coreleft16(2)>=10){
	 for (int i=0;i<10;i++) scbuf[i] = malloc16(2);
	 fCache=1;
      }
      fCheck=1;
   }
#endif
}

#pragma argsused
void Store (void){
#ifndef UE2
   to16 (scbuf[0],0,pwLen,16384);

   to16 (scbuf[1],0,pwHash,16384);

   to16 (scbuf[2],0,pwPrevL,16384);
   to16 (scbuf[3],0,pwPrevL+8192,16384);
   to16 (scbuf[4],0,pwPrevL+16384U,16384);
   to16 (scbuf[5],0,pwPrevL+24576U,16384);

   to16 (scbuf[6],0,pwPrevH,16384);
   to16 (scbuf[7],0,pwPrevH+8192,16384);
   to16 (scbuf[8],0,pwPrevH+16384U,16384);
   to16 (scbuf[9],0,pwPrevH+24576U,(BUF_SIZE-32768U)*2-49152U);
#endif
}

#pragma argsused
void ReStore (int max){
#ifndef UE2
//   Out (7,"[");
   from16 (pwLen,scbuf[0],0,16384);

   from16 (pwHash,scbuf[1],0,16384);

   if (max){
//      Out (7,"M");
      from16 (pwPrevL,scbuf[2],0,16384);
      from16 (pwPrevL+8192,scbuf[3],0,16384);
      from16 (pwPrevL+16384U,scbuf[4],0,16384);
      from16 (pwPrevL+24576U,scbuf[5],0,16384);

      from16 (pwPrevH,scbuf[6],0,16384);
      from16 (pwPrevH+8192,scbuf[7],0,16384);
      from16 (pwPrevH+16384U,scbuf[8],0,16384);
      from16 (pwPrevH+24576U,scbuf[9],0,(BUF_SIZE-32768U)*2-49152U);
   } // else
//      Out (7,"R");
//   Out (7,"]");
#endif
}

extern DWORD dwLM1;

// MUST be called on multi archive processing
void InvHashC (void){
   dwLM=4000000000L;
   dwLM1=4000000000L;
}

BYTE bDDelta;
BYTE bRel;

#pragma argsused
void InitANA1 (DWORD dwIndex){
#ifndef UE2
   bCtr=0;

/**** MASTER DEPENDENT PART ****/
   OptInit();

   // determine master length
   WORD wLen;
   Transfer (NULL, &wLen, dwIndex); // determine length

   wLH = MAX_DIST+READ_SIZE-wLen;
   if (dwLM==dwIndex && dwLM==dwLasMas && MODE.bCompressor<4){
      bRel=1;
   } else {
      if (MODE.bCompressor>3){
	 InvHashC();
	 dwLasMas=4000000000L;
      }
      Transfer ((BYTE *)(pbDataC+wLH), &wLen, dwIndex);
      bRel=0;
   }
#endif
}

#pragma argsused
BYTE InitANA2 (DWORD dwIndex, BYTE bDelta){
#ifndef UE2
   static BYTE bLDelta=255;
   if ((dwLasMas==dwIndex) && fCache){
      if (dwLM==dwIndex && dwLM==dwLasMas && bLDelta==bDelta){
	 ReStore (0);
      } else
	 ReStore (1);
   } else {
//      Out (7,"F");
      memset (pwLen, 0, HASH_SIZE*2);
      EnterMuch (wLH, MAX_DIST+READ_SIZE-10); // macro
      if (fCache)
	 Store();
   }
   bLDelta=bDelta;
   dwLM=dwLasMas=dwIndex;

/**** FILE DEPENDENT PART ****/

   // init data buffer
   wLR = MAX_DIST+READ_SIZE-1; // 'fake' ReadBlock
   WORD b1, b2=0;
   if (((b1=ReadBlock())==READ_SIZE)&&((b2=ReadBlock())==READ_SIZE)){
      // normal case, plenty data
      wLP = READ_SIZE-1;
      wLR = READ_SIZE*2-1;
   } else {
      // few data
      if (b1+b2==0) return 0; // special case, empty input
      wLP = wLR = b1+b2-1;
   }
   wTOE = 0;

   EnterMuch (MAX_DIST+READ_SIZE-9,MAX_DIST+READ_SIZE-1); // macro

   return 1; // no problems, input contains data
#endif
}

#pragma argsused
void ReadMore (void){ // called if wTOE passed wLP
#ifndef UE2
   if ((wLR+1)%(MAX_DIST+READ_SIZE)==wLH){ // near collision; clean chains
       RemoveMuch (wLH, wLH+READ_SIZE-1);
      wLH = (wLH+READ_SIZE)%(MAX_DIST+READ_SIZE);
   }
   wLP = wLR;
   if (ReadBlock()!=READ_SIZE){
      if (wLR<512){
	 wLR+=MAX_DIST+READ_SIZE;
      }
      wLP = wLR; // flag end of data
      if (wTOE<512){
	 wTOE+=MAX_DIST+READ_SIZE;
      }
      if (wLP<2000 && wTOE >2000){
	 wTOE-=MAX_DIST+READ_SIZE;
      }
   }
#endif
}


/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       Match management.
*/

// determine match length at p1,p2 combination
#pragma argsused
static WORD near pascal MatchLen (WORD p1, WORD p2){
#ifndef UE2
   WORD DS = _DS;

   _ES = _DS = FP_SEG(pbDataC);

   _AX = 0;
   _SI = p1;
   _DI = p2;

   asm cmpsb
   asm jne fast
   asm inc AX
   asm cmpsb
   asm jne fast
   asm inc AX
   asm cmpsb
   asm jne fast
   asm inc AX
   asm cmpsb
   asm jne fast
   asm inc AX
   asm cmpsb
   asm jne fast
   asm inc AX
more:
   asm cmpsb
   asm jne done
   asm inc AX
   asm cmpsb
   asm jne done
   asm inc AX
   asm cmpsb
   asm jne done
   asm inc AX
   asm cmpsb
   asm jne done
   asm inc AX
   asm cmpsb
   asm jne done
   asm inc AX
   asm cmp AX, MAX_LEN-1
   asm jng more

done:
   if (_AX>MAX_LEN) _AX=MAX_LEN;
fast:
   _DS = DS;
   return _AX;
#endif
}

// find best match
//    pos    current position (is added to hash chains!)
//    len    length of found match
//    mpos   position of found match
//    upper  upper limit to search depth
//    min    minimal length of match to find (7 -> 8 size at least etc)

/*

This function might seem complicated but what happens is actually
quite simple.
   The first part (until label "useg:") initializes all kinds of local data.
   Since the machine code changes the data segment (DS) ALL data has to
   be available in either a register or on the stack. All addresses have
   been normalized (zero offset) and their segments are stored.

   The second part (largest) until the label "checkit" processes the hash
   chain. Of every string only the n'th (stored in the BX register)
   character is compared. If this one matches (only about 10% of all
   half of which turn out to be OK) checkit takes over. The major reason
   for the size of the code is speed.
   -  There are two almost equal sized elements. The LOW and the HIGH
      one. Each of them assumes the current position in either pwPrevH
      or pwPrevL. Since the pointers are near each other swaps between
      the two partitions are rare. By using this method the disadvangate
      of an 128kb array on the Intel processor is minimized.
   -  The second "trick" is loop unrolling. To minimize the number
      of counter check&updates (dec cx, jz ...) they are processed
      in chunks. There are numerous two line routines which "repair"
      the temporary damage caused by the counter not being updated.

   The third part (from checkit: to giveup:) does a real check of
   the length of the candidate match. There is a limit to the
   length a match can have (due to the size of the read ahead buffer
   which is limited because of the 64kb data buffer limit) but loop
   enrolling has been used to minimize the disadvantages this has.
      The "gup" variable (GIVE_UP dirived) is for faster processing
      of long rows of zero's etc. Executables tend to have these
      a lot (arrays etc).

   The rest is just to connect the ASM results with the C/C++ interface and
   to add the new enrty to the hash chain.
*/

#pragma argsused
static void near pascal FindMaxLen (WORD pos, WORD *len, WORD *mpos, WORD upper, WORD min, WORD hash, WORD wlen){
#ifndef UE2
// prepare match finding
//   WORD hash = HASH (pos);
   WORD maxlen = 0;
   WORD segData = FP_SEG(pbDataC);
   WORD segHi = FP_SEG(pwPrevH);
   WORD segLo = FP_SEG(pwPrevL);
   WORD segDS = _DS;
   WORD gup = GIVE_UP;

   _SI = pos;

   _CX = wlen;

   WORD maxpos=0;

   // are there matches?
   if (_CX){

      if (_CX>upper) _CX=upper; // not too deep
      _CX++;
      _DI = pwHash[hash]; // first potential match

      // compare all potential matches and pick the winner

//***** one entrance, two leaves

      _DS = segData;
      _BX = min;
      asm mov DL, [SI+BX];

useg:
      if (_DI&0x8000) goto hiseg;

lowseg:
      asm mov ES, segLo
      if (_CX>5) goto lspeed;
      asm dec CX
      asm jne lowagain
      asm jmp giveup
lowagain:
      asm cmp [DI+BX], DL
      asm je checkitsp1
      asm shl DI, 1
      asm jc himidsp1
lowmidp:
      asm mov DI, ES:[DI]
lresp:
      if (_CX>5) goto lspeed;
      asm dec CX
      asm jnz lowagain
      goto giveup;

checkitsp1:
      asm jmp checkit

himidsp1:
      asm jmp himid

lspeed:
      asm cmp [DI+BX], DL
      asm je checkit_l1
      asm shl DI, 1
      asm jc himid_l1
      asm mov DI, ES:[DI]

      asm cmp [DI+BX], DL
      asm je checkit_l2
      asm shl DI, 1
      asm jc himid_l2
      asm mov DI, ES:[DI]

      asm cmp [DI+BX], DL
      asm je checkit_l3
      asm shl DI, 1
      asm jc himid_l3
      asm mov DI, ES:[DI]

      asm cmp [DI+BX], DL
      asm je checkit_l4
      asm shl DI, 1
      asm jc himid_l4
      asm mov DI, ES:[DI]

      asm sub CX, 4
      asm jmp lresp

checkit_l1:
      asm dec CX
      asm jmp checkit

himid_l1:
      asm dec CX
      asm jmp himid

checkit_l2:
      asm sub CX,2
      asm jmp checkit

himid_l2:
      asm sub CX,2
      asm jmp himid

checkit_l3:
      asm sub CX,3
      asm jmp checkit

himid_l3:
      asm sub CX,3
      asm jmp himid

checkit_l4:
      asm sub CX,4
      asm jmp checkit

himid_l4:
      asm sub CX,4
      asm jmp himid

lowmid:
      asm mov ES, segLo
      goto lowmidp;

hiseg:
      asm mov ES, segHi;
      if (_CX>5) goto hspeed;
      asm dec CX
      asm jnz hiagain
      asm jmp giveup
hiagain:
      asm cmp [DI+BX], DL
      asm je checkitsp2
      asm shl DI, 1
      asm jnc lowmidsp2
himidp:
      asm mov DI, ES:[DI]
hresp:
      if (_CX>5) goto hspeed;
      asm dec CX
      asm jnz hiagain
      goto giveup;

himid:
      asm mov ES, segHi
      goto himidp;

checkitsp2:
      asm jmp checkit

lowmidsp2:
      asm jmp lowmid

hspeed:
      asm cmp [DI+BX], DL
      asm je checkit_h1
      asm shl DI, 1
      asm jnc lowmid_h1
      asm mov DI, ES:[DI]

      asm cmp [DI+BX], DL
      asm je checkit_h2
      asm shl DI, 1
      asm jnc lowmid_h2
      asm mov DI, ES:[DI]

      asm cmp [DI+BX], DL
      asm je checkit_h3
      asm shl DI, 1
      asm jnc lowmid_h3
      asm mov DI, ES:[DI]

      asm cmp [DI+BX], DL
      asm je checkit_h4
      asm shl DI, 1
      asm jnc lowmid_h4
      asm mov DI, ES:[DI]

      asm sub CX, 4
      asm jmp hresp

checkit_h1:
      asm dec CX
      asm jmp checkit

lowmid_h1:
      asm dec CX
      asm jmp lowmid

checkit_h2:
      asm sub CX,2
      asm jmp checkit

lowmid_h2:
      asm sub CX,2
      asm jmp lowmid

checkit_h3:
      asm sub CX,3
      asm jmp checkit

lowmid_h3:
      asm sub CX,3
      asm jmp lowmid

checkit_h4:
      asm sub CX,4
      asm jmp checkit

lowmid_h4:
      asm sub CX,4
      asm jmp lowmid

//*****

checkit: // leave 1
      _ES = _DS;
      asm push si
      asm push di
      asm xor ax,ax

      asm cmpsb
      asm jne done
      asm inc ax
      asm cmpsb
      asm jne done
      asm inc ax
      asm cmpsb
      asm jne done
      asm inc ax
      asm cmpsb
      asm jne done
      asm inc ax
more:
      asm cmpsb
      asm jne check
      asm inc ax
      asm cmpsb
      asm jne check
      asm inc ax
      asm cmpsb
      asm jne check
      asm inc ax
      asm cmpsb
      asm jne check
      asm inc ax
      asm cmp ax, MAX_LEN-1
      asm jng more
check:
      if (_AX>MAX_LEN) _AX=MAX_LEN;
done:
      asm pop di
      asm pop si
      if (_AX>maxlen){
	 maxlen = _AX;
	 _BX = _AX;
	 maxpos = _DI;
	 if (maxlen>gup){
	    goto giveup; // speed up repeated zero etc
	 }
	 asm mov DL, [SI+BX]
      }

      asm shl DI,1
      asm jc his
      asm mov ES, segLo
      asm mov DI, ES:[DI]
      goto useg;
his:
      asm mov ES, segHi
      asm mov DI, ES:[DI]
      goto useg;
   }

giveup: // leave 2
   _DS = segDS;
   *len = maxlen;
   *mpos = maxpos;

   // add current position to hash chains
   WORD tmp = pwHash[hash];
   pwHash[hash] = _SI;
   PutPrev (_SI, tmp);
   pwLen[hash]++;
#endif
}

/*
static void near pascal Skip (WORD pos, WORD hash){
   WORD tmp = pwHash[hash];
   pwHash[hash] = pos;
   PutPrev (pos, tmp);
   pwLen[hash]++;
}
*/


/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       Huffman stuff.
*/

WORD wLDFreq[2*(256+60)];  // frequency counts for literal distance
WORD wLFreq[2*28];         // frequency counts for length
BYTE bLen[256+60+28];      // huffman tree code lengths
WORD wCode[256+60+28];     // huffman tree code codes
WORD *LDdecodeVal;         // literal+distance decode values
BYTE *LDdecodeLen;         // lit+dis decode lengths
WORD *LdecodeVal;          // length decode values
BYTE *LdecodeLen;          // length decode lengths

/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       Smasher etc stuff.
*/

WORD wBNo=28;        // number of blocks in HBUFF system
#define MAXBNO 50    // maximal number of blocks in HBUFF system
#define HIBUFS 490   // size of a single block (in words)

WORD *ppwBufs[MAXBNO]; // special high-speed buffer
WORD wSpSiz=0;         // size of this buffer

VPTR vpBufs[MAXBNO];  // virtual pointers to HBUFF system
WORD wLimit[MAXBNO];  // last value of biptr

WORD *pwIBuf;        // pointer to current active buffer block
WORD bbptr;          // index of current active buffer block

WORD biptr;          // index in currect active buffer block (for NEXT item)
WORD bgptr;          // index number of current subbuffer (for NEXT item)

void UCBoost (BYTE *data, int size){
   for (WORD i=0;i<size;i++){
      ppwBufs[i] = (WORD *)(data+1024U*i);
   }
   wSpSiz=size;
}

void SetIBuf (int index){
   if (bbptr==index) return;
   if (bbptr!=255){
      if (bbptr+1>wSpSiz)
	 UnAcc (vpBufs[bbptr]);
   }
   bbptr = index;
   if (index!=255){
      if (bbptr+1>wSpSiz){
	 pwIBuf = (WORD *)Acc(vpBufs[bbptr]);
      }else
	 pwIBuf = ppwBufs[bbptr];
   }
}

void ClearFreq (){
   for (int i=0;i<256+60;i++){
      wLDFreq[i]=0;
   }
   for (i=0;i<28;i++){
      wLFreq[i]=0;
   }
}

#ifdef UCPROX
   void FakeFreq (){
      for (int i=0;i<256+60;i++){
         Out (7,"[%3d %d]\r\n",i,wLDFreq[i]);
	 wLDFreq[i]=1;
      }
      for (i=0;i<28;i++){
         Out (7,"[%3d %d]\r\n",i,wLFreq[i]);
	 wLFreq[i]=1;
      }
   }
#endif

void BufInit (void){
   static int first=1;
   if (first)
      first=0;
   else
      return;
   for (int i=0;i<MAXBNO;i++){
      if (i+1>wSpSiz) {
	 vpBufs[i] = Vmalloc (HIBUFS*2+20); // (reasonable) overflow allowed
      } else
	 vpBufs[i] = VNULL;
   }
   bbptr=255; // special signal value
   SetIBuf(0);
   if (getenv("UC2_HUFBUF")){
      wBNo = (WORD)(atol(getenv("UC2_HUFBUF"))/HIBUFS);
      if (wBNo<5) wBNo=5;
      if (wBNo>MAXBNO-3) wBNo=MAXBNO-3;
   }
}

#pragma argsused
void InitPut (){
#ifndef UE2
   BufInit();
   biptr=0;
   bgptr=0;
   SetIBuf(bgptr);

   ClearFreq();
   TreeInit();
#endif
}

#define HPutLen(val) PUTBITS (wCode[val+256+60],bLen[val+256+60])

#define HPutLD(val) PUTBITS (wCode[val],bLen[val])

#pragma argsused
void FlushLLD(){
#ifndef UE2
//   printf ("[%d",bptr);
//   Out (7,"{TREE}");
   if ((biptr==0)&&(bgptr==0)) return;
   RPutLenDst (3,EOB_MARK); // flag end of block (note length==3!!)
#ifdef UCPROX
   if (beta && 0==strcmp(strupr(getenv("NOTREE")),"ON")) FakeFreq();
#endif
   TreeGen (wLDFreq, 256+60, 13, bLen);
   TreeGen (wLFreq, 28, 13, bLen+256+60);
//   PUTBITS (no,16);
   PUTBITS (1,1);
   if ((bgptr==0)&&(biptr<256))
      TreeEnc (bLen, 1);
   else
      TreeEnc (bLen, 0);
   CodeGen (256+60, bLen, wCode);
   CodeGen (28,bLen+256+60, wCode+256+60);

   wLimit[bgptr] = biptr;

//   printf ("|");
   for (WORD j=0;j<bgptr+1;j++){
      WORD lim=wLimit[j];
      SetIBuf(j);
      if (lim) for (WORD i=0;i<lim;i++){
	 if (pwIBuf[i]<256){ // literal
	    HPutLD(pwIBuf[i]);
	 } else { // length+distance
	    WORD dst = pwIBuf[i]-256;
	    if (dst<16){
	       HPutLD(dst+255); // 256..270
	    } else if (dst<256) {
	       HPutLD(dst/16+14+256);
	       PUTBITS(dst%16, 4);
	    } else if (dst<4096) {
	       HPutLD(dst/256+29+256);
	       PUTBITS(dst%256, 8);
	    } else {
	       HPutLD(dst/4096+44+256);
	       PUTBITS(dst%4096, 12);
	    }
	    WORD len = pwIBuf[++i];
	    if (len<11){
	       HPutLen (len-3);
	    } else if (len<27){
	       HPutLen((len-11)/2+8);
	       PUTBITS((len-11)%2,1);
	    } else if (len<91){
	       HPutLen((len-27)/8+16);
	       PUTBITS((len-27)%8,3);
	    } else if (len<155){
	       HPutLen(24);
	       PUTBITS(len-91,6);
	    } else if (len<667){
	       HPutLen(25);
	       PUTBITS(len-155,9);
	    } else if (len<2715){
	       HPutLen(26);
	       PUTBITS(len-667,11);
	    } else {
	       HPutLen(27);
	       PUTBITS(len-2715,15);
	    }
	 }
      }
   }
   biptr=0;
   bgptr=0;
   SetIBuf(bgptr);
#endif
}

#ifdef HYPT
DWORD ins;
#endif

DWORD ccc;


void MFlush (void){
   if (bgptr==wBNo-1){
      FlushLLD();
      ClearFreq();
   } else {
      wLimit[bgptr++]=biptr;
      biptr=0;
      SetIBuf(bgptr);
   }
}

void  PutLit (BYTE lit){
   ccc++;
#ifdef HYPT
   ins++;
#endif
#ifdef UCPROX
   if (beta && 0==strcmp(strupr(getenv("SMASHDUMP")),"ON")){
#ifdef HYPT
      Out (7,"[%u]%ld\n\r",(WORD)lit,ins);
#else
      Out (7,"[%u]\n\r",(WORD)lit);
#endif
   }
   aa++;
#endif
   pwIBuf[biptr++] = lit;
   wLDFreq[lit]++; // count literal
   if(biptr>HIBUFS){
      MFlush();
   }
}

void  PutLenDst (WORD len, WORD dst){
   ccc+=len;
#ifdef HYPT
   ins+=len;
#endif
#ifdef UCPROX
   bb+=len;
#endif
   RPutLenDst (len,dst);
#ifdef UCPROX
   if (beta && 0==strcmp(strupr(getenv("SMASHDUMP")),"ON")){
#ifdef HYPT
      Out (7,"[%u %u]%ld\n\r",len,dst,ins);
#else
      Out (7,"[%u %u]\n\r",len,dst);
#endif
   }
#endif
   if(biptr>HIBUFS){
      MFlush();
   }
}

#pragma argsused
void  RPutLenDst (WORD len, WORD dst){
#ifndef UE2
   pwIBuf[biptr++] = dst+256;
   pwIBuf[biptr++] = len;
   if (dst<16){
      wLDFreq[dst+255]++;
   } else if (dst<256) {
      wLDFreq[dst/16+14+256]++;
   } else if (dst<4096) {
      wLDFreq[dst/256+29+256]++;
   } else {
      wLDFreq[dst/4096+44+256]++;
   }
   if (len<11){
      wLFreq[len-3]++;
   } else if (len<27){
      wLFreq[(len-11)/2+8]++;
   } else if (len<91){
      wLFreq[(len-27)/8+16]++;
   } else if (len<155){
      wLFreq[24]++;
   } else if (len<667){
      wLFreq[25]++;
   } else if (len<2715){
      wLFreq[26]++;
   } else {
      wLFreq[27]++;
   }
#endif
}

#pragma argsused
void FlushPut (){
#ifndef UE2
   FlushLLD();
//   PUTBITS (0,16);
   PUTBITS (0,1);
   FlushBitsOut();
#endif
}

WORD GetNo (){
   WORD ret = GETBITS(1);
   if (ret){
      TreeDec (bLen);
      DCodeGen (256+60, bLen, LDdecodeVal, LDdecodeLen);
      DCodeGen (28, bLen+256+60, LdecodeVal, LdecodeLen);
   }
   return ret;
}

WORD GetLitDst (){
   WORD Snoop = SNOOP13();
   WORD val = LDdecodeVal[Snoop];
   SKIP (LDdecodeLen[Snoop]);
   if (val<256) return val;
   if (val<271) return (val-255)+256;
   if (val<286) return (val-270)*16+GETBITS(4)+256;
   if (val<301) return (val-285)*256+GETBITS(8)+256;
   return (val-300)*4096+GETBITS(12)+256;
}

WORD GetLen (){
   WORD Snoop = SNOOP13();
   BYTE sym = LdecodeVal[Snoop];
   SKIP (LdecodeLen[Snoop]);
   if (sym<8) return sym+3;
   if (sym<16) return (sym-8)*2+11+GETBITS(1);
   if (sym<24) return (sym-16)*8+27+GETBITS(3);
   if (sym==24) return (91+GETBITS(6));
   if (sym==25) return (155+GETBITS(9));
   if (sym==26) return (667+GETBITS(11));
   return (2715+GETBITS(15));
}

/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       Compressor/decompressor core code.
*/

void pascal far SpComp (WORD len, WORD dst);
WORD hhint=0, llen, ddst;

void SecureSmast (void);

#pragma argsused
void pascal far UltraCompressor (DWORD dwMaster, BYTE bDelta){
#ifndef UE2
   ccc=0;
#ifdef HYPT
   ins=0;
   Out (7,"\n\r[[[\n\r");
#endif
   if (dwMaster!=NOMASTER) SecureSmast();

   bDDelta = bDelta;

   if (hhint){
//      Out (7,"HINT");
      SpComp (llen, ddst);
      SetIBuf (255);
      return;
   }


   FREC SFin, SFout;
   WORD len, pos, dst;
   WORD tlen=0, tpos=0; // temporary's for lazy evaluation
   BYTE flag=0;

   FletchInit (&Fin);
   FletchInit (&Fout);

   // Now there is enough memory for decompression.
   WORD wLen;

   InitAlloc1();

   InitANA1(dwMaster);

   SFin = Fin;
   SFout = Fout;
   Transfer (NULL, &wLen, dwMaster);
   Fin = SFin;
   Fout = SFout;

   InitPrevL();

   if (!bRel && bDelta && (dwMaster!=SUPERMASTER)){
      DeltaBlah db;
      InitDelta (&db, bDelta);
      Delta (&db, (BYTE *)(pbDataC+wLH), wLen);
   }

   InitBitsOut();
   InitPut();
   InitAlloc2();
   if (InitANA2(dwMaster, bDelta)){ // if not empty input (else skip)
      for (;;){
	 if (flag){ // lazy evaluation in progress
	    len = tlen;
	    pos = tpos;
	    flag = 0;
	 } else {
	    WORD hash = HASH (wTOE);
	    WORD wlen = pwLen[hash];
#ifdef UCPROX
	    if (beta && 0==strcmp(strupr(getenv("NOMATCH")),"ON"))
	       FindMaxLen (wTOE, &len, &pos, MAX_DEPTH, 2, hash, 0);
	    else
#endif
	    FindMaxLen (wTOE, &len, &pos, MAX_DEPTH, 2, hash, wlen);
	 }
	 if (len>2){ // match found
	    // calculate distance
	    if (wTOE>pos)
	       dst = wTOE-pos;
	    else
	       dst = wTOE-pos+MAX_DIST+READ_SIZE;
	    if (dst>MAX_DIST){
	       goto skip;
	    }
//	    if (dst==63548U){
//	       Out (7,"[%u,%u,%ld]",dst,len,ins);
//	    }

	    if (len<LAZY_LIMIT){ // lazy evaluation allowed here
	       WORD hash = HASH (wTOE+1);
	       WORD wlen = pwLen[hash];
#ifdef UCPROX
	       if (beta && 0==strcmp(strupr(getenv("NOMATCH")),"ON"))
		  FindMaxLen (wTOE+1, &tlen, &tpos, LAZY_DEPTH, len, hash, 0);
	       else
#endif
	       FindMaxLen (wTOE+1, &tlen, &tpos, LAZY_DEPTH, len, hash, wlen);
	       if (tlen>len) {
		  flag=1;
		  goto skip;
	       }
	    } else {
	       Enter(wTOE+1);
	    }
	    if (len>3) {
	       EnterMuch (wTOE+2, wTOE+len-1); // macro!
	    } else
	       Enter (wTOE+2);

	    // calculate new wTOE
	    wTOE+=len;
even_more:
	    if (wTOE>wLP){
	       if (wLP==wLR){
		  len-=wTOE-wLP-1;
		  wTOE=wLP+1;
		  if (len>2){
		     PutLenDst (len, dst);
		     goto done;
		  } else {
		     wTOE-=len;
		     PutLit (pbDataC[wTOE]);
		     if (len==2){
			PutLit (pbDataC[wTOE+1]);
		     }
		  }
		  goto done;
	       } else {
		  ReadMore();
		  if (wLP!=wLR) wTOE %= (MAX_DIST+READ_SIZE);
	       }
	    }

	    if ((len>=MAX_LEN) && (dst<124*512U)){ // extended match length enhancement
#ifdef UCPROX
	       if (!beta || 0!=strcmp(strupr(getenv("NOMALEN")),"ON"))
#endif

	       if (len<MAX_XLEN){
		  WORD other;
		  if (wTOE<dst)
		     other = wTOE+MAX_DIST+READ_SIZE-dst;
		  else
		     other = wTOE-dst;
		  WORD tmp = MatchLen (wTOE, other);
		  if (tmp+len>MAX_XLEN){
		     tmp = MAX_XLEN-len;
		  }
		  if (tmp){
		     EnterMuch (wTOE, wTOE+tmp-1); // macro
		     wTOE+=tmp;
		     len+=tmp;
		     goto even_more;
		  }
	       }
	    }

report:
	    PutLenDst (len, dst);
	 } else { // no match found, literal
skip:
	    PutLit (pbDataC[wTOE]);
	    wTOE++;
	    if (wTOE>wLP){ // detect if boundary has been reached
	       if (wLP==wLR)
		  goto done;
	       else {
		  ReadMore();
		  if (wLP!=wLR) wTOE %= (MAX_DIST+READ_SIZE);
	       }
	    }
	 }
      }

   }
done:
   FlushPut();
   ExitAlloc();
#ifdef HYPT
   Out (7,"\n\r %ld ]]]\n\r",ins);
#endif
   SetIBuf (255);
#endif
}

void pascal far SpComp (WORD len, WORD dst){
   InitBitsOut();
   InitPut();

#ifdef UCPROX
   if (beta){
      if (0==stricmp(getenv("BUGS"),"ON")){
	 dst-=1;
	 Out (7,"AUTO-BUG (tm)");
      }
   }
#endif

   while (len){
      if (len>30000){
	 PutLenDst (30000, dst);
	 len-=30000;
      } else {
	 PutLenDst (len, dst);
	 len=0;
      }
   }
   Hint();

   FlushPut();
}

static WORD wSkip;
WORD wComp;

BYTE bDBar=0; // decompression bar

DWORD maxwrite;
DWORD written;

int FlushIt (void){
   int ret=0;
   if (bDBar) Hint();
   WORD tot, ofs;
   tot = 32768U;
   wComp ^= 0x8000;
   ofs = wComp;
   if (wSkip){
      if (wSkip>=32768U){
	 wSkip -= 32768U;
	 return 0;
      } else {
	 tot-=wSkip;
	 ofs+=wSkip;
	 wSkip=0;
      }
   }
   if ((tot+written)>maxwrite){
      tot=(WORD)(maxwrite-written);
      ret=1;
   }
   written+=tot;
   if (tot){
      CWriter (pbDataD+ofs, tot);
      FletchUpdate (&Fout, pbDataD+ofs, tot);
   }
#ifdef UCPROX
   if (debug&heavy) Out (7,"{DCW:%u}",tot);
#endif
   return ret;
}

int FlushAll (){
   int ret;
   WORD tot, ofs;
   ofs = wTOE&0x8000;
   tot = wTOE-ofs;
   tot -= wSkip;
   ofs += wSkip;
   if ((tot+written)>maxwrite){
      tot=(WORD)(maxwrite-written);
      ret=1;
   }
   written+=tot;
   if (tot){
      CWriter (pbDataD+ofs, tot);
      FletchUpdate (&Fout, pbDataD+ofs, tot);
#ifdef UCPROX
      if (debug&heavy) Out (7,"{DCW:%u (flushall)}",tot);
#endif
   }
   return ret;
}

extern WORD *DCodeTable;
extern BYTE *DCodeLengths;

extern int nuke1 (WORD es, WORD ds);

#ifdef HYPT
DWORD glob;
#endif

#ifdef UCPROX

int nuke2 (){
   WORD dst, len;
   for (;;){
      dst = GetLitDst();
      if (dst<256){
	 pbDataD[wTOE++] = (BYTE)dst;
#ifdef HYPT
	 glob++;
#endif
#ifdef UCPROX
	 if (beta && 0==strcmp(strupr(getenv("SMASHDUMP")),"ON"))
	    Out (7,"{%u}\n\r",(WORD)pbDataD[wTOE-1]);
#endif
      } else {
	 dst-=256;
	 len = GetLen();
	 if (dst==MAX_DIST+1){ // end of block signal
	    return 0;
	 }
#ifdef HYPT
	 glob+=len;
#endif
#ifdef UCPROX
	 if (beta && 0==strcmp(strupr(getenv("SMASHDUMP")),"ON"))
	    Out (7,"{%u %u}\n\r",len,dst);
#endif
	 for (WORD i=0;i<len;i++)
	    pbDataD[wTOE+i] = pbDataD[wTOE-dst+i];
	 wTOE+=len;
      }
      if ((wTOE&0x8000) == wComp){
	 if (FlushIt()) return 1;
      }
   }
}

#endif

void pascal far UltraDecompressor (DWORD dwMaster, BYTE bDelta, DWORD len){
#ifdef HYPT
   glob=0;
   Out (7,"\n\r[[[\n\r");
#endif
   if (dwMaster!=NOMASTER) SecureSmast();

   written=0;
   maxwrite=len;
   BYTE *sa, *sb;
   FREC SFin, SFout;

   FletchInit (&Fin);
   FletchInit (&Fout);

   mal_pbDataD = xmalloc (64*1024L+15,TMP);
   pbDataD = normalize (mal_pbDataD);
   sa = mal_pbDataD;
   sb = pbDataD;

   SFin = Fin;
   SFout = Fout;
   BYTE tmp = bDBar;
   bDBar=0;
   DWORD writ=written;
   DWORD max=maxwrite;
   Transfer (pbDataD, &wSkip, dwMaster);
   written=writ;
   maxwrite=max;
   bDBar = tmp;
   Fin = SFin;
   Fout = SFout;

   if (bDelta && dwMaster != SUPERMASTER){
      DeltaBlah db;
      InitDelta (&db, bDelta);
      Delta (&db, pbDataD, wSkip);
   }
   mal_pbDataD = sa;
   pbDataD = sb;

   wTOE = wSkip;

   TreeInit();
   BYTE *powerbuf;
   powerbuf = (BYTE *)exmalloc (64*1024L);
   if (!powerbuf){
      powerbuf = xmalloc (64*1024L+15, TMP);
   }
   BYTE *pownor = normalize (powerbuf);
   RegDat (pbDataD, 65534U);
//   Out (7,"UD:RegDat\n\r");

   DCodeTable = LDdecodeVal = (WORD *)pownor; // 16384
   DCodeLengths = LDdecodeLen = pownor+16384; // 8192
   LdecodeVal  = (WORD *)(pownor+24576); // 16384
   LdecodeLen  = pownor+40960U;// 8192

   if (wSkip>32767U){
      wComp = 0;
      wSkip -= 32768U;
   } else {
      wComp = 0x8000;
   }

   InitBitsIn((WORD *)(pownor+49152U));
   WORD no = GetNo();
   while (no){
#ifdef UCPROX
      if (beta && 0==strcmp(strupr(getenv("SMASHDUMP")),"ON")) {
	 if (nuke2()) goto end; // C++
      } else {
	 if (nuke1(FP_SEG(pownor), FP_SEG(pbDataD))) goto end; // ASM
      }
#else
      if (nuke1(FP_SEG(pownor), FP_SEG(pbDataD))) goto end; // ASM
#endif
      BrkQ();
      no = GetNo();
   }
   FlushAll();
end:
   xfree (mal_pbDataD, TMP);
   ExitBitsIn();
   UnRegDat ();
//   Out (7,"UD:UnRegDat\n\r");
   xfree (powerbuf, TMP);
#ifdef HYPT
   Out (7,"\n\r %ld ]]]\n\r",glob);
#endif
}

