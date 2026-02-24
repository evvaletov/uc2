// Copyright 1992, all rights reserved, AIP, Nico de Vries
// VIDEO.CPP

#include <stdlib.h>
#include <stdio.h>
#include <conio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include "handle.h"
#include "main.h"
#include "video.h"
#include "diverse.h"
#include "llio.h"
#include "dirman.h"

extern int dosvid;

void Plop (char *line1, char*line2);
void UnPlop (void);

static int iVideoMode;

void Mode (int iMode){
   iVideoMode = iMode;
}

static unsigned CX;

void Checkit (void)
{
    static int first=1;
    if (first)
    {
        first=0;
        _AH = 3;
        _BH = 0;
        geninterrupt (0x10);
        CX = _CX;
    }
}

void NoCursor (void){
   Checkit();
   if (CONFIG.bVideo<3)
   {
	_CH = 0x20;
	_CL = 0x0;
	_AH = 1;
	geninterrupt (0x10);
   }
}

void Cursor (void){
   Checkit();
   if (CONFIG.bVideo<3)
   {
      struct text_info t;

      gettextinfo( &t );

      if (t.currmode == MONO)
	_CH = 11, _CL = 12;
      else
        _CX = CX;

      _AH = 1;
      geninterrupt (0x10);
   }
}

void NH (char *pcBuf){
   static int stat=2;
again:
   if (!stat) return;
   if (stat==2){
      stat = !!getenv ("UC2_NO_HIGH_ASCII");
      goto again;
   }
   int max=strlen(pcBuf);
   for (int i=0;i<max;i++){
      if (pcBuf[i]=='ú') pcBuf[i]='-';
      if (pcBuf[i]=='þ') pcBuf[i]='*';
      if (pcBuf[i]=='Ä') pcBuf[i]='-';
      if (pcBuf[i]=='Í') pcBuf[i]='=';
      if (pcBuf[i]=='°') pcBuf[i]='@';
      if (pcBuf[i]=='Û') pcBuf[i]='@';
      if (pcBuf[i]&0x80) pcBuf[i]='*';
   }
}

void NoR (char *pcBuf){
   int dst=0;
   int max=strlen(pcBuf);
   for (int i=0;i<max;i++){
      if (pcBuf[i]!='\r' && (BYTE)pcBuf[i]>'\x9'){
	 pcBuf[dst++]=pcBuf[i];
      }
   }
   pcBuf[dst]=0;
}

void NoD (char *pcBuf){
   int dst=0;
   int max=strlen(pcBuf);
   for (int i=0;i<max;i++){
      if ((BYTE)pcBuf[i]>'\x9'){
	 pcBuf[dst++]=pcBuf[i];
      }
   }
   pcBuf[dst]=0;
}

/*
   Video table
   ===========
   9  interesting blabla
   7  background blabla
   5  is all right (OK)
   8  errors
   6  ask question (request action)
   4  header
   3  title (bright)
*/

int NotAllowed (BYTE l){
   return (!(l&CONFIG.fOut));
}

extern int attri;

static int plop;

int checkit;

