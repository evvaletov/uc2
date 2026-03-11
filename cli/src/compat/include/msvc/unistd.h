/* Minimal POSIX unistd.h for MSVC */
#ifndef _COMPAT_UNISTD_H
#define _COMPAT_UNISTD_H

#include <io.h>
#include <direct.h>

#ifndef F_OK
#define F_OK 0
#endif
#ifndef R_OK
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef X_OK
#define X_OK 0
#endif

#ifndef PATH_MAX
#define PATH_MAX 260
#endif

/* Provided by compat_win32.c (UTF-8-aware via wide-char APIs) */
int access(const char *path, int mode);
int unlink(const char *path);
int chdir(const char *path);
int mkdir(const char *path, int mode);
int chmod(const char *path, int mode);

#endif
