#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Reproduce Windows' refusal to unlink a file that this process still holds. */
int unlink(const char *path)
{
    int (*real_unlink)(const char *) = dlsym(RTLD_NEXT, "unlink");
    struct stat target;
    if (strstr(path, ".tmp-") && stat(path, &target) == 0) {
        for (int fd = 0; fd < 256; fd++) {
            struct stat held;
            if (fstat(fd, &held) == 0 && target.st_dev == held.st_dev && target.st_ino == held.st_ino) {
                errno = EACCES;
                return -1;
            }
        }
    }
    return real_unlink(path);
}

/* A real close followed by a reported flush failure checks ownership release. */
int fclose(FILE *stream)
{
    int (*real_close)(FILE *) = dlsym(RTLD_NEXT, "fclose");
    char link[64], path[4096];
    snprintf(link, sizeof link, "/proc/self/fd/%d", fileno(stream));
    ssize_t n = readlink(link, path, sizeof path - 1);
    if (n >= 0) path[n] = 0;
    int fail = n >= 0 && strstr(path, ".tmp-") && getenv("UC2_TEST_CLOSE_ERROR");
    int result = real_close(stream);
    if (fail) { errno = EIO; return EOF; }
    return result;
}
