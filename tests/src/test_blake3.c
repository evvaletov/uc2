/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <uc2/uc2_blake3.h>
#include "../data/blake3-vectors.h"

static void check(const uint8_t hash[32], const char *expected)
{
    char actual[65];
    for (size_t i = 0; i < 32; ++i) sprintf(actual + 2*i, "%02x", hash[i]);
    assert(strcmp(actual, expected) == 0);
}

int main(void)
{
    uint8_t data[102400], digest[32], again[32];
    const size_t strides[] = {1, 63, 64, 65, 1023, 1024, 1025, 4096};
    for (size_t i = 0; i < sizeof data; ++i) data[i] = (uint8_t)(i % 251);
    for (size_t v = 0; v < sizeof vectors / sizeof vectors[0]; ++v) {
        size_t len = vectors[v].len;
        uc2_blake3_hash(data, len, digest);
        check(digest, vectors[v].hash);
        for (size_t s = 0; s < sizeof strides / sizeof strides[0]; ++s) {
            struct uc2_blake3 ctx, saved;
            uc2_blake3_init(&ctx);
            uc2_blake3_update(&ctx, NULL, 0);
            for (size_t pos = 0; pos < len;) {
                size_t n = len - pos;
                if (n > strides[s]) n = strides[s];
                uc2_blake3_update(&ctx, data + pos, n);
                pos += n;
                saved = ctx;
                uc2_blake3_final(&ctx, again);
                assert(memcmp(&saved, &ctx, sizeof ctx) == 0);
            }
            uc2_blake3_final(&ctx, again);
            check(again, vectors[v].hash);
            assert(uc2_blake3_equal(digest, again));
        }
        /* Every split for short inputs, plus splits around chunk boundaries. */
        for (size_t split = 0; split <= len; ++split) {
            if (len > 129 && split > 65 && split % 1024 > 1 && split % 1024 < 1023)
                continue;
            struct uc2_blake3 ctx;
            uc2_blake3_init(&ctx);
            uc2_blake3_update(&ctx, data, split);
            uc2_blake3_final(&ctx, again);
            uc2_blake3_update(&ctx, data + split, len - split);
            uc2_blake3_final(&ctx, again);
            check(again, vectors[v].hash);
        }
    }
    for (size_t i = 0; i < 32; ++i) {
        memcpy(again, digest, 32);
        again[i] ^= 1;
        assert(!uc2_blake3_equal(digest, again));
    }
    puts("35 official BLAKE3 vectors, incremental boundaries and reusable finalization passed");
    return 0;
}
