// Copyright 1992, all rights reserved, AIP, Nico de Vries
// ULTRACMP.H

void InvHashC (void);

void TuneComp (WORD wMaxSearch, WORD wMaxLazySearch,
	       WORD wLimitLazy, WORD wLimitSearch);

void pascal far UltraCompressor (DWORD dwMaster, BYTE bDelta);
void pascal far UltraDecompressor (DWORD dwMaster, BYTE bDelta, DWORD len);


   void far pascal TreeGen(WORD far *pwFreqCounters,   // input
                           WORD wNrSymbols,            // input
                           WORD wMaxLenCode,           // input
                           BYTE far *pbLengths         // output
                          );


   void far pascal CodeGen(WORD wNrSymbols,            // input
                           BYTE far *pbLengths,        // input
                           WORD far *pwCodes           // output
                          );


   void far pascal DCodeGen(WORD wNrSymbols,           // input
                            BYTE far *pbLengths,       // input
                            WORD far *pwTable,         // output
                            BYTE far *pbTableLengths   // output
                           );


   void far pascal TreeInit(void);


   void far pascal TreeEnc(BYTE far *pbLengths,    // input
			   BYTE bFlag              // input
                          );                       // output is I/O


   void far pascal TreeDec(BYTE far *pbLengths     // output
                          );                       // input is I/O



