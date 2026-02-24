// Copyright 1992, all rights reserved, AIP, Nico de Vries
// LLIO.H

// file open error mode
#define MAY  1   // ignore critical errors
#define TRY  2   // ask at critical errors (with skip option)
#define MUST 4   // succeed completely or FatalError

// file open I/O mode
#define RO   8   // read only
#define RW   16  // read & write
#define CR   32  // create

// file open CACHE mode
#define NOC  64  // no caching
#define CRI  128 // cache reads (incremental)
#define CRT  256 // cache reads (total)
#define CWR  512 // cache writes

int Open (char *pcName, int mode);         // open file
void Close (int iHandle);                  // close file

WORD Read (BYTE *bBuf, int iHan, WORD wSiz);  // read data from file
void Write (BYTE *bBuf, int iHan, WORD wSiz); // write data to file

void Seek (int iHandle, DWORD dwPos);          // set file pointer
DWORD Tell (int iHandle);                     // read file pointer

DWORD GetFileSize (int iHan);              // get file size
void SetFileSize (int iHan, DWORD dwSiz);  // set file size

char *TmpFile (char *pcLocat, int pure, char *useext);   // return TMP name, pure->path only

int Exists (char *pcPath);                 // does file exist ?

void Rename (char *pcOld, char *pcNew);    // rename file (Win31 proof)
void Delete (char *pcPath);                // delete file
void CopyFile (char *from, char *onto);    // copy file

void CacheW (int han);   // faster write
void CacheR (int han);   // faster read
void CacheRT (int han);  // maximal speed read

int Han (int iHan);  // convert internal to external handle

void DFlush (char *why); // flush (external) defered write cache