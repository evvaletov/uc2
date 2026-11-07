/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <uc2/libuc2.h>
#include <uc2/uc2_rans.h>
#include <uc2/uc2_sha256.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { int ok=!!(x); assert(ok); (void)ok; } while (0)
struct bytes { unsigned char *p; unsigned n, pos; };
static int append(void *ctx,const void *p,unsigned n)
{
    struct bytes *b=ctx;
    void *next=realloc(b->p,b->n+n); CHECK(next || !n);
    b->p=next; memcpy(b->p+b->n,p,n); b->n+=n; return 0;
}
static int read_at(void *ctx,unsigned pos,void *p,unsigned n)
{
    struct bytes *b=ctx;
    if (pos>=b->n) return 0;
    if (n>b->n-pos) n=b->n-pos;
    memcpy(p,b->p+pos,n); return (int)n;
}
static int read_next(void *ctx,void *p,unsigned n)
{ struct bytes *b=ctx; int got=read_at(b,b->pos,p,n); b->pos+=(unsigned)got; return got; }
static void *allocate(void *ctx,unsigned n) { (void)ctx; return malloc(n); }
static void release(void *ctx,void *p) { (void)ctx; free(p); }
static void w16(unsigned char *p,unsigned n) { p[0]=(unsigned char)n; p[1]=(unsigned char)(n>>8); }
static void w32(unsigned char *p,unsigned n) { w16(p,n); w16(p+2,n>>16); }
static unsigned sum(const unsigned char *p,unsigned n)
{ unsigned s=0xa55a; for (unsigned i=0;i<n;++i) s^=(unsigned)p[i]<<((i&1)*8); return s; }
static void sha(const void *p,unsigned n,unsigned char *out)
{ struct uc2_sha256 h; uc2_sha256_init(&h); uc2_sha256_update(&h,p,n); uc2_sha256_final(&h,out); }

