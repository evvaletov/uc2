/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Inject allocator failures in the real rANS implementation; the static
 * library supplies the compressor and the remaining decoder dependencies. */
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <uc2/libuc2.h>
#include <uc2/uc2_rans.h>
static unsigned calls, fail_at;
static void *test_malloc(size_t n) { return ++calls==fail_at ? NULL : malloc(n); }
static void *test_realloc(void *p,size_t n) { return ++calls==fail_at ? NULL : realloc(p,n); }
#define malloc test_malloc
#define realloc test_realloc
#include "../../lib/src/uc2_rans.c"
#undef malloc
#undef realloc
static unsigned position;
static int input(void *ctx,void *p,unsigned n)
{
    uint8_t *bytes=p;
    for (unsigned i=0;i<n;i++,position++) bytes[i]=(uint8_t)(position*29+position/7);
    return (int)n;
}
static int output(void *ctx,const void *p,unsigned n) { return 0; }
static void standalone(unsigned failure)
{
    uint32_t frequencies[256];
    for (int i=0;i<256;i++) frequencies[i]=1;
    struct uc2_rans_table table;
    uc2_rans_build_table(&table,frequencies,256);
    struct uc2_rans_enc enc;
    uc2_rans_enc_init(&enc,&table);
    calls=0; fail_at=failure;
    for (int i=9999;i>=0;i--) uc2_rans_encode(&enc,i%256);
    uint8_t *data=(uint8_t *)1;
    size_t n=uc2_rans_enc_finish(&enc,&data);
    if (failure) assert(n==0 && data==NULL && enc.error);
    else { assert(n>4096 && data); free(data); }
    uc2_rans_enc_free(&enc);
}
int main(void)
{
    standalone(0);
    unsigned total=calls;
    for (unsigned f=1;f<=total;f++) standalone(f);
    calls=0;fail_at=0;position=0;
    int rc=uc2_compress(6,input,NULL,output,NULL,90000,NULL,NULL);
    assert(rc==0); total=calls; assert(total>=2);
    for (unsigned f=1;f<=total;f++) {
        calls=0;fail_at=f;position=0;
        rc=uc2_compress(6,input,NULL,output,NULL,90000,NULL,NULL);
        assert(rc<0);
    }
    return 0;
}
