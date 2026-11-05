/* Exercise the actual DLL through the documented WCX ABI. */
#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <stdlib.h>
#include <string.h>
#include "wcxhead.h"

typedef void *(__stdcall *open_fn)(tOpenArchiveDataW *);
typedef int (__stdcall *read_fn)(void *, tHeaderDataExW *);
typedef int (__stdcall *process_fn)(void *, int, WCHAR *, WCHAR *);
typedef int (__stdcall *close_fn)(void *);
typedef void (__stdcall *set_fn)(void *, tProcessDataProcW);
typedef BOOL (__stdcall *can_fn)(WCHAR *);
static int __stdcall cancel(WCHAR *name, int bytes) { (void)name; (void)bytes; return 0; }

static int equal(const wchar_t *a, const wchar_t *b)
{
    FILE *x=_wfopen(a,L"rb"), *y=_wfopen(b,L"rb");
    if (!x || !y) { if(x)fclose(x); if(y)fclose(y); return 0; }
    int ok=1;
    for (;;) {
        unsigned char xb[8192],yb[8192];
        size_t xn=fread(xb,1,sizeof xb,x),yn=fread(yb,1,sizeof yb,y);
        if(xn!=yn || memcmp(xb,yb,xn)) {ok=0;break;}
        if(!xn)break;
    }
    if(ferror(x)||ferror(y))ok=0;
    fclose(x);fclose(y);return ok;
}
static void parents(wchar_t *path)
{
    for(wchar_t *p=path+3;*p;p++)if(*p==L'\\'){
        *p=0;CreateDirectoryW(path,NULL);*p=L'\\';
    }
}

int wmain(int argc,wchar_t **argv)
{
    int reject=argc==4 && !wcscmp(argv[1],L"--reject");
    if(argc!=5 && !reject)return 2;
    HMODULE dll=LoadLibraryW(argv[reject ? 2 : 1]);
    if(!dll){fprintf(stderr,"LoadLibrary failed: %lu\n",GetLastError());return 1;}
    open_fn open=(open_fn)GetProcAddress(dll,"OpenArchiveW");
    read_fn read=(read_fn)GetProcAddress(dll,"ReadHeaderExW");
    process_fn process=(process_fn)GetProcAddress(dll,"ProcessFileW");
    close_fn close=(close_fn)GetProcAddress(dll,"CloseArchive");
    set_fn set=(set_fn)GetProcAddress(dll,"SetProcessDataProcW");
    can_fn can=(can_fn)GetProcAddress(dll,"CanYouHandleThisFileW");
    if(!open||!read||!process||!close||!set||!can)return 1;
    if(reject){
        tOpenArchiveDataW ad={0};ad.ArcName=argv[3];ad.OpenMode=PK_OM_EXTRACT;
        void *r=open(&ad);
        if(r){close(r);return 1;}
        if(!ad.OpenResult)return 1;
        FreeLibrary(dll);
        puts("WCX DLL: ambiguous version names rejected");
        return 0;
    }
    if(!can(argv[2]))return 1;
    for(int mode=0;mode<4;mode++){
        tOpenArchiveDataW ad={0};ad.ArcName=argv[2];ad.OpenMode=PK_OM_EXTRACT;
        void *r=open(&ad);if(!r||ad.OpenResult)return 1;
        tHeaderDataExW h;
        int rc,count=0,cancelled=0;
        while((rc=read(r,&h))==0){
            wchar_t dest[32768],original[32768];
            _snwprintf(dest,32768,L"%ls\\%ls",argv[4],h.FileName);
            _snwprintf(original,32768,L"%ls\\%ls",argv[3],h.FileName);
            parents(dest);
            if(mode==0)rc=process(r,PK_SKIP,NULL,NULL);
            else if(mode==1)rc=process(r,PK_TEST,NULL,NULL);
            else if(mode==2){
                rc=process(r,PK_EXTRACT,NULL,dest);
                if(!rc && !(h.FileAttr&FILE_ATTRIBUTE_DIRECTORY) && !equal(dest,original))return 1;
            }else if(!(h.FileAttr&FILE_ATTRIBUTE_DIRECTORY)&&h.UnpSize){
                set(r,cancel);
                rc=process(r,PK_EXTRACT,NULL,dest);
                if(rc!=E_EABORTED||!equal(dest,original))return 1;
                cancelled=1;break;
            }
            if(rc){fprintf(stderr,"ProcessFile mode %d: %d\n",mode,rc);return 1;}
            count++;
        }
        close(r);
        if(mode<3 && (rc!=E_END_ARCHIVE||!count))return 1;
        if(mode==3&&!cancelled)return 1;
    }
    FreeLibrary(dll);
    puts("WCX DLL: recognition, Unicode archive path, listing, skip, test, extract and cancellation passed");
    return 0;
}
