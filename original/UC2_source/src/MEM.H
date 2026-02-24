/*
   MEM.H (C) 1992, all rights reserved, Nico de Vries. Unlimited
   use is granted to Jean-loup Gailly and Robert Jung.
   Partial (C) 1993 Jan-Pieter Cornet.
*/

// INIT & EXIT
void InitMem (void);                    // Init memory manager.
void ExitMem (void);                    // Exit memory manager. (called implicitly)

// 'extra' memory (UBM's, EMS emulated UMB)
void far* exmalloc (long);              // Allocate "extra" memory.
void exfree (void far*);                // Release it.
int extest (void far*);                 // Test if "extra" memory.
int coreleft4 (void);                   // Number of free 4k "extra" memory blocks.

void donate16(void far*);               // Donate a 16k base memory block.
// donate16 should be called BEFORE calls to other mem16 functions!

int malloc16 (char);                    // Allocate 16k cache.
void free16 (char, int);                // Release it.
int coreleft16 (char);                  // Number of free 16k cache blocks in a group.
void to16 (int, unsigned, void far*, int);              // to cache
void from16 (void far*, int, unsigned, int);            // from cache


// Global variables that indicate maximum EMS, XMS, INT15 usage (in
// 16K blocks) and UMB usage (in paragraphs). Default value==0 ("do not use")
// Set to 0xFFFF to allow any usage. Notice these values should be
// specified BEFORE InitMem is called.
extern unsigned gmaxEMS;
extern unsigned gmaxXMS;
extern unsigned gmaxI15;
extern unsigned gmaxUMB;

// Return error codes for EMS, XMS and generic extended memory faults.
#define EMSFAIL 1
#define XMSFAIL 1
#define I15FAIL 1

// MEMORY GROUPS (current UltraCompressor settings)
//
// 1 supermaster storage
// 2 most recently used hash scheme
// 3 master storage
// 4 disk cache
// 5 vmem support
// 6 most recently use master cache
// 7 booster I/O temp buffer
