/* SPDX-License-Identifier: LGPL-3.0-only */
/* UC2 reader for Total Commander, adapted from Jan Bobrowski's 2021
 * unuc2-wcx.c. Copyright (c) Jan Bobrowski 2021, Eremey Valetov 2026.
 * Bobrowski's LGPL license is retained. The linked UC2 distribution
 * includes GPL components; see docs/license-audit.md. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include <io.h>
#include <fcntl.h>
#include <ctype.h>
#include <direct.h>
#include "wcxhead.h"
#include <uc2/libuc2.h>

#define API __declspec(dllexport)
#define NAME_CAP 2048

struct item { struct uc2_entry entry; char *path; wchar_t *visible; unsigned version; };
struct reader {
    FILE *input;
    uc2_handle handle;
    struct item *items;
    size_t count, next;
    tProcessDataProc progress;
    tProcessDataProcW progress_w;
};

static int read_at(void *ctx, unsigned pos, void *buf, unsigned len)
{
    struct reader *r = ctx;
    if (_fseeki64(r->input, pos, SEEK_SET)) return -1;
    size_t got = fread(buf, 1, len, r->input);
    return ferror(r->input) ? -1 : (int)got;
}
static void *allocate(void *ctx, unsigned size) { (void)ctx; return malloc(size); }
static void release(void *ctx, void *p) { (void)ctx; free(p); }
static struct uc2_io callbacks = { read_at, allocate, release, NULL };

API int __stdcall CloseArchive(void *handle)
{
    struct reader *r = handle;
    if (!r) return 0;
    uc2_close(r->handle);
    if (r->input) fclose(r->input);
    for (size_t i = 0; i < r->count; i++) {
        free(r->items[i].path); free(r->items[i].visible);
    }
    free(r->items); free(r);
    return 0;
}

static int safe_component(const char *name)
{
    if (!*name || !strcmp(name, ".") || !strcmp(name, "..")) return 0;
    size_t n = strlen(name);
    if (name[n-1] == '.' || name[n-1] == ' ') return 0;
    char base[16];
    size_t i = 0;
    while (name[i] && name[i] != '.' && i < sizeof base - 1) {
        base[i] = (char)toupper((unsigned char)name[i]); i++;
    }
    base[i] = 0;
    if (!strcmp(base, "CON") || !strcmp(base, "PRN") || !strcmp(base, "AUX") ||
        !strcmp(base, "NUL") || (!strncmp(base, "COM", 3) && i == 4 && base[3] >= '1' && base[3] <= '9') ||
        (!strncmp(base, "LPT", 3) && i == 4 && base[3] >= '1' && base[3] <= '9')) return 0;
    for (; *name; name++)
        if ((unsigned char)*name < 32 || strchr("/\\:<>\"|?*", *name)) return 0;
    return 1;
}

static int parent_path(struct reader *r, unsigned id, char *out, size_t cap, unsigned depth)
{
    if (!id) { out[0] = 0; return 0; }
    if (depth > 64) return -1;
    for (size_t i = 0; i < r->count; i++) {
        struct uc2_entry *e = &r->items[i].entry;
        if (!e->is_dir || e->id != id) continue;
        int n = parent_path(r, e->dirid, out, cap, depth + 1);
        if (n < 0) return -1;
        int added = snprintf(out + n, cap - (size_t)n, "%s\\", e->name);
        return added < 0 || (size_t)added >= cap - (size_t)n ? -1 : n + added;
    }
    return -1;
}

static int compare_visible(const void *a, const void *b)
{
    const struct item *x = *(const struct item * const *)a;
    const struct item *y = *(const struct item * const *)b;
    return CompareStringOrdinal(x->visible, -1, y->visible, -1, TRUE) - CSTR_EQUAL;
}

static int prepare_visible_names(struct reader *r)
{
    struct item **sorted = malloc(r->count * sizeof *sorted);
    if (!sorted && r->count) return -1;
    for (size_t i = 0; i < r->count; i++) {
        struct item *item = &r->items[i];
        char name[NAME_CAP + 32];
        if (item->version) snprintf(name, sizeof name, "%s;%u", item->path, item->version);
        else snprintf(name, sizeof name, "%s", item->path);
        int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, NULL, 0);
        if (!size || size > 1024) { free(sorted); return -1; }
        item->visible = malloc((size_t)size * sizeof *item->visible);
        if (!item->visible) { free(sorted); return -1; }
        MultiByteToWideChar(CP_UTF8, 0, name, -1, item->visible, size);
        sorted[i] = item;
    }
    if (r->count > 1) qsort(sorted, r->count, sizeof *sorted, compare_visible);
    for (size_t i = 1; i < r->count; i++) {
        if (!compare_visible(&sorted[i-1], &sorted[i])) {
            free(sorted); return -1;
        }
    }
    free(sorted);
    return 0;
}

static void *open_file(FILE *f, int *result)
{
    *result = E_EOPEN;
    if (!f) return NULL;
    struct reader *r = calloc(1, sizeof *r);
    if (!r) { fclose(f); *result = E_NO_MEMORY; return NULL; }
    r->input = f;
    r->handle = uc2_open(&callbacks, r);
    *result = E_BAD_ARCHIVE;
    if (!r->handle) goto fail;
    for (;;) {
        struct uc2_entry e;
        int rc = uc2_read_cdir(r->handle, &e);
        if (rc == UC2_End) break;
        if (rc < 0) goto fail;
        while (rc == UC2_TaggedEntry) {
            rc = uc2_get_tag(r->handle, &e, NULL, NULL, NULL);
            if (rc < 0) goto fail;
        }
        if (!safe_component(e.name)) goto fail;
        if (r->count >= SIZE_MAX / sizeof *r->items - 1) goto fail;
        void *grown = realloc(r->items, (r->count + 1) * sizeof *r->items);
        if (!grown) { *result = E_NO_MEMORY; goto fail; }
        r->items = grown;
        r->items[r->count++] = (struct item){ .entry = e };
    }
    if (uc2_finish_cdir(r->handle, NULL) < 0) goto fail;
    for (size_t i = 0; i < r->count; i++) {
        struct item *item = &r->items[i];
        char path[NAME_CAP];
        int n = parent_path(r, item->entry.dirid, path, sizeof path, 0);
        if (n < 0 || strlen(item->entry.name) >= sizeof path - (size_t)n) goto fail;
        strcpy(path + n, item->entry.name);
        item->path = strdup(path);
        if (!item->path) { *result = E_NO_MEMORY; goto fail; }
        if (!item->entry.is_dir)
            for (size_t j = 0; j < i; j++)
                if (!r->items[j].entry.is_dir && !strcmp(r->items[j].path, path))
                    r->items[j].version++;
    }
    /* A literal name;1 can collide with a generated historical-version name.
     * Windows also folds case. Reject ambiguous listings before extraction. */
    if (prepare_visible_names(r) < 0) goto fail;
    *result = 0;
    return r;