/* Word-oriented MSB-first bit stream, as used by the UC2 codecs. */
struct bits { struct bytes out; unsigned word, count; };
static void put(struct bits *b,unsigned n,unsigned count)
{
    while (count) {
        b->word=(b->word<<1)|((n>>--count)&1); ++b->count;
        if (b->count==16) {
            unsigned char word[2]; w16(word,b->word); append(&b->out,word,2);
            b->word=b->count=0;
        }
    }
}
static struct bytes rans_payload(int fault)
{
    /* fault 1 omits EOB; fault 2 puts a length symbol in primary position. */
    int symbols[]={65,316,315,316}, count=4;
    if (!fault) { symbols[1]=315; symbols[2]=316; count=3; }
    if (fault==1) count=1;
    uint32_t frequencies[344]={0}; frequencies[66]=1;
    for (int i=0;i<count;++i) ++frequencies[symbols[i]];
    struct uc2_rans_table table; uc2_rans_build_table(&table,frequencies,344);
    struct uc2_rans_enc enc; uc2_rans_enc_init(&enc,&table);
    for (int i=count-1;i>=0;--i) uc2_rans_encode(&enc,symbols[i]);
    unsigned char *data; size_t size=uc2_rans_enc_finish(&enc,&data);
    uc2_rans_enc_free(&enc); CHECK(size<=65535);
    struct bits b={0}; put(&b,1,1); put(&b,(unsigned)count,16); put(&b,(unsigned)size,16);
    for (unsigned i=0;i<344;++i) put(&b,table.freq[i],12);
    for (unsigned i=0;i<size;++i) put(&b,data[i],8);
    if (fault!=1) put(&b,64001-61440,12); /* EOB distance extra bits */
    put(&b,0,1); if (b.count) put(&b,0,16-b.count);
    free(data); return b.out;
}
static void check_archive(struct bytes payload,unsigned method,unsigned csize,int damaged)
{
    unsigned char recipe[136]={0}, raw[69]={0}, header[29]={0}, crec[10]={0};
    memcpy(recipe,"UC2CHN1",8); w32(recipe+8,1); w32(recipe+12,1); w32(recipe+16,48);
    sha("A",1,recipe+24);
    unsigned char *record=recipe+88;
    w32(record,29); w32(record+4,payload.n); w32(record+8,1);
    w16(record+12,method); w16(record+14,sum((const unsigned char *)"A",1));
    sha("A",1,record+16);
    struct uc2_sha256 hash; uc2_sha256_init(&hash);
    uc2_sha256_update(&hash,recipe,56); uc2_sha256_update(&hash,record,48);
    uc2_sha256_final(&hash,recipe+56);
    raw[0]=2; raw[5]=0x20; memcpy(raw+10,"DATA       ",11);
    w32(raw+23,1); w16(raw+27,sum((const unsigned char *)"A",1)); w32(raw+29,csize);
    w16(raw+33,11); w32(raw+35,1); w32(raw+39,1); w32(raw+43,29+payload.n);
    raw[47]=4; memset(raw+54,' ',11);
    struct bytes input={raw,sizeof raw,0}, cdir={0}, archive={0};
    unsigned short checksum; unsigned encoded;
    CHECK(uc2_compress(4,read_next,&input,append,&cdir,sizeof raw,&checksum,&encoded)==0);
    unsigned offset=29+payload.n+sizeof recipe, total=offset+10+cdir.n;
    w32(header,0x1a324355); w32(header+4,total-13); w32(header+8,total-13+0x01b2c3d4);
    w32(header+13,1); w32(header+17,offset); w16(header+21,sum(raw,sizeof raw));
    w16(header+24,300); w16(header+26,300); w16(crec+4,4); w32(crec+6,1);
    append(&archive,header,sizeof header); append(&archive,payload.p,payload.n);
    append(&archive,recipe,sizeof recipe); append(&archive,crec,sizeof crec); append(&archive,cdir.p,cdir.n);
    struct uc2_io io={read_at,allocate,release,NULL}; uc2_handle h=uc2_open(&io,&archive); CHECK(h);
    struct uc2_entry e; int result=uc2_read_cdir(h,&e);
    if (csize!=sizeof recipe) CHECK(result==UC2_Damaged);
    else {
        CHECK(result==UC2_BareEntry); CHECK(uc2_read_cdir(h,NULL)==UC2_End);
        CHECK(uc2_finish_cdir(h,NULL)==0);
        struct bytes decoded={0}; result=uc2_extract(h,&e.xi,e.size,append,&decoded);
        if (damaged) CHECK(result==UC2_Damaged);
        else CHECK(result==0 && decoded.n==1 && decoded.p[0]=='A');
        free(decoded.p);
    }
    uc2_close(h); free(archive.p); free(cdir.p);
}
static int repeated(void *ctx,void *p,unsigned n)
{
    unsigned *left=ctx;
    if (n>*left) n=*left;
    memset(p,'A',n); *left-=n; return (int)n;
}
static void oversized_cdir(void)
{
    /* Highly compressible metadata must stop at the absolute byte ceiling,
     * reporting damaged input rather than falsely blaming an I/O callback. */
    unsigned left=64u*1024u*1024u+1, encoded;
    unsigned char header[29]={0}, crec[10]={0};
    struct bytes compressed={0}, archive={0}; unsigned short checksum;
    CHECK(uc2_compress(6,repeated,&left,append,&compressed,left,&checksum,&encoded)==0);
    unsigned total=39+compressed.n;
    w32(header,0x1a324355); w32(header+4,total-13); w32(header+8,total-13+0x01b2c3d4);
    w32(header+13,1); w32(header+17,29); w16(header+21,checksum);
    w16(header+24,203); w16(header+26,200); w16(crec+4,10); w32(crec+6,1);
    append(&archive,header,sizeof header); append(&archive,crec,sizeof crec);
    append(&archive,compressed.p,compressed.n);
    struct uc2_io io={read_at,allocate,release,NULL}; uc2_handle h=uc2_open(&io,&archive); CHECK(h);
    struct uc2_entry entry; CHECK(uc2_read_cdir(h,&entry)==UC2_Damaged);
    uc2_close(h); free(archive.p); free(compressed.p);
}
int main(void)
{
    struct bytes raw={(unsigned char *)"A",1,0};
    check_archive(raw,0,136,0); check_archive(raw,0,UINT_MAX,1); check_archive(raw,0,137,1);
    for (int fault=0;fault<3;++fault) {
        struct bytes payload=rans_payload(fault); check_archive(payload,10,136,fault!=0); free(payload.p);
    }
    oversized_cdir();
    puts("chunk corruption: cdir sizes, rANS primary symbols and EOB validated"); return 0;
}
