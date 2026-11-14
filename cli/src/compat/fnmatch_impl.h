/* SPDX-License-Identifier: LGPL-3.0-only */
/* Shared Windows/DJGPP matcher, derived from the compatibility layers.
   Copyright © Jan Bobrowski 2020 / Licence: LGPL */

#include "fnmatch.h"

int fnmatch(const char *pattern, const char *string, int flags)
{
	for (;;) {
		char c = *pattern++;
		switch (c) {
		case '\\':
			if (*pattern && !(flags & FNM_NOESCAPE))
				c = *pattern++;
			/* fall through */
		default:
		literal:
			if (c != *string++) return FNM_NOMATCH;
			if (!c) return 0;
			continue;
		case '?':
			c = *string++;
			if (!c || (flags & FNM_PATHNAME && c == '/')) return FNM_NOMATCH;
			continue;
		case '*':
			do {
				if (fnmatch(pattern, string, flags) == 0) return 0;
				if (flags & FNM_PATHNAME && *string == '/') return FNM_NOMATCH;
			} while (*string++);
			return FNM_NOMATCH;
		case '[': {
			const char *p = pattern;
			int negate = *p == '!';
			if (negate) p++;
			const char *end = p;
			if (*end == ']') end++; /* A first ']' is a member of the set. */
			while (*end && *end != ']') {
				if (*end == '\\' && end[1] && !(flags & FNM_NOESCAPE)) end++;
				end++;
			}
			if (!*end) goto literal; /* An unmatched '[' is literal. */
			unsigned char target = (unsigned char)*string++;
			if (!target || (flags & FNM_PATHNAME && target == '/')) return FNM_NOMATCH;
			int matched = 0;
			while (p < end) {
				unsigned char low = (unsigned char)*p++;
				if (low == '\\' && p < end && !(flags & FNM_NOESCAPE)) low = *p++;
				if (p + 1 < end && *p == '-') {
					p++;
					unsigned char high = (unsigned char)*p++;
					if (high == '\\' && p < end && !(flags & FNM_NOESCAPE)) high = *p++;
					if (low <= target && target <= high) matched = 1;
				} else if (low == target) matched = 1;
			}
			if (matched == negate) return FNM_NOMATCH;
			pattern = end + 1;
			continue;
		}
		}
	}
}
