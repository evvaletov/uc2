// Copyright 1992, all rights reserved, AIP, Nico de Vries
// SUPERMAN.H

/*** General FILE format. ***

Total (multivolume) archive format:
   XHEAD [BAREBLOCKS...] CDIR [SEAL] [CRYPT] [BANNER]

File format (single volume):
   FHEAD 'part of archive' [DAMAGE_RECOVERY_INFO]

BAREBLOCK ::=
   none     // directory
   'data'   // file
   'data'   // master

CDIR ::=
   COMPRESS '[BASINF...]'

BASINF ::=
   OHEAD - OSMETA DIRMETA
         - OSMETA FILEMETA COMPRESS LOCATION
         - MASMETA COMPRESS
         - end of list

SEAL ::=
   ... // documented in USAFE/USEAL software

CRYPT ::=
   ... // documented in UCRYPT software

The order of the BASINF objects is NOT random. There are many possible orders
but some things have to be correct:

 - first the directory then its contents
 - revisions in reverse order (first n then n-1 until 0) only !!
 - no missing revisions (always 5,4,3,2,1 etc never 5,2,1)
   (notice we are talking EXTERNAL revision numbers here)

But notice:

 - masters, directories and files can bee freely intermixed
 - neuro models can be put both before and after usage
 - the order in the archive does not have to resemble the order in the CDIR

*/

#define UC2_LABEL 0x1A324355L  // "UC2^Z" (notice Intel ordering)

// location of start of object header
struct LOCATION {
   DWORD dwVolume;
   DWORD dwOffset;
};
inline LOCATION locat (DWORD vol, DWORD off){
   LOCATION loc;
   loc.dwVolume = vol;
   loc.dwOffset = off;
   return loc;
}
#define LNULL locat(0,0)

// file header
struct FHEAD {
   DWORD dwHead;            // UC2^Z (^Z = MS-DOS eof char)
   DWORD dwComponentLength;  // length of component contents
   DWORD dwComponentLength2; // length of component contents
   BYTE fDamageProtected;   // component damage protected
};

// global object header
struct XHEAD {
   // (start of basic objects is imediate)
   LOCATION locCdir;     // start of CDIR
   WORD wFletch;         // fletcher checksum of CDIR
   BYTE fBusy;           // 0=OK 1=BUSY
   WORD wVersionMadeBy;           // e.g. 200 means 2.00
   WORD wVersionNeededToExtract;
   BYTE dummy;
};
// BUSY indicates an archive update is in progress (e.g. reset during upd)
// STREAM GEN determines a single pass write was used (e.g. tape streamer)

struct XTAIL {
   BYTE bBeta;           // archive made with beta test version
                         // 0=no beta, 1..255 is specific beta version
   BYTE bLock;           // locked archive
                         // 0   -> no lock at all
                         // 1   -> add always I, delete allowed
                         // 2   -> add always I, no delete allowed
                         // 3   -> no changes allowed at all
                         // 128 -> private lock, even ULOCK won't work
   DWORD serial;         // special serial number (0 = none)
   BYTE pbLabel[11];     // (MS-DOS) volume label
};


// compressed information block meta information
struct COMPRESS {
   DWORD dwCompressedLength;
   WORD wMethod;                 // compression method
   DWORD dwMasterPrefix;
};

#define SUPERMASTER  0           // master is supermaster
#define NOMASTER     1           // no master defined at all
#define FIRSTMASTER  2           // lowest master index

// basic object header
struct OHEAD {
   BYTE bType;    // basic object type
};
#define BO_DIR   1  // directory
#define BO_FILE  2  // file
#define BO_MAST  3  // master
#define BO_EOL   4  // end of "list"

// OS meta information
struct OSMETA {
   DWORD dwParent;   // parent directory index
   BYTE bAttrib;     // file attributes (MSDOS)
   WORD wTime;       // time last modified (MSDOS)
   WORD wDate;       // date last modified (MSDOS)
   BYTE pbName[11];  // MS-DOS compatible name (also seen EXTMETA)
   BYTE bHidden;     // 0 = plain visual, 1 = completely hidden
   BYTE tag;         // tags?
};

#define TAG_SIZE 16

struct EXTMETA {
   char tag[TAG_SIZE]; // zero terminated TAG string
   DWORD size;  // size of object
   BYTE next;   // more tags?
};

// Possible tags:
#define TAG_OS2EA   "AIP:OS/2 2.x EA"
#define TAG_VERLBL  "AIP:Version LBL"
#define TAG_COMMENT "AIP:Comment"

// extended basic object meta information (files only)
struct FILEMETA {
//   LOCATION loc;      // where is file in archive
   DWORD dwLength;    // file length
   WORD wFletch;      // fletcher checksum of raw data
};

