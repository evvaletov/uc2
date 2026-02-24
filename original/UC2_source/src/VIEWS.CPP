// Copyright 1992, all rights reserved, AIP, Nico de Vries
// VIEWS.CPP

#include <alloc.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <conio.h>
#include <dos.h>
#include "main.h"
#include "video.h"
#include "views.h"
#include "diverse.h"

static int master;
static int jump=0;

static char **pppcLines[11];
static int status[11];

static int aiLines[11], aiSum[11];
static int aofs[11];
#define MAXLINES 1000

#define ppcLines pppcLines[master+1]
#define iLines aiLines[master+1]
#define iSum aiSum[master+1]
#define ofs aofs[master+1]

char *sspec;

int siz=25;


void ask (char *q, char *a);

int detectegavga (void){
#ifndef UE2
   unsigned char tmp;
   union REGS r;
   r.h.ah = 18;
   r.x.bx = 0x1010;
   int86 (0x10, &r, &r);
   if (r.h.bh <= 1){
      r.x.ax = 0x1a00;
      int86 (0x10, &r, &r);
      if (r.h.al==0x1a){
         if (0==strncmp((char *)MK_FP(0xC000,0x31),"761295520",9)){ // ATI ?
            if (0==strncmp ((char *)MK_FP(0xC000,0x40),"22",2)){
               return 0; // lying ATI EGA Wonder (!)
            }
         }
         if ((r.h.bl<4) && (r.h.bh>3)){
            tmp = r.h.bl; r.h.bl = r.h.bh; r.h.bh = tmp;
         }
         switch (r.h.bl){
            case 2:
	    case 4:
            case 6:
            case 10:
               return 1; // VGA but TTL monitor
            case 1:
            case 5:
            case 7:
            case 11:
               return 1; // VGA but Mono monitor
            case 8:
            case 12:
               return 1; // VGA and Analog monitor
         }
      }
      return 2; // EGA
   }
#endif
   return 0; // no EGA/VGA
}

BYTE *xxmalloc (unsigned size){
    static BYTE *ret, *buf;
    static int suply=0;
    if ((size>1000) && (size>suply)) return xmalloc (size, STMP);
    if (size>suply){
        buf=xmalloc(5000, STMP);
        suply=5000;
    }
    ret=buf;
    buf+=size;
    suply-=size;
    return ret;
}

char *pcVName;

#pragma argsused
void ReadFile (){
   char *pcFileName;
   if (!sspec)
   {
      switch (master){
         case -1:
            pcFileName = sspec;
            break;
         case 0:
       	    pcFileName = "WHATSNEW.DOC";
   	    break;
         case 1:
   	    pcFileName = "README.DOC";
   	    break;
         case 2:
   	    pcFileName = "LICENSE.DOC";
   	    break;
         case 3:
	    pcFileName = "BASIC.DOC";
   	    break;
         case 4:
   	    pcFileName = "MAIN.DOC";
   	    break;
         case 5:
   	    pcFileName = "BBS.DOC";
   	    break;
         case 6:
   	    pcFileName = "CONFIG.DOC";
   	    break;
         case 7:
   	    pcFileName = "BACKGRND.DOC";
   	    break;
         case 8:
   	    pcFileName = "EXTEND.DOC";
   	    break;
      }
   } else {
      pcFileName = sspec;
   }
   pcVName=pcFileName;
   if (status[master+1]) return;
   status[master+1]=1;
   char path[260];
   if (!LocateF(pcFileName,0)){
      FatalError (115,"cannot locate file %s (for viewing)",pcFileName);
   }
   strcpy (path, LocateF (pcFileName,0));
   pcFileName = path;

#ifndef UE2
   iLines=0;
   ppcLines = (char **)xxmalloc (sizeof(char *)*MAXLINES);
   FILE *pf = fopen (pcFileName,"r");
   setvbuf (pf, NULL, _IOFBF, 10000);
   if (!pf){
      FatalError (115,"cannot open file %s (for viewing)",pcFileName);
   }
   while (!feof(pf)){
      static char lino[90];
      fgets(lino,85,pf);
      // filter formfeeds
      for (int i=0;i<80;i++)
	 if (lino[i]==12)
	    for (int j=i;j<85;j++)
	       lino[j]=lino[j+1];
      if (lino[strlen(lino)-1]=='\n')
	 lino[strlen(lino)-1]='\0';
//      for (i=strlen(lino);i<80;i++)
//         lino[i]=' ';
//      lino[80]='\0';
      ppcLines[iLines] = (char *)xxmalloc (strlen(lino)+1);
      strcpy (ppcLines[iLines], lino);
      if (!feof(pf)){
	 iLines++;
      }
   }
   fclose (pf);
   if (iLines-iSum>siz-1) iSum=0;
#endif
}

