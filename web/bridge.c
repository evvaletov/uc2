/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <uc2/libuc2.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENTRY_LIMIT 10000u
#define DECODER_LIMIT (64u * 1024u * 1024u)
#define OUTPUT_LIMIT (64u * 1024u * 1024u)

static FILE *source;
static uc2_handle handle;
static struct uc2_entry *entries;
static unsigned count, allocated, refused;
static unsigned char *output;
static unsigned written, expected;
static char error_text[256];
union allocation { size_t size; long double align; void *pointer; };

static void *allocate(void *ctx, unsigned size)
{
    if (size > DECODER_LIMIT - allocated || size > SIZE_MAX - sizeof(union allocation)) {
        refused = 1;
        return NULL;
    }
    union allocation *p = malloc(sizeof *p + size);
    if (!p) { refused = 1; return NULL; }
    p->size = size;
    allocated += size;
    return p + 1;
}
static void release(void *ctx, void *ptr)
{
    if (!ptr) return;
    union allocation *p = (union allocation *)ptr - 1;
    allocated -= p->size;
    free(p);
}
static int read_at(void *ctx, unsigned pos, void *buf, unsigned len)
{
    if (fseek(source, (long)pos, SEEK_SET) != 0) return -1;
    size_t got = fread(buf, 1, len, source);
    return ferror(source) ? -1 : (int)got;
}
static struct uc2_io io = {read_at, allocate, release, NULL};

static int fail(int code)
{
    const char *message = refused ? "Decoder memory limit exceeded (64 MiB)" :
        (handle ? uc2_message(handle, code) : "Could not open the UC2 archive");
    snprintf(error_text, sizeof error_text, "%s", message);
    return code < 0 ? code : -1;
}

void uc2_web_close(void)
{
    if (handle) handle = uc2_close(handle);
    if (source) fclose(source);
    source = NULL;
    free(entries); entries = NULL; count = 0;
    free(output); output = NULL;
    allocated = refused = 0;
}

int uc2_web_open(void)
{
    uc2_web_close();
    error_text[0] = 0;
    source = fopen("/input/archive.uc2", "rb");
    if (!source) return fail(-1);
    handle = uc2_open(&io, NULL);
    if (!handle) return fail(-1);
    entries = calloc(ENTRY_LIMIT, sizeof *entries);
    if (!entries) { refused = 1; return fail(-1); }
    for (;;) {
        struct uc2_entry entry;
        memset(&entry, 0, sizeof entry);
        int result = uc2_read_cdir(handle, &entry);
        if (result == UC2_End) break;
        if (result < 0) return fail(result);
        while (result == UC2_TaggedEntry) {
            result = uc2_get_tag(handle, &entry, NULL, NULL, NULL);
            if (result < 0) return fail(result);
        }
        if (count == ENTRY_LIMIT) {
            snprintf(error_text, sizeof error_text, "Archive exceeds the 10,000 entry browser limit");
            return -1;
        }
        if (!memchr(entry.name, 0, sizeof entry.name)) return fail(UC2_Damaged);
        entries[count++] = entry;
    }
    int result = uc2_finish_cdir(handle, NULL);
    return result < 0 ? fail(result) : (int)count;
}

const char *uc2_web_error(void) { return error_text; }
const char *uc2_web_name(unsigned index)
{ return index < count ? entries[index].name : ""; }

unsigned uc2_web_field(unsigned index, unsigned field)
{
    if (index >= count) return 0;
    struct uc2_entry *e = entries + index;
    switch (field) {
        case 0: return e->dirid;
        case 1: return e->id;
        case 2: return e->is_dir;
        case 3: return e->size;
        case 4: return e->csize;
        case 5: return e->dos_time;
        case 6: return e->attr;
        case 7: return e->xi.method;
        default: return 0;
    }
}

static int write_output(void *ctx, const void *data, unsigned len)
{
    if (written > expected || len > expected - written) return -1;
    if (output) memcpy(output + written, data, len);
    written += len;
    return 0;
}

int uc2_web_extract(unsigned index, int keep)
{
    free(output); output = NULL;
    error_text[0] = 0; refused = 0;
    if (!handle || index >= count || entries[index].is_dir) return fail(UC2_BadState);
    expected = entries[index].size; written = 0;
    if (expected > OUTPUT_LIMIT) {
        snprintf(error_text, sizeof error_text, "Entry exceeds the 64 MiB browser limit; use uc2-preserve");
        return -1;
    }
    if (keep) {
        output = malloc(expected ? expected : 1);
        if (!output) { refused = 1; return fail(-1); }
    }
    int result = uc2_extract(handle, &entries[index].xi, expected, write_output, NULL);
    if (result < 0 || written != expected) {
        free(output); output = NULL;
        return fail(result < 0 ? result : UC2_Damaged);
    }
    return 0;
}
const unsigned char *uc2_web_output(void) { return output; }
void uc2_web_free_output(void) { free(output); output = NULL; }