void FSOut (BYTE l, char *fmt, ...){
   if (!plop) UnPlop();

   if (NotAllowed (l)) return;

   if (!plop && checkit) EndProgress();

//   Correct();
   char buf[200];
   char sp[200];
   int s;
   va_list argptr;
   va_start(argptr, fmt);
   vsprintf(buf, fmt, argptr);
   if (dosvid){
      NoR(buf);
      NH(buf);
      printf ("%s",buf);
      return;
   }
   switch (iVideoMode){
      case 1:
      case 2:
	 directvideo=1;
	 break;
      case 3:
      case 4:
	 directvideo=0;
	 break;
   }
   if (l&8){
      NH (buf);
      cputs (buf);
      return;
   }
   s=0;
   int sl=strlen(buf);
   for (int i=0;i<sl;i++){
      if ((unsigned char)buf[i]<='\x9'){
	 sp[s]=0;
	 NH (sp);
	 cputs (sp);
	 s=0;
	 if (iVideoMode==1||iVideoMode==3){ // COLOR
	    switch (buf[i]){
	       case 3: // program title
		  textbackground (BLACK);
		  textcolor (LIGHTCYAN);
		  break;
	       case 4: // header
		  textbackground (BLACK);
		  textcolor (CYAN);
		  break;
	       case 5: // OK
		  textbackground (BLACK);
		  textcolor (GREEN);
		  break;
	       case 6: // ASK
		  textbackground (BLACK);
		  textcolor (YELLOW);
		  break;
	       case 7: // blahblah
		  textbackground (BLACK);
		  textcolor (LIGHTGRAY);
		  break;
	       case 8: // error, danger
		  textbackground (BLACK);
		  textcolor (LIGHTRED);
		  break;
	       case 9: // plop border1
		  textbackground (BLUE);
		  textcolor (WHITE | BLINK);
		  break;
	       case 1: // plop border2
		  textbackground (BLUE);
		  textcolor (LIGHTCYAN);
		  break;
	       case 2: // plop contents
		  textbackground (BLUE);
		  textcolor (WHITE);
		  break;
	    }
	 } else { // B&W
	    textattr (attri);
	    if (CONFIG.fOut!=4){
	       switch (buf[i]){
		  case 3: // program title
//		     textbackground (BLACK);
//		     textcolor (WHITE);
		     highvideo();
		     break;
		  case 4: // header
//		     textbackground (BLACK);
//		     textcolor (LIGHTGRAY);
		     break;
		  case 5: // OK
//		     textbackground (BLACK);
//		     textcolor (WHITE);
		     highvideo();
		     break;
		  case 6: // ASK
//		     textbackground (BLACK);
//		     textcolor (WHITE);
		     highvideo();
		     break;
		  case 7: // blahblah
//		     textbackground (BLACK);
//		     textcolor (LIGHTGRAY);
		     break;
		  case 8: // error, danger
//		     textbackground (BLACK);
//		     textcolor (WHITE);
		     highvideo();
		     break;
		  case 9: // plop border1
		     textbackground (LIGHTGRAY);
		     textcolor (WHITE | BLINK);
		     break;
		  case 1: // plop border2
		     textbackground (LIGHTGRAY);
		     textcolor (WHITE);
		     break;
		  case 2: // plop contents
		     textbackground (LIGHTGRAY);
		     textcolor (WHITE);
		     break;
	       }
	    } else {
//	       textbackground (BLACK);
//	       textcolor (LIGHTGRAY);
	    }
	 }
      } else {
	 sp[s++]=buf[i];
      }
   }
   sp[s]=0;
   NH (sp);
   cputs(sp);
   va_end(argptr);
}

void Out (BYTE l, char *fmt, ...){
   if (!plop) UnPlop();

   if (NotAllowed (l)) return;

   EndProgress ();

   static char buf[200];
   va_list argptr;
   va_start(argptr, fmt);
   vsprintf(buf, fmt, argptr);
   if (StdOutType()!=D_CON){
      NoR (buf);
      NH (buf);
      CSB;
	 printf ("%s",buf);
      CSE;
   } else {
      FSOut (l,"%s",buf);
   }
   va_end(argptr);
}

int dumpo;
void COut (BYTE l, char *fmt, ...){
   if (!plop) UnPlop();

   if (NotAllowed (l)) return;

   static char buf[200];
   va_list argptr;
   va_start(argptr, fmt);
   vsprintf(buf, fmt, argptr);
   if (StdOutType()==D_CON || dumpo)
      Out (l,"%s",buf);
   va_end(argptr);
}

void EOut (BYTE l, char *fmt, ...){
   if (!plop) UnPlop();

   if (NotAllowed (l)) return;

   static char buf[200];
   va_list argptr;
   va_start(argptr, fmt);
   vsprintf(buf, fmt, argptr);
   if ((StdOutType()!=D_CON) && (!dosvid))
      FSOut (l,"%s",buf);
   va_end(argptr);
}

void FROut (BYTE l, char *fmt, ...){
   if (!plop) UnPlop();

   if (NotAllowed (l)) return;

   static char buf[200];
   va_list argptr;
   va_start(argptr, fmt);
   vsprintf(buf, fmt, argptr);
   if (StdOutType()!=D_CON){
      NoR (buf);
      NH (buf);
      CSB;
	 printf ("%s",buf);
      CSE;
   }
   va_end(argptr);
}

char *X (int len, char *str){
   static char buf[200];
   strcpy (buf, str);
   while (strlen (buf)<len) strcat (buf," ");
   return buf;
}

extern int fBreak;

static int flag=0;
static char key[150];

void UnGetKey (char c){
   key[flag++]=c;
}

char GetKey (){
   if (flag){
      return key[--flag];
   }
   fBreak=2;
   while (!kbhit());
   char c;
   CSB;
      c = getch();
   CSE;
   c = toupper (c);
   fBreak=0;
   if (c==0) return 128+getch();
   return c;
}

