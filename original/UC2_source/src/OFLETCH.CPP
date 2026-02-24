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
   return;
   while (len>16384){
      FletchUpdate (fr,dptr,16384);
      dptr+=16384;
      len-=16384;
   }
//   if (len<20U){ // qqq machine code disabled
      unsigned i;
      for (i=0;i<len;i++){
	 fr->sum1 = (fr->sum1 + dptr[i]) % 255;
	 fr->sum2 = (fr->sum2 + fr->sum1) % 255;
      }
/*
      return;
   }
   _SI = fr->sum1;
   _DI = fr->sum2;
   _CX = len;
   asm les bx, dword ptr dptr
   asm xor ax, ax
   asm push ds
   asm push es
   asm pop ds
   while (_CX>15){
      asm mov al, byte ptr [bx]
      _SI += _AX;
      _DI += _SI;
      asm mov al, byte ptr [bx+1]
      _SI += _AX;
      _DI += _SI;
      asm mov al, byte ptr [bx+2]
      _SI += _AX;
      _DI += _SI;
      asm mov al, byte ptr [bx+3]
      _SI += _AX;
      _DI += _SI;
      asm mov al, byte ptr [bx+4]
      _SI += _AX;
      _DI += _SI;
      asm mov al, byte ptr [bx+5]
      _SI += _AX;
      _DI += _SI;
      asm mov al, byte ptr [bx+6]
      _SI += _AX;
      _DI += _SI;
      asm mov al, byte ptr [bx+7]
      _SI += _AX;
      _DI += _SI;
      asm mov al, byte ptr [bx+8]
      _SI += _AX;
      _DI += _SI;
      asm mov al, byte ptr [bx+9]
      _SI += _AX;
      _DI += _SI;
      asm mov al, byte ptr [bx+10]
      _SI += _AX;
      _DI += _SI;
      asm mov al, byte ptr [bx+11]
      _SI += _AX;
      _DI += _SI;
      asm mov al, byte ptr [bx+12]
      _SI += _AX;
      _DI += _SI;
      asm mov al, byte ptr [bx+13]
      _SI += _AX;
      _DI += _SI;
      asm mov al, byte ptr [bx+14]
      _SI += _AX;
      _DI += _SI;
      asm push  bx
      asm mov	bx,255
      asm mov	ax,si
      asm xor	dx,dx
      asm div	bx
      asm mov	si,dx

      asm mov	bx,255
      asm mov	ax,di
      asm xor	dx,dx
      asm div	bx
      asm mov	di,dx
      asm pop   bx
      asm xor   ax, ax
      _BX+=15;
      _CX-=15;
   }
   while (_CX){
      asm mov al, byte ptr [bx]
      _SI += _AX;
      _DI += _SI;
      _BX++;
      _CX--;
   }
   asm pop ds
   _SI%=255;
   _DI%=255;
   fr->sum1 = _SI;
   fr->sum2 = _DI;
*/
}

WORD Fletcher (FREC *fr){
   WORD check1, check2;
   check1 = 255 - ((fr->sum1 + fr->sum2) % 255);
   check2 = 255 - ((fr->sum1 + check1) % 255);
   return (check1<<8) + check2;
}

/*

void FletchInit (struct FREC *fr){
   fr->sum1 = 0;
   fr->sum2 = 0;
}

void FletchUpdate (struct FREC *fr, BYTE far *dptr, int len){
   fr->sum1+=len;
}

WORD Fletcher (FREC *fr){
   return fr->sum1;
}

*/