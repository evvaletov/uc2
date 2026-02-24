#pragma inline

#include <dos.h>
#include <mem.h>
#include "main.h"
#include "compint.h"
#include "video.h"
#include "diverse.h"
#include "handle.h"

#define EOB_MARK  125*512+1U   // copy from ULTRACMP !!!
#define IBUFS     1024U     // copy from BITIO !!!

extern WORD wTOE;
extern BYTE *pbData;
extern WORD wComp;
extern int FlushIt(void);
extern WORD *LDdecodeVal;
extern BYTE *LDdecodeLen;
extern WORD *LdecodeVal;
extern BYTE *LdecodeLen;

WORD wInBuf;   // right alligned remaining bits (processed bits are 0)
BYTE bInLeft;  // number of unprocessed bits (1..16) in wInBuf
WORD wIn13;    // next 13 bits of input stream

WORD ptr;

extern WORD *buffer;
#define RSKIP()                                      \
   _SI = (_SI<<_DL) & 0x1FFF;                   \
   if (bInLeftc<_DL){                                \
      _SI |= wInBufc<<(_DL-bInLeftc);               \
						      \
      asm mov bx,ptr_;                                 \
      asm mov ax,word ptr es:[bx];                   \
      asm mov wInBufc,ax;                               \
						      \
      if (ptr_==49152U+2*(IBUFS/2-1)){                   \
	 asm push ods;                                   \
	 asm pop ds;                                    \
	 asm push dx;                                   \
	 BrkQ();                                       \
	 CReader ((BYTE *)buffer, IBUFS);             \
	 asm pop dx;                                   \
	 asm push vds;                                  \
	 asm pop ds;                                   \
	 ptr_=49152U;                                   \
	 asm push ves;                                    \
	 asm pop es;                                      \
      }else                                           \
	 ptr_+=2;                                       \
						      \
      bInLeftc = 16-_DL+bInLeftc;                     \
      _SI |= wInBufc>>(bInLeftc);                     \
      wInBufc &= 0xFFFF>>(16-bInLeftc);                 \
   } else if (bInLeftc!=_DL){                        \
      _SI |= wInBufc>>(bInLeftc-_DL);               \
      bInLeftc -= _DL;                               \
      wInBufc &= 0xFFFF>>(16-bInLeftc);                 \
   } else {                                           \
      _SI |= wInBufc;                                \
						      \
      asm mov bx,ptr_;                                 \
      asm mov ax,word ptr es:[bx];                     \
      asm mov wInBufc,ax;                              \
						      \
      if (ptr_==49152U+2*(IBUFS/2-1)){                            \
	 asm push ods;                                  \
	 asm pop ds;                                    \
	 asm push dx;                                   \
	 BrkQ();                                        \
	 CReader ((BYTE *)buffer, IBUFS);             \
	 asm pop dx;                                    \
	 asm push vds;                                  \
	 asm pop ds;                                   \
	 ptr_=49152U;                                       \
	 asm push ves;                                    \
	 asm pop es;                                      \
      }else                                           \
	 ptr_+=2;                                       \
						      \
      bInLeftc = 16;                                   \
   }

#define FSKIP(fix)                                 \
   _SI = (_SI<<fix) & 0x1FFF;                   \
   if (bInLeftc<fix){                                \
      _SI |= wInBufc<<(fix-bInLeftc);               \
						      \
      asm mov bx,ptr_;                                 \
      asm mov ax,word ptr es:[bx];                   \
      asm mov wInBufc,ax;                               \
						      \
      if (ptr_==49152U+2*(IBUFS/2-1)){                   \
	 asm push ods;                                   \
	 asm pop ds;                                    \
	 asm push dx;                                   \
	 CReader ((BYTE *)buffer, IBUFS);             \
	 asm pop dx;                                   \
	 asm push vds;                                  \
	 asm pop ds;                                   \
	 ptr_=49152U;                                   \
	 asm push ves;                                    \
	 asm pop es;                                      \
      }else                                           \
	 ptr_+=2;                                       \
						      \
      bInLeftc = 16-fix+bInLeftc;                     \
      _SI |= wInBufc>>(bInLeftc);                     \
      wInBufc &= 0xFFFF>>(16-bInLeftc);                 \
   } else if (bInLeftc!=fix){                        \
      _SI |= wInBufc>>(bInLeftc-fix);               \
      bInLeftc -= fix;                               \
      wInBufc &= 0xFFFF>>(16-bInLeftc);                 \
   } else {                                           \
      _SI |= wInBufc;                                \
						      \
      asm mov bx,ptr_;                                 \
      asm mov ax,word ptr es:[bx];                     \
      asm mov wInBufc,ax;                              \
						      \
      if (ptr_==49152U+2*(IBUFS/2-1)){                            \
	 asm push ods;                                  \
	 asm pop ds;                                    \
	 asm push dx;                                   \
	 CReader ((BYTE *)buffer, IBUFS);             \
	 asm pop dx;                                    \
	 asm push vds;                                  \
	 asm pop ds;                                   \
	 ptr_=49152U;                                       \
	 asm push ves;                                    \
	 asm pop es;                                      \
      }else                                           \
	 ptr_+=2;                                       \
						      \
      bInLeftc = 16;                                   \
   }


