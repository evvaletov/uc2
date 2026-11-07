/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uc2/libuc2.h>
#include <uc2/uc2_path.h>
#include "fixture_archive.h"

static void check(const char *file, const char *name, int damaged, int good, unsigned size)
{
    unsigned char data[70000], record[256]={0}, zeros[512]={0}, header[101]={0};
    memset(data,'x',size);
    unsigned n=(unsigned)strlen(name)+14;
    record[0]=(unsigned char)n;
    put_u16le(record+1,0x20);
    put_u32le(record+7,size+((unsigned)fletcher_checksum(data,size)<<16)+(damaged==1));
    put_u32le(record+11,size); memcpy(record+15,name,n-14);
    struct membuf raw, compressed; membuf_init(&raw);
    membuf_append(&raw,record,n+1);membuf_append(&raw,data,size);
    unsigned short checksum;
    int ret=compress_data(raw.data,raw.len,2,zeros,sizeof zeros,&compressed,&checksum);assert(ret==0);
    memcpy(header,"MZ",2);put_u16le(header+2,64);put_u16le(header+4,1);
    memcpy(header+64,"UC2SFX Header\0",14);put_u32le(header+79,101);
    put_u32le(header+85,damaged==2 ? 2 : 1);put_u32le(header+93,0xac283746u);
    FILE *f=fopen(file,"wb");assert(f);
    size_t wrote=fwrite(header,1,sizeof header,f);assert(wrote==sizeof header);
    wrote=fwrite(compressed.data,1,compressed.len-(damaged==3 ? 2 : 0),f);
    assert(wrote==compressed.len-(damaged==3 ? 2u : 0u));fclose(f);
    char error[256];f=uc2_open_path(file,error,sizeof error);
    if (good) { assert(f);fclose(f); } else { assert(!f); }
    struct uc2_io io={archive_read,my_alloc,my_free,NULL};struct membuf out;membuf_init(&out);
    ret=uc2_decompress_raw(&io,&compressed,0,compressed.len,raw.len,membuf_write,&out);
    assert(ret==(int)raw.len && out.len==raw.len && !memcmp(out.data,raw.data,raw.len));membuf_free(&out);
    ret=uc2_decompress_raw(&io,&compressed,0,compressed.len,raw.len-1,membuf_write,&out);assert(ret<0);membuf_free(&out);
    if (raw.len>65024) {
        ret=uc2_decompress_raw(&io,&compressed,0,compressed.len,65024,membuf_write,&out);assert(ret<0);membuf_free(&out);
    }
    membuf_append(&compressed,"\0\0",2);
    ret=uc2_decompress_raw(&io,&compressed,0,compressed.len,raw.len,membuf_write,&out);assert(ret<0);membuf_free(&out);
    compressed.len-=2;
    for (unsigned end=2;end<compressed.len;end+=2) {
        ret=uc2_decompress_raw(&io,&compressed,0,end,raw.len,membuf_write,&out);assert(ret<0);membuf_free(&out);
    }
    membuf_free(&raw);membuf_free(&compressed);
}
int main(int argc,char **argv)
{
    assert(argc==2);
    check(argv[1],"SUB\\FILE.TXT",0,1,4096);
    check(argv[1],"../escape",0,0,4096);
    check(argv[1],"/absolute",0,0,4096);
    check(argv[1],"C:\\escape",0,0,4096);
    check(argv[1],"file",1,0,4096);
    check(argv[1],"file",2,0,4096);
    check(argv[1],"file",3,0,4096);
    check(argv[1],"bigfile",0,1,65025);
    remove(argv[1]);puts("SEA data-only framing, unsafe names, checksum and raw bounds passed");return 0;
}
