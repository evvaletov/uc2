/*

   TREEGEN.C


   This file contains functions for building a huffman tree. This includes
   the generation of the lengths, the codes, and the huffman decode table.


   void far pascal TreeGen(WORD far *pwFreqCounters,   // input
                           WORD wNrSymbols,            // input
                           WORD wMaxLenCode,           // input
                           BYTE far *pbLengths         // output
                          );

      Generate a tree specification for the frequencies given in the first
      argument. The second argument describes the number of elements that
      are in the array of which the first argument is a pointer. The third
      arguments denotes the maximum number of bits that a huffman code may
      be. The output of this function is the fourth argument which contains
      the codes for the characters.


   void far pascal CodeGen(WORD wNrSymbols,            // input
                           BYTE far *pbLengths,        // input
                           WORD far *pwCodes           // output
                          );

      Take a tree specification and generate the huffman tree codes. This
      is done by sorting the lengths of the specification and generating
      the codes using dcode.


   void far pascal DCodeGen(WORD wNrSymbols,           // input
			    BYTE far *pbLengths,       // input
			    WORD far *pwTable,         // output
			    BYTE far *pbTableLengths   // output
			   );

      Take a huffman tree specification and generate the translation
      table which is indexed on the huffman code. If you read a 13 bit
      number n from the decoded file you can use that to get the actual
      character by taking the n-th element of the table. Suppose the
      length of the encoded character was only 5 bits. These 5 bits are
      the most significant bits and the lower 8 are not important because
      for every possibility of the 8 bites (256) we put the character
      representing the 5 bit number in the array. The length is also
      restored meaning that we throw away 5 bits and shift the others to
      the left yielding the next code in front.

*/

#define NO_MAIN                        // Don't compile testmain

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>
#include <alloc.h>
#include <mem.h>
#include "main.h"
#include "video.h"
#include "diverse.h"
#include "test.h"
#include "tree.h"


/*
   Global variables for huffman tree encoding
*/
static int  *iFather;
static WORD wHeapLength;
static WORD wNrSyms;
static WORD *wHeap;


/*
   Prototypes of local functions
*/
// Initialise heap and sort the elements on their frequency
void far pascal BuildHeap(WORD far *pwFreqCount);

// Order the heap starting with a certain element
void far pascal Reheap(WORD wHeapEntry, WORD far *pwFreqCount);

// Build the huffmantree from the current heap
void far pascal BuildCodeTree(WORD far *pwFreqCount);

// Determine the codelengths from the codetree
void far pascal GenCodeLengths(WORD far *pwFreqCount, BYTE far *pbLen);

// Adjust lengths
void far pascal RepairLengths(BYTE far *pbLength, WORD wMaxCodeLen);


/*
   Functions
*/

void far pascal BuildHeap(WORD far *pwFreqCount)
/*
   This function builds a heap from the initial frequency-count data.
*/
{
   register WORD i;

   wHeapLength = 0;

   // Select important symbols
   for(i = 0;i < wNrSyms;i++)
      if (pwFreqCount[i])
         wHeap[++wHeapLength] = i;

   // Sort the symbols of the heap on frequency-count
   for(i = wHeapLength;i > 0;i--)
      Reheap(i,pwFreqCount);
}


void far pascal Reheap(WORD wHeapEntry, WORD far *pwFreqCount)
/*
   This function creates a "legal" heap from the current heap tree
   structure, i.e. make sure that the smallest frequency count index is
   placed on array index 1.
*/
{
   register int  iIndex;
   register int  iFlag = 1;
   register WORD wHeapValue;

   wHeapValue = wHeap[wHeapEntry];

   while ((wHeapEntry <= (wHeapLength >> 1)) && (iFlag))
   {
      iIndex = wHeapEntry << 1;

      if (iIndex < wHeapLength)
	 if (pwFreqCount[wHeap[iIndex]] >= pwFreqCount[wHeap[iIndex+1]])
            iIndex++;

      if (pwFreqCount[wHeapValue] < pwFreqCount[wHeap[iIndex]])
	 iFlag--;
      else
      {
         wHeap[wHeapEntry] = wHeap[iIndex];
         wHeapEntry        = iIndex;
      }
   }

   wHeap[wHeapEntry] = wHeapValue;
}


void far pascal BuildCodeTree(WORD far *pwFreqCount)
/*
   This function builds the compression code tree.
*/
{
   register int  iFIndex;
   register WORD wHeapValue;

   // Build the huffmantree from the sorted heap
   while (wHeapLength > 1)
   {
      wHeapValue = wHeap[1];
      wHeap[1]   = wHeap[wHeapLength--];

      Reheap(1,pwFreqCount);

      iFIndex = wHeapLength + wNrSyms-1;

      pwFreqCount[iFIndex] = pwFreqCount[wHeap[1]] + pwFreqCount[wHeapValue];
      iFather[wHeapValue]  =  iFIndex;
      iFather[wHeap[1]]    = -iFIndex;
      wHeap[1]             =  iFIndex;

      Reheap(1,pwFreqCount);
   }

   iFather[wNrSyms] = 0;
}


