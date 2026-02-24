// Copyright 1992, all rights reserved, AIP, Nico de Vries
// VIDEO.H

// special (getKey) keyboard codes
#define KEY_UP     -56 // cursor keys
#define KEY_DOWN   -48
#define KEY_LEFT   -53
#define KEY_RIGHT  -51
#define KEY_PUP    -55 // page/home etc keys
#define KEY_PDOWN  -47
#define KEY_HOME   -57
#define KEY_END    -49
#define KEY_ESC     27 // escape key
#define KEY_SPACE   32 // space bar
#define KEY_ENDTER  13 // enter (return) key


#define V_DCOLOR 1 // color DIRECT
#define V_DBW    2 // B&W DIRECT
#define V_BCOLOR 3 // color BIOS
#define V_BBW    4 // B&W BIOS

void Mode (int iMode);
   // determine SOut mode

// Output functions (ala printf); IOut is the most common one for info!
void COut  (BYTE l, char *fmt, ...); // counters (only to screen if no redir or dumpo)
void EOut  (BYTE l, char *fmt, ...); // extra-errors (only to screen if redir)
void FSOut (BYTE l, char *fmt, ...); // menu's, questions (force to the screen)
void Out   (BYTE l, char *fmt, ...); // standard information (redir or screen)
void FROut (BYTE l, char *fnt, ...); // interative summary (only to redir if poss.)

char *X (int len, char *str);
   // make string 'len' long (static return value)

char GetKey ();
   // get key from console

char Echo (char c);
   // echo key (neat) to SOut

char *GetString (int size);
   // get string from SOut

void FatalError (int level, char *fmt, ...);   // fatal error, terminate program imidiately
void Error (int level, char *fmt, ...);   // error, report but continue
void Warning (int level, char *fmt, ...);   // warning, report but continue
void ErrorLog (char *fmt, ...);   // add specifically to the error log
void InternalError (char *file, long line);   // internal error in UC2 program
void Doing (char *fmt, ...); // specify NULL or extra information for error

char *TagName (char *in); // remove AIP: from tagnames

void StartProgress (int hints, int level);
void Hint (void);
void EndProgress (void);
