/*

   TREEENC.C


   This file contains the functions to compress and decompress a huffman
   tree defined by lengths. The more important functions are described.



   void far pascal TreeInit(void);

      Initialise the table to convert the lengths to the deltas and
      initialise the array representing the previous tree.



   void far pascal TreeEnc(BYTE far *pbLengths,    // input
                           BYTE bFlag              // input
                          );                       // output is I/O

      Encode the tree from the given lengths.



   void far pascal TreeDec(BYTE far *pbLengths     // output
                          );                       // input is I/O

      Decode the tree and give the lengths back.

*/

#define NO_MAIN                       // Do not compile testmain

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <mem.h>
#include <io.h>
#include "main.h"
#include "bitio.h"
#include "tree.h"
#include "video.h"
#include "diverse.h"


/*
   Globals
*/
static BYTE table[NR_DELTA_CODES][NR_DELTA_CODES];
static BYTE vval[NR_DELTA_CODES][NR_DELTA_CODES];
static BYTE symprev[NR_SYMBOLS];

void BasePrev (void){
   // Define the first previous trees
   for(int i = 0;i < 32;i++) symprev[i] = 9;               // Symbol/Distance
   symprev[10] = 7; symprev[12] = 7; symprev[32] = 7;
   for(i = 33;i < 128;i++) symprev[i] = 8;
   symprev[46] = 7; symprev[58] = 7; symprev[92] = 7;
   for(i = 128;i < 256;i++) symprev[i] = 10;
   for(i = 256;i < 272;i++) symprev[i] = 6;
   for(i = 272;i < 284;i++) symprev[i] = 7;
   for(i = 284;i < 290;i++) symprev[i] = 8;
   for(i = 290;i < 300;i++) symprev[i] = 9;
   for(i = 300;i < 316;i++) symprev[i] = 10;
   for(i = 316;i < 325;i++) symprev[i] = 4;
   for(i = 325;i < 334;i++) symprev[i] = 5;
   for(i = 334;i < 344;i++) symprev[i] = 6;            // Length
}

WORD *DCodeTable;
BYTE *DCodeLengths;


/*
   Prototypes of local functions
*/
// Search for repetitions of codes
int  far pascal InsertExtraCode(BYTE far *p, int PLength);

// Initialise the delta tables
void far pascal InitTables(void);


/*
   Functions
*/

void far pascal InitTables(void)
/*
   Initialise tables which translate the lengths to delta codes. Depending
   on the previous length the new length gets a number from 0..13. This
   number represents the number of bits that the new length differs from
   the previous length.
*/
{
   int prob, i, j, x;

   // First handle row 0 (deltas with previous length set to zero)
   table[0][0] = vval[0][0] = 0;
   for(i = 1;i < NR_DELTA_CODES;i++)
      vval[0][i] = table[0][i] = NR_DELTA_CODES-i;

   // Now initialise other deltas
   for(i = 1;i < NR_DELTA_CODES;i++) {
      prob = 0;
      if (i < 9) x = i-1; else x = NR_DELTA_CODES-i+1;
      for(j = 1;j < x;j++) {             // Fill alternating part of tables
	 table[i][i+j-1] = prob;
	 vval[i][prob] = i+j-1;
	 prob++;
	 table[i][i-j] = prob;
	 vval[i][prob] = i-j;
	 prob++;
      }
      table[i][i-j] = prob+1; vval[i][prob+1] = i-j;
      if (i < 8) {                       // Fill remaining part of tables
	 table[i][0] = NR_DELTA_CODES-1; vval[i][NR_DELTA_CODES-1] = 0;
	 table[i][i+j-1] = prob; vval[i][prob] = i+j-1;
	 for(j = NR_DELTA_CODES-1;j >= i*2-1;j--) {
	    table[i][j] = j-1;
	    vval[i][j-1] = j;
	 }
      } else {                           // Fill remaining part of tables
	 table[i][0] = prob; vval[i][prob] = 0;
	 for(j = 1;j <= i*2-15;j++) {
	    table[i][j] = NR_DELTA_CODES-j;
	    vval[i][NR_DELTA_CODES-j] = j;
	 }
      }
   }
}


void far pascal TreeInit(void)
/*
   Initialise tables and array for previous trees.
*/
{
   // Initialise delta tables
   InitTables();

   BasePrev();
}


int far pascal InsertExtraCode(BYTE far *p, int PLength)
/*
   Go through the array p and if there are more than 6 same lengths in
   a row this is replaced by one of the many lengths, a special code 14
   and the remaining number of same lengths.

   NOTE: The source array is also the destination array. So don't use a
         repeat length of 2 because the minimum length needed is 3.
*/
{
   int i = 0, j, iLen, iIndex = 0;

   while (i < PLength)
   {
      j = i+1;
      iLen = 1;
      while ((j < PLength) && (p[j] == p[i])) {
         iLen++; j++;
      }
      if (iLen > MIN_REPEAT_LENGTH)
      {  // We have found more than X same lengths
	 if (iLen > FIRST_REPEAT_CODE+MIN_REPEAT_LENGTH)
            iLen = FIRST_REPEAT_CODE+MIN_REPEAT_LENGTH;

	 p[iIndex++] = p[i]; p[iIndex++] = FIRST_REPEAT_CODE;
         p[iIndex++] = iLen - MIN_REPEAT_LENGTH;
	 i += iLen;
      } else {
	 p[iIndex++] = p[i];
	 i++;
      }
   }

   // Return the new length of the array
   return iIndex;
}


