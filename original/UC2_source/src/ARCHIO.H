// Copyright 1992, all rights reserved, BITECH, Nico de Vries
// ARCHIO.H

#define MAX_AREA 2 // number of allowed concurrent accessible archives

extern int iArchArea;    // current active archive area

void SetArea (int a);    // define work area
int NewArch ();          // true if current archive is NEW (created)

int SetArchive (char *pcPath, BYTE mode);
   // mode 0=read only 1=update 2=incremental update

void CloseArchiveFile (void);                     // close link to archive
void ARSeek (DWORD dwVolume, DWORD dwOffset);      // multi volume seek
int ARTell (DWORD *pdwVolume, DWORD *pdwOffset);  // multi volume tell
void AWSeek (DWORD dwVolume, DWORD dwOffset);      // multi volume seek
int AWTell (DWORD *pdwVolume, DWORD *pdwOffset);  // multi volume tell

void AWCut (void); // cut end of archive off                                // cut off write file
void AWEnd (void); // seek to end of archive

// (AR is for the ARead function, AW for the AWrite function)

void far pascal AWrite (BYTE *bBuf, WORD wSize);
WORD far pascal ARead (BYTE *bBuf, WORD wSize);

// measure (compressor) output
void ResetOutCtr ();
DWORD GetOutCtr ();

void Old2New (DWORD dwLen); // copy form old to new archive