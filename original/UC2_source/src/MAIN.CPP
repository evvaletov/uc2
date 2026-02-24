// Copyright 1992, all rights reserved, AIP, Nico de Vries
// MAIN.CPP

unsigned _stklen = 10240;

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
#include <ctype.h>
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

/**********************************************************************/
/*$ PROTOTYPES
 */


/**********************************************************************/
/*$ DEFENITION BLOCK  // classes,types,global data,constants etc
    PURPOSE OF DEFENITION BLOCK:
       Super global UC/UCPRO/UCPROX information.
*/

CONF CONFIG, SPARE;

char pcManPath[260]; // path for manuals etc (location of EXE)
char pcEXEPath[260]; // full path of executable (for config info)

int attri;
int install=0;

/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       Validate serial number.
*/

#ifndef UE2

DWORD table[32]={
   0x8B57E552l,
   0x7796E926l,
   0x4DF3EEE3l,
   0x9924875Al,
   0x7B35E75Cl,
   0x9783649Dl,
   0x5DF13E4Al,
   0xA964483El,
   0xBBF47551l,
   0x74E8692El,
   0x4FFE3AEFl,
   0x9E84E487l,
   0x5B5765CEl,
   0xC7864C92l,
   0xDDF3CAEEl,
   0x4A3C4488l,
   0xEBC5E75El,
   0xF78694C2l,
   0x7FF9ECA4l,
   0x89348E76l,
   0x4E5745E5l,
   0x78A69238l,
   0x52F8A4ECl,
   0xF9E348C7l,
   0x835E7C55l,
   0x40C6E9C1l,
   0xEDFFA1E6l,
   0x49A49C43l,
   0xDB575544l,
   0x47D6C923l,
   0x8DF7AEF3l,
   0x59548C47l
};

#endif

#pragma argsused
DWORD ser2int (DWORD n){
#ifndef UE2
   for (int i=0;i<32;i++){
      if ((n&0xFF000000l)==(table[i]&0xFF000000l)){
         n^=table[i];
         if (((n%17)==0) && (((n/17)&0x1F)==i))
            return n/17;
         else
            return 0;
      }
   }
#endif
   return 0;
}

#pragma argsused
DWORD int2ser (DWORD n){
    static char* q="*%I3#5";
#ifndef UE2
   return (n*17) ^ table[(WORD)(n&0x1f)];
#else
   return 0;
#endif
}

#pragma argsused
unsigned ser2pin (DWORD n){
#ifndef UE2
   WORD w = (WORD)(((n*17) ^ table[(WORD)(n&0x1f)])%9999);
   if (w<999)w+=1234;
   return w;
#else
   return 0;
#endif
}

int Validate (){
#ifndef UE2
   DWORD dw = CONFIG.dSerial;
   for (int i=0;i<32;i++){
      if ((dw&0xFF000000l)==(table[i]&0xFF000000l)){
         dw^=table[i];
         if (((dw%17)==0) && (((dw/17)&0x1F)==i))
            return 1;
         else
            return 0;
      }
   }
#endif
   return 0;
}


/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       Help menu.
       Configuration menu.
*/

void OnOff (int i){
#ifndef UE2
   if (i==0){
      FSOut(7,"\x5\OFF\x7\ (on)");
   } else {
      FSOut(7,"\x5\ON\x7\ (off)");
   }
#endif
}

void QTrans (void){
   BYTE q=0x27;
   for (int i=0;i<sizeof(CONF);i++){
      ((BYTE*)&CONFIG)[i]^=q;
      q+=(0x31^i);
   }
}

void UpdateCFG (){
#ifndef UE2
   FILE *fp = fopen (pcEXEPath,"r+b");
   if (!fp){
      Doing ("updating configuration");
      FatalError (105, "cannot write to %s\n\r",pcEXEPath);
   }
   fseek (fp, filelength(fileno(fp))-sizeof(CONF), SEEK_SET);
   QTrans();
   fwrite (&CONFIG, 1, sizeof (CONF), fp);
   QTrans();
   fclose (fp);
#endif
}


void Default (void);

extern int dosvid;

void GetCFG (){
   FILE *fp = _fsopen (pcEXEPath,"rb",SH_DENYWR);
   if (!fp){
      FatalError (105, "cannot read configuration from %s",pcEXEPath);
   }
   fseek (fp, filelength(fileno(fp))-sizeof(CONF), SEEK_SET);
   fread (&CONFIG, 1, sizeof (CONF), fp);
   QTrans();
   if (dosvid){
      CONFIG.bVideo = 4;
      CONFIG.fOut = 4;
   }
   SPARE=CONFIG;
   if (CONFIG.dwMagic!=MAGIC){
      fseek (fp, -17, SEEK_END);
      BYTE b;
      fread (&b, 1, 1, fp);            // line3
      fseek (fp, -b-2, SEEK_CUR);
      fread (&b, 1, 1, fp);            // line2
      fseek (fp, -b-2, SEEK_CUR);
      fread (&b, 1, 1, fp);            // line1
      fseek (fp, -b-12, SEEK_CUR);
      fread (&b, 1, 1, fp);            // e
      fseek (fp, -130+b*2, SEEK_CUR);
      fread (&b, 1, 1, fp);            // N
      fseek (fp, -130+b*2, SEEK_CUR);
      fread (&b, 1, 1, fp);            // c
      fseek (fp, -130+b*2, SEEK_CUR);
      fread (&b, 1, 1, fp);            // AIPc
      fseek (fp, -129+b*2-(long)(sizeof(CONF)), SEEK_CUR);
      fread (&CONFIG, 1, sizeof (CONF), fp);
      QTrans();
      SPARE=CONFIG;
      fclose (fp);
      if (CONFIG.dwMagic!=MAGIC){
	 Default();
	 FatalError (105, "%s is damaged",pcEXEPath);
      }
   } else {
      fclose (fp);
   }
#ifdef UE2
   Default();
   if (dosvid){
      CONFIG.fOut = 4;
   } else {
      CONFIG.fOut = 2;
   }
   CONFIG.bVideo = 4;
   strcpy (CONFIG.pcLog,"C:\\*");
   SPARE=CONFIG;
#endif
//   Out (7,"[[%ld]]",CONFIG.dwSoffset);
}

void Cursor (void);
void NoCursor (void);

int noclear=0;

#pragma argsused
void ask (char *q, char *a){
#ifndef UE2
   int j = strlen(a);
   BYTE ch;


   FSOut (7,"\x6\n\r%s%s",q,a);

   for (;;){
      ch = GetKey();
      if (ch==13){
	 a[j]=0;
	 if (!noclear){
	    FSOut (7,"\n\r");
	    FSOut (7,"\x7");
	    clrscr();
	 }
	 return;
      } else if (ch==8){
	 if (j>0){
	    j--;
	    a[j]=0;
	    FSOut (7,"\r%s%s ",q,a);
	    FSOut (7,"\r%s%s",q,a);
	 }
      } else if (ch==27) {
	 while (j>0){
	    j--;
	    a[j]=0;
	    FSOut (7,"\r%s%s ",q,a);
	    FSOut (7,"\r%s%s",q,a);
	 }
      } else {
	 if (ch>=32 && ch<127){
	    if (j==80) j=79;
	    else FSOut (7,"%c",ch);
	    a[j++]=ch;
	    a[j]=0;
	 }
      }
   }
#endif
}

