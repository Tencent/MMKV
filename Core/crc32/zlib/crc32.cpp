/* crc32.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-2006, 2010, 2011, 2012, 2016 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 * Thanks to Rodney Brown <rbrown64@csc.com.au> for his contribution of faster
 * CRC methods: exclusive-oring 32 bits of data at a time, and pre-computing
 * tables for updating the shift register in one step with three exclusive-ors
 * instead of four steps with four exclusive-ors.  This results in about a
 * factor of two increase in speed on a Power PC G4 (PPC7455) using gcc -O3.
 */

/* @(#) $Id$ */

#include "../../MMKVPredef.h"

#if MMKV_EMBED_ZLIB

#include "zutil.h"      /* for STDC and FAR definitions */

#define Z_NULL 0   /* for initializing zalloc, zfree, opaque */
#  define TBLS 1

#include "crc32.h"

namespace zlib {

/* ========================================================================= */
#define DO1 crc = crc_table[0][((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8)
#define DO8 DO1; DO1; DO1; DO1; DO1; DO1; DO1; DO1

/* ========================================================================= */
unsigned long local crc32_z(unsigned long crc, const unsigned char FAR *buf, z_size_t len)
{
    if (buf == Z_NULL) return 0UL;

    crc = crc ^ 0xffffffffUL;
    while (len >= 8) {
        DO8;
        len -= 8;
    }
    if (len) do {
        DO1;
    } while (--len);
    return crc ^ 0xffffffffUL;
}

/* ========================================================================= */

unsigned long ZEXPORT crc32(unsigned long crc, const unsigned char FAR *buf, z_size_t len) {
    return crc32_z(crc, buf, len);
}

} // namespace zlib

#endif // MMKV_EMBED_ZLIB