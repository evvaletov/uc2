/*

   TREE.H

*/
/*
   Adjustable definitions
*/
#define MIN_REPEAT_LENGTH                6
#define MAX_CODE_LENGTH                 13
#define NO_MORE                         -1


/*
   General standards
*/
#define NR_CHARS                       256
#define ASCII_HIGH_DIFF                128
#define ASCII_LOW_DIFF                  28


/*
   Standards in this program
*/
#define NR_LENGTHS                      28
#define NR_DISTANCES                    60
#define NR_COMBINATIONS                (NR_CHARS + NR_DISTANCES)
#define NR_SYMBOLS                     (NR_COMBINATIONS + NR_LENGTHS)

#define NR_EXTRA_CODES                   1
#define FIRST_REPEAT_CODE              (MAX_CODE_LENGTH+1)
#define NR_DELTA_CODES                 (MAX_CODE_LENGTH+1)
#define NR_LENGTH_CODES                (NR_DELTA_CODES + NR_EXTRA_CODES)
#define DECODE_TABLE_SIZE              (1 << MAX_CODE_LENGTH)


/*
   Special type for bucketsort
*/
typedef struct {
   WORD ch;
   int next;
}                                      LENGTHELEMENT;


/*
   Prototypes of global functions
*/
void far pascal TreeGen(WORD far *pwFreqCounters, WORD wNrSymbols,
                        WORD wMaxLenCode, BYTE far *pbLengths);
void far pascal CodeGen(WORD wNrSymbols, BYTE far *pbLengths,
                        WORD far *pwCodes);
void far pascal DCodeGen(WORD wNrSymbols, BYTE far *pbLengths,
                         WORD far *pwTable, BYTE far *pbTableLengths);
void far pascal TreeInit(void);
void far pascal TreeEnc(BYTE far *pbLengths, BYTE bFlag);
void far pascal TreeDec(BYTE far *pbLengths);