void Default (void){
   CONFIG.fEMS = 1;
   CONFIG.fXMS = 1;
   CONFIG.fMul = 0;
   CONFIG.fEA  = 0;
   CONFIG.fHID = 2;

   CONFIG.fx86 = 1;
   CONFIG.bVideo = 1;
   CONFIG.fOut=2;

   CONFIG.fNet = 1;
   CONFIG.fSwap = 1;

   CONFIG.fSMSkip = 1;
   CONFIG.bRelia = 0;
   CONFIG.bDComp = 1;
   CONFIG.fInc = 0;
   CONFIG.fAutoCon = 0;
   CONFIG.fVscan = 0;

   strcpy ((char *)CONFIG.pbTPATH,pcManPath);
   strcpy ((char *)CONFIG.pcMan,pcManPath);
   strcpy ((char *)CONFIG.pcLog,pcManPath);
   strcpy ((char *)CONFIG.pcBat,pcManPath);
}

void NoLastS (char *buf){
   if (buf[0]==0){
      buf[0]='\\';
      buf[1]=0;
   }
   if (buf[strlen(buf)-1]=='\\')
      buf[strlen(buf)-1]=0;
   if (buf[1]!=':'){
      char tmp[300];
      strcpy (tmp,"C:");
      strcat (tmp, buf);
      strcpy (buf, tmp);
   }
   if ((buf[2]!=0) && (buf[2]!='\\')){
      char tmp[300];
      strcpy (tmp,"?:");
      tmp[0] = buf[0];
      strcat (tmp,"\\");
      strcat (tmp+3, buf+2);
      strcpy (buf, tmp);
   }
   strupr (buf);
}

int menu=0;
int Logo (void);

int searcher=1;

#pragma argsused
char *Check (int i, char *file, char *blah){
#ifndef UE2
   static char stat[8];
   if (i==100){
      for (i=0; i<8; i++)
	 stat[i]=0;
      return "";
   }
   if (stat[i]==0){
      if (LocateF(file,0))
	 stat[i]=1;
      else
	 stat[i]=2;
   }
   if (stat[i]==1) return blah;
   static char* ret = "\x8<this document is not available>\n\r";
   searcher=0;
   return ret;
#else
   return NULL;
#endif
}

void ConfigMenu (void);

extern "C" void Turbo (int);

void HelpMenu (){
#ifndef UE2
   char ch[9];
   int opt=3;
   Out (7,"\x7");
   clrscr();
   for (;;){
      menu=1;
      NoCursor();
      Logo();
      for (int i=0;i<9;i++) ch[i]='\x7';
      ch[opt]='\x6';
      FSOut(7,"\x6\HELP MENU \x7(view\x3 & search \x7\documentation)\n\r");
      FSOut(7,"  \x6 0%c   -> WHATSNEW  %s",ch[0],Check(-1,"WHATSNEW.DOC","what changed from UC2 to UC2 revision 2\x3 NEW!\n\r"));
      FSOut(7,"  \x6 1%c   -> README    %s",ch[1],Check(0,"README.DOC","how to get started, overview, features etc.\n\r"));
      FSOut(7,"  \x6 2%c   -> LICENSE   %s",ch[2],Check(1,"LICENSE.DOC","the licenses, warranty etc.\n\r"));
      FSOut(7,"  \x6 3%c   -> BASIC     %s",ch[3],Check(2,"BASIC.DOC","the most essential commands of UC\n\r"));
      FSOut(7,"  \x6 4%c   -> MAIN      %s",ch[4],Check(3,"MAIN.DOC","the use of the UC command in detail\n\r"));
      FSOut(7,"  \x6 5%c   -> BBS       %s",ch[5],Check(4,"BBS.DOC","special features for BBS sysops\n\r"));
      FSOut(7,"  \x6 6%c   -> CONFIG    %s",ch[6],Check(5,"CONFIG.DOC","how to configure UC\n\r"));
      FSOut(7,"  \x6 7%c   -> BACKGRND  %s",ch[7],Check(6,"BACKGRND.DOC","concepts, program design, compressor design, benchmarks\n\r"));
      FSOut(7,"  \x6 8%c   -> EXTEND    %s\n\r",ch[8],Check(7,"EXTEND.DOC","tools; extended commands and options\x3 NEW!\n\r"));

#ifdef UCPROX
      if (debug) FSOut(7,"\x6\CONFIGURATION [[MEM FREE : %lu]]\n\r",farcoreleft());
      else FSOut(7,"\x6\CONFIGURATION\n\r");
#else
      FSOut(7,"\x6\CONFIGURATION\n\r");
#endif
      FSOut(7,"\x6   C\x7 -> (re)configure\n\r\n\r");
again:
      FSOut(7,"\x6\CHOICE (Escape to quit)?                                      \x7Mini help: \x3UC -?\r");
      FSOut(7,"\x6\CHOICE (Escape to quit)? ");
      Cursor();
      char ch=Echo(GetKey());
got:
      switch(ch){
	 case KEY_UP:
	    if (opt==0) opt=9;
	    opt--;
	    gotoxy(1,1);
	    break;
	 case KEY_DOWN:
	    opt++;
	    if (opt==9) opt=0;
	    gotoxy(1,1);
	    break;
	 case 13:
	    ch='0'+opt;
	    goto got;
	 case 27:
         case 'q':
         case 'Q':
         case 'x':
         case 'X':
	    FSOut (7,"\n\r");
	    return;
	 case '0':
	 case '1':
	 case '2':
	 case '3':
	 case '4':
	 case '5':
	 case '6':
	 case '7':
	 case '8':
	    ViewFile (ch-'0');
	    break;
	 case 'C':
	    Check (100,"","");
	    ConfigMenu();
	    gotoxy (1,1);
	    break;
	 case 'R':
	    FSOut (7,"\x7");
	    clrscr();
	    char c[100], ss[100];
	    char ser[100]="";
	    ask ("Please enter serial number : ",ser);
	    c[1]=0;
	    strcpy (ss,"");
	    for (i=0;i<strlen(ser);i++){
	       if (isdigit(ser[i])){
		  c[0]=ser[i];
		  strcat (ss, c);
	       }
	    }
	    if (strlen(ss)!=10){
	       FatalError (110,"this is not a valid serial number");
	    } else if (ss[0]>'4'){
	       FatalError (110, "this is not a valid serial number");
	    } else {
	       unsigned long l = strtoul (ss, NULL, 10);
	       if (!ser2int(l)){
		  FatalError (110,"this is not a valid serial number");
	       } else {
		  char sec[100]="";
		  ask ("Please enter security code : ",sec);
		  unsigned se = atoi (sec);
		  if (se!=ser2pin(l)){
		     FatalError (110,"this is not a valid security code");
		  }  else {
		     char c[300];
		     sprintf (c, "%s ~~%lu-%u", LocateF("UC.EXE",2), l, se);
		     ssystem (c);
		  }
	       }
	    }
	    FSOut (7,"\x7");
	    clrscr();
	    FSOut (7,"\n\r");
	    return;
	 default:
	    FSOut (7,"\x8\ *** WRONG CHOICE ***\r");
            delay (500);
            FSOut (7,"                                                         \r");
            goto again;
      }
   }
#endif
}