void ClearFile (void){
   for (int i=0;i<iLines+1;i++){
      xfree ((BYTE *)ppcLines[i], STMP);
   }
   xfree ((BYTE *)ppcLines, STMP);
}

static void Up (int no){
   while (no--){
      if (ofs>0) ofs--;
   }
}

static void Down (int no){
    while (no--){
       if (iLines>siz-3 && ofs<iLines-(siz-3)) ofs++;
    }
}

extern int attri;
extern int qdk; // DesqView active?

void UnGetKey (char c);

static int flg = 0;
static int cm;
static int opt;
static int mod=0;

extern int searcher;

char szSearcher[80]="";

void writeline (char *line){
   char lino[200];
   char lin[200];
   char ln[100];
   char src[100];
   char *s;
   int head=0;
   int len2=strlen(szSearcher);
   strcpy (src, szSearcher);
   strupr (src);
   strcpy (lino, line);
   strcat (lino,"                                        ");
   strcat (lino,"                                        ");
   lino[80]=0;
   if (isdigit(lino[0]) && (lino[1]=='.') && (isupper(lino[2]) || lino[2]==' '))
      head=1;
   if ((lino[0]=='=')&&(lino[1]=='=')&&(lino[2]=='='))
      head=1;
   if (CONFIG.bVideo==1 || CONFIG.bVideo==3){
      if (head){
	  textcolor (WHITE);
	  textbackground (BLUE);
      } else {
	  textcolor (LIGHTCYAN);
	  textbackground (BLUE);
      }
   } else {
      textcolor (LIGHTGRAY);
   }
   if (!szSearcher[0]){
      cputs (lino);
   } else {
      strcpy (lin, line);
      strcat (lin,"                                        ");
      strcat (lin,"                                        ");
      lin[80]=0;
      strupr (lino);
again:
      s=strstr (lino, src);
      if (s) {
         int len1=(int)(s-lino);
         if (len1){
            strcpy (ln, lin);
            ln[len1]=0;
	    cputs(ln);
            strcpy (lino, lino+len1);
            strcpy (lin, lin+len1);
         }
         if (CONFIG.bVideo==1 || CONFIG.bVideo==3){
            textcolor (YELLOW);
            textbackground (BLACK);
         } else {
            textcolor (WHITE);
         }
         strcpy (ln, lin);
         ln[len2]=0;
         cputs (ln);
         strcpy (lino, lino+len2);
         strcpy (lin, lin+len2);
         if (CONFIG.bVideo==1 || CONFIG.bVideo==3){
            if (head){
                textcolor (WHITE);
                textbackground (BLUE);
            } else {
                textcolor (LIGHTCYAN);
                textbackground (BLUE);
            }
         } else {
            textcolor (LIGHTGRAY);
         }
         goto again;
      } else {
         cputs (lin);
      }
   }
}

extern int noclear;

int Search (void){
   int swaps=10;
   char src[100];
   char lino[100];
   strcpy (src, szSearcher);
   strupr (src);
   ofs+=siz-3;
   while (swaps){
      if (ofs>=iLines){
	 if (++master==9) master=0;
	 ReadFile();
	 ofs=0;
	 swaps--;
      }
      strcpy (lino, ppcLines[ofs]);
      strupr (lino);
      if (strstr(lino,src)){
	 Up (5);
         if (ofs>0) for (int i=0; i<60; i++){
            Up(1);
            Down(1);
         }
	 return 1;
      }
      ofs++;
   }
   return 0;
}

