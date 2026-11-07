/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef UC2_CHUNKS_H
#define UC2_CHUNKS_H

/* Revival-only recipe format. See docs/chunk-sharing.md for the wire layout. */
#define UC2_CHUNK_METHOD 11
#define UC2_CHUNK_VERSION 300
#define UC2_CHUNK_MAGIC "UC2CHN1\0"
#define UC2_CHUNK_HEADER 88u
#define UC2_CHUNK_RECORD 48u
#define UC2_CHUNK_MAX 32768u
#define UC2_CHUNK_LIMIT 1048576u

#endif
