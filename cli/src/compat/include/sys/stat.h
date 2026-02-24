#ifndef NO_OLDNAMES
#define NO_OLDNAMES
#endif
#include_next <sys/stat.h>
int chmod(const char *path, mode_t mode);
int mkdir(const char *path, int mode);
