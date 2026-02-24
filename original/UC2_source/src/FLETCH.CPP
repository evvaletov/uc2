#pragma inline
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <io.h>
#include "main.h"
#include "fletch.h"

void FletchInit (struct FREC *fr){
   fr->sum1 = 0;
   fr->sum2 = 0;
}

void FletchUpdate (struct FREC *fr, BYTE far *dptr, unsigned len){
   if (!len) return;
   // process pending halfword
   if (fr->sum1){
      fr->sum2^=*dptr<<8;
      len--;
      dptr++;
      fr->sum1=0;
   }
   // force len to become even
   if (len%2==1){
      fr->sum2^=(dptr[len-1]); // process extra byte
      len--;
      fr->sum1=1; // mark upkoming byte for halfword processing
   }
   // force len to become multiple of 4
   if (len%4==2){
      fr->sum2^=*((WORD*)dptr);
      dptr+=2;
      len-=2;
   }
   if (len && !m386){
      _DI=fr->sum2; // current parity
      _CX=len/2;    // word counter
      asm push ds
      asm lds bx,dptr
again:
      if (_CX>=16){
	 asm xor di,word ptr [bx]
	 asm xor di,word ptr [bx+2]
	 asm xor di,word ptr [bx+4]
	 asm xor di,word ptr [bx+6]
	 asm xor di,word ptr [bx+8]
	 asm xor di,word ptr [bx+10]
	 asm xor di,word ptr [bx+12]
	 asm xor di,word ptr [bx+14]
	 asm xor di,word ptr [bx+16]
	 asm xor di,word ptr [bx+18]
	 asm xor di,word ptr [bx+20]
	 asm xor di,word ptr [bx+22]
	 asm xor di,word ptr [bx+24]
	 asm xor di,word ptr [bx+26]
	 asm xor di,word ptr [bx+28]
	 asm xor di,word ptr [bx+30]
	 _CX-=16;
	 _BX+=32;
	 goto again;
      }
      while (_CX){
	 asm xor di,word ptr [bx]
	 _BX+=2;
	 _CX--;
      }
      asm pop ds
      fr->sum2=_DI;
   }
   if (len && m386){
      asm .386
      asm xor edi,edi
      _DI=fr->sum2; // current parity
      _CX=len/4;    // dword counter
      asm push ds
      asm lds bx,dptr
again2:
      if (_CX>=16){
	 asm xor edi,dword ptr [bx]
	 asm xor edi,dword ptr [bx+4]
	 asm xor edi,dword ptr [bx+8]
	 asm xor edi,dword ptr [bx+12]
	 asm xor edi,dword ptr [bx+16]
	 asm xor edi,dword ptr [bx+20]
	 asm xor edi,dword ptr [bx+24]
	 asm xor edi,dword ptr [bx+28]
	 asm xor edi,dword ptr [bx+32]
	 asm xor edi,dword ptr [bx+36]
	 asm xor edi,dword ptr [bx+40]
	 asm xor edi,dword ptr [bx+44]
	 asm xor edi,dword ptr [bx+48]
	 asm xor edi,dword ptr [bx+52]
	 asm xor edi,dword ptr [bx+56]
	 asm xor edi,dword ptr [bx+60]
	 _CX-=16;
	 _BX+=64;
	 goto again2;
      }
      while (_CX){
	 asm xor edi,dword ptr [bx]
	 _BX+=4;
	 _CX--;
      }
      asm pop ds
      asm push edi
      asm pop di
      asm pop ax
      asm xor di,ax
      fr->sum2=_DI;
      asm .8086
   }
}

WORD Fletcher (FREC *fr){
   return fr->sum2^0xA55A;
}

