/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <uc2/uc2_writer.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
    assert(p); b->data=p; memcpy(p+b->size,data,n); b->size+=n; return 0;
}
static int read_at(void *ctx, unsigned pos, void *data, unsigned n)
{
    struct buffer *b=ctx;
    if (pos>b->size) return -1;
    if (n>b->size-pos) n=b->size-pos;
    memcpy(data,b->data+pos,n); return (int)n;
}
static void *allocate(void *ctx, unsigned n) { return malloc(n); }
static void release(void *ctx, void *p) { free(p); }
static struct uc2_io io={read_at,allocate,release,NULL};
static int fail_read(void *ctx,void *p,unsigned n) { return -1; }
static int bad_read(void *ctx,void *p,unsigned n) { return (int)n+1; }
static int fail_write(void *ctx,const void *p,unsigned n) { return -1; }
static void roundtrip(int level)
{
    unsigned char data[70001];
    for (unsigned i=0; i<sizeof data; ++i) data[i]=(unsigned char)(i*17+i/13);
    struct buffer source={data,sizeof data,0}, encoded={0};
    struct uc2_writer_options options=UC2_WRITER_OPTIONS_INIT; options.level=level;
    uc2_writer *w=uc2_writer_create(&options); assert(w);
    unsigned directory;
    assert(uc2_writer_add_directory(w,0,"nested",0,0,&directory)==0);
    assert(uc2_writer_add_file(w,directory,"caf\xc3\xa9.bin",0,0x20,source.size,input,&source)==0);
    source.pos=0;
    assert(uc2_writer_add_file(w,directory,"caf\xc3\xa9.bin",0,0x20,UC2_WRITER_UNKNOWN_SIZE,input,&source)==0);
    source.size=0; source.pos=0;
    assert(uc2_writer_add_file(w,0,"empty",0,0x20,0,input,&source)==0);
    assert(uc2_writer_finish(w,output,&encoded)==0);
    assert(uc2_writer_finish(w,output,&encoded)==UC2_BadState);
    struct uc2_writer_stats stats; uc2_writer_get_stats(w,&stats);
    assert(stats.files==3 && stats.directories==1 && stats.archive_size==encoded.size);
    uc2_writer_free(w);
    uc2_handle h=uc2_open(&io,&encoded); assert(h);
    struct uc2_entry entries[4]; unsigned count=0;
    for (;;) {
        struct uc2_entry entry={0}; int r=uc2_read_cdir(h,&entry);
        if (r==UC2_End) break;
        assert(r>0);
        while (r==UC2_TaggedEntry) r=uc2_get_tag(h,&entry,NULL,NULL,NULL);
        assert(r>=0 && count<4); entries[count++]=entry;
    }
    assert(count==4 && uc2_finish_cdir(h,NULL)==0);
    for (unsigned i=0; i<count; ++i) if (!entries[i].is_dir) {
        struct buffer decoded={0};
        int result = uc2_extract(h,&entries[i].xi,entries[i].size,output,&decoded);
        if (result < 0) fprintf(stderr,"level %d, entry %s: extract returned %d\n", level, entries[i].name, result);
        assert(result >= 0);
        if (!strcmp(entries[i].name,"empty")) assert(decoded.size==0);
        else {
            assert(!strcmp(entries[i].name,"caf\xc3\xa9.bin"));
            assert(decoded.size==sizeof data && !memcmp(decoded.data,data,sizeof data));
        }
        free(decoded.data);
    }
    uc2_close(h); free(encoded.data);
}
static void errors(void)
{
    unsigned char bytes[]={1,2,3}; struct buffer in={bytes,3,0},out={0}; unsigned id;
    struct uc2_writer_options options=UC2_WRITER_OPTIONS_INIT;
    options.max_input_size=2;
    uc2_writer *w=uc2_writer_create(&options); assert(w);
    assert(uc2_writer_add_file(w,0,"x",0,0,3,input,&in)==UC2_WRITER_LIMIT);
    assert(in.pos==0 && uc2_writer_finish(w,output,&out)<0 && !out.size); uc2_writer_free(w);
    for (int mode=0; mode<4; ++mode) {
        w=uc2_writer_create(NULL); assert(w); in.pos=0;
        int r=uc2_writer_add_file(w,0,"x",0,0,mode==0 ? 4 : 2,
                                mode==2 ? fail_read : mode==3 ? bad_read : input,&in);
        assert(r<0 && uc2_writer_finish(w,output,&out)<0 && !out.size); uc2_writer_free(w);
    }
    const char *invalid[]={"", ".", "..", "a/b", "a\\b", "x:y", "\xf0\x9f\x98\x80", "\xc0\x80", "\xe2"};
    for (unsigned i=0; i<sizeof invalid/sizeof *invalid; ++i) {
        w=uc2_writer_create(NULL); assert(w);
        assert(uc2_writer_add_directory(w,0,invalid[i],0,0,&id)==UC2_WRITER_INVALID); uc2_writer_free(w);
    }
    w=uc2_writer_create(NULL); assert(w);
    assert(uc2_writer_add_directory(w,99,"x",0,0,&id)==UC2_WRITER_INVALID); uc2_writer_free(w);
    w=uc2_writer_create(NULL); assert(w);
    assert(uc2_writer_add_directory(w,0,"x",0,0,&id)==0);
    assert(uc2_writer_add_directory(w,0,"x",0,0,&id)==UC2_WRITER_INVALID); uc2_writer_free(w);
    w=uc2_writer_create(NULL); assert(w);
    assert(uc2_writer_finish(w,fail_write,NULL)==UC2_UserFault); uc2_writer_free(w);
    options.max_entries=1; w=uc2_writer_create(&options); assert(w);
    assert(uc2_writer_add_directory(w,0,"x",0,0,&id)==0);
    assert(uc2_writer_add_directory(w,0,"y",0,0,&id)==UC2_WRITER_LIMIT); uc2_writer_free(w);
    w=uc2_writer_create(NULL); assert(w); id=0;
    for (unsigned i=0; i<64; ++i) assert(uc2_writer_add_directory(w,id,"d",0,0,&id)==0);
    assert(uc2_writer_add_directory(w,id,"d",0,0,&id)==UC2_WRITER_LIMIT); uc2_writer_free(w);
    w=uc2_writer_create(NULL); assert(w); assert(uc2_writer_finish(w,output,&out)==0); uc2_writer_free(w);
    uc2_handle h=uc2_open(&io,&out); struct uc2_entry e={0}; assert(h);
    assert(uc2_read_cdir(h,&e)==UC2_End && uc2_finish_cdir(h,NULL)==0); uc2_close(h); free(out.data);
}
int main(void)
{
    for (int level=2; level<=9; ++level) roundtrip(level);
    errors(); puts("writer: all levels, versions, directories, streams and failures passed"); return 0;
}
