/* Plugin for Total Commander handling UltraCompressor II archives.
   Copyright © Jan Bobrowski 2021
   torinak.com/~jb/unuc2/

   This program is free software; you can redistribute it and
   modify it under the terms of the GNU Lesser General Public
   License version 3 as published by the Free Software Foundation.
*/

#include <windows.h>
#include <stdio.h>

// git clone https://github.com/ghisler/WCX-SDK.git
#include "WCX-SDK/src/wcxhead.h"

#include "libunuc2.h"
#include "list.h"

#define elemof(T) (sizeof T/sizeof*T)

struct uc2_wcx {
	uc2_handle uc2;
	struct list entries;
	struct list *current;
	FILE *file;
};

struct entry {
	struct list list;
	struct entry *dir;
	unsigned id;
	unsigned size, csize, time, version;
	struct uc2_xinfo xi;
	unsigned name_len;
	unsigned char attr;
	char name[];
};

static int my_read(void *ctx, unsigned pos, void *ptr, unsigned len)
{
	struct uc2_wcx *wcx = ctx;
	if (fseek(wcx->file, pos, SEEK_SET) < 0)
		return -1;
	return fread(ptr, 1, len, wcx->file);
}

static void *my_alloc(void *ctx, unsigned size)
{
	return malloc(size);
}

static void my_free(void *ctx, void *ptr)
{
	free(ptr);
}

static struct uc2_io io = {
	.read = my_read,
	.alloc = my_alloc,
	.free = my_free
};

static struct uc2_wcx *open_archive(FILE *f, int *result)
{
	int r = E_EOPEN;
	if (!f)
		goto error;
	struct uc2_wcx *wcx = malloc(sizeof *wcx);
	r = E_NO_MEMORY;
	if (!wcx)
		goto error_close;
	list_init(&wcx->entries);
	wcx->current = &wcx->entries;
	wcx->file = f;

	uc2_handle uc2 = uc2_open(&io, wcx);
	if (!uc2)
		goto error_free;
	wcx->uc2 = uc2;

	for (;;) {
		struct uc2_entry ue;
		int r = uc2_read_cdir(uc2, &ue);
		if (r < 0 || r == UC2_End) {
			if (r == UC2_End)
				uc2_finish_cdir(uc2, 0);
			break;
		}
		if (r == UC2_TaggedEntry) {
			r = uc2_get_tag(uc2, &ue, 0, 0, 0);
			if (r < 0)
				break;
		}

		struct entry *e = malloc(sizeof *e + ue.name_len + 1);
		if (!e) {
			r = UC2_UserFault; // ⇒ E_NO_MEMORY
			break;
		}
		e->id = ue.id;
		e->name_len = ue.name_len;
		e->attr = ue.attr;
		memcpy(e->name, ue.name, ue.name_len + 1);
		e->size = ue.size;
		e->csize = ue.csize;
		e->time = ue.dos_time;
		e->dir = 0;
		for (struct list *l = wcx->entries.next; l != &wcx->entries; l = l->next) {
			struct entry *e0 = list_item(l, struct entry, list);
			if (e0->id && e0->id == ue.dirid)
				e->dir = e0;
			if (e->name_len == e0->name_len
			 && memcmp(e->name, e0->name, e->name_len) == 0)
				e0->version++;
		}
		e->version = 0;
		list_append(&wcx->entries, &e->list);
		e->xi = ue.xi;
	}

	if (r >= 0 || !list_is_empty(&wcx->entries))
		return wcx;

	r = r == UC2_UserFault ? E_NO_MEMORY : E_BAD_ARCHIVE;
	uc2_close(uc2);
error_free:
	free(wcx);
error_close:
	fclose(f);
error:
	*result = r;
	return 0;
}

__declspec(dllexport) void *__stdcall OpenArchive(tOpenArchiveData *ad)
{
	FILE *f = fopen(ad->ArcName, "rb");
	return open_archive(f, &ad->OpenResult);
}

