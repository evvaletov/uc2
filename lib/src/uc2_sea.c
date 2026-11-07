/* SPDX-License-Identifier: GPL-3.0-or-later */
/* UC2SEA 2.3 data records. Executable bytes are never executed. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#endif
#include <uc2/uc2_path.h>
#include <uc2/uc2_writer.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>
#define SEA_LIMIT (512u*1024u*1024u)
#define SEA_FILES 100000
extern FILE *uc2_path_regular(const char *);
static char *extension(char *path)
{
    char *base=path;
    for (char *p=path; *p; ++p) if (*p=='/' || *p=='\\') base=p+1;
    return strrchr(base,'.');
}
static unsigned r16(const unsigned char *p) { return p[0]|(unsigned)p[1]<<8; }
static uint32_t r32(const unsigned char *p) { return r16(p)|(uint32_t)r16(p+2)<<16; }
static int seek(FILE *f,unsigned offset)
{
#ifdef _WIN32
    return _fseeki64(f,offset,SEEK_SET);
#elif defined(__DJGPP__)
    return fseek(f,(long)offset,SEEK_SET);
#else
    return fseeko(f,(off_t)offset,SEEK_SET);
#endif
}
static int read_at(void *ctx,unsigned offset,void *out,unsigned len)
{
    if (seek(ctx,offset)!=0) return -1;
    size_t n=fread(out,1,len,ctx); return ferror(ctx) ? -1 : (int)n;
}
static void *allocate(void *ctx,unsigned n) { (void)ctx; return n<8u*1024u*1024u ? malloc(n) : NULL; }
static void release(void *ctx,void *p) { (void)ctx;free(p); }
static int output(void *ctx,const void *p,unsigned n) { return fwrite(p,1,n,ctx)==n ? 0 : -1; }
struct payload { FILE *file; unsigned left, position, checksum; };
static int member(void *ctx,void *buf,unsigned n)
{
    struct payload *p=ctx;
    if (n>p->left) n=p->left;
    unsigned got=(unsigned)fread(buf,1,n,p->file);
    const unsigned char *b=buf;
    for (unsigned i=0; i<got; ++i) p->checksum^=(unsigned)b[i]<<((p->position++&1)*8);
    p->left-=got;
    return ferror(p->file) ? -1 : (int)got;
}
/* CP850 is the original SEA name encoding. */
static const unsigned short cp850[128] = {

    0x00c7,0x00fc,0x00e9,0x00e2,0x00e4,0x00e0,0x00e5,0x00e7,
    0x00ea,0x00eb,0x00e8,0x00ef,0x00ee,0x00ec,0x00c4,0x00c5,
    0x00c9,0x00e6,0x00c6,0x00f4,0x00f6,0x00f2,0x00fb,0x00f9,
    0x00ff,0x00d6,0x00dc,0x00f8,0x00a3,0x00d8,0x00d7,0x0192,
    0x00e1,0x00ed,0x00f3,0x00fa,0x00f1,0x00d1,0x00aa,0x00ba,
    0x00bf,0x00ae,0x00ac,0x00bd,0x00bc,0x00a1,0x00ab,0x00bb,
    0x2591,0x2592,0x2593,0x2502,0x2524,0x00c1,0x00c2,0x00c0,
    0x00a9,0x2563,0x2551,0x2557,0x255d,0x00a2,0x00a5,0x2510,
    0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x00e3,0x00c3,
    0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x00a4,
    0x00f0,0x00d0,0x00ca,0x00cb,0x00c8,0x0131,0x00cd,0x00ce,
    0x00cf,0x2518,0x250c,0x2588,0x2584,0x00a6,0x00cc,0x2580,
    0x00d3,0x00df,0x00d4,0x00d2,0x00f5,0x00d5,0x00b5,0x00fe,
    0x00de,0x00da,0x00db,0x00d9,0x00fd,0x00dd,0x00af,0x00b4,
    0x00ad,0x00b1,0x2017,0x00be,0x00b6,0x00a7,0x00f7,0x00b8,
    0x00b0,0x00a8,0x00b7,0x00b9,0x00b3,0x00b2,0x25a0,0x00a0,
};
struct directory { char *path; unsigned id; };
static int path_utf8(char out[768],const unsigned char *src,unsigned n)
{
    char *p=out;
    for (unsigned i=0; i<n; ++i) {
        unsigned u=src[i];
        if (u<32 || u==127 || u==':') return -1;
        if (u=='\\') u='/';
        if (u>=128) u=cp850[u-128];
        if (u<128) *p++=(char)u;
        else {
            if (u>=2048) { *p++=(char)(0xe0|(u>>12)); *p++=(char)(0x80|((u>>6)&63)); }
            else *p++=(char)(0xc0|(u>>6));
            *p++=(char)(0x80|(u&63));
        }
    }
    *p=0; return 0;
}
static int add_path(uc2_writer *writer,struct directory *dirs,unsigned *count,char *path,
                    unsigned date,unsigned attr,unsigned size,struct payload *payload)
{
    unsigned parent=0, depth=0; char *leaf=path;
    for (char *p=path;; ++p) {
        if (*p && *p!='/') continue;
        char saved=*p;*p=0;
        if (!*leaf || !strcmp(leaf,".") || !strcmp(leaf,"..") || ++depth>64) return -1;
        if (!saved) return uc2_writer_add_file(writer,parent,leaf,date,attr,size,member,payload);
        unsigned j;
        for (j=0; j<*count && strcmp(dirs[j].path,path); ++j) {}
        if (j==*count) {
            if (*count==SEA_FILES) return -1;
            if (uc2_writer_add_directory(writer,parent,leaf,0,0,&dirs[j].id)<0) return -1;
            dirs[j].path=malloc(strlen(path)+1);
            if (!dirs[j].path) return -1;
            strcpy(dirs[j].path,path); ++*count;
        }
        parent=dirs[j].id;*p=saved;leaf=p+1;
    }
}
/* Called only for an MZ input or numbered non-UC2 input. recognized tells the
 * caller whether this is an identified SEA (including a damaged SEA set). */
