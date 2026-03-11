#define NO_OLDNAMES
#include_next <sys/utime.h>

struct utimbuf {
	time_t actime;
	time_t modtime;
};

int utime(const char *,struct utimbuf *);