char Echo (char c){
   if (dosvid) return c;
   if (c==13){
      FSOut(7,"[Enter]");
   } else if (c==27) {
      FSOut(7,"[Esc]");
   } else if (c<32) {
      FSOut (7,"þ");
   } else {
      FSOut (7,"%c",c);
   }
   delay (50);
   return c;
}

char *GetString (int size){
   static char tmp[140];
   gets(tmp);
   tmp[size]=0; // assure maxlen
   return tmp;
}

extern int problemos;

static char dbuf[200]; // 'doing' buffer
static int dodo;

void Doing (char *fmt, ...){
   if (fmt){
      va_list argptr;
      va_start(argptr, fmt);
      vsprintf(dbuf, fmt, argptr);
      va_end(argptr);
      dodo=1;
   } else {
      dodo=0;
   }
}

extern char month[12][4];

char *DT (void){
   static char buf[50];
   struct dostime_t t;
   struct dosdate_t d;
   _dos_gettime (&t);
   _dos_getdate (&d);
   sprintf (buf, "%s-%02d-%4d %2d:%02d:%02d",
      strupr(month[d.month-1]),
      (WORD)d.day,
      (WORD)d.year,
      (WORD)t.hour,
      (WORD)t.minute,
      (WORD)t.second);
   return buf;
}

void diskfull (char);
void SKeep (void);
void SBack (void);

extern char pcArchivePath [260];
static char pcLastPath [260]="";

static int handle;

DWORD dwLogSize (void){
   return GetFileSize (handle);
}

extern int install, errors;

void ErrorLog (char *fmt, ...){
   static int ok=1;
   static int superfail=0;
   static int o=0;
   if (superfail) goto fail;
   if (!ok) return;
   if (CONFIG.finstall) return; // no error logging during install
   if (CONFIG.pcLog[3]=='*') return; // error logging has been disabled
   SKeep();
   GBack();
   char buf[200];
   if (fmt==NULL) {
      Close (handle);
      SBack();
      return;
   }
   if (!o){
      strcpy (buf, CONFIG.pcLog);
      strcat (buf,"\\UC2_ERR.LOG");
      ok=0;
      handle=-1;
      if (Exists(buf)){
         while (handle==-1){
            handle=Open (buf, MAY|RW|NOC);
            superfail=1;
            BrkQ();
            superfail=0;
         }
      } else {
          handle=Open (buf, MAY|CR|NOC);
      }
      if (handle==-1) goto fail;
      sprintf (buf, "UltraCompressor II revision 2 => error logfile <=");
      Write ((BYTE *)buf, handle, strlen(buf));
      Seek (handle, GetFileSize(handle));
      if (handle==-1) {
fail:
	 problemos=2; // special case for failing to access logfile
         errors++;
	 Out (7,"\n\r\x8\FATAL ERROR 250: cannot open logfile %s\n\r",buf);
	 EOut (7,"\n\r\x8\FATAL ERROR 250: cannot open logfile %s\n\r",buf);
         doexit (250);
      } else
	 ok=1;
      o=1;
      if (!install)
      {
          sprintf (buf,"\r\n\r\nCOMMAND ");
          Write ((BYTE *)buf, handle, strlen(buf));
          for (int i=0;i<_argc;i++){
             sprintf (buf,"%s ",_argv[i]);
             Write ((BYTE *)buf, handle, strlen(buf));
          }
      } else {
          sprintf (buf,"\r\n\r\nCOMMAND UC2R2 ");
          Write ((BYTE *)buf, handle, strlen(buf));
      }

      extern char gpcPath[260];

      if ((gpcPath[1]==':')&&(gpcPath[3]==0))
         sprintf (buf," (called from %s)",gpcPath);
      else
         sprintf (buf," (called from %s\\)",gpcPath);
      Write ((BYTE *)buf, handle, strlen(buf));

      sprintf (buf,"\r\n");
      Write ((BYTE *)buf, handle, strlen(buf));
   }
   if (strcmp(pcArchivePath,pcLastPath)!=0){
      strcpy (pcLastPath, pcArchivePath);
      sprintf (buf," Processing archive %s\r\n",pcLastPath);
      Write ((BYTE *)buf, handle, strlen(buf));
   }
   va_list argptr;
   va_start(argptr, fmt);
   char buf2[150];
   vsprintf(buf2, fmt, argptr);
   NoR (buf2);
   sprintf (buf, "   %s %s\r\n",DT(),buf2);
   Write ((BYTE *)buf, handle, strlen(buf));
   va_end(argptr);
   SBack();
}

