/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Generated format fixtures, NOT historical original-encoder archives. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uc2/libuc2.h>
#include "fixture_archive.h"

/* Encoder used only to construct independent-format test cases. The format
 * is specified in original/UC2_source/src/COMP_TT.CPP (LGPL release). */
static void encode(const unsigned char *data, unsigned size, unsigned master,
                   struct membuf *out)
{
    unsigned char table[32768] = {0}, dictionary[65535], block[28125];
    int len = master == 0 ? uc2_get_supermaster(dictionary, sizeof dictionary) : 512;
    assert(len > 0);
    if (master == 1) memset(dictionary, 0, 512);
    for (int i = 0; i + 10 < len; i++) table[(dictionary[i]<<7)^dictionary[i+1]] = dictionary[i+2];
    unsigned p1 = 0, p2 = 0;
    membuf_init(out);
    for (unsigned start = 0; start < size;) {
        unsigned end = size - start > 25000 ? start + 25000 : size;
        unsigned count = 0;
        while (start < end) {
            unsigned ctrl = count++;
            block[ctrl] = 0;
            for (int bit = 0; bit < 8 && start < end; bit++) {
                unsigned h = (p2<<7)^p1;
                unsigned char c = data[start++];
                if (table[h] == c) block[ctrl] |= 0x80 >> bit;
                else { table[h] = c; block[count++] = c; }
                p2 = p1; p1 = c;
            }
        }
        unsigned char header[2]; put_u16le(header, count);
        membuf_append(out, header, 2); membuf_append(out, block, count);
    }
    membuf_append(out, "\0\0", 2);
}

static int extract(struct membuf *archive, struct membuf *out)
{
    struct uc2_io io = {archive_read, my_alloc, my_free, NULL};
    uc2_handle h = uc2_open(&io, archive);
    assert(h);
    struct uc2_entry entry;
    int ret = uc2_read_cdir(h, &entry);
    assert(ret >= 0 && entry.xi.method == 80);
    while (ret == UC2_TaggedEntry) ret = uc2_get_tag(h, &entry, NULL, NULL, NULL);
    assert(ret >= 0);
    ret = uc2_read_cdir(h, NULL); assert(ret >= 0);
    membuf_init(out);
    ret = uc2_extract(h, &entry.xi, entry.size, membuf_write, out);
    uc2_close(h);
    return ret;
}

int main(int argc, char **argv)
{
    static unsigned char data[70001];
    unsigned x = 80;
    for (unsigned i = 0; i < sizeof data; i++) {
        x = x * 1664525u + 1013904223u;
        data[i] = i < 30000 ? (unsigned char)(x >> 24) : (unsigned char)(i % 251);
    }
    const unsigned sizes[] = {0,1,7,8,9,24999,25000,25001,70001};
    for (unsigned master = 0; master <= 1; master++)
    for (unsigned i = 0; i < sizeof sizes / sizeof *sizes; i++) {
        unsigned size = sizes[i];
        struct membuf compressed, archive, out;
        encode(data, size, master, &compressed);
        int ret = build_archive(compressed.data, compressed.len, size,
            fletcher_checksum(data, size), master, 80, &archive);
        assert(ret >= 0);
        ret = extract(&archive, &out);
        assert(ret >= 0 && out.len == size && (!size || memcmp(out.data, data, size) == 0));
        membuf_free(&out);
        if (argc == 2 && size == sizeof data) {
            char path[4096]; snprintf(path, sizeof path, "%s/turbo%u.uc2", argv[1], master);
            FILE *f = fopen(path, "wb"); assert(f);
            size_t n = fwrite(archive.data, 1, archive.len, f); assert(n == archive.len);
            ret = fclose(f); assert(ret == 0);
            snprintf(path, sizeof path, "%s/turbo-payload.bin", argv[1]);
            f = fopen(path, "wb"); assert(f);
            n = fwrite(data, 1, size, f); assert(n == size);
            ret = fclose(f); assert(ret == 0);
        }
        membuf_free(&archive);
        if (size) {
            /* No terminator: the following cdir is not a valid Turbo tail. */
            ret = build_archive(compressed.data, compressed.len - 2, size,
                fletcher_checksum(data, size), master, 80, &archive); assert(ret >= 0);
            ret = extract(&archive, &out); assert(ret < 0);
            membuf_free(&out); membuf_free(&archive);
            /* Declared output is too small. */
            ret = build_archive(compressed.data, compressed.len, size - 1,
                fletcher_checksum(data, size), master, 80, &archive); assert(ret >= 0);
            ret = extract(&archive, &out); assert(ret < 0 && out.len <= size - 1);
            membuf_free(&out); membuf_free(&archive);
            /* A valid checksum cannot conceal a declared length mismatch. */
            ret = build_archive(compressed.data, compressed.len, size + 1,
                fletcher_checksum(data, size), master, 80, &archive); assert(ret >= 0);
            ret = extract(&archive, &out); assert(ret == UC2_Truncated);
            membuf_free(&out); membuf_free(&archive);
        }
        membuf_free(&compressed);
    }
    const unsigned char bad[][5] = {{255,255,0,0,0},{1,0,0x40,0,0},{1,0,0,0,0}};
    for (unsigned i = 0; i < sizeof bad / sizeof *bad; i++) {
        struct membuf archive, out;
        int ret = build_archive(bad[i], sizeof bad[i], 16, 0xA55A, 1, 80, &archive); assert(ret >= 0);
        ret = extract(&archive, &out); assert(ret < 0);
        membuf_free(&out); membuf_free(&archive);
    }
    puts("Turbo: 18 size/master cases and malformed stream checks passed");
    return 0;
}