// extended basic object meta information (directories only)
struct DIRMETA {
   DWORD dwIndex;     // directory index for referencing
};

// master meta information
struct MASMETA {
   DWORD dwIndex;     // master index for referencing
   DWORD dwKey;       // master hash key
   DWORD dwRefLen;    // total size of refering data
   DWORD dwRefCtr;    // total number of refering files
   WORD wLength;      // master length
   WORD wFletch;      // fletcher checksum of raw data
};

// CDIR meta information
struct CDIRMETA {
   DWORD dwMasters;   // number of masters
   DWORD dwObjects;   // number of objects
};

/*** SUPERMAN internal records ***/

struct EXTDAT { // data item
   VPTR next;
   BYTE dat[1000];
};

struct EXTNODE { // extended data block
   EXTMETA extmeta;
   VPTR extdat; // list of data items
   VPTR next;
};

struct DIRNODE {
// links
   VPTR vpLeft, vpRight; // brothers
   VPTR vpFiles;         // files in this directory
   VPTR vpChilds;        // subdirectories
   VPTR vpParent;        // parent directory
// currefs
   VPTR vpCurDir;        // current processed directory
   VPTR vpCurFile;       // current processed file
// info
   OSMETA osmeta;
   VPTR extnode;
   DIRMETA dirmeta;
   BYTE bStatus;
};

#define DST_OLD   1 // directory in "old" archive
#define DST_NEW   2 // directory detected at last scan (old or real new)
#define DST_DEL   3 // directory (and ALL its contents) deleted
#define DST_MARK  4 // directory marked for processing (not for delete!)

struct FILENAMENODE {
// links
   VPTR vpLeft, vpRight; // brothers
   VPTR vpRevs;          // list of all revisions
};

struct REVNODE {
// links
   VPTR vpOlder; // older revision
   VPTR vpDLink; // link to external position (drive, directory etc)
   VPTR vpMas;   // link to master record
   VPTR vpDir;   // link to 'current' directory
   VPTR vpFNN;   // link to file-name-node
   VPTR vpSis;   // link to next node of same file type
// info
   OSMETA osmeta;
   VPTR extnode;
   FILEMETA filemeta;
   COMPRESS compress;
   LOCATION location;
// flags
   BYTE bStatus;
// special
   BYTE bSpecial; // totaly in master
   WORD wLen;    // length of file
   WORD wOfs;    // offset of file
   WORD wFletch; // checksum of file
};

#define FST_OLD   2 // file in 'old' archive
#define FST_SPEC  3 // file on disk with special name
#define FST_DISK  4 // file on disk (plain, see vpDLink)
#define FST_NEW   5 // file in 'new' archive (after 1,2,3)
#define FST_MARK  7 // file marked (extract, list, whatever)
#define FST_DEL   8 // file has been deleted (skip always)

// archive access
void ReadArchive (char *pcPath);      // read archive (extract)
void UpdateArchive (char *pcPath);    // update archive (I- add, delete)
void CreateArchive (char *pcPath);    // update archive (I- add, delete)
void IncUpdateArchive (char *pcPath); // access archive (I add)
void CloseArchive ();                 // close archive (all)

// current location management
void TCD (VPTR dir);      // change directory (VNULL root)
void TUP ();              // go up single directory
VPTR TWD ();              // get current directory
int  TDeep ();            // get current depth (0=root)

// create objects
VPTR TMLD (BYTE *pbName); // locate or create directory
VPTR TMF (BYTE *pbName);  // add filenode returns revision 0 (newest)

// walk trough dirs, files etc
VPTR TFirstDir ();         // get first subdir (VNULL if none)
VPTR TNextDir ();          // get next subdir (VNULL if none)
VPTR TFirstFileNN ();      // get first fileNN (VNULL if none)
VPTR TNextFileNN ();       // get next fileNN (VNULL if none)
DWORD TRevs ();            // number of revisions
VPTR TRev (DWORD dwIndex); // access revision (0..n-1)
   // The first&next stuff is memorized. Leaving a dir and returning
   // to it returns you to the same walk status!
DWORD TRevNo (VPTR rev);   // get index number of revision record


char * GetPath (VPTR here, int levels);
   // determine path of current dir (n levels higher=root) e.g. "\aa\bb"

void ScanAdd (VPTR dir, VPTR Mpath, int parents);
   // scan harddisk and add to tree

void MarkUp (char *rdir, VPTR dir, VPTR Mpath, int parents);
   // scan internal tree and mark items

typedef void (*ToToFun)(VPTR dir, VPTR rev, DWORD dwRevno);
void ToToWalk (ToToFun ttf);
void ToToWalk (ToToFun ttf, DWORD dwIndex, WORD idx);

char * Full (VPTR rev);

void AddFiles ();
void CopyFiles ();
void ExtractFiles ();
