/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#endif
#include <uc2/uc2_path.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif
static uint32_t u32(const unsigned char *p)
{ return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
FILE *uc2_path_regular(const char *path)
{
#ifdef _WIN32
    wchar_t wide[32768];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, 32768)) return NULL;
    DWORD attr=GetFileAttributesW(wide);
    if (attr==INVALID_FILE_ATTRIBUTES || attr & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)) return NULL;
    return _wfopen(wide,L"rb");
#else
    struct stat s;
    if (lstat(path,&s)!=0 || !S_ISREG(s.st_mode)) return NULL;
    int flags=O_RDONLY;
#ifdef O_NOFOLLOW
    flags|=O_NOFOLLOW;
#endif
#ifdef O_NONBLOCK
    flags|=O_NONBLOCK;
#endif
    int fd=open(path,flags);
    if (fd<0) return NULL;
    struct stat actual;
    if (fstat(fd,&actual)!=0 || !S_ISREG(actual.st_mode) || actual.st_dev!=s.st_dev || actual.st_ino!=s.st_ino) {
        close(fd); return NULL;
    }
    FILE *f=fdopen(fd,"rb");
    if (!f) close(fd);
    return f;
#endif
}
extern FILE *uc2_open_sea(const char *, char *, size_t, int *);
FILE *uc2_open_path(const char *path, char *error, size_t cap)
{
    FILE *input=NULL, *output=NULL;
    char *part=NULL;
    const char *message="cannot open regular archive file";
    unsigned char header[14], buffer[65536];
    unsigned wanted=1;
    input=uc2_path_regular(path);
    if (!input) goto bad;
    size_t n=fread(header,1,sizeof header,input);
    if (n<4 || u32(header)!=0x0f021445u) {
        const char *suffix=strrchr(path,'.');
        int numbered=suffix && strlen(suffix)==4 && suffix[1]>='0' && suffix[1]<='9' &&
            suffix[2]>='0' && suffix[2]<='9' && suffix[3]>='0' && suffix[3]<='9';
        if ((n>=2 && !memcmp(header,"MZ",2)) || (numbered && (n<4 || u32(header)!=0x1a324355u))) {
            int recognized=0;
            FILE *sea=uc2_open_sea(path,error,cap,&recognized);
            if (sea || recognized) { fclose(input);return sea; }
        }
        if (ferror(input) || fseek(input,0,SEEK_SET)!=0) goto bad;
        return input;
    }
    message="invalid SAS volume header";
    if (n!=sizeof header || !u32(header+4)) goto bad;
    uint32_t total=u32(header+4);
    const char *dot=strrchr(path,'.');
    message="SAS input must have a .pNN or .PNN volume suffix";
    if (!dot || (dot[1]!='p' && dot[1]!='P') || !dot[2]) goto bad;
    unsigned selected=0;
    for (const char *s=dot+2; *s; ++s) {
        if (*s<'0' || *s>'9' || selected>99) goto bad;
        selected=selected*10+(unsigned)(*s-'0');
    }
    if (!selected || selected>999 || strlen(dot+2)!=(selected<100 ? 2u : 3u) ||
        (unsigned)(header[12]|header[13]<<8)!=selected) goto bad;
    size_t prefix=(size_t)(dot-path)+2;
    part=malloc(prefix+8);
    if (!part) { message="cannot allocate SAS path"; goto bad; }
    memcpy(part,path,prefix);
    fclose(input); input=NULL;
    output=tmpfile();
    if (!output) { message="cannot create SAS temporary spool"; goto bad; }
    uint32_t copied=0;
    int selected_seen=0;
    for (wanted=1; copied<total; ++wanted) {
        if (wanted>999) { message="SAS set exceeds 999 parts"; goto bad; }
        snprintf(part+prefix,8,"%02u",wanted);
        input=uc2_path_regular(part);
        if (!input) { message="missing or non-regular SAS volume"; goto bad; }
        message="truncated, out-of-order or mixed SAS header";
        if (fread(header,1,14,input)!=14 || u32(header)!=0x0f021445u ||
            u32(header+4)!=total || (unsigned)(header[12]|header[13]<<8)!=wanted) goto bad;
        uint32_t bytes=u32(header+8);
        if (bytes<=14 || bytes-14>total-copied) goto bad;
        bytes-=14;
        while (bytes) {
            unsigned take=bytes>sizeof buffer ? (unsigned)sizeof buffer : bytes;
            if (fread(buffer,1,take,input)!=take) { message="truncated SAS payload"; goto bad; }
            if (fwrite(buffer,1,take,output)!=take) { message="SAS temporary spool write failed"; goto bad; }
            bytes-=take; copied+=take;
        }
        if (fgetc(input)!=EOF || ferror(input)) { message="SAS volume length differs from header"; goto bad; }
        fclose(input); input=NULL;
        if (wanted==selected) selected_seen=1;
    }
    if (!selected_seen) { message="selected SAS part lies beyond the complete set"; goto bad; }
    if (fflush(output)!=0 || fseek(output,0,SEEK_SET)!=0) { message="SAS spool flush/seek failed"; goto bad; }
    free(part);
    return output;
bad:
    if (error && cap) {
        if (part) snprintf(error,cap,"%s: part %u",message,wanted);
        else snprintf(error,cap,"%s",message);
    }
    if (input) fclose(input);
    if (output) fclose(output);
    free(part);
    return NULL;
}
