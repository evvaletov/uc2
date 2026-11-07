/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Archive framing moved from the GPL CLI writer. Payloads use temporary files
 * so non-seekable inputs and large collections do not require file-sized RAM. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#endif
#include <uc2/uc2_writer.h>
#include <uc2/uc2_cdc.h>
#include <uc2/uc2_sha256.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MASTER_MAX 64000
#define CHUNK_MAX 16384
#define CHUNKS_MAX 1048576
#define CDIR_MAX (64u * 1024u * 1024u)
struct entry {
    char name[256];
    unsigned parent, id, depth, time, attr;
    unsigned size, csize, offset, master, spool, duplicate;
    unsigned char digest[32];
    unsigned short checksum;
};
struct master {
    unsigned source, size, csize, offset, key, ref_len, ref_count;
    unsigned short checksum;
};
struct chunk { uint32_t hash; unsigned file; };
struct uc2_writer {
    struct uc2_writer_options options;
    FILE *input, *output;
    struct entry *entries;
    unsigned count, capacity;
    struct master *masters;
    unsigned master_count;
    struct chunk *chunks;
    unsigned chunk_count, chunk_capacity;
    uint64_t input_size, output_size;
    int state, error;
    char message[160];
    struct uc2_writer_stats stats;
};
static int fail(uc2_writer *w, int code, const char *message)
{
    if (w && !w->error) {
        w->error = code; w->state = 2;
        snprintf(w->message, sizeof w->message, "%s", message);
    }
    return w && w->error ? w->error : code;
}
static int ready(uc2_writer *w)
{
    if (!w) return UC2_BadState;
    if (w->error) return w->error;
    if (w->state) return UC2_BadState;
    return 0;
}
static int seek_file(FILE *f, uint64_t pos)
{
#ifdef _WIN32
    return _fseeki64(f, (__int64)pos, SEEK_SET);
#elif defined(__DJGPP__)
    if (pos > LONG_MAX) return -1;
    return fseek(f, (long)pos, SEEK_SET);
#else
    return fseeko(f, (off_t)pos, SEEK_SET);
#endif
}
static void w16(unsigned char *p, unsigned v) { p[0]=(unsigned char)v; p[1]=(unsigned char)(v>>8); }
static void w32(unsigned char *p, unsigned v) { w16(p,v); w16(p+2,v>>16); }
/* Unicode values for the CP850 byte repertoire. */
static const unsigned cp850[128] = {
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
static int encode_name(const char *src, char dst[256])
{
    size_t n = 0, bytes = 0;
    if (!src || !*src || !strcmp(src,".") || !strcmp(src,"..")) return -1;
    while (*src) {
        unsigned c = (unsigned char)*src++, u = c;
        ++bytes;
        if (c >= 0x80) {
            unsigned continuation;
            if (c >= 0xc2 && c <= 0xdf) { u=c&31; continuation=1; }
            else if (c >= 0xe0 && c <= 0xef) { u=c&15; continuation=2; }
            else return -1;
            for (unsigned i=0; i<continuation; ++i) {
                c = (unsigned char)*src;
                if ((c&0xc0) != 0x80) return -1;
                ++src; ++bytes; u=(u<<6)|(c&63);
            }
            if (u < (continuation==1 ? 0x80u : 0x800u)) return -1;
            for (c=128; c<256 && cp850[c-128]!=u; ++c) {}
            if (c==256) return -1;
        }
        if (n==255 || bytes>299 || c<32 || c==127 || c=='/' || c=='\\' || c==':') return -1;
        dst[n++]=(char)c;
    }
    dst[n]=0;
    return 0;
}
static void dos_name(unsigned char out[11], const char *name)
{
    const char *dot = strrchr(name,'.');
    size_t n = dot ? (size_t)(dot-name) : strlen(name);
    memset(out,' ',11);
    for (size_t i=0; i<n && i<8; ++i) {
        unsigned char c=(unsigned char)name[i];
        out[i]=c>='a' && c<='z' ? c-'a'+'A' : c;
    }
    if (dot) for (unsigned i=0; i<3 && dot[i+1]; ++i) {
        unsigned char c=(unsigned char)dot[i+1];
        out[8+i]=c>='a' && c<='z' ? c-'a'+'A' : c;
    }
}
uc2_writer *uc2_writer_create(const struct uc2_writer_options *options)
{
    struct uc2_writer_options defaults = UC2_WRITER_OPTIONS_INIT;
    if (!options) options=&defaults;
    if (options->level<2 || options->level>9 || !options->max_entries ||
        options->max_entries>1000000 || !options->max_input_size ||
        options->max_input_size>UINT32_MAX) return NULL;
    uc2_writer *w=calloc(1,sizeof *w);
    if (!w) return NULL;
    w->options=*options;
    w->input=tmpfile();
    if (!w->input) { free(w); return NULL; }
    return w;
}
void uc2_writer_free(uc2_writer *w)
{
    if (!w) return;
    if (w->input) fclose(w->input);
    if (w->output) fclose(w->output);
    free(w->entries); free(w->masters); free(w->chunks); free(w);
}
const char *uc2_writer_error(const uc2_writer *w)
{ return w ? (w->error ? w->message : "writer is closed or already finalized") : "invalid writer"; }
void uc2_writer_get_stats(const uc2_writer *w, struct uc2_writer_stats *s)
{ if (w && s) *s=w->stats; }
static int add_entry(uc2_writer *w, unsigned parent, const char *name,
                     unsigned time, unsigned attr, int directory, struct entry **result)
{
    int r=ready(w);
    if (r<0) return r;
    if (attr & ~0x27u) return fail(w,UC2_WRITER_INVALID,"unsupported DOS attributes");
    struct entry e; memset(&e,0,sizeof e);
    if (encode_name(name,e.name)<0) return fail(w,UC2_WRITER_INVALID,"name must be a CP850-representable UTF-8 component within 255 CP850 / 299 UTF-8 bytes");
    if (parent) {
        if (parent>w->count || w->entries[parent-1].id!=parent)
            return fail(w,UC2_WRITER_INVALID,"parent is not a previously added directory");
        e.depth=w->entries[parent-1].depth;
    }
    if (directory && ++e.depth>64) return fail(w,UC2_WRITER_LIMIT,"directory nesting exceeds 64");
    for (unsigned i=0; i<w->count; ++i)
        if (w->entries[i].parent==parent && !strcmp(w->entries[i].name,e.name) &&
            (directory || w->entries[i].id))
            return fail(w,UC2_WRITER_INVALID,"conflicting directory or file name");
    if (w->count==w->options.max_entries) return fail(w,UC2_WRITER_LIMIT,"entry limit exceeded");
    if (w->count==w->capacity) {
        unsigned cap=w->capacity ? w->capacity*2 : 32;
        if (cap>w->options.max_entries) cap=w->options.max_entries;
        void *p=realloc(w->entries,(size_t)cap*sizeof e);
        if (!p) return fail(w,UC2_UserFault,"cannot allocate entry metadata");
        w->entries=p; w->capacity=cap;
    }
    e.parent=parent; e.time=time; e.attr=attr|(directory ? 0x10u : 0);
    e.id=directory ? w->count+1 : 0;
    w->entries[w->count]=e;
    *result=&w->entries[w->count++];
    if (directory) ++w->stats.directories; else ++w->stats.files;
    return 0;
}
int uc2_writer_add_directory(uc2_writer *w, unsigned parent, const char *name,
                             unsigned time, unsigned attr, unsigned *id)
{
    if (!id) return fail(w,UC2_WRITER_INVALID,"directory ID output is required");
    struct entry *e; int r=add_entry(w,parent,name,time,attr,1,&e);
    if (r==0) *id=e->id;
    return r;
}
int uc2_writer_add_file(uc2_writer *w, unsigned parent, const char *name,
                        unsigned time, unsigned attr, uint64_t size,
                        int (*read_cb)(void *,void *,unsigned), void *ctx)
{
    int r=ready(w); if (r<0) return r;
    if (!read_cb) return fail(w,UC2_WRITER_INVALID,"input callback is required");
    if (size!=UC2_WRITER_UNKNOWN_SIZE && size>w->options.max_input_size-w->input_size)
        return fail(w,UC2_WRITER_LIMIT,"declared input size exceeds limit");
    struct entry *e;
    r=add_entry(w,parent,name,time,attr,0,&e); if (r<0) return r;
    e->spool=(unsigned)w->input_size;
    unsigned char buffer[65536];
    uint64_t consumed=0;
    struct uc2_sha256 sha; uc2_sha256_init(&sha);
    w->state=1;
    for (;;) {
        uint64_t limit=w->options.max_input_size-w->input_size;
        if (size!=UC2_WRITER_UNKNOWN_SIZE && size-consumed<limit) limit=size-consumed;
        unsigned want=limit<sizeof buffer ? (unsigned)limit+1 : (unsigned)sizeof buffer;
        int n=read_cb(ctx,buffer,want);
        if (n<0 || (unsigned)n>want) return fail(w,UC2_UserFault,"input callback failed or returned too many bytes");
        if (!n) break;
        if ((unsigned)n>limit) return fail(w,UC2_WRITER_LIMIT,"input exceeds declared size or total input limit");
        if (fwrite(buffer,1,(size_t)n,w->input)!=(size_t)n) return fail(w,UC2_UserFault,"temporary input write failed");
        uc2_sha256_update(&sha,buffer,(unsigned)n);
        consumed+=(unsigned)n; w->input_size+=(unsigned)n;
    }
    if (size!=UC2_WRITER_UNKNOWN_SIZE && consumed!=size) return fail(w,UC2_Truncated,"input ended before its declared size");
    uc2_sha256_final(&sha,e->digest);
    e->size=(unsigned)consumed; w->state=0;
    return 0;
}
static int load_at(uc2_writer *w, unsigned pos, void *buf, unsigned n)
{
    if (seek_file(w->input,pos)!=0 || fread(buf,1,n,w->input)!=n)
        return fail(w,UC2_UserFault,"temporary input read failed");
    return 0;
}
static int chunk_compare(const void *a, const void *b)
{
    const struct chunk *x=a,*y=b;
    if (x->hash!=y->hash) return x->hash<y->hash ? -1 : 1;
    return x->file<y->file ? -1 : x->file>y->file;
}
static int add_master(uc2_writer *w, const unsigned *files, unsigned count)
{
    struct master *m=&w->masters[w->master_count]; memset(m,0,sizeof *m);
    m->source=files[0];
    for (unsigned i=0; i<count; ++i) {
        struct entry *e=&w->entries[files[i]];
        if (e->size>w->entries[m->source].size) m->source=files[i];
        e->master=w->master_count+2; m->ref_len+=e->size; ++m->ref_count;
    }
    m->size=w->entries[m->source].size;
    if (m->size>MASTER_MAX) m->size=MASTER_MAX;
    if (!m->size) m->size=1;
    ++w->master_count;
    return 0;
}
static int group_files(uc2_writer *w)
{
    unsigned char buffer[CHUNK_MAX];
    for (unsigned i=0; i<w->count; ++i) {
        struct entry *e=&w->entries[i];
        if (e->id || e->duplicate || e->size<1024) continue;
        unsigned pos=0;
        while (pos<e->size) {
            unsigned n=e->size-pos; if (n>sizeof buffer) n=sizeof buffer;
            if (load_at(w,e->spool+pos,buffer,n)<0) return w->error;
            struct uc2_chunker c; size_t off=0,len=0;
            uc2_chunker_init(&c,12,0,0); uc2_chunker_next(&c,buffer,n,&off,&len);
            if (!len) return fail(w,UC2_InternalError,"chunker made no progress");
            if (w->chunk_count==CHUNKS_MAX) return fail(w,UC2_WRITER_LIMIT,"CDC index exceeds 1048576 chunks");
            if (w->chunk_count==w->chunk_capacity) {
                unsigned cap=w->chunk_capacity ? w->chunk_capacity*2 : 256;
                void *p=realloc(w->chunks,(size_t)cap*sizeof *w->chunks);
                if (!p) return fail(w,UC2_UserFault,"cannot allocate CDC index");
                w->chunks=p; w->chunk_capacity=cap;
            }
            w->chunks[w->chunk_count++]=(struct chunk){uc2_fnv1a(buffer,len),i};
            pos+=(unsigned)len;
        }
    }
    w->masters=calloc(w->stats.files ? w->stats.files : 1,sizeof *w->masters);
    if (!w->masters) return fail(w,UC2_UserFault,"cannot allocate master index");
    if (w->chunk_count) qsort(w->chunks,w->chunk_count,sizeof *w->chunks,chunk_compare);
    for (unsigned i=0; i<w->chunk_count;) {
        unsigned end=i+1;
        while (end<w->chunk_count && w->chunks[end].hash==w->chunks[i].hash) ++end;
        unsigned ids[256], n=0, previous=UINT_MAX;
        for (unsigned j=i; j<end; ++j) {
            unsigned file=w->chunks[j].file;
            if (file==previous || w->entries[file].master) continue;
            previous=file; ids[n++]=file;
            if (n==256) { add_master(w,ids,n); n=0; }
        }
        if (n>=2) add_master(w,ids,n);
        i=end;
    }
    unsigned *remaining=malloc((size_t)(w->stats.files ? w->stats.files : 1)*sizeof *remaining);
    if (!remaining) return fail(w,UC2_UserFault,"cannot allocate fallback group");
    unsigned n=0;
    for (unsigned i=0; i<w->count; ++i)
        if (!w->entries[i].id && !w->entries[i].duplicate && !w->entries[i].master) remaining[n++]=i;
    if (n) add_master(w,remaining,n);
    free(remaining); free(w->chunks); w->chunks=NULL;
    w->stats.masters=w->master_count;
    return 0;
}
static int output_cb(void *ctx, const void *data, unsigned len)
{
    uc2_writer *w=ctx;
    if (len>UINT32_MAX-w->output_size) return fail(w,UC2_WRITER_LIMIT,"archive exceeds UINT32_MAX bytes");
    if (fwrite(data,1,len,w->output)!=len) return fail(w,UC2_UserFault,"temporary archive write failed");
    w->output_size+=len;
    return 0;
}
static int input_cb(void *ctx, void *data, unsigned len)
{
    FILE *f=ctx; size_t n=fread(data,1,len,f);
    return ferror(f) ? -1 : (int)n;
}
struct memory { const unsigned char *data; unsigned pos,len; };
static int memory_cb(void *ctx, void *data, unsigned len)
{
    struct memory *m=ctx;
    if (len>m->len-m->pos) len=m->len-m->pos;
    if (len) memcpy(data,m->data+m->pos,len);
    m->pos+=len; return (int)len;
}
static unsigned short checksum(const unsigned char *p, unsigned n)
{
    unsigned v=0xa55a;
    for (unsigned i=0; i<n; ++i) v^=(unsigned)p[i]<<((i&1)*8);
    return (unsigned short)v;
}
static int load_master(uc2_writer *w, const struct master *m, unsigned char *buf)
{
    const struct entry *e=&w->entries[m->source];
    if (!e->size) { buf[0]=0; return 0; }
    return load_at(w,e->spool,buf,m->size);
}
struct fingerprint { unsigned file, size; unsigned char digest[32]; };
static int fingerprint_compare(const void *left, const void *right)
{
    const struct fingerprint *a=left,*b=right;
    if (a->size!=b->size) return a->size<b->size ? -1 : 1;
    int r=memcmp(a->digest,b->digest,32);
    return r ? r : a->file<b->file ? -1 : a->file>b->file;
}
static int duplicates(uc2_writer *w)
{
    struct fingerprint *index=calloc(w->stats.files ? w->stats.files : 1,sizeof *index);
    if (!index) return fail(w,UC2_UserFault,"cannot allocate duplicate index");
    unsigned count=0;
    for (unsigned i=0; i<w->count; ++i) if (!w->entries[i].id) {
        index[count].file=i; index[count].size=w->entries[i].size;
        memcpy(index[count++].digest,w->entries[i].digest,32);
    }
    qsort(index,count,sizeof *index,fingerprint_compare);
    unsigned char a[16384],b[16384];
    for (unsigned i=0; i<count;) {
        unsigned end=i+1;
        while (end<count && index[end].size==index[i].size && !memcmp(index[end].digest,index[i].digest,32)) ++end;
        struct entry *first=&w->entries[index[i].file];
        for (unsigned j=i+1; j<end; ++j) {
            struct entry *e=&w->entries[index[j].file];
            unsigned pos=0;
            while (pos<e->size) {
                unsigned n=e->size-pos; if (n>sizeof a) n=sizeof a;
                if (load_at(w,first->spool+pos,a,n)<0 || load_at(w,e->spool+pos,b,n)<0) { free(index); return w->error; }
                if (memcmp(a,b,n)) break;
                pos+=n;
            }
            if (pos==e->size) e->duplicate=index[i].file+1;
        }
        i=end;
    }
    free(index); return 0;
}
static int assemble(uc2_writer *w)
{
    if (fflush(w->input)!=0) return fail(w,UC2_UserFault,"temporary input flush failed");
    if (duplicates(w)<0 || group_files(w)<0) return w->error;
    for (unsigned i=0; i<w->count; ++i) if (w->entries[i].duplicate) {
        struct entry *e=&w->entries[i];
        e->master=w->entries[e->duplicate-1].master;
        struct master *m=&w->masters[e->master-2];
        m->ref_len+=e->size; ++m->ref_count;
    }
    w->output=tmpfile();
    if (!w->output) return fail(w,UC2_UserFault,"cannot create temporary archive");
    unsigned char header[29]={0}, super[49152], master_data[MASTER_MAX];
    if (uc2_get_supermaster(super,sizeof super)<0) return fail(w,UC2_InternalError,"SuperMaster decoding failed");
    if (output_cb(w,header,sizeof header)<0) return w->error;
    for (unsigned i=0; i<w->master_count; ++i) {
        struct master *m=&w->masters[i];
        if (load_master(w,m,master_data)<0) return w->error;
        m->key=uc2_fnv1a(master_data,m->size); m->offset=(unsigned)w->output_size;
        struct memory in={master_data,0,m->size};
        int r=uc2_compress_ex(w->options.level,super,sizeof super,memory_cb,&in,output_cb,w,m->size,&m->checksum,&m->csize);
        if (r<0) return fail(w,r,"master compression failed");
    }
    for (unsigned i=0; i<w->count; ++i) {
        struct entry *e=&w->entries[i]; if (e->id) continue;
        if (e->duplicate) {
            const struct entry *original=&w->entries[e->duplicate-1];
            e->offset=original->offset; e->csize=original->csize; e->checksum=original->checksum;
            continue;
        }
        struct master *m=&w->masters[e->master-2];
        if (load_master(w,m,master_data)<0) return w->error;
        if (seek_file(w->input,e->spool)!=0) return fail(w,UC2_UserFault,"temporary input seek failed");
        e->offset=(unsigned)w->output_size;
        int r=uc2_compress_ex(w->options.level,master_data,m->size,input_cb,w->input,output_cb,w,e->size,&e->checksum,&e->csize);
        if (r<0) return fail(w,r,"file compression failed");
    }
    uint64_t cap=22+(uint64_t)39*w->master_count;
    for (unsigned i=0; i<w->count; ++i) cap+=(w->entries[i].id ? 27 : 47)+22+strlen(w->entries[i].name);
    if (cap>CDIR_MAX) return fail(w,UC2_WRITER_LIMIT,"central directory exceeds 64 MiB");
    unsigned char *raw=malloc((size_t)cap);
    if (!raw) return fail(w,UC2_UserFault,"cannot allocate central directory");
    unsigned char *p=raw;
    unsigned method=w->options.level>=6 ? 10 : (unsigned)w->options.level;
    for (unsigned i=0; i<w->master_count; ++i) {
        struct master *m=&w->masters[i]; *p++=3;
        w32(p,i+2); p+=4; w32(p,m->key); p+=4; w32(p,m->ref_len); p+=4; w32(p,m->ref_count); p+=4;
        w16(p,m->size); p+=2; w16(p,m->checksum); p+=2; w32(p,m->csize); p+=4;
        w16(p,method==10 ? 10 : 1); p+=2; w32(p,0); p+=4; w32(p,1); p+=4; w32(p,m->offset); p+=4;
    }
    /* Parents have smaller indices than children. Files may follow directories
     * in the input order, and duplicate files retain their version order. */
    for (int dirs=1; dirs>=0; --dirs) for (unsigned i=0; i<w->count; ++i) {
        struct entry *e=&w->entries[i]; if (!!e->id!=dirs) continue;
        *p++=e->id ? 1 : 2; w32(p,e->parent); p+=4; *p++=(unsigned char)e->attr;
        w32(p,e->time); p+=4; dos_name(p,e->name); p+=11; *p++=0; *p++=1;
        if (e->id) { w32(p,e->id); p+=4; }
        else {
            w32(p,e->size); p+=4; w16(p,e->checksum); p+=2;
            w32(p,e->csize); p+=4; w16(p,method); p+=2; w32(p,e->master); p+=4;
            w32(p,1); p+=4; w32(p,e->offset); p+=4;
        }
        unsigned n=(unsigned)strlen(e->name)+1;
        memcpy(p,"AIP:Win95 LongN",16); p+=16; w32(p,n); p+=4; *p++=0;
        memcpy(p,e->name,n); p+=n;
    }
    *p++=4; *p++=0; *p++=0; w32(p,0); p+=4; memset(p,' ',11); p+=11; w32(p,0); p+=4;
    unsigned cdir_size=(unsigned)(p-raw), cdir_offset=(unsigned)w->output_size;
    unsigned short csum=checksum(raw,cdir_size), unused_sum;
    unsigned char crec[10]={0}; w16(crec+4,method); w32(crec+6,1);
    if (output_cb(w,crec,10)<0) { free(raw); return w->error; }
    struct memory in={raw,0,cdir_size}; unsigned compressed;
    int r=uc2_compress(w->options.level,memory_cb,&in,output_cb,w,cdir_size,&unused_sum,&compressed);
    free(raw); if (r<0) return fail(w,r,"central directory compression failed");
    unsigned total=(unsigned)w->output_size, length=total-13;
    w32(header,0x1a324355); w32(header+4,length); w32(header+8,length+0x01b2c3d4);
    w32(header+13,1); w32(header+17,cdir_offset); w16(header+21,csum); w16(header+24,203); w16(header+26,200);
    if (seek_file(w->output,0)!=0 || fwrite(header,1,sizeof header,w->output)!=sizeof header || fflush(w->output)!=0)
        return fail(w,UC2_UserFault,"archive header write or flush failed");
    w->stats.archive_size=total;
    return 0;
}
int uc2_writer_finish(uc2_writer *w, int (*write_cb)(void *,const void *,unsigned), void *ctx)
{
    int r=ready(w); if (r<0) return r;
    if (!write_cb) return fail(w,UC2_WRITER_INVALID,"output callback is required");
    w->state=1;
    r=assemble(w); if (r<0) return r;
    if (seek_file(w->output,0)!=0) return fail(w,UC2_UserFault,"temporary archive rewind failed");
    unsigned char buffer[65536];
    for (;;) {
        size_t n=fread(buffer,1,sizeof buffer,w->output);
        if (ferror(w->output)) return fail(w,UC2_UserFault,"temporary archive read failed");
        if (!n) break;
        if (write_cb(ctx,buffer,(unsigned)n)<0) return fail(w,UC2_UserFault,"output callback failed");
    }
    FILE *input=w->input,*output=w->output; w->input=w->output=NULL;
    int a=fclose(input),b=fclose(output);
    if (a!=0 || b!=0) return fail(w,UC2_UserFault,"temporary file close failed");
    w->state=2;
    return 0;
}
