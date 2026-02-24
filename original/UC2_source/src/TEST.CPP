// Copyright 1992, all rights reserved, AIP, Nico de Vries
// TEST.CPP

#include <stdio.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include "main.h"
#include "video.h"
#include "vmem.h"
#include "llio.h"
#include "compint.h"
#include "archio.h"
#include "superman.h"
#include "list.h"
#include "diverse.h"
#include "neuroman.h"


#ifdef UCPROX

long aa=0,bb=0,cc=0,dd=0;

/*** serial number generation ***/

extern int Validate ();

void ListVal (void){
   DWORD i, x, y;
   if (CONFIG.dSerial!=148706026L){
      FatalError (255,"option not allowed");
   }
   for (x=0;x<225997L;x++){
      for (y=441;y<4423;y++){
         i = y*225997L + x;
         CONFIG.dSerial=i;
         if (Validate()){
	    Out (7,"%lu ",i);
	    switch (Validate()){
	       case 1:
		  Out (7,"UC 2\n\r");
		  break;
	       case 2:
		  Out (7,"UC PRO\n\r");
		  break;
	       case 3:
		  Out (7,"UC PRO X\n\r");
                  break;
            }
         }
	 if (kbhit()) doexit (EXIT_SUCCESS);
      }
   }
}


/*** VMEM.CPP ***/

struct PRE {
   WORD wSize;  // size of memory block
   VPTR vpNext; // next block in memory chain
};

extern VPTR pvpFirst[257];

#define BLOCK_SIZE 2054   // tricky! must be excactly same as one in VMEM
#define TOT_BLOCKS 64512U // tricky! VMEM

BYTE GetStat (WORD wIndex);

extern unsigned ram_blocks;

WORD VdiskUsage (void){
   for (unsigned i=0;i<60000U;i++){
      if (GetStat(i)==255) break;
   }
   if (i>ram_blocks)
      return i-ram_blocks;
   return 0;
}

DWORD VmemLeft (void){
   DWORD res=0;
   VPTR walk=pvpFirst[256];
   PRE *pre = (PRE *)Acc(walk);
   while (walk.wBlock!=VNULL.wBlock){
      if ((*pre).wSize==BLOCK_SIZE-6){ // save lots of time
         res+=(DWORD)(TOT_BLOCKS-walk.wBlock)*(BLOCK_SIZE-6);
         break;
      }
      res+=(*pre).wSize;
      VPTR tmp = walk;
      walk = pre->vpNext;
      UnAcc (tmp);
      pre = (PRE *)Acc(walk);
   }
   UnAcc(walk);
   return res;
}


#define LIM 512
#define SIZ 100

void TestVmem (int boost){
   long claimed=0;
   BYTE val[LIM];
   BYTE siz[LIM];
   VPTR vp[LIM];
   BYTE *bp;
   int i,j;
   long x;
   printf ("Test started ('ERROR' on problems)\n");
   for (i=0;i<LIM;i++) val[i]=0;
   for (x=0;x<10000;x++){
      if (x%400 == 0){
         printf ("%3ld %lu ",24-x/400,VmemLeft());
         if (boost){
            BoosterOff();
            BoosterOn();
         }
         j=0;
         for (i=0;i<LIM;i++) if (val[i]) j++;
         printf ("%d (%d)\n",j,LIM);
      }
      j = random (LIM);
      if (val[j]){
         BYTE nw = random(235)+10;
         bp = (BYTE *)V(vp[j]);
         for (i=0;i<siz[j];i++){
            if (bp[i]!=val[j]) printf ("ERROR ");
            bp[i] = nw;
         }
         val[j]=nw;
         if (random(5)==1){
            claimed--;
            Vfree(vp[j]);
            val[j]=0;
         }
      } else {
         if (random(2)==1){
            siz[j] = random(SIZ-4)+5;
            claimed++;
            vp[j] = Vmalloc (siz[j]);
            val[j] = random(235)+10;
	    bp = (BYTE *)V(vp[j]);
            for (i=0;i<siz[j];i++)
               bp[i] = val[j];
         }
      }
   }
   for (i=0;i<512;i++){
      if (val[i]){
         Vfree(vp[i]);
         claimed--;
      }
   }
   printf ("Test completed (error should be %ld)\n",claimed);
}

/*** compressor tester ***/

int h_in, h_out;

WORD far pascal rd (BYTE far *pbBuffer, WORD wSize){
   return Read (pbBuffer, h_in, wSize);
}

void far pascal wt (BYTE far *pbBuffer, WORD wSize){
   Write (pbBuffer, h_out, wSize);
}

void TestComp (int m, char *in, char *out){
   h_in = Open (in, RO|MUST|NOC);
   h_out = Open (out, CR|MUST|NOC);
   Write ((BYTE *)&m, h_out, sizeof(int));
   if (m>10 && m<20)
      Compressor (m-10, rd, wt, NOMASTER);
   else
      Compressor (m, rd, wt, SUPERMASTER);
   Close (h_in);
   Close (h_out);
}