fail:
    CloseArchive(r);
    return NULL;
}

API void *__stdcall OpenArchive(tOpenArchiveData *ad)
{ return open_file(fopen(ad->ArcName, "rb"), &ad->OpenResult); }
API void *__stdcall OpenArchiveW(tOpenArchiveDataW *ad)
{ return open_file(_wfopen(ad->ArcName, L"rb"), &ad->OpenResult); }

static int next_header(struct reader *r, tHeaderDataExW *header)
{
    if (r->next >= r->count) return E_END_ARCHIVE;
    struct item *item = &r->items[r->next];
    memset(header, 0, sizeof *header);
    wcscpy(header->FileName, item->visible);
    header->FileAttr = item->entry.attr | (item->entry.is_dir ? FILE_ATTRIBUTE_DIRECTORY : 0);
    header->UnpSize = item->entry.is_dir ? 0 : item->entry.size;
    header->PackSize = item->entry.is_dir ? 0 : item->entry.csize;
    header->FileTime = item->entry.dos_time;
    r->next++;
    return 0;
}
API int __stdcall ReadHeaderExW(void *r, tHeaderDataExW *header)
{ return next_header(r, header); }

API int __stdcall ReadHeaderEx(void *handle, tHeaderDataEx *header)
{
    struct reader *r = handle;
    tHeaderDataExW wide;
    int rc = next_header(r, &wide);
    if (rc) return rc;
    memset(header, 0, sizeof *header);
    BOOL lost = FALSE;
    if (!WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide.FileName, -1,
                            header->FileName, 1024, NULL, &lost) || lost) {
        r->next--; return E_SMALL_BUF;
    }
    header->FileAttr = wide.FileAttr; header->FileTime = wide.FileTime;
    header->PackSize = wide.PackSize; header->UnpSize = wide.UnpSize;
    return 0;
}
API int __stdcall ReadHeader(void *handle, tHeaderData *header)
{
    tHeaderDataEx large;
    int rc = ReadHeaderEx(handle, &large);
    if (rc) return rc;
    if (strlen(large.FileName) >= sizeof header->FileName) {
        ((struct reader *)handle)->next--; return E_SMALL_BUF;
    }
    memset(header, 0, sizeof *header);
    strcpy(header->FileName, large.FileName);
    header->FileAttr = large.FileAttr; header->FileTime = large.FileTime;
    header->PackSize = (int)large.PackSize; header->UnpSize = (int)large.UnpSize;
    return 0;
}

