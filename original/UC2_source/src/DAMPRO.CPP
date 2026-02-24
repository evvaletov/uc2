// dampro.cpp

#include <stdlib.h>
#include <mem.h>
#include "main.h"
#include "mem.h"
#include "llio.h"
#include "video.h"
#include "diverse.h"
#include "vmem.h"
#include "fletch.h"
#include "archio.h"
#include "superman.h"

#define BLKS 128

struct tab{
   WORD ent[BLKS];
   VPTR next;
};

static VPTR root;
static VPTR current;
static int curdx;

#pragma argsused
void InitChk (void){
#ifndef UE2
   root = Vmalloc (sizeof (tab));
   ((tab*)V(root))->next=VNULL;
   current=root;
   curdx=0;
#endif
}

#pragma argsused
void SetChk (DWORD ndx, WORD val){
#ifndef UE2
   WORD ofs=(WORD)(ndx%BLKS);
   DWORD seg=(ndx-ofs)/BLKS;
   if (seg!=curdx){
      if (seg<curdx){
	 curdx=0;
	 current=root;
      }
      while (seg!=curdx){
	 if (IS_VNULL(((tab*)V(current))->next)){
	    VPTR tmp = ((tab*)V(current))->next=Vmalloc (sizeof (tab));
	    current = tmp;
	    ((tab*)V(current))->next=VNULL;
	    curdx++;
	 } else {
	    VPTR tmp = ((tab*)V(current))->next;
	    current = tmp;
	    curdx++;
	 }
      }
   }
   ((tab*)V(current))->ent[ofs] = val;
#endif
}

#pragma argsused
WORD GetChk (DWORD ndx){
#ifndef UE2
   WORD ofs=(WORD)(ndx%BLKS);
   DWORD seg=(ndx-ofs)/BLKS;
   if (seg!=curdx){
      if (seg<curdx){
	 curdx=0;
	 current=root;
      }
      while (seg!=curdx){
	 if (IS_VNULL(((tab*)V(current))->next)){
	    VPTR tmp = ((tab*)V(current))->next=Vmalloc (sizeof (tab));
	    current = tmp;
	    ((tab*)V(current))->next=VNULL;
	    curdx++;
	 } else {
	    VPTR tmp = ((tab*)V(current))->next;
	    current = tmp;
	    curdx++;
	 }
      }
   }
   return ((tab*)V(current))->ent[ofs];
#else
   return 0;
#endif
}

#pragma argsused
void ExitChk (void){
#ifndef UE2
   while (!IS_VNULL(root)){
      VPTR tmp = ((tab*)V(root))->next;
      Vfree (root);
      root = tmp;
   }
#endif
}

WORD GetCheck (BYTE *data){
   FREC q;
   FletchInit (&q);
   FletchUpdate (&q, data, 512);
   return Fletcher (&q);
}

void XorBlock (BYTE *dst, BYTE *b);

WORD Calc (DWORD dwSecs){
   // determine number of extra sectors
   if (dwSecs<200){
      return 1;
   } else if (dwSecs<400){
      return 2;
   } else if (dwSecs<800){
      return 4;
   } else if (dwSecs<1600){
      return 8;
   } else {
      return 16;
   }
}


