/* Minimal POSIX getopt for MSVC */
#ifndef _COMPAT_GETOPT_H
#define _COMPAT_GETOPT_H

extern char *optarg;
extern int optind, opterr, optopt;

int getopt(int argc, char *const argv[], const char *optstring);

#endif