/**********************************************************************/
/*$ FUNCTION BLOCK  // method(s), function(s)
    PURPOSE OF FUNCTION BLOCK:
       Main UC/UCPRO/UCPROX program.
*/

extern int os2;  // OS/2 is alive
int win=0;       // windows detected
int qdk=0;       // DesqVIEW detected

void repit (void);

static int ctr;

void oe1 (void){
   if (ctr==0)
#ifdef UE2
      Out (3,"\x7[");
#else
      Out (3,"\x7 [");
#endif
   else
      Out (3,"\x7,");
}

void oe2 (void){
   ctr++;
}

void oel (void){
   Out (3,"]");
}

// DRDosVer
// Returns the version of DR-DOS active or -1 if not DR-DOS. Returns the
// version as a word, high word major version, low word minor version.
int DRDosVer(void) {
   unsigned tmp;

   _FLAGS |= 1; // Set carry flag
   _AX = 0x4452;
   geninterrupt (0x21);
   tmp = _AX;
   if (_FLAGS & 0x1)
      return -1; // Not DR-DOS
   if (tmp==0x1063){
      oe1();
      Out (3,"DR-DOS 3.41");
      oe2();
   } else if (tmp==0x1064){
      oe1();
      Out (3,"DR-DOS 3.42");
      oe2();
   } else if (tmp==0x1065){
      oe1();
      Out (3,"DR-DOS 5.00");
      oe2();
   } else if (tmp==0x1067){
      oe1();
      Out (3,"DR-DOS 6.00");
      oe2();
   } else {
      oe1();
      Out (3,"NOVELL-DOS");
      oe2();
   }
   return 0;
}

void DVver(int chk) {
   _AX = 0x2B01;
   _CX = 0x4445;
   _DX = 0x5351;
   geninterrupt(0x21);
   if (_AL==0xFF){ // AL = FF means no Desqview
      return;
   } else if (_BX==2){
      qdk=1;
      if (!chk){
	 oe1();
	 Out (3,"DesqV");
	 oe2();
      }
   } else {
      qdk=1;
      unsigned tmp = _BX;
      if (!chk){
	 oe1();
	 Out (3,"DesqV %d.%d", (tmp >> 8) & 0xFF, tmp & 0xFF);
	 oe2();
      }
   }
}

void DWin(void) {
   _AX = 0x160a;
   geninterrupt (0x2F);
   if (_AX==0){ // Windows 3.1
      win=1;
   } else {
      _AX = 0x1600;
      geninterrupt (0x2F);
      if (_AL==0x7F){
	 win=1;
      } else {
	 _AX=0x4680;
	 geninterrupt (0x2F);
	 if (_AX==0){
	    _AX = 0x4b02;
	    _BX = 0;
	    _ES = 0;
	    _DI = 0;
	    geninterrupt (0x2F);
	    if (_AX!=0){
	       win=1;
	    }
	 }
      }
   }
}

void Win(void) {
   _AX = 0x160a;
   geninterrupt (0x2F);
   if (_AX==0){ // Windows 3.1
      win=1;
      if (_CX==3){
	 oe1();
	 Out (3,"Win31 enh");
	 oe2();
      } else {
	 oe1();
	 Out (3,"Win31 std");
	 oe2();
      }
   } else {
      _AX = 0x1600;
      geninterrupt (0x2F);
      if (_AL==0x7F){
	 win=1;
	 oe1();
	 Out (3,"Win enh");
	 oe2();
      } else {
	 _AX=0x4680;
	 geninterrupt (0x2F);
	 if (_AX==0){
	    _AX = 0x4b02;
	    _BX = 0;
	    _ES = 0;
	    _DI = 0;
	    geninterrupt (0x2F);
	    if (_AX!=0){
	       win=1;
	       _AX = 0x1605;
	       geninterrupt (0x2F);
	       if (_CX==0xFFFF){
		  oe1();
		  Out (3,"Win std");
		  oe2();
	       } else {
		  _AX = 0x1606;
		  geninterrupt (0x2F);
		  oe1();
		  Out (3,"Win real");
		  oe2();
	       }
	    }
	 }
      }
   }
}

int Logo (void){
   if (_argv[1][0]=='~'){
      if (StdOutType()==D_CON)
	 return 0;
      else
	 return 1;
   }
   int ret=0;
   Out (7,"\x7");
   if (StdOutType()==D_CON)
      if (((attri&0xF0)!=0)&&((CONFIG.bVideo&0x01)==1))
	 for (int i=0;i<50;i++)
	    delline();
   if ((StdOutType()==D_CON)&&(wherex()!=1)) Out (7,"\r\n");
   Out (4,"\x7\UC2, Copyright 1994, Ad Infinitum Programs, all rights reserved\r\n");
   if (StdOutType()==D_CON)
      Out (3,"\x4ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ\r\n");
   else
      Out (3,"\x4ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ\r\n");
#ifdef UE2
   Out (3,"\x4 °ÛÛÛ   °ÛÛÛ  °ÛÛÛÛ      UltraExpander II\r\n");
   Out (3,"\x4°Û  °Û   °Û   °Û  °Û     \"Free of charge archive expander for UC2 archives\"\r\n");
   Out (3,"\x4°ÛÛÛÛÛ   °Û   °ÛÛÛÛ  -NL Í> For non commercial use (=private/home) only! <Í\r\n");
   Out (3,"\x4°Û  °Û   °Û   °Û ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\r\n");
   Out (3,"\x4°Û  °Û  °ÛÛÛ  °Û Copyright 1994, Ad Infinitum Programs, all rights reserved\r\n");
#else
   Out (3,"\x4 °ÛÛÛ   °ÛÛÛ  °ÛÛÛÛ      \x3\UltraCompressor II (tm)  revision 2\n\r");
   Out (3,"\x4°Û  °Û   °Û   °Û  °Û     \"The new way of archiving.\"\r\n");
   Out (3,"\x4°ÛÛÛÛÛ   °Û   °ÛÛÛÛ  -NL \"Fast, reliable and superior compression.\"\r\n");
   Out (3,"\x4°Û  °Û   °Û   °Û ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\r\n");
   Out (3,"\x4°Û  °Û  °ÛÛÛ  °Û Copyright 1994, Ad Infinitum Programs, all rights reserved\r\n");
#endif

//#define NONDIS // non-disclosure version

#ifdef NONDIS
      Out (3,"\x4ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ\x8 Non-disclosure!!! (#021) \x4ÍÍÍ\n\r");
      struct date dat;
      getdate (&dat);
      if ((dat.da_year>1994) || (dat.da_mon>8)) FatalError (255,"you can no longer use this PRE-RELEASE");
#else
   Out (3,"\x4ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ\r\n");
#endif

#ifndef UE2
   if (Validate()){
      if (CONFIG.dSerial==2212433133L){
	 Out(3,"\x4Professional Evaluation Copy ");
      } else {
	 Out(3,"\x4Serial number : %s ",neat(CONFIG.dSerial));
      }
   } else {
      if (menu)
	 Out (7,"\x6R\x4 -> enter serial number");
      else
	 Out (3,"\x4Please register ");
   }
#endif
   ctr=0;
   union REGS regs;
   regs.h.ah = 0x30;
   intdos (&regs,&regs);
   if (regs.h.al==0){
      oe1();
      Out (3,"DOS");
      oe2();
   } else if (regs.h.al<10) {
      if (DRDosVer()==-1){
	 oe1();
	 if ((regs.h.al==5) && (regs.h.ah==2))
	    Out (3,"DOS 5.02");
	 else
	    Out (3,"DOS %d.%d",regs.h.al,regs.h.ah);
	 oe2();
      }
   } else {
      oe1();
      Out (3,"OS/2 %d.%d",regs.h.al/10,regs.h.ah);
      oe2();
      os2=1;
   }
   DVver(0);
   Win();

   if (m386){
      oe1();
      Out (3,"32bit");
      oe2();
   }

   repit();

   switch (StdOutType()){
      case D_CON:
	 break;
      case D_NUL:
	 oe1();
	 Out (3,"output to NUL");
	 oe2();
	 ret=1;
	 break;
      case D_DEV:
	 oe1();
	 Out (3,"output to device");
	 oe2();
	 ret=1;
	 break;
      case D_FILE:
	 oe1();
	 Out (3,"output to file/pipe");
	 oe2();
	 ret=1;
	 break;
   }
   oel();
   Out (7,"\n\r\n\r");
   return ret;
}

