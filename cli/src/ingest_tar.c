/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bounded sequential tar reader; no member is written to the filesystem. */
#include "ingest_tar.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#define PATH_LIMIT 4096
#define EXT_LIMIT (1024u*1024u)
#define DIR_LIMIT 100000
#define BUCKETS 65536
struct directory { char *path; unsigned id, next; int explicit_metadata; };
struct tar { FILE *in; uc2_writer *writer; char *error; size_t error_size;
    struct directory *dirs; unsigned *buckets, count; };
struct overrides { char path[PATH_LIMIT]; uint64_t size, mtime; int has_size, has_mtime; };
static int fail(struct tar *t, const char *text)
{ snprintf(t->error,t->error_size,"tar: %s",text); return -1; }
static int number(const unsigned char *s, unsigned n, uint64_t *result)
{
    uint64_t value=0;
    if (s[0]&128) {
        if (s[0]&64) return -1;
        value=s[0]&63;
        for (unsigned i=1; i<n; ++i) {
            if (value>UINT64_MAX/256) return -1;
            value=value*256+s[i];
        }
    } else {
        unsigned i=0;
        while (i<n && s[i]==' ') ++i;
        for (; i<n && s[i]>='0' && s[i]<='7'; ++i) {
            if (value>(UINT64_MAX-7)/8) return -1;
            value=value*8+s[i]-'0';
        }
        for (; i<n; ++i) if (s[i]!=' ' && s[i]!=0) return -1;
    }
    *result=value; return 0;
}
static int zero(const unsigned char *p, size_t n)
{ for (size_t i=0; i<n; ++i) if (p[i]) return 0; return 1; }
static int block(struct tar *t, unsigned char *p)
{ return fread(p,1,512,t->in)==512 ? 0 : fail(t,"truncated block"); }
static int padding(struct tar *t, uint64_t size)
{
    unsigned n=(unsigned)((512-size%512)%512); unsigned char b[512];
    if (fread(b,1,n,t->in)!=n || !zero(b,n)) return fail(t,"invalid/truncated member padding");
    return 0;
}
static int field(char *dst, size_t cap, const unsigned char *src, size_t n)
{
    size_t len=0; while (len<n && src[len]) ++len;
    if (len>=cap) return -1;
    memcpy(dst,src,len); dst[len]=0; return 0;
}
static int decimal(const char *s, size_t n, uint64_t *out, int fraction)
{
    uint64_t value=0; size_t i=0;
    for (; i<n && s[i]>='0' && s[i]<='9'; ++i) {
        if (value>(UINT64_MAX-9)/10) return -1;
        value=value*10+(unsigned)(s[i]-'0');
    }
    if (!i) return -1;
    if (fraction && i<n && s[i]=='.') {
        if (++i==n) return -1;
        for (; i<n && s[i]>='0' && s[i]<='9'; ++i) {}
    }
    if (i!=n) return -1;
    *out=value; return 0;
}
static int pax(struct tar *t, char *data, size_t size, struct overrides *o, int global)
{
    for (size_t pos=0; pos<size;) {
        size_t start=pos;
        while (pos<size && data[pos]>='0' && data[pos]<='9') ++pos;
        uint64_t count;
        if (pos==size || data[pos]!=' ' || decimal(data+start,pos-start,&count,0)<0 ||
            count>size-start || count<pos-start+4 || data[start+count-1]!='\n') return fail(t,"malformed PAX record");
        size_t end=start+(size_t)count-1;
        char *key=data+(++pos), *eq=memchr(key,'=',end-pos);
        if (!eq || eq==key || memchr(key,0,end-pos)) return fail(t,"invalid PAX key/value");
        *eq=0; const char *value=eq+1; size_t len=(size_t)(data+end-value);
        if (!strcmp(key,"path")) {
            if (global || !len || len>=sizeof o->path) return fail(t,"invalid PAX path");
            memcpy(o->path,value,len); o->path[len]=0;
        } else if (!strcmp(key,"size")) {
            if (global || decimal(value,len,&o->size,0)<0) return fail(t,"invalid PAX size");
            o->has_size=1;
        } else if (!strcmp(key,"mtime")) {
            if (decimal(value,len,&o->mtime,1)<0) return fail(t,"invalid PAX mtime");
            o->has_mtime=1;
        } else if (strcmp(key,"uid") && strcmp(key,"gid") && strcmp(key,"uname") &&
                   strcmp(key,"gname") && strcmp(key,"atime") && strcmp(key,"ctime")) {
            return fail(t,"unsupported PAX key (links, sparse data, xattrs and other extensions are not preserved)");
        }
        pos=end+1;
    }
    return 0;
}
static int clean_path(struct tar *t, char *path, int directory)
{
    if (!path[0] || path[0]=='/') return fail(t,"empty/absolute member path");
    char *p=path;
    while (p[0]=='.' && p[1]=='/') p+=2;
    if (p!=path) memmove(path,p,strlen(p)+1);
    size_t n=strlen(path);
    if (directory) while (n && path[n-1]=='/') path[--n]=0;
    if (directory && (!n || !strcmp(path,"."))) { path[0]=0; return 0; }
    if (!n || path[0]=='/') return fail(t,"empty/absolute member path");
    unsigned depth=0;
    for (p=path; *p;) {
        char *end=strchr(p,'/'); size_t len=end ? (size_t)(end-p) : strlen(p);
        if (!len || (len==1 && p[0]=='.') || (len==2 && p[0]=='.' && p[1]=='.') || ++depth>64)
            return fail(t,"unsafe/deep member path");
        for (size_t i=0; i<len; ++i) if ((unsigned char)p[i]<32 || p[i]==127 || p[i]=='\\' || p[i]==':')
            return fail(t,"unsafe member component");
        p+=len; if (*p) ++p;
    }
    if (path[n-1]=='/') return fail(t,"empty path component");
    return 0;
}
static unsigned timestamp(uint64_t seconds)
{
    if (seconds>INT64_MAX) return 0;
    time_t value=(time_t)seconds;
    if (value<0 || (uint64_t)value!=seconds) return 0;
    struct tm *tm=localtime(&value);
    if (!tm || tm->tm_year<80 || tm->tm_year>207) return 0;
    return (unsigned)(tm->tm_year-80)<<25 | (unsigned)(tm->tm_mon+1)<<21 |
        (unsigned)tm->tm_mday<<16 | (unsigned)tm->tm_hour<<11 | (unsigned)tm->tm_min<<5 | (unsigned)tm->tm_sec/2;
}
static int parents(struct tar *t, char *path, int directory, unsigned date, unsigned attr, unsigned *parent)
{
    unsigned id=0;
    for (char *p=path; *p; ++p) {
        if (*p!='/' && (p[1] || !directory)) continue;
        char *end=*p=='/' ? p : p+1; char saved=*end; *end=0;
        unsigned hash=2166136261u;
        for (char *s=path; *s; ++s) hash=(hash^(unsigned char)*s)*16777619u;
        hash%=BUCKETS;
        unsigned item=t->buckets[hash];
        while (item && strcmp(t->dirs[item-1].path,path)) item=t->dirs[item-1].next;
        if (!item) {
            if (t->count==DIR_LIMIT) return fail(t,"too many directories");
            const char *leaf=strrchr(path,'/'); leaf=leaf ? leaf+1 : path;
            unsigned new_id;
            if (uc2_writer_add_directory(t->writer,id,leaf,saved ? 0 : date,saved ? 0 : attr,&new_id)<0)
                return fail(t,uc2_writer_error(t->writer));
            struct directory *d=&t->dirs[t->count];
            d->path=malloc(strlen(path)+1); if (!d->path) return fail(t,"out of memory");
            strcpy(d->path,path); d->id=new_id; d->next=t->buckets[hash];
            d->explicit_metadata=!saved;
            item=++t->count; t->buckets[hash]=item;
        } else if (!saved && !t->dirs[item-1].explicit_metadata) {
            /* A child's path can create this directory before its own header.
             * Keep its ID/order and retain the first explicit header's metadata,
             * as for directories that were explicit when first encountered. */
            if (uc2_writer_set_directory_metadata(t->writer,t->dirs[item-1].id,date,attr)<0)
                return fail(t,uc2_writer_error(t->writer));
            t->dirs[item-1].explicit_metadata=1;
        }
        id=t->dirs[item-1].id; *end=saved;
    }
    *parent=id; return 0;
}
struct member { FILE *in; uint64_t left; };
static int member_read(void *ctx, void *buf, unsigned len)
{
    struct member *m=ctx;
    if (len>m->left) len=(unsigned)m->left;
    size_t n=fread(buf,1,len,m->in); m->left-=n;
    return ferror(m->in) ? -1 : (int)n;
}
int uc2_tar_read(uc2_writer *writer, FILE *input, char *error, size_t error_size)
{
    struct tar t={input,writer,error,error_size,NULL,NULL,0};
    t.dirs=calloc(DIR_LIMIT,sizeof *t.dirs); t.buckets=calloc(BUCKETS,sizeof *t.buckets);
    int result=-1;
    if (!t.dirs || !t.buckets) { fail(&t,"out of memory"); goto done; }
    struct overrides local={0}, global={0};
    unsigned char header[512]; unsigned extension_bytes=0, extension_count=0;
    for (;;) {
        if (block(&t,header)<0) goto done;
        if (zero(header,512)) {
            if (extension_count || block(&t,header)<0 || !zero(header,512)) { fail(&t,"missing tar end marker or dangling extension"); goto done; }
            size_t n; unsigned tail=0;
            while ((n=fread(header,1,512,input))!=0) {
                if (n!=512 || !zero(header,n) || (tail+=512)>EXT_LIMIT) { fail(&t,"nonzero/excessive trailing data"); goto done; }
            }
            if (ferror(input)) { fail(&t,"input read error"); goto done; }
            result=0; break;
        }
        uint64_t sum, size, mode, mtime; unsigned actual=0;
        for (unsigned i=0; i<512; ++i) actual+=(i>=148 && i<156) ? 32 : header[i];
        if (number(header+148,8,&sum)<0 || sum!=actual || number(header+124,12,&size)<0 ||
            number(header+100,8,&mode)<0 || number(header+136,12,&mtime)<0) { fail(&t,"invalid header checksum/number"); goto done; }
        int type=header[156];
        if (type=='x' || type=='g' || type=='L') {
            if (++extension_count>128 || size>EXT_LIMIT-extension_bytes) { fail(&t,"extension exceeds limit"); goto done; }
            extension_bytes+=(unsigned)size;
            char *data=calloc((size_t)size+1,1);
            if (!data) { fail(&t,"out of memory"); goto done; }
            int bad=fread(data,1,(size_t)size,input)!=size || padding(&t,size)<0;
            if (!bad && type=='L') {
                size_t len=strlen(data);
                if (!len || len>=sizeof local.path || !zero((unsigned char*)data+len,(size_t)size-len)) bad=1;
                else strcpy(local.path,data);
            } else if (!bad) bad=pax(&t,data,(size_t)size,type=='g' ? &global : &local,type=='g')<0;
            free(data);
            if (bad) { fail(&t,"invalid/unsupported extended header"); goto done; }
            continue;
        }
        if (type!=0 && type!='0' && type!='5') { fail(&t,"only regular files and directories are supported"); goto done; }
        int directory=type=='5'; char path[PATH_LIMIT], name[101], prefix[156];
        field(name,sizeof name,header,100); prefix[0]=0;
        if (!memcmp(header+257,"ustar\0",6)) field(prefix,sizeof prefix,header+345,155);
        if (local.path[0]) strcpy(path,local.path);
        else snprintf(path,sizeof path,"%s%s%s",prefix,prefix[0] ? "/" : "",name);
        if (local.has_size) size=local.size;
        if (local.has_mtime) mtime=local.mtime; else if (global.has_mtime) mtime=global.mtime;
        /* V7 represents a directory as an empty regular record ending in '/'. */
        size_t path_length=strlen(path);
        if (!type && !size && path_length && path[path_length-1]=='/') directory=1;
        if (size>UINT32_MAX || (directory && size)) { fail(&t,"member size exceeds limit or directory has data"); goto done; }
        if (clean_path(&t,path,directory)<0) goto done;
        unsigned parent=0, date=timestamp(mtime), attr=(mode&0222) ? 0 : 1;
        if (path[0] && parents(&t,path,directory,date,attr,&parent)<0) goto done;
        if (!directory) {
            const char *leaf=strrchr(path,'/'); leaf=leaf ? leaf+1 : path;
            struct member member={input,size};
            if (uc2_writer_add_file(writer,parent,leaf,date,attr|0x20,size,member_read,&member)<0) {
                fail(&t,uc2_writer_error(writer)); goto done;
            }
        }
        if (padding(&t,size)<0) goto done;
        memset(&local,0,sizeof local); extension_bytes=extension_count=0;
    }
done:
    for (unsigned i=0; i<t.count; ++i) free(t.dirs[i].path);
    free(t.dirs); free(t.buckets); return result;
}
