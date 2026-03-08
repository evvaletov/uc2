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

#include "fnmatch.h"

int fnmatch(const char *pattern, const char *string, int flags)
{
	for (;;) {
		char c = *pattern++;
		switch (c) {
		case '\\':
			if (*pattern && !(flags & FNM_NOESCAPE))
				c = *pattern++;
		default:
			if (c != *string++)
				return FNM_NOMATCH;
			if (!c)
				return 0;
			continue;
		case '?':
			c = *string++;
			if (!c || (flags & FNM_PATHNAME && c == '/'))
				return FNM_NOMATCH;
			continue;
		case '*':
			do {
				if (fnmatch(pattern, string, flags) == 0)
					return 0;
				if (flags & FNM_PATHNAME && *string == '/')
					return FNM_NOMATCH;
			} while (*string++);
			return FNM_NOMATCH;
		case '[':;
			const char *p = pattern;
			if (!*pattern++)
				return FNM_NOMATCH;
			for (;;) {
				c = *pattern;
				if (c == ']')
					break;
				if (!c)
					return FNM_NOMATCH;
				pattern++;
			}
			c = *string++;
			if (flags & FNM_PATHNAME && c == '/')
				return FNM_NOMATCH;
			for (;;) {
				if (c == *p++)
					break;
				if (*p == '-' && p + 1 < pattern) {
					if (p[-1] <= c && c <= p[1])
						break;
					p++;
				}
				if (p == pattern)
					return FNM_NOMATCH;
			}
			pattern++;
			continue;
		}
	}
}