void far pascal GenCodeLengths(WORD far *pwFreqCount, BYTE far *pbLen)
/*
   Only generate the code lengths and NOT the codes. Beacuse the lengths
   are stored in the file and not the codes. The codes can be derived from
   the lengths if you use the same tree build function for both encoding and
   decoding of the tree.
*/
{
   register int   i;
   register BYTE  bCurLength;
   register short sParent;

   // for each symbol walk the path to the top of the huffmantree
   for(i = 0;i < wNrSyms;i++)
   {
      if (pwFreqCount[i])
      {
	 bCurLength = 0;
	 sParent = iFather[i];
	 while (sParent)
         {
	    if (sParent < 0)
	       sParent = -sParent;
	    sParent = iFather[sParent];
	    bCurLength++;
	 }
	 pbLen[i] = bCurLength;
      }
      else
	 pbLen[i] = 0;
   }
}


void far pascal RepairLengths(BYTE far *pbLength, WORD wMaxCodeLen)
/*
   Receive the length array and check the total huffman tree. The second
   argument states how large a length may be. If a length exceeds this
   limit the length is set to the maximum and the tree must be repaired.
   The second part of this function checks if the tree is ok. If it isn't
   the tree (actually some lengths) are repaired.
*/
{
#ifdef UCPROX
//   if (debug) DDump ("u$replen.dat",pbLength,wNrSyms);
#endif
   int scl[MAX_CODE_LENGTH+1]; // sorted code lengths, koppen lijsten
   int pp, rr, *qq, qq2;
   LENGTHELEMENT far *p, far *q, far *r;
   LENGTHELEMENT far *Allocated;
   DWORD DCode = 0L;
//   Out (7,"                       [ENTER]\r\n");
   int   k, aantal, j, i, iLen,
	 LengthCount[MAX_CODE_LENGTH+1]; // elementen met lengte x

   // Allocate memory (256+60 elementen)
   Allocated = (LENGTHELEMENT far *)xmalloc(NR_COMBINATIONS*sizeof(LENGTHELEMENT),TMP);


   // Initialise array
   for(i = 0;i <= MAX_CODE_LENGTH;i++) scl[i] = NO_MORE;
   memset(LengthCount,0,MAX_CODE_LENGTH*2+2);

   /*
      Now sort all characters on length by storing them in an array of
      linear lists.
   */
   for(pp = 0, p = Allocated, i = wNrSyms-1;i >= 0;i--) { // alle symbolen
      // p = malloc emulatie
      if (pbLength[i] != 0) {
	 p->ch = i;
	 iLen = pbLength[i];
	 if (iLen > wMaxCodeLen)
	    iLen = pbLength[i] = wMaxCodeLen;
	 LengthCount[iLen]++;
	 p->next = scl[iLen];
	 scl[iLen] = pp++; p++;
      }
   }

//   for (i=0;i<14;i++)
//   {
//	Out (7,"(%d)",LengthCount[i]);
//   }
//   Out (7,"\r\n");

   // Calculate DCode
   for(i = 1;i <= MAX_CODE_LENGTH;i++)
      DCode += (1L << (wMaxCodeLen - i)) * LengthCount[i];

   /*
      Get the first error (-> 1) in the lengths. The range of the error
      given by dcode must be limited from 1L << (wMaxCodeLen-1) to
      (1L << wMaxCodeLen) + (1L << (wMaxCodeLen-1)).
   */
   i = 0;
   while (!(DCode & (1L << i)))
      i++;

   // If an error has occured this loop is entered
   while (i != wMaxCodeLen) {
      j = iLen = wMaxCodeLen - i;

      /* We have a length which is too short and because DCode is
	 calculated using wMaxCodeLen - length DCode is too large.
      */
      while (!LengthCount[--j]);

      // We have found a length
      pp = scl[j];
      p  = Allocated + pp;               // Here p is the element
      scl[j] = p->next;                  // that is lengthened
      q = Allocated + scl[j+1];
      if ((scl[j+1] == NO_MORE) || (q->ch > p->ch)) { // Insert now
	 p->next = scl[j+1];
	 scl[j+1] = pp;
      } else { // Insert the element in the list
	 rr = q->next; r = Allocated + rr;
	 while ((rr != NO_MORE) && (r->ch < p->ch)) {
	    q = r;
	    rr = r->next; r = Allocated + rr;
	 }
	 p->next = rr;                   // Insert p between q and r
	 q->next = pp;
      }
      pbLength[p->ch]++;                 // Adjust length of symbol
      DCode -= 1L << (wMaxCodeLen-j-1);  // Adjust DCode
      LengthCount[j]--;
      LengthCount[j+1]++;

      if (iLen - j > 1) {
	 j = (1 << (iLen - j - 1)) - 1;  // Calculate the space created

	 /* Construct a correct huffmantree by reducing the created space.
	    If there is no space left the tree has no uncovered leafs. The
	    variable j stands for the number of spaces left for the current
	    length.
	 */
	 while (j > 0) {
	    aantal = (LengthCount[iLen] > j ? j : LengthCount[iLen]);
	    for(pp = scl[iLen], k = 0;k < aantal;k++) {
	       p = Allocated + pp;
	       pbLength[p->ch]--;
	       pp = p->next;
	    }
	    if (aantal){
	       pp = scl[iLen];
	       scl[iLen] = p->next;
	       p->next = NO_MORE;
	       qq = &scl[iLen-1];
	       while ((*qq!=NO_MORE)&&(pp!=NO_MORE)){
		  q = Allocated + *qq;
		  if (q->ch > p->ch) {
		     qq2 = *qq; *qq = pp; pp = p->next; p->next = qq2;
		     qq = &(p->next);
		     p = Allocated + pp;
		  } else
		     qq = &(q->next);
	       }
	       if (pp!=NO_MORE) *qq = pp;
	    }

	    LengthCount[iLen-1] += aantal;
	    LengthCount[iLen] -= aantal;
	    DCode += (1L << (wMaxCodeLen-iLen)) * aantal;       // Adjust DCode
	    j -= aantal; j <<= 1; iLen++;
	 }
      }

      /*
	 Because the current (larger) lengths are OK we check the new DCode.
      */
      while (!(DCode & (1L << i)))
	 i++;
   }

   // Free memory
   xfree((BYTE *)Allocated,TMP);

//   Out (7,"                       [LEAVE]\r\n");
}


