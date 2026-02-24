// Copyright 1992, all rights reserved, AIP, Nico de Vries
// MENU.H

#include <setjmp.h>

void BrkQ ();  // test for ctrl-C/Break

void ceask (void);

int CeAskOpen (char *name, char *why, int skip); // smart open ask, 1 -> try again

extern jmp_buf jbCritRep;
extern int ceflag;
   // 0=no problems found
   // 1=critical error ocured
extern int cestat;
   // 0=handle local
   // 1=let external handler take care of it
   // 2=during handling of critical error, abort NOW
   // 3=always retry
   // 4=optionat turboterminate mode (like 0 except for fatal)
extern int nobreak;

// Critical section management:
//    ... inocent code
//    CSR;
//    ... code to be used in retry's (e.g. reposition file pointer)
//    CSS;
//    ... critical code
//    CSE;
//    ... inocent code
//
// In case of no retry code CSR+CSS can be replaced by CSB.
// In case the error handle should ALWAYS call retry, replace CSR with CSRA


#define CSR {\
	       int ces=cestat;\
	       ceflag=0;\
	       jmp_buf jb;\
	       nobreak++;\
	       jb[0]=jbCritRep[0];\
	       setjmp(jbCritRep);\
	       cestat=1;\
	       if (ceflag){\
		  ceflag=0;\
		  ceask();{

#define CSS    }}{
#define CSE    } cestat=ces;\
	       jbCritRep[0]=jb[0];\
	       nobreak--;\
	    }

#define CSB CSR CSS

#define CSRA {\
	       int ces=cestat;\
	       ceflag=0;\
	       jmp_buf jb;\
	       nobreak++;\
	       jb [0]=jbCritRep[0];\
	       setjmp(jbCritRep);\
	       cestat=3;\
	       if (ceflag){\
		  ceflag=0;\
		  ceask();{