extern char * pcVmemPath;

extern char * cdecl _swappath;         /* swap path */

int igno=0;

void GetPath (char **argv){
   // Get full path
   if (getenv("UC2_PATH") && !igno){
      strcpy (pcEXEPath,getenv("UC2_PATH"));
   } else {
      strcpy (pcEXEPath,argv[0]);
   }

   char t1[5];
   char t2[80];
   fnsplit(pcEXEPath,t1,t2,NULL,NULL);
   fnmerge(pcManPath,t1,t2,NULL,NULL);
   NoLastS (pcManPath);

   _swappath=(char *)CONFIG.pbTPATH;
}

void ComoTerp (int argc, char **argv);

#ifdef UCPROX
   extern long claimed;
#endif

extern int cdecl breakHan (void);
extern int cdecl breakNono (void);
void cdecl errHan(unsigned deverr, unsigned errval, unsigned far *devhdr);

#ifdef UCPROX
   int debug=0;
   int heavy=0;
   int beta=0;
#endif

int m386;

#if defined(UCPROX) || defined (UE2)

FILE *spfp, *mfp;

static WORD far pascal Cread (BYTE *buf, WORD len){
   return (WORD) fread (buf, 1, len, mfp);
}

static void far pascal Cwrite (BYTE *buf, WORD len){
   fwrite (buf, 1, len, spfp);
}

#endif

int problemos=0;
extern int errors;
extern int warnings;

#ifdef UCPROX
extern DWORD minimal;
extern DWORD maximal;
DWORD virt; // !!! wrongo, should be improved

DWORD progmem;
DWORD initial;
#endif

extern BYTE bDump;
DWORD dwLogSize (void);


void cdecl exito (void){
   if (bDump){
      switch (problemos){
	 case 0:
	    if (stricmp (getenv("UC2_OK"), "OFF")!=0)
	       Close (Open ("U$~RESLT.OK",MUST|CR|NOC));
	    break;
	 case 1:
	 case 2:
	    if (Exists ("U$~RESLT.OK")) Delete ("U$~RESLT.OK");
	    break;
      }
   } else {
      char tmp[100];
      if (problemos!=0){
	 char err[20];
	 char war[20];

	 strcpy (err,"");
	 if (errors==1)
	    sprintf (err,"1 error");
	 else
	    sprintf (err,"%d errors",errors);

	 strcpy (war,"");
	 if (warnings==1)
	    sprintf (war,"1 warning");
	 else
	    sprintf (war,"%d warnings",warnings);

	 if (errors && warnings){
	    sprintf (tmp,"%s and %s have been reported",err,war);
	 } else if (errors){
	    if (errors==1)
	       sprintf (tmp,"1 error has been reported",err);
	    else
	       sprintf (tmp,"%s have been reported",err);
	 } else {
            if (warnings==1)
               sprintf (tmp,"1 warning has been reported",war);
            else
               sprintf (tmp,"%s have been reported",war);
         }
      }
      switch (problemos){
         case 0:
            Out (3,"\n\r\x5\Everything went OK");
            break;
         case 1:
            if (CONFIG.finstall){
               FSOut (7,"\x8\n%s \n\r",tmp);
               FROut (7,"\x8\n%s \n\r",tmp);
            } else {
               DWORD size;
               if (CONFIG.pcLog[3]!='*'){
                  size=dwLogSize();
               }
               FSOut (7,"\x8\n%s ",tmp);
               FROut (7,"\x8\n%s ",tmp);
               if (CONFIG.pcLog[3]!='*'){
                  FSOut (7,"\x7(logged to %s\\UC2_ERR.LOG)",CONFIG.pcLog);
                  FROut (7,"\x7(logged to %s\\UC2_ERR.LOG)",CONFIG.pcLog);
                  if (size>25000){
                     FSOut (7,"\n\r\x8WARNING: size of logfile is %s bytes",neat(size));
                     FROut (7,"\n\r\x8WARNING: size of logfile is %s bytes",neat(size));
                  }
                  ErrorLog (NULL);
               }
            }
            break;
         case 2:
            FSOut (7,"\x8\n%s ",tmp);
	    FSOut (7,"\x8(failed to write to logfile)");
            break;
      }
   }
#ifdef UCPROX
   if (beta) Out (7,"\n\r\x7Memory: PROG=%ld DYNRAM=%ld BASETOTAL=%ld VIRTUAL=%lu",(long)progmem,(long)maximal-minimal,(long)progmem+maximal-minimal,virt);
#endif
   if (problemos || (!dosvid && !bDump && StdOutType()==D_CON)){
      textattr (attri);
      cputs ("\n\r \r");
   }
#ifdef UCPROX
   if (debug){
      Out (7,"{{Correct exit of program}}\n\r");
   }
#endif
   if (!bDump && (StdOutType()==D_CON))
      if (((attri&0xF0)!=0)&&((CONFIG.bVideo&0x01)==1)){
         textattr (attri);
         for (int i=0;i<50;i++)
            delline();
      }
   GBack();
}

#ifdef TRACE
   void checkx (void);
#endif

extern int cdecl _useems;

int RCMD (char *p); // dirman.cpp

extern unsigned max_dist;

extern long revision;

int dosvid=0;