__declspec(dllexport) void *__stdcall OpenArchiveW(tOpenArchiveDataW* ad)
{
	FILE *f = _wfopen(ad->ArcName, L"rb");
	return open_archive(f, &ad->OpenResult);
}

static struct entry *next_entry(struct uc2_wcx *wcx)
{
	struct list *l = wcx->current->next;
	wcx->current = l;
	if (l == &wcx->entries)
		return 0;
	return list_item(l, struct entry, list);
}

static char *prepare_name(struct uc2_wcx *wcx, char *buf, unsigned space, struct entry *e)
{
	char *p = buf + space;
	*--p = 0;

	if (e->version) {
		p -= _scprintf(";%u", e->version);
		_snprintf(p, INT_MAX, ";%u", e->version);
	}

	for (;;) {
		unsigned l = e->name_len;
		if (p - buf < l)
			return p;
		p -= l;
		memcpy(p, e->name, l);
		if (!e->dir || p - buf < 2)
			return p;

		*--p = '\\';
		e = e->dir;
	}
}

__declspec(dllexport) int __stdcall ReadHeader(void *handle, tHeaderData *hd)
{
	struct uc2_wcx *wcx = handle;
	struct entry *e = next_entry(wcx);
	if (!e)
		return E_END_ARCHIVE;

	char buf[elemof(hd->FileName)];
	char *name = prepare_name(wcx, buf, sizeof buf, e);
	memcpy(hd->FileName, name, buf + sizeof buf - name);

	hd->FileAttr = e->attr;
	hd->UnpSize = e->size;
	hd->PackSize = e->csize;
	hd->FileTime = e->time;
	return 0;
}

__declspec(dllexport) int __stdcall ReadHeaderExW(void *handle, tHeaderDataExW *hd)
{
	struct uc2_wcx *wcx = handle;
	struct entry *e = next_entry(wcx);
	if (!e)
		return E_END_ARCHIVE;

	char buf[1024];
	char *name = prepare_name(wcx, buf, sizeof buf, e);

	MultiByteToWideChar(CP_UTF8, 0, name, -1, hd->FileName, elemof(hd->FileName));
	hd->FileAttr = e->attr;
	hd->UnpSize = e->size; hd->PackSizeHigh = 0;
	hd->PackSize = e->csize; hd->UnpSizeHigh = 0;
	hd->FileTime = e->time;
	return 0;
}

static int write_file(void *file, const void *ptr, unsigned len)
{
	if (file) {
		if (fwrite(ptr, 1, len, file) < len)
			return -1;
	}
	return 0;
}

__declspec(dllexport) int __stdcall ProcessFile(void *handle, int op, char *DestPath, char *DestName)
{
	if (op == PK_SKIP)
		return 0;

	struct uc2_wcx *wcx = handle;
	FILE *f = 0;
	if (op == PK_EXTRACT) {
		f = fopen(DestName, "wb");
		if (!f)
			return E_ECREATE;
	}
	struct entry *e = list_item(wcx->current, struct entry, list);
	int r = uc2_extract(wcx->uc2, &e->xi, e->size, write_file, f);
	if (f)
		fclose(f);
	if (r < 0)
		return r == UC2_UserFault ? E_EWRITE : E_BAD_DATA;
	return 0;
}

__declspec(dllexport) int __stdcall CloseArchive(void *handle)
{
	struct uc2_wcx *wcx = handle;
	uc2_close(wcx->uc2);
	fclose(wcx->file);
	for (struct list *l = wcx->entries.next; l != &wcx->entries;) {
		struct entry *e = list_item(l, struct entry, list);
		l = l->next;
		free(e);
	}
	free(wcx);
	return 0;
}

__declspec(dllexport) void __stdcall SetChangeVolProc(void *handle, tChangeVolProc pChangeVolProc1) {}

__declspec(dllexport) void __stdcall SetProcessDataProc(void *handle, tProcessDataProc pProcessDataProc) {}