#define SVAL(len) (_SI>>(13-len))

int nuke1 (WORD ves, WORD vds){
   WORD ptr_=ptr*2+49152U;
   WORD wInBufc=wInBuf;
   BYTE bInLeftc=bInLeft;

   int ret;
   _SI = wIn13;
   _DI = wTOE;
   WORD wCompc=wComp;
   WORD dst, len;
   WORD ods;
   // save DS
   asm cld
   asm push ds
   asm pop ods
   // set DS to data buffer
   asm push vds
   asm pop ds
   asm push ves;
   asm pop es;
   for (;;){
//    dst = LDdecodeVal[_SI];

      asm mov bx,si
      asm add bx,bx
      asm mov ax, word ptr es:bx
      asm mov dst, ax

//    SKIP (LDdecodeLen[_SI]);

      asm mov bx,si
      asm add bx,16384
      asm mov dl, byte ptr es:bx
      RSKIP();

      _AX=dst;
      if (_AX<256){
//       pbDatac[wTOEc++] = (BYTE)dst;

	 asm mov byte ptr [di], al
	 _DI++;

      } else {
	 if (_AX<271)
	    dst = _AX-255;
	 else if (_AX<286){
	    dst = (_AX-270)*16+SVAL(4);
	    FSKIP(4);
	 } else if (_AX<301){
	    dst = (_AX-285)*256+SVAL(8);
	    FSKIP(8);
	 } else {
	    dst = (_AX-300)*4096+SVAL(12);
	    FSKIP(12);
	    if (dst==EOB_MARK){ // end of block signal
	       ret=0;

//             SKIP (LdecodeLen[_SI]);

	       asm push ves
	       asm pop es
	       asm mov bx,si
	       asm add bx,40960
	       asm xor ax,ax
	       asm mov al, byte ptr es:bx
	       asm mov dx, ax
	       RSKIP()

	       goto leave;
	    }
	 }

//       len = LdecodeVal[_SI];

	 asm mov bx,si
	 asm add bx,bx
	 asm add bx, 24576
	 asm mov ax,word ptr es:bx
	 asm mov len,ax

//       SKIP (LdecodeLen[_SI]);

	 asm mov bx,si
	 asm add bx,40960
	 asm xor ax,ax
	 asm mov al, byte ptr es:bx
	 asm mov dx, ax
	 RSKIP()

	 asm mov cx, len
	 if (_CX<8){
	    asm add cx,3
	    asm push es;
	    asm push si;
	    _SI=_DI-dst;
	    asm push ds;
	    asm pop es;
	    asm rep movsb;
	    asm pop si;
	    asm pop es;
	    goto skip;
	 } else if ((_DX=_CX)<16){
	    _DX =  (_DX-8)*2+11+SVAL(1);
	    FSKIP (1);
	 } else if (_DX<24){
	    _DX = (_DX-16)*8+27+SVAL(3);
	    FSKIP (3);
	 } else if (_DX==24){
	    _DX = (91+SVAL(6));
	    FSKIP (6);
	 } else if (_DX==25){
	    _DX = (155+SVAL(9));
	    FSKIP (9);
	 } else if (_DX==26){
	    _DX = (667+SVAL(11));
	    FSKIP (11);
	 } else {
	    _DX = (2715+(SVAL(13)<<2));
	    FSKIP (13);
	    _DX += SVAL(2);
	    FSKIP (2);
	 }

	 asm push es;
	 asm push si;
	 _SI=_DI-dst;
	 asm push ds;
	 asm pop es;
	 asm mov cx, dx;
	 asm cld;
	 asm rep movsb;
	 asm pop si;
	 asm pop es;
	 goto skip;
      }
skip:
      if ((_DI&0x8000) == wCompc){
	 asm push ods
	 asm pop ds
	 if (FlushIt()){
	    ret=1;
	    goto leave;
	 }
	 wCompc=wComp;
	 asm push vds
	 asm pop ds
	 asm push ves;
	 asm pop es;
      }
   }
leave:
   // restore DS to original state
   asm push ods
   asm pop ds
   wTOE = _DI;
   wIn13 = _SI;
   ptr = (ptr_-49152U)/2;
   bInLeft=bInLeftc;
   wInBuf=wInBufc;
   return ret;
}