int goofs;

#pragma argsused
void ViewIt (){
   char *pcHead=pcVName;
   int failed=0;
#ifndef UE2
   int pofs=9999;
   ofs=goofs;
   goofs=0;
redraw:
   pofs=9999;

   textbackground (LIGHTGRAY);
   textcolor (BLACK);
   gotoxy (1,1);
   cputs ("                                        ");
   cputs ("                                        ");
//   gotoxy (2,1);
//   cprintf ("UltraCompressor II manual viewer  %d. %s  ",master,pcHead);
   gotoxy (1,siz-1);
   cputs ("                                        ");
   cputs ("                                        ");
   gotoxy (1,siz-1);

   textcolor (WHITE);
   if (CONFIG.bVideo==1 || CONFIG.bVideo==3){
      textcolor (RED);
   }
   cputs (" Tab ");
   textcolor (BLACK);
   cputs ("summary & exit  ");
   textcolor (WHITE);
   if (CONFIG.bVideo==1 || CONFIG.bVideo==3){
      textcolor (RED);
   }
   cputs ("Esc ");
   textcolor (BLACK);
   if (sspec)
      cputs ("exit  ");
   else
      cputs ("menu  ");
   textcolor (WHITE);
   if (CONFIG.bVideo==1 || CONFIG.bVideo==3){
      textcolor (RED);
   }
   if (sspec)
      cputs ("A-Z ");
   else
      cputs ("0-8 A-Z ");
   textcolor (BLACK);
   cputs ("jump  ");
   if (opt){
      textcolor (WHITE);
      if (CONFIG.bVideo==1 || CONFIG.bVideo==3){
         textcolor (RED);
      }
      if (detectegavga())
         cprintf ("+ ");
      textcolor (BLACK);
      if (mod!=2){
         switch (detectegavga()){
            case 0:
               break;
            case 1:
               cprintf ("50 lines  ");
               break;
            case 2:
               cprintf ("43 lines  ");
               break;
         }
      } else
         cprintf ("25 lines  ");
   }
   textcolor (WHITE);
   if (CONFIG.bVideo==1 || CONFIG.bVideo==3){
      textcolor (RED);
   }
   cprintf ("%c %c PgUp PgDn  ",24,25);
   if (master!=-1 && searcher){
      cprintf ("S");
      textcolor (BLACK);
      cprintf ("earch");
   }

   for (;;){
      int i;
      if (pofs==ofs) goto again; // save rewrite
      textbackground (LIGHTGRAY);
      textcolor (BLACK);
      gotoxy (77,1);
      {
	long per = (int)((100L*(ofs+(siz-3)))/iLines);
        if (per>100) per=100;
        cprintf ("%3ld%%",per);
      }
      textbackground (BLACK);
      textcolor (LIGHTGRAY);
      if (CONFIG.bVideo==1 || CONFIG.bVideo==3){
         textbackground (BLUE);
         textcolor (LIGHTCYAN);
      }
      gotoxy (1,2);
      if (pofs+1==ofs){
         window (1,2,80,siz-2);
         gotoxy (1,1);
         delline ();
         window (1,1,80,siz);
         gotoxy (1,siz-2);
         if ((siz-4+ofs)<iLines)
            writeline (ppcLines[ofs+siz-4]);
	 else{
            cputs ("                                        ");
            cputs ("                                        ");
         }
      } else if (pofs-1==ofs){
         window (1,2,80,siz-2);
         gotoxy (1,siz-1);
         insline ();
         window (1,1,80,siz);
         gotoxy (1,2);
         if ((ofs)<iLines)
            writeline (ppcLines[ofs]);
         else{
            cputs ("                                        ");
            cputs ("                                        ");
         }
         gotoxy (1,siz-1);
      } else {
         for (i=0;i<siz-3;i++){
            gotoxy (1,2+i);
            if (i+ofs<iLines)
               writeline (ppcLines[ofs+i]);
            else{
               cputs ("                                        ");
               cputs ("                                        ");
            }
         }
         gotoxy (1,siz-1);
      }
      pofs = ofs;
      textbackground (LIGHTGRAY);
      textcolor (BLACK);
      char lino[100];
      char name[20];
      if (master==-1){
	 sprintf (lino, "%s",sspec);
      } else {
	 strcpy (name, pcHead);
	 *strstr (name, ".") = 0;
	 for (i=ofs; i>0; i--){
	    if ((ppcLines[i][1]=='.') &&
		(ppcLines[i][0]=='0'+master) &&
		(ppcLines[i+1][0]=='=') &&
		(ppcLines[i+1][1]=='=')){
	       sprintf (lino, "(%s) %s", name, ppcLines[i]);
	       goto ccc;
	    }
	 }
	 sprintf (lino, "(%s) %s", name, ppcLines[0]);
      }
ccc:
      while (strlen(lino)<80) strcat (lino, "          ");
      lino[74]=0;
      gotoxy (2,1);
      cprintf ("%s",lino);
      gotoxy (1,siz-1);
again:
      if (failed){
          gotoxy (1, siz);
          textbackground (BLACK);
          textcolor (WHITE);
          cputs ("                                        ");
          cputs ("                                       ");
      }
      if (failed==1){
          gotoxy (1, siz);
          textbackground (BLACK);
          if (CONFIG.bVideo==1 || CONFIG.bVideo==3){
             textcolor (LIGHTRED);
          } else {
	     textcolor (WHITE);
          }
	  cprintf ("Could not find \"%s\"", szSearcher);
      }
      char ch=GetKey();
      if (failed){
	  gotoxy (1, siz);
	  textbackground (BLACK);
	  textcolor (WHITE);
	  cputs ("                                        ");
	  cputs ("                                       ");
	  failed=0;
      }
      switch (ch){
	 case 'S':
	    if (master!=-1 && searcher){
	       gotoxy (1,siz-1);
	       noclear=1;
	       ask ("Search all documentation for : ",szSearcher);
               gotoxy (1, siz);
               textbackground (BLACK);
               textcolor (WHITE);
               cputs ("Searching...                            ");
               cputs ("                                       ");
               if (szSearcher[0]){
                  noclear=0;
                  int oldmast=master;
                  int oldofs=ofs;
                  if (Search()){
                     gotoxy (1, siz);
                     textbackground (BLACK);
                     textcolor (WHITE);
                     cputs ("                                        ");
                     cputs ("                                       ");
                     goofs=ofs;
		     jump=1;
                     return;
                  } else {
                     failed=1;
		     master=oldmast;
		     ofs=oldofs;
		  }
	       } else {
		  failed=2;
	       }
	       goto redraw;
	    }
	    goto again;
	 case 'A':
	 case 'B':
	 case 'C':
	 case 'D':
	 case 'E':
	 case 'F':
	 case 'G':
	 case 'H':
	 case 'I':
	 case 'J':
	 case 'K':
	 case 'L':
	 case 'Z':
	    for (i=0;i<iLines-1;i++){
	       if ((ppcLines[i][1]=='.') &&
		   (ppcLines[i][2]==ch) &&
		   (ppcLines[i+1][0]=='=') &&
		   (ppcLines[i+1][1]=='=')){
		  ofs=i;
		  goto done;
	       }
	    }
done:
	    break;
	 case '0':
	 case '1':
	 case '2':
	 case '3':
	 case '4':
	 case '5':
	 case '6':
	 case '7':
	 case '8':
	    if (master!=-1){
	      master=ch-'0';
	      jump=1;
	      return;
	    } else
	      break;
	 case KEY_ESC:
         case 'X':
         case 'Q':
	    textattr (attri);
	    clrscr();
	    return;
	 case 9:
	    if (flg){
	       textmode (cm);
	    }
	    textattr (attri);
	    clrscr();
	    if (CONFIG.bVideo==1 || CONFIG.bVideo==3){
               textbackground (BLUE);
               textcolor (LIGHTCYAN);
            }
	    iSum=0;
            for (i=0;i<iLines-1;i++){
               if ((ppcLines[i][1]=='.') &&
                   (ppcLines[i][2]=='Z') &&
                   (ppcLines[i+1][0]=='=') &&
                   (ppcLines[i+1][1]=='=')){
		  iSum=i;
                  goto done2;
	       }
            }
done2:
	    if (iSum)
	       for (i=0;i<iLines-iSum;i++){
                  gotoxy (1,i+1);
		  writeline (ppcLines[iSum+i]);
                  gotoxy (1,i+1);
               }
            textbackground (BLACK);
            textcolor (LIGHTGRAY);
            textattr (attri);
            doexit (0);
         case KEY_UP:
            Up(1);
            while (kbhit()){
               GetKey();
            }
            break;
         case KEY_DOWN:
            Down(1);
            while (kbhit()){
               GetKey();
            }
            break;
         case KEY_PUP:
            Up(siz-3);
            while (kbhit()){
               GetKey();
            }
            break;
         case KEY_PDOWN:
         case KEY_SPACE:
	    Down(siz-3);
            while (kbhit()){
	       GetKey();
            }
            break;
         case KEY_HOME:
            Up(MAXLINES);
            break;
         case KEY_END:
            Down(MAXLINES);
            break;
         case '+':
            if (detectegavga() && opt){
               if (mod!=2){
                  mod=2;
                  flg=1;
                  struct text_info ti;
                  textmode (C4350);
                  gettextinfo(&ti);
                  siz=ti.screenheight;
                  textattr(attri);
                  FSOut (7," "); // set directvideo to right value
                  clrscr();
                  if (ofs>0) for (int i=0; i<60; i++){
                     Up(1);
                     Down(1);
                  }
                  goto redraw;
               } else {
                  mod=1;
                  flg=1;
                  struct text_info ti;
                  textmode (C80);
                  gettextinfo(&ti);
                  siz=ti.screenheight;
                  textattr(attri);
                  FSOut (7," "); // set directvideo to right value
		  clrscr();
                  if (ofs>0) for (int i=0; i<60; i++){
                     Up(1);
                     Down(1);
                  }
                  goto redraw;
	       }
            }
         default:
            goto again;
      }
   }
#endif
}