#ifdef UE2

   void cdecl main (int argc, char **argv){
      if (argv[1][0]=='='){ // UC2-3PI
	 strcpy (argv[1],argv[1]+1);
	 dosvid=1;
      }
restart:
      if ((argc==1) ||
	  (argv[1][0]=='?' || (argv[1][0]=='-' && argv[1][1]=='?') || (argv[1][0]=='/' && argv[1][1]=='?')) ||
	  (argv[1][0]=='h' || (argv[1][0]=='-' && argv[1][1]=='h') || (argv[1][0]=='/' && argv[1][1]=='h')) ||
	  (argv[1][0]=='H' || (argv[1][0]=='-' && argv[1][1]=='H') || (argv[1][0]=='/' && argv[1][1]=='H'))){ // mini help
	  printf ("ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ\n");
	  printf (" °ÛÛÛ   °ÛÛÛ  °ÛÛÛÛ      UltraExpander II\n");
	  printf ("°Û  °Û   °Û   °Û  °Û     \"Free of charge archive expander for UC2 archives\"\n");
	  printf ("°ÛÛÛÛÛ   °Û   °ÛÛÛÛ  -NL Í> For non commercial use (=private/home) only! <Í\n");
	  printf ("°Û  °Û   °Û   °Û ÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ\n");
	  printf ("°Û  °Û  °ÛÛÛ  °Û (C) Copyright 1994, Ad Infinitum Programs, all rights reserved\n");
	  printf ("ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ\n");
	  printf ("Usage:   UE SAMPLE      Extract all files in SAMPLE.UC2 to the current\n");
	  printf ("                        directory. UE will ask permission before it\n");
	  printf ("                        overwrites files or creates subdirectories.\n");
	  printf ("\n");
	  printf ("         UE -L SAMPLE   List all files in SAMPLE.UC2.\n");
	  printf ("\n");
	  printf ("This package is supplied 'AS IS'. It is up to you to determine its usability.\n");
	  printf ("You may not clone, alter, reverse engineer, decompile, disassemble, etc. this\n");
	  printf ("program or parts of it in any way.\n\n");
	  printf ("For commercial use you should use UltraCompressor II and/or contact us:\n");
	  printf ("     Õ AIP-NL, The Netherlands ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ¸\n");
	  printf ("     ³      Phone: +31-30-662107  Internet  : desk@aip.nl ³\n");
	  printf ("     ÀÄÄÄÄÄ FAX  : +31-30-616571  CompuServe: 100115,2303 Ù\n");
	  printf ("     Õ BMT Micro, U.S.A. ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ¸  UE.EXE has been\n");
	  printf ("     ÀÄÄÄÄÄ Phone: (910) 791-7052  FAX: (910) 350-2937 ÄÄÄÙ  sealed with USEAL\n");
	  exit (EXIT_SUCCESS);
      }

      DWin ();
   //   max_dist=125*512U;
      DVver (1);
   #ifdef UCPROX // special testing software
      if (getenv("BETA") && stricmp(getenv("BETA"),"ON")==0){
	 beta=1;
      }
      if (getenv("BETA") && stricmp(getenv("BETA"),"on")==0){
	 beta=1;
      }
      if (beta && getenv("DEBUG") && strcmp(getenv("DEBUG"),"ON")==0){
	 debug=1;
      }
      if (beta && getenv("DEBUG") && strcmp(getenv("DEBUG"),"on")==0){
	 debug=1;
      }
      if (beta && getenv("HEAVY") && strcmp(getenv("HEAVY"),"ON")==0){
	 heavy=1;
      }
      if (beta && getenv("HEAVY") && strcmp(getenv("HEAVY"),"on")==0){
	 heavy=1;
      }
   #endif
      Default();
      Mode (4); // super conservative for errors during setup
   #ifdef UCPROX
      #undef farmalloc
      #undef farfree
      if (beta){
	 void far*p = farmalloc (1);
	 progmem=(FP_SEG(p)-(long)_psp)*16L;
	 farfree (p);
	 initial=farcoreleft();
      }
   #endif

   //#define MEMUSE // determine PROG memory usage (even with release version!!)
   #ifdef MEMUSE
      #undef farmalloc
      #undef farfree
      void far*p = farmalloc (1);
      Out (7,"PROG=%ld\n",(FP_SEG(p)-(long)_psp)*16L);
      farfree (p);
   #endif

      _harderr(errHan);
      struct text_info ti;
      gettextinfo(&ti);
      attri = ti.attribute;
      setcbrk (0);
      ctrlbrk (breakNono);
      for (int i=0;i<argc;i++) strupr (argv[i]);
      GetPath(argv); // MUST be called before InitVmem !!!

   #ifdef UCPROX
      if (strcmp (argv[1],"###")==0){
	 GKeep();
	 spfp = fopen ("UC.EXE","r+b");

	 // DEFAULT CONFIGURATION
	 Default();
	 CONFIG.finstall=0;
	 CONFIG.dwMagic = MAGIC;
	 CONFIG.dSerial = 0;

	 if (CONFIG.fx86 && (probits()==32)){
	    m386=1;
	 } else {
	    m386=0;
	 }

	 ctrlbrk (breakHan);
	 setcbrk(1);
	 InitMem (); // keep it safe, don't use EMS, XMS
	 InitVmem();

	 CONFIG.dwSoffset = filelength (fileno (spfp));
	 fseek (spfp, CONFIG.dwSoffset, SEEK_SET);
	 mfp = fopen ("SUPER.DAT","rb");
	 Compressor (4,Cread,Cwrite,NOMASTER);
	 QTrans();
	 fwrite (&CONFIG,1,sizeof (CONF),spfp);
	 QTrans();
	 fclose (spfp);
	 fclose (mfp);
	 Out (7,"UC.EXE has been professionalized\n\r");
	 doexit (EXIT_SUCCESS);
      }
      if (strcmp (argv[1],"@@@")==0){
	 GKeep();
	 spfp = fopen ("UC.EXE","r+b");
	 FILE *fparch = fopen ("ALL.UC2","rb");

	 // DEFAULT CONFIGURATION
	 Default();
	 CONFIG.finstall= 1;
	 CONFIG.fx86=0;
	 CONFIG.bVideo=4;
	 CONFIG.fOut=2;

	 CONFIG.exesize = filelength (fileno (spfp));
         CONFIG.archsize = filelength (fileno (fparch));

         CONFIG.dwMagic = MAGIC;
         CONFIG.dSerial = 0;

         if (CONFIG.fx86 && (probits()==32)){
            m386=1;
         } else {
            m386=0;
         }

         ctrlbrk (breakHan);
         setcbrk(1);
         InitMem (); // keep it safe, don't use EMS, XMS
         InitVmem();

         fseek (spfp, CONFIG.exesize, SEEK_SET);

         BYTE *buf = xmalloc (16384,STMP);
         DWORD ctr =  CONFIG.archsize;
	 while (ctr){
            WORD trn = 16384;
            if (trn>ctr) trn = (WORD)ctr;
            fread (buf, 1, trn, fparch);
            fwrite (buf, 1, trn, spfp);
            ctr-=trn;
         }

         QTrans();
         fwrite (&CONFIG,1,sizeof (CONF),spfp);
         QTrans();
         fclose (spfp);
         fclose (mfp);
         fclose (fparch);
         Out (7,"UC.EXE has been converted to the install tool\n\r");
         doexit (EXIT_SUCCESS);
      }
   #endif
      ctrlbrk (breakHan);
      setcbrk(1);

      GetCFG ();
      GKeep();
      if (CONFIG.finstall){
      } else {
         if (CONFIG.fx86 && (probits()==32)){
            m386=1;
         } else {
            m386=0;
         }

         if (argc==1){
         } else {
            if (getenv("UC2_TMP")){
               strcpy ((char *)CONFIG.pbTPATH, getenv("UC2_TMP"));
	       SPARE=CONFIG;
            }
            if (CONFIG.fEMS){
               gmaxEMS=0xFFFF;
               if (getenv("UC2_MAX_EMS")){
                  gmaxEMS=atoi(getenv("UC2_MAX_EMS"))/16;
                  _useems=1; // 1--> XSPAWN cannot use EMS
               } else {
                  _useems=0; // 0--> XSPAWN can use EMS
               }
            } else {
               _useems=1; // 1--> XSPAWN cannot use EMS
            }
            if (CONFIG.fXMS){
               gmaxXMS=0xFFFF;
               if (getenv("UC2_MAX_XMS")) gmaxXMS=atoi(getenv("UC2_MAX_XMS"))/16;
               gmaxUMB=0xFFFF;
               if (getenv("UC2_MAX_UMB")) gmaxUMB=atoi(getenv("UC2_MAX_UMB"))*64;
               union REGS regs;
               regs.h.ah = 0x30;
               intdos (&regs,&regs);
               // if OS/2 then do not use UMB's !!
   //       if (regs.h.al>9) {
   //          gmaxUMB=0;
   //       }
            }

      //      if (CONFIG.fExt) gmaxI15=0xFFFF; //NEVER use raw extended memory
            if (getenv("UC2_RAW_EXT") && !gmaxXMS && !gmaxEMS && !gmaxUMB){
               gmaxI15 = atoi (getenv("UC2_RAW_EXT"))/16;
            }

            InitMem (); // Use EMS,XMS etc if config allows it.
            if (qdk){ // always use BIOS mode under DesqView
               if (CONFIG.bVideo<3)
		  Mode(CONFIG.bVideo+2);
               else
                  Mode(CONFIG.bVideo);
            } else
               Mode(CONFIG.bVideo);
            InitVmem();
            Logo();
      #ifdef UCPROX // special testing software
            if (beta && getenv("DEBUG") && strcmp(getenv("DEBUG"),"ON")==0){
               Out (7,"[[DEBUG MODE ENABLED]]\n\r");
               debug=1;
            }
            if (beta && getenv("DEBUG") && strcmp(getenv("DEBUG"),"on")==0){
               Out (7,"[[DEBUG MODE ENABLED]]\n\r");
               debug=1;
            }
            if (beta && getenv("HEAVY") && strcmp(getenv("HEAVY"),"ON")==0){
               Out (7,"[[HEAVY MODE ENABLED]]\n\r");
               heavy=1;
            }
            if (beta && getenv("HEAVY") && strcmp(getenv("HEAVY"),"on")==0){
               Out (7,"[[HEAVY MODE ENABLED]]\n\r");
               heavy=1;
            }
            if (strcmp(argv[1],"$$$")==0){
               Tester (argc, argv);
            } else
      #endif
           {
              ComoTerp (argc, argv);
           }
         }
      #ifdef UCPROX
         if (debug){
            if (claimed){
	       Out (7,"\n\r[[Memory consistency check failed : %ld]]\n\r",claimed);
            } else {
               Out (7,"\n\r[[Memory consistency check completed NO ERRORS]]\n\r");
            }
            Out (7,"[[aa=%ld bb=%ld cc=%ld dd=%ld]]\n",aa,bb,cc,dd);
         }
      #endif
      }
      doexit (EXIT_SUCCESS);
   }