struct output { struct reader *r; FILE *file; wchar_t *name; int aborted; };
static int write_data(void *ctx, const void *data, unsigned len)
{
    struct output *out = ctx;
    if (out->file && fwrite(data, 1, len, out->file) != len) return -1;
    if ((out->r->progress_w && !out->r->progress_w(out->name, (int)len)) ||
        (out->r->progress && !out->r->progress(NULL, (int)len))) {
        out->aborted = 1; return -1;
    }
    return 0;
}

API int __stdcall ProcessFileW(void *handle, int op, WCHAR *destpath, WCHAR *destname)
{
    struct reader *r = handle;
    if (!r->next || r->next > r->count) return E_BAD_DATA;
    if (op == PK_SKIP) return 0;
    if (op != PK_TEST && op != PK_EXTRACT) return E_NOT_SUPPORTED;
    struct item *item = &r->items[r->next - 1];
    wchar_t path[32768] = L"", temporary[32768];
    FILE *file = NULL;
    if (op == PK_EXTRACT) {
        if (!destname || (destpath && wcslen(destpath) + wcslen(destname) + 2 >= 32768))
            return E_ECREATE;
        int n = _snwprintf(path, 32768, L"%ls%ls%ls", destpath ? destpath : L"",
                          destpath && *destpath && destpath[wcslen(destpath)-1] != L'\\' ? L"\\" : L"", destname);
        if (n < 0 || n >= 32768 - 80) return E_SMALL_BUF;
        if (item->entry.is_dir) {
            if (_wmkdir(path) == 0) return 0;
            DWORD attr = GetFileAttributesW(path);
            return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) ? 0 : E_ECREATE;
        }
        HANDLE h = INVALID_HANDLE_VALUE;
        for (unsigned attempt = 0; attempt < 128; attempt++) {
            _snwprintf(temporary, 32768, L"%ls.uc2-%lu-%u.tmp", path, GetCurrentProcessId(), attempt);
            h = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
            if (h != INVALID_HANDLE_VALUE || GetLastError() != ERROR_FILE_EXISTS) break;
        }
        if (h == INVALID_HANDLE_VALUE) return E_ECREATE;
        int fd = _open_osfhandle((intptr_t)h, _O_BINARY | _O_WRONLY);
        if (fd < 0) { CloseHandle(h); DeleteFileW(temporary); return E_ECREATE; }
        file = _fdopen(fd, "wb");
        if (!file) { _close(fd); DeleteFileW(temporary); return E_ECREATE; }
    }
    if (item->entry.is_dir) return 0;
    struct output out = { r, file, path, 0 };
    int rc = uc2_extract(r->handle, &item->entry.xi, item->entry.size, write_data, &out);
    int closed = file ? fclose(file) : 0;
    if (rc < 0 || closed || out.aborted) {
        if (file) DeleteFileW(temporary);
        return out.aborted ? E_EABORTED : (rc == UC2_UserFault || closed ? E_EWRITE : E_BAD_DATA);
    }
    if (file && !MoveFileExW(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(temporary); return E_EWRITE;
    }
    return 0;
}
API int __stdcall ProcessFile(void *r, int op, char *path, char *name)
{
    wchar_t wp[32768], wn[32768];
    if ((path && !MultiByteToWideChar(CP_ACP, 0, path, -1, wp, 32768)) ||
        (name && !MultiByteToWideChar(CP_ACP, 0, name, -1, wn, 32768))) return E_SMALL_BUF;
    return ProcessFileW(r, op, path ? wp : NULL, name ? wn : NULL);
}
API int __stdcall GetPackerCaps(void)
{ return PK_CAPS_MULTIPLE | PK_CAPS_BY_CONTENT | PK_CAPS_SEARCHTEXT; }
API BOOL __stdcall CanYouHandleThisFileW(WCHAR *path)
{
    unsigned char magic[4];
    FILE *f = _wfopen(path, L"rb");
    if (!f) return FALSE;
    size_t n = fread(magic, 1, sizeof magic, f); fclose(f);
    return n == 4 && uc2_identify(magic, 4) > 0;
}
API BOOL __stdcall CanYouHandleThisFile(char *path)
{
    wchar_t wide[32768];
    return MultiByteToWideChar(CP_ACP, 0, path, -1, wide, 32768) ? CanYouHandleThisFileW(wide) : FALSE;
}
API void __stdcall SetChangeVolProc(void *r, tChangeVolProc cb) { (void)r; (void)cb; }
API void __stdcall SetChangeVolProcW(void *r, tChangeVolProcW cb) { (void)r; (void)cb; }
API void __stdcall SetProcessDataProc(void *r, tProcessDataProc cb) { ((struct reader *)r)->progress = cb; }
API void __stdcall SetProcessDataProcW(void *r, tProcessDataProcW cb) { ((struct reader *)r)->progress_w = cb; }
