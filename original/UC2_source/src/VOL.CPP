// Routines to get, set and remove the volume label

#include <dos.h>
#include <mem.h>
#include <dir.h>
#include <stdio.h>
#include "vol.h"
#include "handle.h"
#include "exetype.h"

#define EXTFCBFLAG 0xFF
#define VOLUME 0x08
#define DOSINT 0x21
#define FCBCLOSE 0x10
#define FCBFINDFIRST 0x11
#define FCBDELETE 0x13
#define FCBCREATE 0x16

// Ensure byte-alignment for the extended fcb structure (pragma is probably
// not really necessary with everything in one struct, but...)
// Note: with -a+ (word alignment) the default struct xfcb DOES NOT WORK!
#pragma option -a-

#ifndef UE2
struct extfcb {
	char flag;
	char resv1[5];
	char attr;
	// The normal fcb starts here
	char drive;
	char name[11];
	char resv2[25];
	// Only the fields needed for these routines are defined.
};
#endif

// Turn word-alignment back on (optional)
#pragma option -a

#pragma argsused
char* getvol(char drive) {
#ifndef UE2
	static char label[12];
	struct extfcb search;
	char tmpdta[128]; // Allocate enough space for the DTA
	char far* dta;
	struct REGPACK r;

	// Save & set the DTA
	dta = getdta();
	setdta((char far*) &tmpdta);

	// Fill the search fcb
	setmem(&search, sizeof(search), 0);
	search.flag = EXTFCBFLAG;
	search.attr = VOLUME;
	search.drive = drive;
	(void) memcpy(search.name, "???????????", 11);

	// Search using findfirst for FCB
	r.r_ax = FCBFINDFIRST << 8;
	r.r_dx = FP_OFF(&search);
	r.r_ds = FP_SEG(&search);
	CSB;
	   intr(DOSINT, &r);
	CSE;

	// Reset dta
	setdta(dta);

	// Test result
	if ( (r.r_ax & 0xFF) == 0 ) {
		// Success, extract volume label and return
		(void) sprintf(label, "%.11s", tmpdta + 8);
		return label;
	}

	for (int i=0;i<11;i++) label[i]=0;

	return label;
#endif
}

#pragma argsused
int setvol(char drive, char* label) {
#ifndef UE2
	struct extfcb create;
	char olddir[MAXDRIVE+MAXDIR];
	char newdir[3];
	struct REGPACK r;
	int result;

	// Resolve default drive
	if ( drive == 0 )
		drive = getdisk() + 1;

	// Save & set the current directory on drive
	CSB;
	   (void) sprintf(olddir, "%c:\\", getdisk() + 'A');
	CSE;
	int f;
	CSB;
	   f = getcurdir(drive, olddir + 3) == -1;
	CSE;
	if ( f )
		return -1;
	(void) sprintf(newdir, "%c:\\", drive - 1 + 'A');
	CSB;
	   f = chdir(newdir) == -1;
	CSE;
	if ( f )
		return -1;

	// Remove the currently present volume label, if any.
	CSB;
	   f = getvol(drive)[0] && rmvol(drive) == -1;
	CSE;
	if ( f ) {
		CSB;
		   (void) chdir(olddir);
		CSE;
		return -1;
	}

	// Fill the create fcb
	setmem(&create, sizeof(create), 0);
	create.flag = EXTFCBFLAG;
	create.attr = VOLUME;
	create.drive = drive;
	(void) sprintf(create.name, "%-11.11s", label);

	// Set volume label using a create or truncate file using fcb call
	r.r_ax = FCBCREATE << 8;
	r.r_dx = FP_OFF(&create);
	r.r_ds = FP_SEG(&create);
	CSB;
	   intr(0x21, &r);
	CSE;
	result = r.r_ax & 0xFF;

	// And close this FCB again (don't know if this is necessary)
	r.r_ax = FCBCLOSE << 8;
	r.r_dx = FP_OFF(&create);
	r.r_ds = FP_SEG(&create);
	CSB;
	   intr(DOSINT, &r);
	CSE;

	// reset default directory for drive;
	CSB;
	   (void) chdir(olddir);
	CSE;

	// Test result of create file operation and return
	return (result == 0) ? 0 : -1;
#else
	return 0;
#endif
}


#pragma argsused
int rmvol(char drive) {
#ifndef UE2
	struct extfcb delet;
	char olddir[MAXPATH];
	char newdir[3];
	struct REGPACK r;

	// Resolve default drive
	if ( drive == 0 )
		drive = getdisk() + 1;

	// Save & set the current directory on drive
	CSB;
	   (void) sprintf(olddir, "%c:\\", getdisk() + 'A');
	CSE;
	int f;
	CSB;
	   f = getcurdir(drive, olddir + 3) == -1;
	CSE;
	if ( f )
		return -1;
	(void) sprintf(newdir, "%c:\\", drive - 1 + 'A');
	CSB;
	   f = chdir(newdir) == -1;
	CSE;
	if ( f )
		return -1;

	// Fill the delet fcb
	setmem(&delet, sizeof(delet), 0);
	delet.flag = EXTFCBFLAG;
	delet.attr = VOLUME;
	delet.drive = drive;
	(void) memcpy(delet.name, "???????????", 11);

	// Delete volume label using a delet file using fcb call
	r.r_ax = FCBDELETE << 8;
	r.r_dx = FP_OFF(&delet);
	r.r_ds = FP_SEG(&delet);
	CSB;
	   intr(DOSINT, &r);
	CSE;

	// reset default directory for drive;
	CSB;
	   (void) chdir(olddir);
	CSE;

	// Test result of delet file operation and return
	return ((r.r_ax & 0xFF) == 0) ? 0 : -1;
#else
	return 0;
#endif
}