#else // AIP-NL UltraCompressor II

   void UnGetKey (char);

    void cdecl main (int argc, char **argv){
      if (getenv("UC2_WIN")){
         int i,ctr;
	 window (
	    atoi (getenv ("UC2_WIN")),
	    atoi (getenv ("UC2_WIN")+4),
	    atoi (getenv ("UC2_WIN")+8),
	    atoi (getenv ("UC2_WIN")+12)
	 );
         ctr = atoi (getenv ("UC2_WIN")+12) - atoi (getenv ("UC2_WIN")+4);

         for (i=0; i<ctr; i++)
         {
            cprintf ("\n\r");
         }
      }
      if (argv[1][0]=='^')
      {
	 fclose (stdout);
	 *stdout = *fopen (argv[1]+1, "w");
	 for (int i=1;i<argc;i++)
	 {
	    argv[i]=argv[i+1];
	 }
	 _argc--;
	 argc--;
      }
	if (argv[1][0]=='~' && argv[1][1]=='*'){
	    struct ffblk ffblk;
	    int done;
	    done = findfirst ("*.*", &ffblk, 0xF7);
	    while (!done)
	    {
		if ((ffblk.ff_attrib&FA_LABEL)) goto next;
		if ((ffblk.ff_attrib&0x40)) goto next;
		if (strcmp(ffblk.ff_name,".") == 0) goto next;
		if (strcmp(ffblk.ff_name,"..") == 0) goto next;
		fclose(fopen ("u$~chk1","w"));
		return;
next:
		done = findnext (&ffblk);
	    }
	    return;
        }
      if (argv[1][0]=='='){ // UC2-3PI
         strcpy (argv[1],argv[1]+1);
         dosvid=1;
      }
restart:
      if ((argv[1][0]=='?' || (argv[1][0]=='-' && argv[1][1]=='?') || (argv[1][0]=='/' && argv[1][1]=='?')) ||
          (argv[1][0]=='h' || (argv[1][0]=='-' && argv[1][1]=='h') || (argv[1][0]=='/' && argv[1][1]=='h')) ||
          (argv[1][0]=='H' || (argv[1][0]=='-' && argv[1][1]=='H') || (argv[1][0]=='/' && argv[1][1]=='H'))){ // mini help

          if (argc>2){
              UnGetKey (13);

              for (int c=argc-1; c>1; c--){
                 strupr (argv[c]);
                 for (int p=strlen(argv[c])-1; p>=0; p--)
                    UnGetKey (argv[c][p]);
                 if (c!=2) UnGetKey (' ');
              }

              UnGetKey ('S');
              if (atoi(argv[2]) && (argv[2][1]!='.')){
                 UnGetKey ('F');
                 UnGetKey ('8');
              } else {
                 UnGetKey ('1');
              }

              argv[1][0]=0;
              argv[1][1]=0;
              argc=1;
              goto restart;
          }

         printf ("UltraCompressor II, Copyright 1994, Ad Infinitum Programs, all rights reserved\n");
         printf ("\n");
         printf ("SYNTAX: UC command [option(s)] archive-name [files]\n");
         printf ("                                                       UC -!      config\n");
         printf ("COMMANDS: A D E   add / delete / extract               UC -? ...  search docs\n");
         printf ("            L V   list / verbose list                  UC -? 105  error details\n");
         printf ("            P U   damage protect / unprotect           UC -? 1.E  jump para\n");
         printf ("              T   test (& repair)\n");
         printf ("              C   convert archive to .UC2 archive\n");
         printf ("              O   optimize (especially with many versions of files)\n");
         printf ("              R   revise archive comment\n");
         printf ("\n");
         printf ("OPTIONS: (directly after command, or preceded by '-' or '/')\n");
         printf ("       TF TN TT   fast / normal / tight-multimedia\n");
         printf ("              S   include subdirectories\n");
         printf ("              M   move mode\n");
         printf ("              F   force mode (never ask, always yes)\n");
         printf ("            I B   incremental mode (keep versions) / basic mode\n");
         printf ("\n");
         printf (";n  specify version      !DTT=YYYY-MM-DD/HH:MM:SS  dynamic time travel\n");
         printf ("!exclude files   #destination   ##+sourcepath   & concat   @script\n");
	 exit (EXIT_SUCCESS);
      }
      DWin ();
   //   max_dist=125*512U;
      DVver (1);
   #ifdef UCPROX // special testing software
      if (getenv("BETA") && strcmp(getenv("BETA"),"ON")==0){
         beta=1;
      }
      if (getenv("BETA") && strcmp(getenv("BETA"),"on")==0){
         beta=1;
      }
      if (beta && getenv("DEBUG") && strcmp(getenv("DEBUG"),"ON")==0){
         debug=1;
      }
      if (beta && getenv("DEBUG") && strcmp(getenv("DEBUG"),"on")==0){
         debug=1;
      }
      if (beta && getenv("HEAVY") && strcmp(getenv("HEAVY"),"ON")==0){
         heavy=1;
      }
      if (beta && getenv("HEAVY") && strcmp(getenv("HEAVY"),"on")==0){
         heavy=1;
      }
   #endif
      Default();
      Mode (4); // super conservative for errors during setup
   #ifdef UCPROX
      #undef farmalloc
      #undef farfree
      if (beta){
         void far*p = farmalloc (1);
         progmem=(FP_SEG(p)-(long)_psp)*16L;
         farfree (p);
         initial=farcoreleft();
      }
   #endif

   //#define MEMUSE // determine PROG memory usage (even with release version!!)
   #ifdef MEMUSE
      #undef farmalloc
      #undef farfree
      void far*p = farmalloc (1);
      Out (7,"PROG=%ld\n",(FP_SEG(p)-(long)_psp)*16L);
      farfree (p);
   #endif

      _harderr(errHan);
      struct text_info ti;
      gettextinfo(&ti);
      attri = ti.attribute;
      setcbrk (0);
      ctrlbrk (breakNono);
      for (int i=0;i<argc;i++) strupr (argv[i]);
      GetPath(argv); // MUST be called before InitVmem !!!

   #ifdef UCPROX
      if (strcmp (argv[1],"###")==0){
         GKeep();
         spfp = fopen ("UC.EXE","r+b");

         // DEFAULT CONFIGURATION
         Default();
         CONFIG.finstall=0;
         CONFIG.dwMagic = MAGIC;
	 CONFIG.dSerial = 0;

         if (CONFIG.fx86 && (probits()==32)){
            m386=1;
         } else {
            m386=0;
         }

         ctrlbrk (breakHan);
         setcbrk(1);
         InitMem (); // keep it safe, don't use EMS, XMS
         InitVmem();

         CONFIG.dwSoffset = filelength (fileno (spfp));
         fseek (spfp, CONFIG.dwSoffset, SEEK_SET);
         mfp = fopen ("SUPER.DAT","rb");
         Compressor (4,Cread,Cwrite,NOMASTER);
         QTrans();
         fwrite (&CONFIG,1,sizeof (CONF),spfp);
         QTrans();
         fclose (spfp);
         fclose (mfp);
         Out (7,"UC.EXE has been professionalized\n\r");
         doexit (EXIT_SUCCESS);
      }
      if (strcmp (argv[1],"@@@")==0){
         GKeep();
         spfp = fopen ("UC.EXE","r+b");
         FILE *fparch = fopen ("ALL.UC2","rb");

         // DEFAULT CONFIGURATION
         Default();
         CONFIG.finstall= 1;
         CONFIG.fx86=0;
         CONFIG.bVideo=4;
	 CONFIG.fOut=2;

         CONFIG.exesize = filelength (fileno (spfp));
         CONFIG.archsize = filelength (fileno (fparch));

         CONFIG.dwMagic = MAGIC;
         CONFIG.dSerial = 0;

         if (CONFIG.fx86 && (probits()==32)){
            m386=1;
         } else {
            m386=0;
         }

         ctrlbrk (breakHan);
         setcbrk(1);
	 InitMem (); // keep it safe, don't use EMS, XMS
         InitVmem();

         fseek (spfp, CONFIG.exesize, SEEK_SET);

         BYTE *buf = xmalloc (16384,STMP);
         DWORD ctr =  CONFIG.archsize;
         while (ctr){
            WORD trn = 16384;
            if (trn>ctr) trn = (WORD)ctr;
            fread (buf, 1, trn, fparch);
            fwrite (buf, 1, trn, spfp);
            ctr-=trn;
         }

         QTrans();
         fwrite (&CONFIG,1,sizeof (CONF),spfp);
         QTrans();
         fclose (spfp);
	 fclose (mfp);
         fclose (fparch);
         Out (7,"UC.EXE has been converted to the install tool\n\r");
         doexit (EXIT_SUCCESS);
      }
   #endif
      ctrlbrk (breakHan);
      setcbrk(1);

      GetCFG ();
      GKeep();
      if (CONFIG.finstall){
         char path [MAXPATH];
         strcpy (path,"C:\\UC2");
   againo:
         clrscr();
	 Out (7,"\x6\AIP-NL UltraCompressor II revision 2 (tm) INSTALL PROGRAM\n\r\n\r");
         Out (7,"\x7\UltraCompressor II is a powerful datacompressor which allows you\n\r");
         Out (7,"to keep collections of files in an archive.\n\r\n\r");
         Out (7,"Some of its special features are:\n\r");
         Out (7,"   - extremely tight compression\n\r");
         Out (7,"   - fast compression, very fast updates\n\r");
         Out (7,"   - very fast decompression\n\r");
         Out (7,"   - simple user interface (integrated help)\n\r");
         Out (7,"   - reliable (e.g. archives can recover from damaged sectors)\n\r");
         Out (7,"   - ability to store multiple versions of a file in an archive\n\r");
         Out (7,"   - project oriented Version Manager \x6\NEW!\x7\n\r");
         Out (7,"   - transparent conversion of non-UC2 archives\n\r");
         Out (7,"   - advanced filtering on contents, date/time, attributes \x6NEW!\x7\n\r\n\r");
         Out (7,"\x6\Please note this software can only be used, (re)distributed,\n\r");
         Out (7,"etc. according to the included license agreement (license.doc)!\n\r");
         Menu ("\x6\Do you want to install UltraCompressor II in %s\\ ?",path);
         Option ("",'Y',"es");
         Option ("",'N',"o");
         Option ("",'D',"ifferent location");
	 switch (Choice()){
            case 1:
               Out (7,"\n\r");
               if (!TstPath(path)){
                  if (!RCMD (path)) goto fail;
               }
               {
                  install=1;
                  FILE *f1 = fopen ("uc2r2.exe","rb");
                  if (!f1){
                     f1 = fopen (argv[0],"rb");
                     if (!f1)
                        FatalError (105,"cannot find UC2R2.EXE");
                  }
                  char path2[MAXPATH], *path3=path2;
                  strcpy (path2,path);
                  strcat (path2,"\\UC.EXE");
                  igno=1; // for debugging purposes

                  GetPath (&path3);
                  FILE *f2 = fopen (path2,"wb");
                  DWORD ctr = CONFIG.exesize;
                  BYTE *buf = xmalloc (16384, STMP);
                  while (ctr){
                     WORD trn=16384;
                     if (trn>ctr) trn=(WORD)ctr;
                     fread (buf, trn, 1, f1);
                     fwrite (buf, trn, 1, f2);
                     ctr-=trn;
                  }
                  xfree (buf, STMP);
                  fclose (f2);
                  strcpy (path2,path);
                  strcat (path2,"\\ALL.UC2");
                  f2 = fopen (path2,"wb");
                  ctr = CONFIG.archsize;
		  buf = xmalloc (16384, STMP);
                  while (ctr){
                     WORD trn=16384;
                     if (trn>ctr) trn=(WORD)ctr;
                     fread (buf, 1, trn, f1);
                     fwrite (buf, 1, trn, f2);
                     ctr-=trn;
                  }
                  xfree (buf, STMP);
                  fclose (f2);
                  fclose (f1);
                  GetCFG();
                  strcpy ((char *)CONFIG.pbTPATH,pcManPath);
                  strcpy ((char *)CONFIG.pcMan,pcManPath);
                  strcpy ((char *)CONFIG.pcLog,pcManPath);
                  strcpy ((char *)CONFIG.pcBat,pcManPath);
                  UpdateCFG();
                  CONFIG.fx86=0;
                  CONFIG.bVideo=2;
                  CONFIG.fOut=2;
                  SPARE=CONFIG;
                  strcpy (path2,path);
                  strcat (path2,"\\ALL.UC2");
                  argv[1]="X";
                  argv[2]=path2;
                  memmove (path+1, path, strlen(path)+1);
                  path[0]='#';
                  argv[3]=path;
                  gmaxEMS=0; // take NO gamble to ease installation
                  if (stricmp("ON",getenv("EMS"))==0) gmaxEMS=0xFFFF;
                  InitMem (); // Use NO EMS,XMS etc.
                  InitVmem();
                  ComoTerp (4, argv);
                  if (strlen(path)==3) strcat (path,"\\");
                  Out (7,"\n\r\n\rUltraCompressor II revision 2 is now ready for use.\n\r\n\r");
		  Out (7,"To allow UC to be called from everywhere on your system, make\n\r");
                  Out (7,"sure %s is in the PATH statement of your AUTOEXEC.BAT.\n\r",path+1);
                  Out (7,"\n\rTo get HELP on UltraCompressor II, just invoke UC without parameters.\n\r");
                  Delete (path2);
                  ChPath (path+1);
                  GKeep ();
               }
   fail:
               break;
            case 2:
               FatalError (100,"user aborted install program");
               break;
            case 3:
               strcpy (path,"");
               ask ("Enter location for UltraCompressor II : ",path);
               NoLastS (path);
               goto againo;
         }
      } else {
         if (CONFIG.fx86 && (probits()==32)){
            m386=1;
         } else {
            m386=0;
         }

         int cfg=0;
         if (argv[1][0]=='!' || (argv[1][0]=='-' && argv[1][1]=='!') || (argv[1][0]=='/' && argv[1][1]=='!')){
            cfg=1;
            goto noarg;
         }

         if (argc==1){
noarg:
            InitMem (); // don't use any special memory
            // ensure BIOS mode in case of trouble
	    if (CONFIG.bVideo<3)
               Mode(CONFIG.bVideo+2);
            else
               Mode(CONFIG.bVideo);
      //      textattr (attri);
            FSOut (7,"\x7");
            clrscr();
      #ifdef UCPROX // special testing software
            if (beta && getenv("DEBUG") && strcmp(getenv("DEBUG"),"ON")==0){
               Out (7,"[[DEBUG MODE ENABLED]]\n\r");
               debug=1;
            }
            if (beta && getenv("DEBUG") && strcmp(getenv("DEBUG"),"on")==0){
               Out (7,"[[DEBUG MODE ENABLED]]\n\r");
               debug=1;
            }
            if (beta && getenv("HEAVY") && strcmp(getenv("HEAVY"),"ON")==0){
               Out (7,"[[HEAVY MODE ENABLED]]\n\r");
               heavy=1;
            }
            if (beta && getenv("HEAVY") && strcmp(getenv("HEAVY"),"on")==0){
               Out (7,"[[HEAVY MODE ENABLED]]\n\r");
               heavy=1;
            }
            if (strcmp(argv[1],"$$$")==0){
               Tester (argc, argv);
               exit (1);
            }
      #endif
            if (cfg)
               ConfigMenu();
            else
               HelpMenu();
         } else {
            if (getenv("UC2_TMP")){
               strcpy ((char *)CONFIG.pbTPATH, getenv("UC2_TMP"));
	       SPARE=CONFIG;
            }
            if (CONFIG.fEMS){
               gmaxEMS=0xFFFF;
               if (getenv("UC2_MAX_EMS")){
                  gmaxEMS=atoi(getenv("UC2_MAX_EMS"))/16;
                  _useems=1; // 1--> XSPAWN cannot use EMS
               } else {
                  _useems=0; // 0--> XSPAWN can use EMS
               }
            } else {
               _useems=1; // 1--> XSPAWN cannot use EMS
            }
            if (CONFIG.fXMS){
               gmaxXMS=0xFFFF;
               if (getenv("UC2_MAX_XMS")) gmaxXMS=atoi(getenv("UC2_MAX_XMS"))/16;
               gmaxUMB=0xFFFF;
               if (getenv("UC2_MAX_UMB")) gmaxUMB=atoi(getenv("UC2_MAX_UMB"))*64;
               union REGS regs;
               regs.h.ah = 0x30;
               intdos (&regs,&regs);
               // if OS/2 then do not use UMB's !!
   //       if (regs.h.al>9) {
   //          gmaxUMB=0;
   //       }
            }

      //      if (CONFIG.fExt) gmaxI15=0xFFFF; //NEVER use raw extended memory
            if (getenv("UC2_RAW_EXT") && !gmaxXMS && !gmaxEMS && !gmaxUMB){
               gmaxI15 = atoi (getenv("UC2_RAW_EXT"))/16;
            }

            InitMem (); // Use EMS,XMS etc if config allows it.
            if (qdk){ // always use BIOS mode under DesqView
               if (CONFIG.bVideo<3)
		  Mode(CONFIG.bVideo+2);
               else
                  Mode(CONFIG.bVideo);
            } else
               Mode(CONFIG.bVideo);
            InitVmem();
            Logo();
      #ifdef UCPROX // special testing software
            if (beta && getenv("DEBUG") && strcmp(getenv("DEBUG"),"ON")==0){
               Out (7,"[[DEBUG MODE ENABLED]]\n\r");
               debug=1;
            }
	    if (beta && getenv("DEBUG") && strcmp(getenv("DEBUG"),"on")==0){
               Out (7,"[[DEBUG MODE ENABLED]]\n\r");
               debug=1;
            }
            if (beta && getenv("HEAVY") && strcmp(getenv("HEAVY"),"ON")==0){
               Out (7,"[[HEAVY MODE ENABLED]]\n\r");
               heavy=1;
            }
            if (beta && getenv("HEAVY") && strcmp(getenv("HEAVY"),"on")==0){
               Out (7,"[[HEAVY MODE ENABLED]]\n\r");
               heavy=1;
            }
            if (strcmp(argv[1],"$$$")==0){
               Tester (argc, argv);
            } else
      #endif
           {
              ComoTerp (argc, argv);
           }
         }
      #ifdef UCPROX
         if (debug){
            if (claimed){
	       Out (7,"\n\r[[Memory consistency check failed : %ld]]\n\r",claimed);
            } else {
               Out (7,"\n\r[[Memory consistency check completed NO ERRORS]]\n\r");
            }
            Out (7,"[[aa=%ld bb=%ld cc=%ld dd=%ld]]\n",aa,bb,cc,dd);
         }
      #endif
      }
      doexit (EXIT_SUCCESS);
   }

#endif
