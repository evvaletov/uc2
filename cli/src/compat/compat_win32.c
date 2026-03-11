/* Win32 compatibility layer for UC2 CLI.
   Provides POSIX/BSD functions missing from MSVC and MinGW.
   All file operations use wide-char Windows APIs for UTF-8 support.
   Copyright (c) Jan Bobrowski 2020 / Licence: LGPL */

#define NO_OLDNAMES
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#ifdef g_err
#include "err.h"
void err(int x, const char* f, ...)
{
	va_list a;
	va_start(a, f);
	vwarn(f, a);
	va_end(a);
	exit(x);
}
#endif

#ifdef g_errx
#include "err.h"
void errx(int x, const char* f, ...)
{
	va_list a;
	va_start(a, f);
	vwarnx(f, a);
	va_end(a);
	exit(x);
}
#endif

#ifdef g_warn
#include "err.h"
void warn(const char* f, ...)
{
	va_list a;
	va_start(a, f);
	vwarn(f, a);
	va_end(a);
}
#endif

#ifdef g_warnx
#include "err.h"
void warnx(const char* f, ...)
{
	va_list a;
	va_start(a, f);
	vwarnx(f, a);
	va_end(a);
}
#endif

#ifdef g_vwarn
#include "err.h"
void vwarn(const char* f, va_list a)
{
	const char *getprogname(void);
	const char *p = getprogname();
	if (*p)
		fprintf(stderr, "%s: ", p);
	if (f) {
		vfprintf(stderr, f, a);
		fprintf(stderr, ": ");
	}
	fflush(stderr);
	perror(0);
}
#endif

#ifdef g_vwarnx
#include "err.h"
void vwarnx(const char* f, va_list a)
{
	const char *getprogname(void);
	const char *p = getprogname();
	if (*p)
		fprintf(stderr, "%s: ", p);
	if (f) {
		fprintf(stderr, ": ");
		vfprintf(stderr, f, a);
	}
	fprintf(stderr, "\n");
	fflush(stderr);
}
#endif

#ifdef g_verr
#include "err.h"
void verr(int x, const char* f, va_list a)
{
	vwarn(f, a);
	exit(x);
}
#endif

#ifdef g_verrx
#include "err.h"
void verrx(int x, const char* f, va_list a)
{
	vwarnx(f, a);
	exit(x);
}
#endif

#ifdef g_getprogname
const char *getprogname(void)
{
	static char name[256];
	if (!name[0]) {
#ifdef _WIN32
		char *p = _pgmptr;
		char *q = p;
		int n;
		while (*q) {
			q++;
			if (q[-1]=='/' || q[-1]=='\\')
				p = q;
		}
		n = q - p;
		if (n > 4 && q[-4]=='.' && q[-3]=='e' && q[-2]=='x' && q[-1]=='e')
			n -= 4;
		if (n > 255)
			n = 255;
		memcpy(name, p, n);
		name[n] = 0;
#endif
	}
	return name;
}
#endif

#ifdef g_setlinebuf
void setlinebuf(FILE *f)
{
	setvbuf(f, NULL, _IOLBF, 0);
}
#endif

#ifdef g_fnmatch
#include "fnmatch.h"

enum {
	Match = 0,
	NoMatch = FNM_NOMATCH
};

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
				return NoMatch;
			if (!c)
				return Match;
			continue;
		case '?':
			c = *string++;
			if (!c || (flags & FNM_PATHNAME && c == '/'))
				return NoMatch;
			continue;
		case '*':
			do {
				if (fnmatch(pattern, string, flags) == Match)
					return Match;
				if (flags & FNM_PATHNAME && *string == '/')
					return NoMatch;
			} while (*string++);
			return NoMatch;
		case '[':;
			const char *p = pattern;
			if (!*pattern++)
				return NoMatch;
			for (;;) {
				c = *pattern;
				if (c == ']')
					break;
				if (!c)
					return NoMatch;
				pattern++;
			}
			c = *string++;
			if (flags & FNM_PATHNAME && c == '/')
				return NoMatch;
			for (;;) {
				if (c == *p++)
					break;
				if (*p == '-' && p+1 < pattern) {
					if (p[-1] <= c && c <= p[1])
						break;
					p++;
				}
				if (p == pattern)
					return NoMatch;
			}
			pattern++;
			continue;
		}
	}
}
#endif

#ifdef _WIN32
#include <windows.h>
#include <wchar.h>

wchar_t *compat__wpath(const char *path);

#ifdef g_compat__utf8_console
#include <fcntl.h>
#ifdef _MSC_VER
/* MSVC: use CRT initializer table (.CRT$XCU) instead of GCC constructor */
static void __cdecl compat__utf8_console_init(void)
{
	setvbuf(stdout, 0, _IOFBF, 1<<16);
	setvbuf(stderr, 0, _IOFBF, 1<<16);
	SetConsoleOutputCP(CP_UTF8);
}
#pragma section(".CRT$XCU", read)
__declspec(allocate(".CRT$XCU"))
static void (__cdecl *compat__utf8_console_p)(void) = compat__utf8_console_init;
#else
__attribute__((constructor))
void compat__utf8_console(void)
{
	setvbuf(stdout, 0, _IOFBF, 1<<16);
	setvbuf(stderr, 0, _IOFBF, 1<<16);
	SetConsoleOutputCP(CP_UTF8);
}
#endif
#endif

#ifdef g_compat__wpath
wchar_t *compat__wpath(const char *path)
{
	static wchar_t wpath[MAX_PATH];
	if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, MAX_PATH))
		return 0;
	return wpath;
}
#endif

#ifdef g_fopen
FILE *fopen(const char *name, const char *mode)
{
	wchar_t wname[MAX_PATH], wmode[16];
	if (!MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, MAX_PATH)
	 || !MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode, MAX_PATH))
		return 0;
	return _wfopen(wname, wmode);
}
#endif

#ifdef g_access
int access(const char *path, int mode)
{
	wchar_t *wpath = compat__wpath(path);
	return wpath ? _waccess(wpath, mode) : -1;
}
#endif

#ifdef g_unlink
int unlink(const char *path)
{
	wchar_t *wpath = compat__wpath(path);
	return wpath ? _wunlink(wpath) : -1;
}
#endif

#ifdef g_chdir
int chdir(const char *path)
{
	wchar_t *wpath = compat__wpath(path);
	return wpath ? _wchdir(wpath) : -1;
}
#endif

#ifdef g_mkdir
int mkdir(const char *path, int mode)
{
	wchar_t *wpath = compat__wpath(path);
	if (!wpath)
		return -1;
	int r = _wmkdir(wpath);
	if (r >= 0)
		r = _wchmod(wpath, mode);
	return r;
}
#endif

#ifdef g_chmod
int chmod(const char *path, int mode)
{
	wchar_t *wpath = compat__wpath(path);
	return wpath ? _wchmod(wpath, mode) : -1;
}
#endif

#ifdef g_utime
#include <sys/utime.h>
#ifdef _MSC_VER
/* MSVC's <sys/utime.h> hides utimbuf behind NO_OLDNAMES */
#include <time.h>
struct utimbuf { time_t actime; time_t modtime; };
#endif
int utime(const char *path, struct utimbuf *ut)
{
	wchar_t *wpath = compat__wpath(path);
	if (!wpath)
		return -1;
	struct __utimbuf32 wut = {.actime = (long)ut->actime, .modtime = (long)ut->modtime};
	return _wutime32(wpath, &wut);
}
#endif

#endif