extern int fatal;

int errors=0, warnings=0;

void FatalError (int level, char *fmt, ...){
   fatal=1;
   char buf[200];
   if (problemos==0) problemos=1;
   errors++;
   va_list argptr;
   va_start(argptr, fmt);
   vsprintf(buf, fmt, argptr);
   Out (7,"\n\r\x8\FATAL ERROR %d: %s\n\r",level,buf);
   if (dodo){
      EOut (7,"\n\r\x8\FATAL ERROR %d: %s (busy with %s)\n\r",level,buf,dbuf);
      ErrorLog (" FATAL ERROR %d: %s (busy with %s)",level,buf,dbuf);
   } else {
      EOut (7,"\n\r\x8\FATAL ERROR %d: %s\n\r",level,buf);
      ErrorLog (" FATAL ERROR %d: %s",level,buf);
   }
   dodo=0;
   va_end(argptr);
   GBack();
   doexit (level);
}

extern void Level (int);

void Error (int level, char *fmt, ...){
   char buf[200];
   Level (level);
   if (problemos==0) problemos=1;
   errors++;
   va_list argptr;
   va_start(argptr, fmt);
   vsprintf(buf, fmt, argptr);
   Out (7,"\x8\ ERROR %d: %s\n\r",level,buf);
   if (dodo){
      EOut (7,"\x8\ERROR %d: %s (busy with %s)\n\r",level,buf,dbuf);
      ErrorLog (" ERROR %d: %s (busy with %s)",level,buf,dbuf);
   } else {
      EOut (7,"\x8\ERROR %d: %s\n\r",level,buf);
      ErrorLog (" ERROR %d: %s",level,buf);
   }
   dodo=0;
   va_end(argptr);
}

void Warning (int level, char *fmt, ...){
   char buf[200];
   Level (level);
   if (problemos==0) problemos=1;
   warnings++;
   va_list argptr;
   va_start(argptr, fmt);
   vsprintf(buf, fmt, argptr);
   Out (7,"\x8\ WARNING %d: %s\n\r",level,buf);
   if (dodo){
      EOut (7,"\x8\WARNING %d: %s (busy with %s)\n\r",level,buf,dbuf);
      ErrorLog (" WARNING %d: %s (busy with %s)",level,buf,dbuf);
   } else {
      EOut (7,"\x8\WARNING %d: %s\n\r",level,buf);
      ErrorLog (" WARNING %d: %s",level,buf);
   }
   dodo=0;
   va_end(argptr);
}

void InternalError (char *file, long line){
   Doing (NULL);
   if (problemos==0) problemos=1;
   errors++;
   char fil[4];
   fil[0]=file[0];
   fil[1]=file[1];
   fil[2]=file[3];
   fil[3]=0;
   Out (7,"\x8\n\rFATAL ERROR: internal error code %s%03lX\n\r\n\r",fil,line);
   Out (3,"The internal integrity protection detected a fatal problem.\n\r");
   Out (3,"This might be caused by a hardware problem, or by a conflict between TSR's,\n\r");
   Out (3,"device drivers, the operating system and UC. If you have unusual or very new\n\r");
   Out (3,"soft- or hardware you might try disabling it to solve this kind of problems.\n\r");
   Out (3,"If you are unable to solve the problem, or if you think UC cannot cooperate\n\r");
   Out (3,"with specific soft- or hardware, please report this to AIP-NL so we can solve\n\r");
   Out (3,"it.\n\r\n\r");

   EOut (7,"\x8\n\rFATAL ERROR : internal error code %s%03lX\n\r\n\r",fil,line);
   ErrorLog (" FATAL ERROR: internal error code %s%03lX",fil,line);
   doexit (255);
}

char *TagName (char *in){
   if (strncmp (in,"AIP:",4)==0)
      return in+4;
   else
      return in;
}