#pragma argsused
void DamPro (int iHandle){
#ifndef UE2
   InitChk();
   WORD wDrs;

   // claim memory
   BYTE *pbIO = xmalloc (512U, STMP);
   BYTE *pbBuf[16];
   for (DWORD i=0;i<16;i++){
      pbBuf[(WORD)i] = xmalloc (512, STMP);
   }

   // get number of sectors in dwSecs, make size multitude of 512
   DWORD dwLen = GetFileSize (iHandle);
   Seek (iHandle, dwLen+1);
//   IOut ("[dwLen=%ld]\n\r",dwLen);
   Out (3,"\x7\Damage protecting archive ");
   StartProgress ((int)(dwLen/512L), 3);
   if (dwLen!=0){
      DWORD dwSecs = dwLen/512L;
      DWORD dwExtra = 512-(dwLen-dwSecs*512L);
      if (dwExtra){
	 memset (pbIO, 0, 512);
	 Write (pbIO, iHandle, (WORD) dwExtra);
//       IOut ("[dwExtra=%ld]",dwExtra);
	 dwSecs++;
      }

      // determine number of extra sectors
      wDrs = Calc (dwSecs);

      for (i=0;i<wDrs;i++){
	 memset (pbBuf[(WORD)i],0,512);
      }

      BYTE *tbuf=xmalloc (512, STMP);
      WORD check;

      Seek (iHandle,0);
      CacheRT (iHandle);
      for (i=0;i<dwSecs;i++){
         Hint();
	 Read (tbuf, iHandle, 512); // read sector
	 XorBlock (pbBuf[(WORD)(i%wDrs)], tbuf); // xor into dampro record
	 check = GetCheck (tbuf);   // determine checksum
//       IOut ("%ld %04X\n\r",i,check);
	 SetChk (i, check);
      }
      CacheW (iHandle);
      for (i=0;i<wDrs;i++){
	 Hint();
	 Write (pbBuf[(WORD)i], iHandle, 512);
      }
      i = dwSecs;
      VPTR tmp=root;

      FREC tt;
      FletchInit (&tt);

      // dump dampro stuff
      while (i){
	 WORD j = BLKS;
	 if (j>i) j=(WORD)i;
	 Write ((BYTE*)(((tab*)V(tmp))->ent), iHandle, j*2);
	 FletchUpdate (&tt, (BYTE*)(((tab*)V(tmp))->ent), j*2);
	 tmp = ((tab*)V(tmp))->next;
	 i-=j;
      }
      WORD w = Fletcher (&tt);
      Write ((BYTE *)&w, iHandle, 2);

      xfree (tbuf, STMP);
   }

   // release used memory
   xfree (pbIO, STMP);
   for (i=0;i<16;i++)
      xfree (pbBuf[(WORD)i], STMP);
   Out (3,"\n\r");
   ExitChk();
#endif
}

extern DWORD dwPLen[MAX_AREA]; // archio.h
extern int iArchInHandle[MAX_AREA];

#pragma argsused
int VerifyDP (void){
#ifndef UE2
   InitChk();
   int ret=1;
   WORD wDrs;
   BYTE *pbBuf[16];
   DWORD dwLen = dwPLen[iArchArea];
   DWORD dwSecs = dwLen/512L;
   DWORD dwExtra = 512-(dwLen-dwSecs*512L);
   if (dwExtra){
      dwSecs++;
   }

   // determine number of extra sectors
   wDrs = Calc (dwSecs);

   for (DWORD i=0;i<wDrs;i++){
      pbBuf[(WORD)i]=xmalloc(512, STMP);
      memset (pbBuf[(WORD)i],0,512);
   }

   int iHandle = iArchInHandle[iArchArea];
   CacheRT (iHandle);
   WORD check;

   Seek (iHandle, 512*(dwSecs+wDrs));
   i = dwSecs;
   GetChk (i); // ensure enough memory
   VPTR tmp=root;

   // read dampro stuff
   FREC tt;
   FletchInit (&tt);

   while (i){
      WORD j = BLKS;
      if (j>i) j=(WORD)i;
      Read ((BYTE*)(((tab*)V(tmp))->ent), iHandle, j*2);
      FletchUpdate (&tt, (BYTE*)(((tab*)V(tmp))->ent), j*2);
      tmp = ((tab*)V(tmp))->next;
      i-=j;
   }

   WORD w;
   Read ((BYTE *)&w, iHandle, 2);
   if (w!=Fletcher (&tt)){
      Error (90,"fatal damage in damage protection records, unable to verify data");
      ret=0;
      goto leave;
   }
   {

      BYTE *tbuf=xmalloc (512, STMP);
      Seek (iHandle,0);

      Out (3,"\x7Testing archive sectors ");
      StartProgress (-1, 3);

      for (i=0;i<dwSecs;i++){
         Hint();
	 Read (tbuf, iHandle, 512); // read sector
	 XorBlock (pbBuf[(WORD)(i%wDrs)], tbuf); // xor into dampro record
	 check = GetCheck (tbuf);   // determine checksum
	 WORD cmp;
	 cmp = GetChk (i);
	 if (cmp!=check){
	    Doing ("testing archive sectors");
	    Error (90,"sector %ld is damaged",i+1);
	    Out (3,"\x7\Continueing test ");
            StartProgress (-1, 3);
	    ret=0;
	 }
      }
      if (ret) Out (3,"  \x5\OK");
      Out (3,"\n\r\x7Testing protection records ");
      StartProgress (-1, 3);
      for (i=0;i<wDrs;i++){
	 Hint();
	 Read (tbuf, iHandle, 512);
	 if (!memcmp(tbuf, pbBuf[(WORD)i],512)==0){
	    if (ret==1){
		Doing ("testing protection records");
		Error (90,"protection record %d is damaged (but all data is 100% OK)",i+1);
		Out (3,"\x7\Continueing test ");
                StartProgress (-1, 3);
		ret=0;
	    }
	 }
      }
      if (ret) Out (3,"  \x5\OK");
      Out (3,"\n\r");
      xfree (tbuf, STMP);
   }
leave:
   ExitChk();
   for (i=0;i<wDrs;i++){
      xfree (pbBuf[(WORD)i], STMP);
   }
   return ret;
#else
   return 0;
#endif
}