#pragma argsused
void ViewFile (int index){
   char *pcFileName;
   master=index;
   struct text_info ti;
   gettextinfo(&ti);
   cm = ti.currmode;
   opt=0;
   if ((cm==C80) || mod) opt=1;
   if (mod==1) textmode (C80);
   if (mod==2) textmode (C4350);
   if (mod) flg=1;
   gettextinfo(&ti);
   siz=ti.screenheight;

   if (qdk){ // always use BIOS mode under DesqView
      if (CONFIG.bVideo<3)
         Mode(CONFIG.bVideo+2);
      else
         Mode(CONFIG.bVideo);
   } else
      Mode(CONFIG.bVideo);

   textattr(attri);
   FSOut (7," "); // set directvideo to right value
   clrscr();

jp:
   jump=0;
   switch (master){
      case 0:
	 pcFileName = "WHATSNEW.DOC";
	 break;
      case 1:
	 pcFileName = "README.DOC";
	 break;
      case 2:
	 pcFileName = "LICENSE.DOC";
	 break;
      case 3:
	 pcFileName = "BASIC.DOC";
	 break;
      case 4:
	 pcFileName = "MAIN.DOC";
	 break;
      case 5:
	 pcFileName = "BBS.DOC";
	 break;
      case 6:
	 pcFileName = "CONFIG.DOC";
	 break;
      case 7:
	 pcFileName = "BACKGRND.DOC";
	 break;
      case 8:
	 pcFileName = "EXTEND.DOC";
	 break;
   }
   if (sspec)
      pcFileName = sspec;
   char path[260];
   if (!LocateF(pcFileName,0)){
      FatalError (115,"cannot locate file %s (for viewing)",pcFileName);
   }
   strcpy (path, LocateF (pcFileName,0));
   ReadFile ();
   ViewIt ();
   if (jump) goto jp;

   if (flg) textmode (cm);
}