void far pascal TreeEnc(BYTE far *pbLengths, BYTE bFlag)
/*
   Encode the tree with following steps:
   1. If lengths didn't change write bit 0
   2. Determine whether we should take ASCII, BINARY or intermediate form
   3. Determine the delta from the table
   4. Use special code 14 to shorten length array further
   5. Make huffman tree of lengths that we have to store
   6. Store huffman tree of lengths (only store the lengths)
   7. Encode the original lengths
*/
{
   BYTE *stream, TreeLengths[NR_LENGTH_CODES];
   static WORD TreeFreqCount[2*NR_LENGTH_CODES], TreeCodes[NR_LENGTH_CODES];
   int i, t = 0;

   PUTBITS(1-bFlag,1);
   if (!bFlag)
   {
      BYTE *pbSym = pbLengths;
      int  Count;

//      fprintf(stderr,".");

      // Allocate memory
      stream = (BYTE *)xmalloc(NR_SYMBOLS, TMP);

      // Start with new tree
      memset(TreeFreqCount,0,2*NR_LENGTH_CODES); // (only first half)

      for(i = 0;(i < 32) && !(t & 0x01);i++, pbSym++)
	 if ((i != 9) && (i != 10) && (i != 12) && (i != 13) && (*pbSym))
	    t |= 0x01;
      for(i = 128, pbSym = pbLengths+i;(i < 256) && !(t & 0x02);i++, pbSym++)
	 if (*pbSym)
	    t |= 0x02;
      // Make an array stream containing the 'important' lengths.
      pbSym = stream;
      // 0 .. 31
      if (t & 0x01)
	 for(i = 0;i < 32;i++)
	    *pbSym++ = table[symprev[i]][pbLengths[i]];
      else {
	 pbSym = stream+4;
	 stream[0] = table[symprev[9]][pbLengths[9]];
	 stream[1] = table[symprev[10]][pbLengths[10]];
	 stream[2] = table[symprev[12]][pbLengths[12]];
	 stream[3] = table[symprev[13]][pbLengths[13]];
      }
      // 32 .. 127
      for(i = 32;i < 128;i++)
	 *pbSym++ = table[symprev[i]][pbLengths[i]];
      // 128 .. 256
      if (t & 0x02)
	 for(i = 128;i < 256;i++)
	    *pbSym++ = table[symprev[i]][pbLengths[i]];
      // 256 .. 344
      for(i = 256;i < 344;i++)
	 *pbSym++ = table[symprev[i]][pbLengths[i]];

      // update symprev array
      for(i = 0;i < 344;i++)
	 symprev[i] = pbLengths[i];

      // Add the special code for repeats
      Count = InsertExtraCode(stream,(int)(pbSym - stream));

      // Determine frequency codes in stream
      for(i = 0, pbSym = stream;i < Count;i++, pbSym++)
	 TreeFreqCount[*pbSym]++;

      // generate tree (length and code)
      TreeGen(TreeFreqCount,NR_LENGTH_CODES,7,TreeLengths);
      CodeGen(NR_LENGTH_CODES,TreeLengths,TreeCodes);

      // Write out control bits and tree length of tree info
      PUTBITS(t,2);
      for(i = 0;i < NR_LENGTH_CODES;i++)
	 PUTBITS(TreeLengths[i],3);

      // write tree
      for(i = 0, pbSym = stream;i < Count;i++, pbSym++)
	 PUTBITS(TreeCodes[*pbSym],TreeLengths[*pbSym]);
      xfree (stream, TMP);
   } else {
      BasePrev();
      for(i = 0;i < NR_SYMBOLS;i++) pbLengths[i] = symprev[i];
   }
}


