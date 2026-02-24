/**********************************************************************/
/*$ INCLUDES */

#include <process.h>
#include <stdlib.h>
#include <string.h>
#include <dir.h>
#include <dos.h>
#include <alloc.h>
#include <conio.h>
#include <stdio.h>
#include <io.h>
#include <share.h>
#include "main.h"
#include "video.h"
#include "vmem.h"
#include "superman.h"
#include "llio.h"
#include "archio.h"
#include "views.h"
#include "diverse.h"
#include "test.h"
#include "mem.h"
#include "compint.h"
#include "dirman.h"
#include "menu.h"
#include "comoterp.h"
#include <ctype.h>

static long found=0;
static int offset=0;

void StartSearch (void){
   found=0;
   offset=0;
}

void Investigate (BYTE *data, WORD len){
   for (int i=0;i<len;i++){
      if (toupper(data[i])==MODE.szContains[offset]){
	 int j=offset;
	 int k=i;
	 while (MODE.szContains[j+1]!=0){
	    k++;
	    j++;
	    if (k==len){
	       offset=j;
	       return;
	    }
	    if (toupper(data[k])!=MODE.szContains[j]) goto next;
	 }
	 found++;
      } else {
	 offset=0;
      }
next:
   }
}

/*
void Investigate (BYTE *data, WORD len){
   while (len){
      int trn=3;
      if (trn>len) trn=len;
      IInvestigate (data, trn);
      data+=trn;
      len-=trn;
   }
}
*/

long Found (void){
   return found;
}

int SearchFile (char *szFileName){
   int iHan = Open (szFileName, TRY|RO|CRT);
   if (iHan==-1) return 0;
   StartSearch();
   BYTE buf[512];
   DWORD todo=GetFileSize (iHan);
   while (todo){
      int trn=512;
      if (trn>todo) trn=(int)todo;
      Read (buf, iHan, trn);
      Investigate (buf, trn);
      todo-=trn;
   }
   Close (iHan);
   return Found()!=0;
}