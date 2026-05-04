/* SPDX-License-Identifier: GPL-3.0-or-later */

/* Minimal POSIX dirent.h for MSVC.
 *
 * Implements only what UC2's archive scanner uses: opendir, readdir,
 * closedir, and a struct dirent with d_name.  UTF-8 paths are
 * round-tripped through the wide-char Win32 APIs to match the rest of
 * the compat layer.  d_name is sized to hold a Windows MAX_PATH-long
 * filename re-encoded to UTF-8 (worst case: 4 bytes per code point). */

#ifndef _COMPAT_DIRENT_H
#define _COMPAT_DIRENT_H

#include <stddef.h>

#define UC2_DIRENT_NAME_MAX  1024  /* 260 wide chars * 4 (UTF-8) rounded up */

struct dirent {
	char d_name[UC2_DIRENT_NAME_MAX];
};

typedef struct UC2_DIR DIR;

DIR *opendir(const char *path);
struct dirent *readdir(DIR *d);
int closedir(DIR *d);

#endif