void far pascal TreeDec(BYTE far *pbLengths)
/*
   Decode the tree that is at the current position in the file:
   1. If first bit 0 then the tree stays the same
   2. Read two bits specifying the gaps in the tree (ASCII .. BINARY)
   3. Read the tree describing the length encoding
   4. Read the codes and remove the special code
   5. Restore the lengths by removing delta effect
   6. Depending on gaps in tree fill length array
*/
{
   int i, t = 0;
   BYTE *stream, TreeLengths[NR_LENGTH_CODES];

   t = GETBITS(1);
   if (t)
   {  // specified tree
      int  Count = 0, Max = NR_SYMBOLS;
      BYTE *pbSym, val = 0;
      WORD Snoop;

      // Allocate memory
      stream = (BYTE *)xmalloc(NR_SYMBOLS, TMP);

      // How many lengths stored and how many gaps are there
      t = GETBITS(2);
      if (!(t & 0x01))
	 Max -= ASCII_LOW_DIFF;
      if (!(t & 0x02))
	 Max -= ASCII_HIGH_DIFF;

      // Read length tree
      for(i = 0;i < NR_LENGTH_CODES;i++) TreeLengths[i] = (BYTE)GETBITS(3);

      // Make a Decodetable
      DCodeGen(NR_LENGTH_CODES,TreeLengths,DCodeTable,DCodeLengths);

      // Decode a bit code with the table and remove special code
      while (Count < Max) {
	 Snoop = SNOOP13();
	 SKIP(DCodeLengths[Snoop]);
	 stream[Count] = DCodeTable[Snoop];
	 if (stream[Count] == FIRST_REPEAT_CODE) {
	    Snoop = SNOOP13();
	    SKIP(DCodeLengths[Snoop]);
	    for(i = DCodeTable[Snoop] + MIN_REPEAT_LENGTH - 1;i > 0;i--)
	       stream[Count++] = val;
	 } else {
	    val = DCodeTable[Snoop];
	    Count++;
	 }
      }

      // Init variables
      setmem(pbLengths,NR_SYMBOLS,0);
      pbSym = stream;

      // Now consider the bits of t that define the empty blocks
      if (t & 0x01)
	 for(i = 0;i < 32;i++) pbLengths[i] = vval[symprev[i]][*pbSym++];
      else {
	 pbLengths[9]  = vval[symprev[9]][*pbSym++];
	 pbLengths[10] = vval[symprev[10]][*pbSym++];
	 pbLengths[12] = vval[symprev[12]][*pbSym++];
	 pbLengths[13] = vval[symprev[13]][*pbSym++];
      }
      for(i = 32;i < 128;i++) pbLengths[i] = vval[symprev[i]][*pbSym++];

      // Are the lengths of the symbols 128..255 stored ?
      if (t & 0x02)
	 for(i = 128;i < 256;i++) pbLengths[i] = vval[symprev[i]][*pbSym++];

      for(i = 256;i < 344;i++) pbLengths[i] = vval[symprev[i]][*pbSym++];

      // Determine real length from delta length
      for(i = 0;i < NR_SYMBOLS;i++) symprev[i] = pbLengths[i];
      xfree (stream, TMP);
   } else {
      BasePrev();
      for(i = 0;i < NR_SYMBOLS;i++) pbLengths[i] = symprev[i];
   }
}


#ifndef NO_MAIN

/*
   Global variables
*/
BYTE symprev[NR_SYMBOLS];

BYTE table[NR_DELTA_CODES][NR_DELTA_CODES];
BYTE vval[NR_DELTA_CODES][NR_DELTA_CODES];

BYTE Lengths[NR_SYMBOLS];
//BYTE stream[NR_SYMBOLS];
BYTE TreeLengths[NR_LENGTH_CODES];

WORD FCount[NR_COMBINATIONS*2];
WORD TreeFreqCount[NR_LENGTH_CODES*2], TreeCodes[NR_LENGTH_CODES];


FILE *fp;


void main(int argc, char *argv[])
{
//   int i;
   WORD nrSym = NR_SYMBOLS; // , MaxLen = MAX_CODE_LENGTH;
   FILE *TreeIn, *TreeOut;
   long dum;

   if (argc != 4)
   {
      fprintf(stderr,"Usage: %s infile comptree outfile\n",argv[0]);
      doexit(EXIT_SUCCESS);
   }

   TreeInit();

   TreeIn = fopen(argv[1],"rb");
   fread(&dum,4,1,TreeIn);
   fread(Lengths,1,nrSym,TreeIn);
   fclose(TreeIn);

/*
   randomize();
   for(i = 0;i < nrSym;i++)
      FCount[i] = random(600) + i;

   if (random(600) < 300)
      for(i = 128;i < 256;i++)
         FCount[i] = 0;

   if (random(600) < 300)
      for(i = 0;(i < 32);i++)
         if ((i != 9) && (i != 10) && (i != 12) && (i != 13))
            FCount[i] = 0;

   TreeGen(FCount,NR_COMBINATIONS,MaxLen,Lengths);
   TreeGen(FCount,NR_LENGTHS,MaxLen,Lengths+NR_COMBINATIONS);
*/

   fp = fopen(argv[2],"wb");
   InitBitsOut();
   TreeEnc(Lengths,0);
   FlushBitsOut();
   fclose(fp);

   TreeInit();

   // Decoding
   DCodeTable = (WORD *)malloc(2*DECODE_TABLE_SIZE);
   DCodeLengths = (BYTE *)malloc(DECODE_TABLE_SIZE);
   fp = fopen(argv[2],"rb");
   InitBitsIn();
   TreeDec(Lengths);
   fclose(fp);

   TreeOut = fopen(argv[3],"wb");
   fwrite(&dum,4,1,TreeOut);
   fwrite(Lengths,1,nrSym,TreeOut);
   fclose(TreeOut);
   ExitBitsIn();
}

#endif