void far pascal TreeGen(WORD far *pwFreqCounters, WORD wNrSymbols,
			WORD wMaxLenCode, BYTE far *pbLengths)
/*
   This is the function that generates the codelengths from the
   frequencies. This function does NOT generate the huffmancode that is
   necessary to encode the data.

   ATTENTION: The frequency array must be twice as large as the number
      of symbols that it contains. This is for the huffman combination
      codes and frequencies.
*/
{
#ifdef UCPROX
   if (debug){
      Out (7,"[SYM=%d]",wNrSymbols);
//      DDump ("u$freq.dat",(BYTE *)pwFreqCounters,wNrSymbols*2);
   }
#endif
   wNrSyms = wNrSymbols;

//   int ctr=0;
//   for (int i=0; i<wNrSymbols; i++)
//   {
//        if (pwFreqCounters[i]) ctr++;
//   }
//   Out(7,"                    [%d]\r\n",ctr);
//   if (ctr<2)
//   {
//        pwFreqCounters[0]++;
//        pwFreqCounters[1]++;
//   }

   // Allocate memory for sorting frequencies
   wHeap = (WORD *)xmalloc((NR_COMBINATIONS+1)*sizeof(WORD),TMP);

   // Build heap and sort symbols on frequency
   BuildHeap(pwFreqCounters);

   // Here we make sure that there are at least two symbols
   if (wHeapLength > 1) {
      // Allocate memory for huffmantree
      iFather = (int *)xmalloc(2*NR_COMBINATIONS*sizeof(int),TMP);

      // Build huffmantree
      BuildCodeTree(pwFreqCounters);

      // Determine the codelengths of all symbols
      GenCodeLengths(pwFreqCounters,pbLengths);

      // Free memory
      xfree((BYTE *)iFather,TMP); xfree((BYTE*)wHeap,TMP);

      // Adjust lengths if necessary
      RepairLengths(pbLengths,wMaxLenCode);
   } else {
      if (wHeapLength == 1) {
         // Only one symbol
	 WORD wSingle = wHeap[1];
	 memset(pbLengths,0,wNrSyms);
	 pbLengths[wSingle] = 1;
	 pbLengths[(wSingle+1)%wNrSyms] = 1;
      } else {
         // No symbols
	 memset(pbLengths,0,wNrSyms);
	 pbLengths[0] = 1;
	 pbLengths[1] = 1;
      }
      // Free memory
      xfree((BYTE *)wHeap,TMP);
   }
}


void far pascal CodeGen(WORD wNrSymbols, BYTE far *pbLengths,
                        WORD far *pwCodes)
