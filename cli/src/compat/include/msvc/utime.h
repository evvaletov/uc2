/* SPDX-License-Identifier: GPL-3.0-or-later */

/* POSIX utime.h for MSVC.
 *
 * Modern MSVC SDKs (Windows 10 SDK 10.0.26100+) provide both
 * struct utimbuf and an inline wrapper named utime() in <sys/utime.h>.
 * The inline wrapper is not UTF-8-aware: it forwards to _utime32,
 * which interprets the path in the local ANSI codepage.
 *
 * UC2 needs UTF-8 paths to round-trip correctly, so this shim
 * substitutes utime() with our compat__utime(), which goes through
 * compat__wpath() before calling _wutime32. */

#ifndef _COMPAT_UTIME_H
#define _COMPAT_UTIME_H

#include <sys/utime.h>

#ifdef _COMPAT_UTIMBUF_FALLBACK
#include <time.h>
struct utimbuf {
	time_t actime;
	time_t modtime;
};
#endif

int compat__utime(const char *path, struct utimbuf *ut);
#define utime compat__utime

#endif
