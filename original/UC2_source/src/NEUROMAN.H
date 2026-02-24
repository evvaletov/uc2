// Copyright 1992, all rights reserved, AIP, Nico de Vries
// NEUROMAN.H

struct MASREC {
   MASMETA masmeta;
   COMPRESS compress;
   LOCATION location;
   VPTR vpChain[3];
   VPTR vpNext;
   BYTE bStatus;
   BYTE bCache; // master in cache?
   int ctab[4]; // cache block table
   BYTE cnum;
   DWORD dwVOffset; // offset in file keeping masters
   VPTR vpNextNtx;
   VPTR vpNextKey;
   char szName[20];
};

#define MS_CLR   0  // new MASREC
#define MS_NEW1  1  // new, never used master, single user
#define MS_NEW2  2  // new, never used master, multiple users
#define MS_NEWC  3  // new defined (but not stored) master
#define MS_OLD   4  // old unloaded (registered only) master
#define MS_OLDN  5  // old but needed master (ScanAdd)
#define MS_OLDC  6  // old master, resides in system
#define MS_WRT   7  // master written to new archive
#define MS_NEED  8  // needed master (extract)

void OpenNeuro ();
void CloseNeuro ();

DWORD ToKey (char *pcFileName);
DWORD ToHKey (char *pcFileName);


VPTR LocMacNtx (DWORD dwIndex);
void RegNtxKey (DWORD dwIndex, DWORD dwKey);
VPTR LocMacKey (DWORD dwIndex);
VPTR LocKey (DWORD dwIndex);

void AddAccess (BYTE fInc); // create new, read old, needed masters
void Transfer (BYTE *pbAddress, WORD *pwLen, DWORD dwIndex);

void ListMast (PIPE &p);
void ClearMasRefLen();

void TuneNeuro (WORD wItem, WORD wTotal);

int ValidMaster (DWORD dwIndex);

void AddToNtx (DWORD ntx, VPTR rev, DWORD size); // add revision to Ntx chain
VPTR GetFirstNtx (DWORD ntx, WORD index);        // locate first element of Ntx chain
void CleanNtx (void);                // clean general Ntx chains