/*
   This function generates the lengths of the symbols. This is done using
   DCode which is initially zero. If a code is generated we add 2^length to
   DCode, to get a new, unique code which we use to generate the next code.

   NOTE: DCode is left-aligned but the resulting codes in pwCodes are
         right-aligned to speed up the output of bits.
*/
{
   int scl[MAX_CODE_LENGTH+1], pp;
   LENGTHELEMENT far *p, far *Allocated;
   DWORD DCode;
   int i, len;

   // Allocate memory
   Allocated = (LENGTHELEMENT far *)xmalloc(NR_COMBINATIONS*sizeof(LENGTHELEMENT),TMP);

   // Initialise array
   for(i = 0;i <= MAX_CODE_LENGTH;i++) scl[i] = NO_MORE;

   // Sort on length and within that on symbol (automatically)
   for(pp = 0, p = Allocated, i = wNrSymbols-1;i >= 0;i--) {
      if (pbLengths[i]) {
	 p->ch = i;
	 len = pbLengths[i];
	 p->next = scl[len];
	 scl[len] = pp++; p++;
      }
   }

   // Now we are ready to generate the codes of the symbols.
   for(DCode = 0L, i = 1;i <= MAX_CODE_LENGTH;i++) {
      for(pp = scl[i];pp != NO_MORE;pp = p->next) {
         p = Allocated + pp;
	 pwCodes[p->ch] = (WORD)(DCode >> (15 - i));
	 DCode += (1L << (15 - i));
      }
   }

   // Free memory
   xfree((BYTE *)Allocated,TMP);
}


void far pascal DCodeGen(WORD wNrSymbols, BYTE far *pbLengths,
			 WORD far *pwTable, BYTE far *pbTableLengths)
/*
   Generate the table which can be used to decode a huffman encoded
   symbol. The table is aligned to the left and has a standard size of
   8192 WORDs. Each entry represents a symbol. Because we want to know
   how many bits are assigned to this code and how many bits must be
   skipped we also fill the length array.

   ATTENTION: Make sure that the arrays pwTable and pbTableLengths have
      a length of 8192 (2^13). This function assumes that this memory is
      allocated. The whole table is always filled with values.
*/
{
   int scl[MAX_CODE_LENGTH+1], pp;
   LENGTHELEMENT far *p, far *Allocated;
   WORD sym;
   int  i, iLen;

   // Allocate memory
   Allocated = (LENGTHELEMENT *)xmalloc(NR_COMBINATIONS*sizeof(LENGTHELEMENT),TMP);

   // Initialise array
   for(i = 0;i <= MAX_CODE_LENGTH;i++) scl[i] = NO_MORE;

   // Sort on length and within that on symbol (automatically)
   for(pp = 0, p = Allocated, i = wNrSymbols-1;i >= 0;i--) {
      if (pbLengths[i]) {
         p->ch = i;
         iLen = pbLengths[i];
         p->next = scl[iLen];
         scl[iLen] = pp++; p++;
      }
   }

   // Now generate the table beginning with the short lengths
   for(i = 1;i <= MAX_CODE_LENGTH;i++) {
      for(pp = scl[i];pp != NO_MORE;pp = p->next) {
	 // Fill part of table
	 p = Allocated + pp; sym = p->ch;
	 /*
	 for(j = 1 << (MAX_CODE_LENGTH - i);j > 0;j--) {
	    *pwTable++ = sym;
	    *pbTableLengths++ = i;
	 }
	 */
	 _SI = 1 << (MAX_CODE_LENGTH - i);
	 asm push ds
	 asm push es
	 asm mov cx, sym
	 asm mov ax, i
	 asm les bx, pwTable
	 asm lds di, pbTableLengths
	 while (_SI>0){
	    asm mov word ptr es:[bx], cx
	    asm mov byte ptr [di], al
	    _BX+=2;
	    _DI++;
	    _SI--;
	 }
	 asm pop es
	 asm pop ds
	 pwTable+= 1<<(MAX_CODE_LENGTH-i);
	 pbTableLengths+= 1<<(MAX_CODE_LENGTH-i);
      }
   }

   // Free memory
   xfree((BYTE *)Allocated,TMP);
}


#ifndef NO_MAIN

/*
   Global Variables
*/

WORD FCount[2*NR_COMBINATIONS];
BYTE Lengths[NR_COMBINATIONS];
WORD Codes[NR_COMBINATIONS];
WORD Table[DECODE_TABLE_SIZE];
BYTE TableLengths[DECODE_TABLE_SIZE];


/*
   The main program
*/
void main(void)
{
   int i;
   WORD nrSym = 284, MaxLen = 13;

   randomize();
   for(i = 0;i < nrSym;i++) FCount[i] = random(2000);

   TreeGen(FCount,nrSym,MaxLen,Lengths);
   printf("Do not worry, you have got the tree\n");
   CodeGen(nrSym,Lengths,Codes);
   printf("Be happy, I generated the codes\n");
   DCodeGen(nrSym,Lengths,Table,TableLengths);
   printf("Is there an end to it or what: I even generated the table\n");
}

#endif