void TestDComp (char *in, char *out){
   int m;
   h_in = Open (in, RO|MUST|NOC);
   h_out = Open (out, CR|MUST|NOC);
   Read ((BYTE *)&m, h_in, sizeof(int));
   Out (7,"Method index = %d\n\r",m);
   if (m>10 && m<20)
      Decompressor (m-10, rd, wt, NOMASTER,1000000L);
   else
      Decompressor (m, rd, wt, SUPERMASTER,1000000L);
   Close (h_in);
   Close (h_out);
}

/*** test pipelining ***/

void TestPipe (char *in, char *out){
   PIPE p;
   MakePipe (p);
   BYTE *buf = (BYTE *)xmalloc (10000, NTMP);
   FILE *i = fopen (in, "rb");
   FILE *o = fopen (out, "wb");
further:
   int len = fread (buf, 1, random(4)*random(2000)+1, i);
   if (len==0) goto next;
   WritePipe (p, buf, len);
   goto further;
next:
   len = ReadPipe (p, buf, random(4)*random(500)+1);
   if (len==0) goto end;
   fwrite (buf, 1, len, o);
   goto next;
end:
   fclose (i);
   fclose (o);
   xfree (buf, NTMP);
   ClosePipe (p);
}

/*** Tester user interface ***/

extern long claimed;

void Tester (int argc, char **argv){
   Out (7,"\n\r**** UC-BETA X testing software ****\n\r");
   Out (7,"(C) 1993 AIP-NL\n\r\n\r");
   if (argc==2){
      Out (7,"List of available test options (uppercase only!):\n\r");
      Out (7,"   VMEM                test virtual memory manager VBOOSTER OFF\n\r");
      Out (7,"   BMEM                test virtual memory manager VBOOSTER ON\n\r");
      Out (7,"   BBMEM               BMEM torture mode\n\r");
      Out (7,"   COMP no in out      low level compress (network aware file I/O)\n\r");
      Out (7,"                       T1=2 T2=3 T3=4 TG1=12 TG2=13 TG3=14\n\r");
      Out (7,"   DCMP in out         low level decompress (naf IO)\n\r");
      Out (7,"   PIPE in out         test virtual pipelining (does file copy)\n\r");
   } else {
      if (strcmp(argv[2],"BBMEM")==0){
	 BoosterOn();
	 TestVmem(1);
	 BoosterOff();
      } else if (strcmp(argv[2],"BMEM")==0){
	 BoosterOn();
	 TestVmem(0);
	 BoosterOff();
      } else if (strcmp(argv[2],"VMEM")==0){
	 TestVmem(0);
      } else if (strcmp(argv[2],"LIST")==0){
	 SetArea (0);
	 ReadArchive (argv[3]);
	 ListArch (VNULL);
	 CloseArchive();
      } else if (strcmp(argv[2],"PIPE")==0){
	 printf ("MEM:%lu\n",VmemLeft());
	 TestPipe (argv[3], argv[4]);
	 printf ("MEM:%lu\n",VmemLeft());
	 TestPipe (argv[3], argv[4]);
	 printf ("MEM:%lu\n",VmemLeft());
	 printf ("BOOSTER ON\n");
	 BoosterOn();
	 TestPipe (argv[3], argv[4]);
	 printf ("MEM:%lu\n",VmemLeft());
	 TestPipe (argv[3], argv[4]);
	 printf ("MEM:%lu\n",VmemLeft());
	 printf ("BOOSTER OFF\n");
	 BoosterOff();
	 TestPipe (argv[3], argv[4]);
	 printf ("MEM:%lu\n",VmemLeft());
      } else if (strcmp(argv[2],"LISS")==0){
	 ListVal();
      } else if (strcmp(argv[2],"DCMP")==0){
	 Out (7,"DCMP (%s,%s)\n\r",argv[3],argv[4]);
	 TestDComp (argv[3], argv[4]);
      } else if (strcmp(argv[2],"COMP")==0){
	 Out (7,"COMP (%d,%s,%s)\n\r",atoi(argv[3]),argv[4],argv[5]);
	 TestComp (atoi(argv[3]),argv[4],argv[5]);
      } else {
	 FatalError (255,"Unknown TESTER command");
      }
      if (claimed){
	 Out (7,"\n\r[[Memory consistency check failed : %ld]]\n\r",claimed);
      } else {
	 Out (7,"\n\r[[Memory consitency check completed NO ERRORS]]\n\r");
      }
      Out (7,"[[aa=%ld bb=%ld cc=%ld dd=%ld]]\n",aa,bb,cc,dd);
   }
}

#endif
