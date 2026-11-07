/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef UC2_PATH_H
#define UC2_PATH_H
#include <stdio.h>
#include <stddef.h>
/* Open a normal archive, original SAS volumes, or DOS UC2SEA data.
 * No executable is run. SAS opens siblings of the selected .pNN/.PNN path;
 * headers never supply host paths. Caller owns the returned seekable FILE.
 * SAS input is bounded to UINT32_MAX payload bytes and 999 parts. The splitter
 * stores no set identifier/checksum: native archive verification is still
 * required to detect same-size mixed sets or altered payloads. UC2SEA is
 * normalized into a temporary UC2 container; .EXE/.exe + .001..999 siblings,
 * 512 MiB compressed/decoded limits, no executable code or startup execution. */
FILE *uc2_open_path(const char *path, char *error, size_t error_size);
#endif
