// DIRMAN.H

void Keep (void);               // store current location
void Back (void);               // return to location stored at Keep
void GKeep (void);              // store current location
void GBack (void);              // return to location stored at Keep
void ChPath (char *path);       // ChPath (drive allowed)
int TstPath (char *path);       // check if Path exists (drive allowed)
int MkPath (char *path);        // smart make path (fail VERY possible)
char* MkTmpPath (void);         // create temp dir
void KillTmpPath (void);        // delete temp dir