FILE *uc2_open_sea(const char *path,char *error,size_t error_size,int *recognized)
{
    FILE *input=NULL,*joined=NULL,*raw=NULL,*result=NULL;
    uc2_writer *writer=NULL; struct directory *dirs=NULL;
    unsigned dir_count=0; char *name=NULL;
    const char *message="invalid UC2SEA header";
    unsigned char header[37],buffer[65536+37];
    *recognized=0;
    size_t length=strlen(path);name=malloc(length+8);
    if (!name) { *recognized=1;message="cannot allocate UC2SEA path";goto bad; }
    memcpy(name,path,length+1);
    char *dot=extension(name);unsigned selected=0;
    if (dot && strlen(dot)==4 && dot[1]>='0' && dot[1]<='9' && dot[2]>='0' && dot[2]<='9' && dot[3]>='0' && dot[3]<='9') {
        selected=(unsigned)(dot[1]-'0')*100+(unsigned)(dot[2]-'0')*10+(unsigned)(dot[3]-'0');
        *recognized=1;
        if (!selected) { message="UC2SEA continuation indices start at .001";goto bad; }
        strcpy(dot,".EXE");input=uc2_path_regular(name);
        if (!input) { strcpy(dot,".exe");input=uc2_path_regular(name); }
        if (!input) { message="missing UC2SEA first .EXE volume";goto bad; }
    } else input=uc2_path_regular(name);
    if (!input) { message="cannot open UC2SEA first volume";goto bad; }
    if (fread(header,1,32,input)!=32 || memcmp(header,"MZ",2)) goto bad;
    unsigned pages=r16(header+4), tail=r16(header+2);
    if (!pages || tail>511) goto bad;
    unsigned stub=(pages-1)*512+(tail ? tail : 512);
    if (stub<32 || stub>1024u*1024u || seek(input,stub)!=0) goto bad;
    /* DOS.SEA may be followed by a small text banner before its config. */
    size_t available=fread(buffer,1,sizeof buffer,input);
    unsigned banner;
    for (banner=0; banner+sizeof header<=available; ++banner) {
        if (!memcmp(buffer+banner,"UC2SFX Header\0",14)) break;
    }
    if (banner+sizeof header>available) goto bad;
    memcpy(header,buffer+banner,sizeof header);
    *recognized=1;
    unsigned offset=r32(header+15),parts=r16(header+19),files=r32(header+21);
    if (offset!=stub+banner+37 || r16(header+35)!=banner || parts>999 || !files ||
        files>SEA_FILES || selected>parts || r32(header+29)!=0xac283746u || seek(input,offset)!=0) goto bad;
    dot=extension(name);
    if (parts && !dot) { message="UC2SEA split input needs a filename extension";goto bad; }
    joined=tmpfile(); raw=tmpfile();
    if (!joined || !raw) { message="cannot create UC2SEA temporary spool";goto bad; }
    unsigned total=0;
    for (unsigned part=0; part<=parts; ++part) {
        if (part) {
            snprintf(dot,8,".%03u",part);input=uc2_path_regular(name);
            if (!input) { message="missing or non-regular UC2SEA volume";goto bad; }
        }
        size_t n;
        while ((n=fread(buffer,1,sizeof buffer,input))) {
            if (n>SEA_LIMIT-total) { message="UC2SEA compressed stream exceeds 512 MiB";goto bad; }
            if (fwrite(buffer,1,n,joined)!=n) { message="UC2SEA spool write failed";goto bad; }
            total+=(unsigned)n;
        }
        if (ferror(input)) { message="UC2SEA volume read failed";goto bad; }
        fclose(input);input=NULL;
    }
    if (fflush(joined)!=0) { message="UC2SEA spool flush failed";goto bad; }
    struct uc2_io io={read_at,allocate,release,NULL};
    int decoded=uc2_decompress_raw(&io,joined,0,total,SEA_LIMIT,output,raw);
    if (decoded<0 || fflush(raw)!=0 || seek(raw,0)!=0) { message="damaged, truncated or oversized UC2SEA stream";goto bad; }
    writer=uc2_writer_create(NULL);dirs=calloc(SEA_FILES,sizeof *dirs);
    if (!writer || !dirs) { message="cannot allocate UC2SEA writer";goto bad; }
    for (unsigned i=0; i<files; ++i) {
        int n=fgetc(raw); unsigned char record[255]; char path_utf[768];
        if (n<15 || fread(record,1,(unsigned)n,raw)!=(unsigned)n || path_utf8(path_utf,record+14,(unsigned)n-14)<0) {
            message="invalid/truncated UC2SEA file record";goto bad;
        }
        unsigned size=r32(record+10),attr=r16(record),date=r16(record+2)<<16|r16(record+4);
        if (size>SEA_LIMIT || (attr&~0x27u)) { message="unsupported UC2SEA file size/type";goto bad; }
        struct payload payload={raw,size,0,0xa55a};
        if (add_path(writer,dirs,&dir_count,path_utf,date,attr,size,&payload)<0) {
            message="invalid UC2SEA path, file data or entry limit";goto bad;
        }
        if (r32(record+6)!=(uint32_t)(size+((uint32_t)payload.checksum<<16))) {
            message="UC2SEA file checksum mismatch (possibly mixed volumes)";goto bad;
        }
    }
    if (fgetc(raw)!=EOF || ferror(raw)) { message="UC2SEA file count/decoded length mismatch";goto bad; }
    result=tmpfile();
    if (!result || uc2_writer_finish(writer,output,result)<0 || fflush(result)!=0 || seek(result,0)!=0) {
        message="cannot create normalized UC2SEA archive";goto bad;
    }
    goto done;
bad:
    if (error && error_size) snprintf(error,error_size,"%s",message);
    if (result) { fclose(result);result=NULL; }
done:
    if (input) fclose(input);
    if (joined) fclose(joined);
    if (raw) fclose(raw);
    uc2_writer_free(writer);
    for (unsigned i=0; i<dir_count; ++i) free(dirs[i].path);
    free(dirs);free(name);return result;
}
