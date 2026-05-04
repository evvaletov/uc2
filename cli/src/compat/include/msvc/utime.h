/* SPDX-License-Identifier: GPL-3.0-or-later */

/* POSIX utime.h for MSVC (which only provides sys/utime.h).
 *
 * Modern MSVC SDKs (Windows 10 SDK 10.0.26100+) declare struct utimbuf
 * in <sys/utime.h>, so we just pull it in.  Older SDKs that hid the
 * struct behind _CRT_DECLARE_NONSTDC_NAMES (which our build disables
 * via NO_OLDNAMES) will not see it; for those, define
 * _COMPAT_UTIMBUF_FALLBACK at the compiler command line. */

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

int utime(const char *path, struct utimbuf *ut);

#endif