void DoPlop (void){
/*
   if (!plopped){
      plopped=1;

      gettext(19, 8, 61, 12, buf);

      _AH = 3;
      _BH = 0;
      geninterrupt(0x10);
      curpos  = _DX;
      curtype = _CX;

      _setcursortype (_NOCURSOR) ;

      struct text_info ti;
      gettextinfo(&ti);
      oldattr = ti.attribute;
   }

   plop=1;

   int oldcheck = checkit;
   checkit=0;

   gotoxy (19, 8); FSOut (7,"\x1 ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿ ");
   gotoxy (19, 9); FSOut (7,"\x1 ³\x2                                      \x1³ ");
   gotoxy (19,10); FSOut (7,"\x1 ³\x2                                      \x1³ ");
   gotoxy (19,11); FSOut (7,"\x1 ³\x2                                      \x1³ ");
   gotoxy (19,12); FSOut (7,"\x1 ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ ");

   gotoxy (20+18-strlen(head)/2, 8); FSOut (7,"\x1 %s ", head);

   gotoxy (20+18-strlen(cont)/2,10); FSOut (7,"\x2%s \x9(þ)", cont);

   checkit = oldcheck;

   plop=0;
*/
}

#define clock() *((DWORD *)MK_FP(0x40,0x6C))

int period=0;

#pragma argsused
void Plop (char *line1, char *line2){
/*
   if (dosvid) return;
   if (CONFIG.fOut==4) return;
   if (CONFIG.fOut==2) return;
   strcpy (head, line1);
   strcpy (cont, line2);
   if (!wantplop){
      wantplop = 1;
      if (!period){
	 ct = clock();
	 period=1;
      }
   }
*/
}

void PlopQ (void){
/*
   if ((labs(clock()-ct)>3) && wantplop){
      DoPlop();
      wantplop=0;
   }
*/
}

void UnPlop (void){
/*
   period=0;
   wantplop=0;

   if (!plopped) return;

   plopped=0;

   puttext(19, 8, 61, 12, buf);

   _DX = curpos;
   _AH = 2;
   _BH = 0;
   geninterrupt(0x10);

   _CX = curtype;
   _AH = 1;
   geninterrupt(0x10);

   textattr (oldattr);
*/
}

static int x, y;
static int hints;
static int count;
static int rcount;
static int lastseg;
static int busy;

void StartProgress (int ihints, int level)
{
   if (NotAllowed (level)) return;
   if (StdOutType()!=D_CON) return;
   if (dosvid) return;
   if (busy) return;
   hints=ihints;
//   if (hints<1) hints=1;
   count=0;
   rcount=0;
   x = wherex();
   if (x>69){
      FSOut (7,"\n\r                                                                    ");
      x = wherex();
   }
   y = wherey();
   gotoxy (x, y);
//   FSOut (7,"\x7°°°°°°°°°°");
   FSOut (7,"\x7úúúúúú");
   lastseg=0;
   busy=1;
   checkit=1;
}

int pulsar (void);

void Hint (void)
{
   char tmp [20];
   if (!busy) return;
   UnPlop();
   rcount++;
   if (hints>0){
      if (++count>hints) count=hints;
      int seg=(int)((long)count*13/hints/2);
      if (seg<1) seg=1;
      if (seg>6) seg=6;
      if (seg==lastseg && !pulsar()) return;
      lastseg=seg;
      strcpy (tmp,"\x7");
      for (int i=0;i<seg;i++)
	 strcat (tmp, "þ");
      for (i=0;i<(6-seg);i++)
	 strcat (tmp, "ú");
   } else {
      if (!pulsar()) return;
      ++lastseg;
      int seg=1;
      if (lastseg>10) seg++;
      if (lastseg>30) seg++;
      if (lastseg>90) seg++;
      if (lastseg>180) seg++;
      strcpy (tmp,"\x7");
      for (int i=0;i<seg;i++)
	 strcat (tmp, "þ");
      for (i=0;i<(6-seg);i++)
	 strcat (tmp, "ú");
   }
      static int rel;
      switch (rel++)
      {
           case 3:
               strcat (tmp," \\");
               break;
           case 2:
               strcat (tmp," -");
               break;
           case 1:
               strcat (tmp," /");
               break;
           case 0:
               strcat (tmp," |");
               break;
      }
      if (rel==4) rel=0;
   gotoxy (x, y);
   checkit=0;
   FSOut (7, tmp);
   gotoxy (x+7, y);
   checkit=1;
//   gotoxy (x, y);
}

void EndProgress (void)
{
   if (!busy) return;
   UnPlop();
   gotoxy (x,y);
   checkit=0;
   FSOut (7,"\x7þþþþþþ  ");
   gotoxy (x+7, y);
//   Out (3,"\n\r");
//   gotoxy (x,y);
#ifdef UCPROX
   if (getenv("PROGRESS")) FSOut (7,"[%d %d]", hints, rcount);
#endif
   busy=0;
}
