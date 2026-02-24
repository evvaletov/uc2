#ifndef NO_OLDNAMES
#define NO_OLDNAMES
#endif
#include_next <unistd.h>
int access(const char *path, int mode);
int unlink(const char *path);
int chdir(const char *path);
