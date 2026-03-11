/* POSIX utime.h for MSVC (which only provides sys/utime.h) */
#ifndef _COMPAT_UTIME_H
#define _COMPAT_UTIME_H

#include <time.h>

struct utimbuf {
	time_t actime;
	time_t modtime;
};

int utime(const char *path, struct utimbuf *ut);

#endif
