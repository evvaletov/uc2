/* SPDX-License-Identifier: LGPL-3.0-only */

/* DOS/DJGPP compatibility layer for UC2.
   Provides BSD err.h functions and fnmatch for DJGPP,
   which lacks these POSIX/BSD extensions.
   Copyright © Jan Bobrowski 2020 / Licence: LGPL
   Adapted for DOS by Eremey Valetov 2026 */

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

/* err/errx/warn/warnx family */

#include "err.h"

static const char *_progname = "uc2";

const char *getprogname(void)
{
	return _progname;
}

void setprogname(const char *argv0)
{
	const char *p = argv0;
	for (const char *q = argv0; *q; q++)
		if (*q == '/' || *q == '\\')
			p = q + 1;
	_progname = p;
}

void vwarn(const char *f, va_list a)
{
	fprintf(stderr, "%s: ", getprogname());
	if (f) {
		vfprintf(stderr, f, a);
		fprintf(stderr, ": ");
	}
	fflush(stderr);
	perror(0);
}

void vwarnx(const char *f, va_list a)
{
	fprintf(stderr, "%s: ", getprogname());
	if (f)
		vfprintf(stderr, f, a);
	fprintf(stderr, "\n");
	fflush(stderr);
}

void warn(const char *f, ...)
{
	va_list a;
	va_start(a, f);
	vwarn(f, a);
	va_end(a);
}

void warnx(const char *f, ...)
{
	va_list a;
	va_start(a, f);
	vwarnx(f, a);
	va_end(a);
}

void verr(int x, const char *f, va_list a)
{
	vwarn(f, a);
	exit(x);
}

void verrx(int x, const char *f, va_list a)
{
	vwarnx(f, a);
	exit(x);
}

void err(int x, const char *f, ...)
{
	va_list a;
	va_start(a, f);
	verr(x, f, a);
}

void errx(int x, const char *f, ...)
{
	va_list a;
	va_start(a, f);
	verrx(x, f, a);
}

/* fnmatch */

#include "fnmatch_impl.h"
