#include <dos.h>
#include "main.h"

void XorBlock (BYTE *dst, BYTE *b){
   WORD *p1, *p2;
   p1 = (WORD *)dst;
   p2 = (WORD *)b;

/*
   for (int i=0;i<256;i++){
      p1[i] ^= p2[i];
   }
*/
   asm push DS

   _DS = FP_SEG (p1);
   _BX = FP_OFF (p1);

   _ES = FP_SEG (p2);
   _SI = FP_OFF (p2);


   for (_CX=0; _CX<256; _CX++){

      asm mov AX, [BX]
      asm xor AX, ES:[SI]
      asm mov [BX], AX

      _BX+=2;
      _SI+=2;
   }

   asm pop DS
}

