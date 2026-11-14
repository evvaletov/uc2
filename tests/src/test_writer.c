/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#define _FILE_OFFSET_BITS 64
#endif
#include <uc2/uc2_writer.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Exercise allocation failures in the real writer without production hooks.
 * These definitions replace the archive's writer object for this test only. */
static unsigned writer_allocations, writer_fail_allocation;
static void *writer_calloc(size_t count,size_t size)
{ return ++writer_allocations==writer_fail_allocation ? NULL : calloc(count,size); }
static void *writer_realloc(void *p,size_t size)
{ return ++writer_allocations==writer_fail_allocation ? NULL : realloc(p,size); }
#define calloc writer_calloc
#define realloc writer_realloc
#include "../../lib/src/uc2_writer.c"
#undef calloc
#undef realloc

/* Evaluate API calls even if this test is compiled with NDEBUG externally. */
#define CHECK(expr) do { int ok = !!(expr); assert(ok); (void)ok; } while (0)

struct buffer { unsigned char *data; unsigned size, pos; };
static int input(void *ctx, void *data, unsigned n)
{
    struct buffer *b=ctx;
    if (n>b->size-b->pos) n=b->size-b->pos;
    if (n>113) n=113; /* Deliberately short reads across codec boundaries. */
    if (n) memcpy(data,b->data+b->pos,n);
    b->pos+=n; return (int)n;
}
static int output(void *ctx, const void *data, unsigned n)
{
    if (!n) return 0;
    struct buffer *b=ctx; unsigned char *p=realloc(b->data,(size_t)b->size+n);
    CHECK(p); b->data=p; memcpy(p+b->size,data,n); b->size+=n; return 0;
}
static int read_at(void *ctx, unsigned pos, void *data, unsigned n)
{
    struct buffer *b=ctx;
    if (pos>b->size) return -1;
    if (n>b->size-pos) n=b->size-pos;
    memcpy(data,b->data+pos,n); return (int)n;
}
static int refuse_allocation;
static void *allocate(void *ctx, unsigned n) { return refuse_allocation ? NULL : malloc(n); }
static void release(void *ctx, void *p) { free(p); }
static struct uc2_io io={read_at,allocate,release,NULL};
static int fail_read(void *ctx,void *p,unsigned n) { return -1; }
static int bad_read(void *ctx,void *p,unsigned n) { return (int)n+1; }
static int fail_write(void *ctx,const void *p,unsigned n) { return -1; }
static void roundtrip(int level, int chunked)
{
    unsigned char data[70001];
    for (unsigned i=0; i<sizeof data; ++i) data[i]=(unsigned char)(i*17+i/13);
    struct buffer source={data,sizeof data,0}, encoded={0};
    struct uc2_writer_options options=UC2_WRITER_OPTIONS_INIT; options.level=level;
    options.chunked=chunked;
    uc2_writer *w=uc2_writer_create(&options); CHECK(w);
    unsigned directory;
    CHECK(uc2_writer_add_directory(w,0,"nested",0,0x27,&directory)==0);
    CHECK(uc2_writer_add_file(w,directory,"caf\xc3\xa9.bin",0,0x20,source.size,input,&source)==0);
    source.pos=0;
    CHECK(uc2_writer_add_file(w,directory,"caf\xc3\xa9.bin",0,0x20,UC2_WRITER_UNKNOWN_SIZE,input,&source)==0);
    source.size=0; source.pos=0;
    CHECK(uc2_writer_add_file(w,0,"empty",0,0x20,0,input,&source)==0);
    CHECK(uc2_writer_set_directory_metadata(w,directory,0x56b4722a,1)==0);
    CHECK(uc2_writer_finish(w,output,&encoded)==0);
    CHECK(uc2_writer_set_directory_metadata(w,directory,0,0)==UC2_BadState);
    CHECK(uc2_writer_finish(w,output,&encoded)==UC2_BadState);
    struct uc2_writer_stats stats; uc2_writer_get_stats(w,&stats);
    CHECK(stats.files==3 && stats.directories==1 && stats.archive_size==encoded.size);
    if (chunked) CHECK(stats.unique_chunks && stats.chunk_references>stats.unique_chunks && stats.shared_bytes>=sizeof data);
    uc2_writer_free(w);
    uc2_handle h=uc2_open(&io,&encoded); CHECK(h);
    struct uc2_entry entries[4]; unsigned count=0;
    for (;;) {
        struct uc2_entry entry={0}; int r=uc2_read_cdir(h,&entry);
        if (r==UC2_End) break;
        CHECK(r>0);
        while (r==UC2_TaggedEntry) r=uc2_get_tag(h,&entry,NULL,NULL,NULL);
        CHECK(r>=0 && count<4); entries[count++]=entry;
    }
    CHECK(count==4 && uc2_finish_cdir(h,NULL)==0);
    CHECK(entries[0].is_dir && entries[0].id==directory && entries[0].dirid==0);
    CHECK(!strcmp(entries[0].name,"nested") && entries[0].attr==0x11);
    CHECK(entries[0].dos_time==0x56b4722a);
    for (unsigned i=0; i<count; ++i) if (!entries[i].is_dir) {
        if (chunked) CHECK(entries[i].xi.method==11);
        struct buffer decoded={0};
        int result = uc2_extract(h,&entries[i].xi,entries[i].size,output,&decoded);
        if (result < 0) fprintf(stderr,"level %d, entry %s: extract returned %d\n", level, entries[i].name, result);
        CHECK(result >= 0);
        if (!strcmp(entries[i].name,"empty")) CHECK(decoded.size==0);
        else {
            CHECK(!strcmp(entries[i].name,"caf\xc3\xa9.bin"));
            CHECK(decoded.size==sizeof data && !memcmp(decoded.data,data,sizeof data));
        }
        free(decoded.data);
        if (chunked && entries[i].size) {
            CHECK(uc2_extract(h,&entries[i].xi,entries[i].size,fail_write,NULL)==UC2_UserFault);
            refuse_allocation=1;
            CHECK(uc2_extract(h,&entries[i].xi,entries[i].size,fail_write,NULL)==UC2_UserFault);
            refuse_allocation=0;
        }
    }
    uc2_close(h); free(encoded.data);
}
static void directory_metadata_errors(void)
{
    CHECK(uc2_writer_set_directory_metadata(NULL,1,0,0)==UC2_BadState);
    /* 0 is the implicit root; 2 is a file; 3 and UINT_MAX are not assigned. */
    const unsigned ids[]={0,2,3,~0u};
    const unsigned attrs[]={8,0x10,0x80,~0u};
    for (unsigned i=0; i<sizeof ids/sizeof *ids+sizeof attrs/sizeof *attrs; ++i) {
        uc2_writer *w=uc2_writer_create(NULL); CHECK(w);
        unsigned directory;
        struct buffer in={0},out={0};
        CHECK(uc2_writer_add_directory(w,0,"d",1,1,&directory)==0);
        CHECK(uc2_writer_add_file(w,directory,"f",0,0,0,input,&in)==0);
        unsigned invalid_ids=sizeof ids/sizeof *ids;
        unsigned id=i<invalid_ids ? ids[i] : directory;
        unsigned attr=i<invalid_ids ? 0 : attrs[i-invalid_ids];
        CHECK(uc2_writer_set_directory_metadata(w,id,2,attr)==UC2_WRITER_INVALID);
        /* Rejected metadata cannot become a partial archive or be repaired on
         * this poisoned writer; counts are unchanged and no output is emitted. */
        struct uc2_writer_stats stats; uc2_writer_get_stats(w,&stats);
        CHECK(stats.files==1 && stats.directories==1 && stats.archive_size==0);
        CHECK(uc2_writer_set_directory_metadata(w,directory,0,0)==UC2_WRITER_INVALID);
        CHECK(uc2_writer_finish(w,output,&out)==UC2_WRITER_INVALID && !out.size);
        uc2_writer_free(w);
    }
    uc2_writer *w=uc2_writer_create(NULL); CHECK(w);
    unsigned id;
    CHECK(uc2_writer_add_directory(w,99,"invalid",0,0,&id)==UC2_WRITER_INVALID);
    CHECK(uc2_writer_set_directory_metadata(w,0,0,0)==UC2_WRITER_INVALID);
    uc2_writer_free(w);
}
static void indexed_names(void)
{
    uc2_writer *w=uc2_writer_create(NULL); CHECK(w);
    /* These distinct names have the same 32-bit FNV-1a hash at the root. */
    const char *prefix[]={"costarring","liquid","same","same","Case","case",
                          "caf\xc3\xa9","caf\xc3\xa9"};
    unsigned parents[]={0,0,1,2,0,0,0,0};
    unsigned id;
    CHECK(uc2_writer_add_directory(w,0,prefix[0],0,0,&id)==0 && id==1);
    CHECK(uc2_writer_add_directory(w,0,prefix[1],0,0,&id)==0 && id==2);
    unsigned char bytes[]={1,2,3};
    struct buffer in={0},out={0};
    for (unsigned i=2; i<8; ++i) {
        in=(struct buffer){bytes+(i%3),i>=6 ? 1 : 0,0};
        CHECK(uc2_writer_add_file(w,parents[i],prefix[i],i,0x20,in.size,input,&in)==0);
    }
    for (unsigned i=0; i<4096; ++i) {
        char name[32]; snprintf(name,sizeof name,"file-%04u",i);
        in=(struct buffer){0};
        CHECK(uc2_writer_add_file(w,0,name,0,0x20,0,input,&in)==0);
    }
    in=(struct buffer){bytes,1,0};
    CHECK(uc2_writer_add_file(w,0,"file-0000",0,0x20,1,input,&in)==0);
    CHECK(uc2_writer_finish(w,output,&out)==0);
    struct uc2_writer_stats stats; uc2_writer_get_stats(w,&stats);
    CHECK(stats.directories==2 && stats.files==4103);
    uc2_writer_free(w);
    uc2_handle h=uc2_open(&io,&out); CHECK(h);
    unsigned count=0, stored_count=0;
    struct uc2_entry stored[3];
    for (;;) {
        struct uc2_entry e={0}; int r=uc2_read_cdir(h,&e);
        if (r==UC2_End) break;
        CHECK(r>0);
        while (r==UC2_TaggedEntry) r=uc2_get_tag(h,&e,NULL,NULL,NULL);
        CHECK(r>=0 && count<4105);
        if (count<8) {
            CHECK(!strcmp(e.name,prefix[count]) && e.dirid==parents[count]);
            CHECK(e.is_dir==(count<2));
            if (count<2) CHECK(e.id==count+1);
        } else {
            char name[32]; snprintf(name,sizeof name,"file-%04u",count==4104 ? 0 : count-8);
            CHECK(!strcmp(e.name,name) && !e.is_dir && e.dirid==0);
        }
        if (count==6 || count==7 || count==4104) {
            CHECK(e.size==1 && stored_count<3);
            stored[stored_count++]=e;
        } else if (!e.is_dir) CHECK(e.size==0);
        ++count;
    }
    CHECK(count==4105 && uc2_finish_cdir(h,NULL)==0);
    CHECK(stored_count==3);
    for (unsigned i=0; i<stored_count; ++i) {
        struct buffer decoded={0};
        CHECK(uc2_extract(h,&stored[i].xi,stored[i].size,output,&decoded)>=0);
        CHECK(decoded.size==1 && decoded.data[0]==bytes[i==2 ? 0 : i]);
        free(decoded.data);
    }
    uc2_close(h); free(out.data);

    for (int first_dir=0; first_dir<2; ++first_dir) for (int second_dir=0; second_dir<2; ++second_dir) {
        if (!first_dir && !second_dir) continue; /* File versions tested above. */
        w=uc2_writer_create(NULL); CHECK(w); in=(struct buffer){0};
        CHECK((first_dir ? uc2_writer_add_directory(w,0,"caf\xc3\xa9",0,0,&id) :
               uc2_writer_add_file(w,0,"caf\xc3\xa9",0,0,0,input,&in))==0);
        for (unsigned i=0; i<128; ++i) {
            char name[32]; snprintf(name,sizeof name,"grow-%u",i);
            CHECK(uc2_writer_add_file(w,0,name,0,0,0,input,&in)==0);
        }
        CHECK((second_dir ? uc2_writer_add_directory(w,0,"caf\xc3\xa9",0,0,&id) :
               uc2_writer_add_file(w,0,"caf\xc3\xa9",0,0,0,fail_read,NULL))==UC2_WRITER_INVALID);
        out=(struct buffer){0};
        CHECK(uc2_writer_finish(w,output,&out)==UC2_WRITER_INVALID && out.size==0);
        uc2_writer_free(w);
    }
}
static unsigned insertion_allocations(unsigned failure)
{
    writer_allocations=0; writer_fail_allocation=failure;
    uc2_writer *w=uc2_writer_create(NULL);
    if (!w) { CHECK(failure==1); writer_fail_allocation=0; return writer_allocations; }
    unsigned inserted=0;
    struct buffer in={0},out={0};
    for (unsigned i=0; i<300; ++i) {
        char name[32]; snprintf(name,sizeof name,"entry-%u",i);
        int r=uc2_writer_add_file(w,0,name,0,0,0,input,&in);
        if (r<0) {
            CHECK(failure && writer_allocations==failure && r==UC2_UserFault);
            struct uc2_writer_stats stats; uc2_writer_get_stats(w,&stats);
            CHECK(stats.files==inserted && stats.directories==0);
            CHECK(uc2_writer_add_file(w,0,"retry",0,0,0,input,&in)==UC2_UserFault);
            CHECK(uc2_writer_finish(w,output,&out)==UC2_UserFault && !out.size);
            break;
        }
        ++inserted;
    }
    CHECK(failure ? inserted<300 : inserted==300);
    unsigned allocations=writer_allocations;
    writer_fail_allocation=0; uc2_writer_free(w);
    return allocations;
}
static void errors(void)
{
    unsigned char bytes[]={1,2,3}; struct buffer in={bytes,3,0},out={0}; unsigned id;
    struct uc2_writer_options options=UC2_WRITER_OPTIONS_INIT;
    options.max_input_size=2;
    uc2_writer *w=uc2_writer_create(&options); CHECK(w);
    CHECK(uc2_writer_add_file(w,0,"x",0,0,3,input,&in)==UC2_WRITER_LIMIT);
    CHECK(in.pos==0 && uc2_writer_finish(w,output,&out)<0 && !out.size); uc2_writer_free(w);
    for (int mode=0; mode<4; ++mode) {
        w=uc2_writer_create(NULL); CHECK(w); in.pos=0;
        int r=uc2_writer_add_file(w,0,"x",0,0,mode==0 ? 4 : 2,
                                mode==2 ? fail_read : mode==3 ? bad_read : input,&in);
        CHECK(r<0 && uc2_writer_finish(w,output,&out)<0 && !out.size); uc2_writer_free(w);
    }
    const char *invalid[]={"", ".", "..", "a/b", "a\\b", "x:y", "\xf0\x9f\x98\x80", "\xc0\x80", "\xe2"};
    for (unsigned i=0; i<sizeof invalid/sizeof *invalid; ++i) {
        w=uc2_writer_create(NULL); CHECK(w);
        CHECK(uc2_writer_add_directory(w,0,invalid[i],0,0,&id)==UC2_WRITER_INVALID); uc2_writer_free(w);
    }
    w=uc2_writer_create(NULL); CHECK(w);
    CHECK(uc2_writer_add_directory(w,99,"x",0,0,&id)==UC2_WRITER_INVALID); uc2_writer_free(w);
    w=uc2_writer_create(NULL); CHECK(w);
    CHECK(uc2_writer_add_directory(w,0,"x",0,0,&id)==0);
    CHECK(uc2_writer_add_directory(w,0,"x",0,0,&id)==UC2_WRITER_INVALID); uc2_writer_free(w);
    w=uc2_writer_create(NULL); CHECK(w);
    CHECK(uc2_writer_finish(w,fail_write,NULL)==UC2_UserFault); uc2_writer_free(w);
    options.max_entries=1; w=uc2_writer_create(&options); CHECK(w);
    CHECK(uc2_writer_add_directory(w,0,"x",0,0,&id)==0);
    CHECK(uc2_writer_set_directory_metadata(w,id,0,0x27)==0);
    CHECK(uc2_writer_add_directory(w,0,"y",0,0,&id)==UC2_WRITER_LIMIT); uc2_writer_free(w);
    w=uc2_writer_create(NULL); CHECK(w); id=0;
    for (unsigned i=0; i<64; ++i) CHECK(uc2_writer_add_directory(w,id,"d",0,0,&id)==0);
    CHECK(uc2_writer_add_directory(w,id,"d",0,0,&id)==UC2_WRITER_LIMIT); uc2_writer_free(w);
    w=uc2_writer_create(NULL); CHECK(w); CHECK(uc2_writer_finish(w,output,&out)==0); uc2_writer_free(w);
    uc2_handle h=uc2_open(&io,&out); struct uc2_entry e={0}; CHECK(h);
    CHECK(uc2_read_cdir(h,&e)==UC2_End && uc2_finish_cdir(h,NULL)==0); uc2_close(h); free(out.data);
}
int main(void)
{
    for (int level=2; level<=9; ++level) {
        roundtrip(level,0); roundtrip(level,1);
    }
    directory_metadata_errors();
    indexed_names();
    unsigned allocations=insertion_allocations(0);
    CHECK(allocations>4);
    for (unsigned failure=1; failure<=allocations; ++failure) insertion_allocations(failure);
    errors(); puts("writer: all levels, versions, directories, streams and failures passed"); return 0;
}
