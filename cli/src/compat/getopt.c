/* Minimal POSIX getopt() for MSVC.
   Supports short options with optional arguments (e.g., "d:"). */

#include <stdio.h>
#include <string.h>
#include "include/msvc/getopt.h"

char *optarg;
int optind = 1, opterr = 1, optopt;

int getopt(int argc, char *const argv[], const char *optstring)
{
	static int optpos = 0;

	if (optind >= argc || !argv[optind])
		return -1;

	if (argv[optind][0] != '-' || !argv[optind][1])
		return -1;

	if (argv[optind][1] == '-' && !argv[optind][2]) {
		optind++;
		return -1;
	}

	if (!optpos)
		optpos = 1;

	int c = argv[optind][optpos];
	const char *p = strchr(optstring, c);

	if (!p || c == ':') {
		optopt = c;
		if (opterr && optstring[0] != ':')
			fprintf(stderr, "%s: invalid option -- '%c'\n", argv[0], c);
		if (!argv[optind][++optpos]) {
			optind++;
			optpos = 0;
		}
		return '?';
	}

	if (p[1] == ':') {
		if (argv[optind][optpos + 1]) {
			optarg = &argv[optind][optpos + 1];
		} else if (++optind >= argc) {
			optopt = c;
			if (opterr && optstring[0] != ':')
				fprintf(stderr, "%s: option requires an argument -- '%c'\n",
				        argv[0], c);
			optpos = 0;
			return optstring[0] == ':' ? ':' : '?';
		} else {
			optarg = argv[optind];
		}
		optind++;
		optpos = 0;
	} else {
		if (!argv[optind][++optpos]) {
			optind++;
			optpos = 0;
		}
	}

	return c;
}