static int bc=0;
void BC (void){
   Hint();
}

#pragma argsused
int RepairDP (char *dest){
#ifndef UE2
   InitChk();
   int ret=1;
   WORD wDrs;
   DWORD dwLen = dwPLen[iArchArea];
   DWORD dwSecs = dwLen/512L;
   DWORD dwExtra = 512-(dwLen-dwSecs*512L);
   if (dwExtra){
      dwSecs++;
   }

   wDrs = Calc (dwSecs);

   int iHandle = iArchInHandle[iArchArea];
   int oHandle = Open (dest, CR|MUST|CWR);
   WORD check;

   Seek (iHandle, 512*(dwSecs+wDrs));
   DWORD i = dwSecs;
   GetChk (i); // ensure enough memory
   VPTR tmp=root;

   // read dampro stuff
   FREC tt;
   FletchInit (&tt);

   while (i){
      WORD j = BLKS;
      if (j>i) j=(WORD)i;
      Read ((BYTE*)(((tab*)V(tmp))->ent), iHandle, j*2);
      FletchUpdate (&tt, (BYTE*)(((tab*)V(tmp))->ent), j*2);
      tmp = ((tab*)V(tmp))->next;
      i-=j;
   }

   WORD w;
   BYTE spec=0;
   Read ((BYTE *)&w, iHandle, 2);
   if (w!=Fletcher (&tt)){
      Error (90,"fatal damage in damage protection records, unable to verify or repair data");
      ret=0;
      spec=1;
   }
   BYTE *tbuf=xmalloc (512, STMP);
   BYTE *cbuf=xmalloc (512, STMP);
   Seek (iHandle,0);

   int cop=0;
   bc=0;
   for (i=0;i<dwSecs;i++){
      Read (tbuf, iHandle, 512); // read sector
      check = GetCheck (tbuf);   // determine checksum
      WORD cmp = GetChk (i);
      if ((cmp==check) || spec){
	 Write (tbuf, oHandle, 512);
	 if (cop==0){
	    Out (3,"\x7\Copying archive sectors ");
	    cop=1;
	 }
	 BC();
      } else {
	 bc=0;
	 Out (7,"\n\r\x7\Reconstructing sector %ld ",i+1);
	 memset (tbuf, 0, 512);
	 WORD  x= (WORD)(i%wDrs);
	 DWORD j= x;
	 while (j<dwSecs){
	    if (j!=i){
	       BC();
	       Seek (iHandle, 512*j);
	       Read (cbuf, iHandle, 512);
	       XorBlock (tbuf, cbuf);
	    }
	    j+=wDrs;
	 }
	 BC();
	 Seek (iHandle, (dwSecs+x)*512);
	 Read (cbuf, iHandle, 512);
	 XorBlock (tbuf, cbuf);
	 if (cmp==GetCheck (tbuf)){
	    Write (tbuf, oHandle, 512);
	    Out (7,"\x5 OK");
	 } else {
	    Seek (iHandle, i*512);
	    Read (tbuf, iHandle, 512);
	    Write (tbuf, oHandle, 512);
	    Doing ("reconstructing sector %ld",i+1);
	    Error (90,"failed");
	    ret=0;
	 }
	 Seek (iHandle,(i+1)*512);
	 cop=0;
	 bc=0;
	 Out (7,"\n\r");
      }
   }
   if (cop) Out (3,"\n\r");
   xfree (tbuf, STMP);
   ExitChk();
   SetFileSize (oHandle, dwLen);
   DamPro (oHandle);
   FHEAD q;

   Seek (oHandle, 0);
   Read ((BYTE *)&q, oHandle, sizeof(q));
   Seek (oHandle, GetFileSize (oHandle));
   Write ((BYTE *)&q, oHandle, sizeof(q));

   Close (oHandle);
   return ret;
#else
   return 0;
#endif
}

