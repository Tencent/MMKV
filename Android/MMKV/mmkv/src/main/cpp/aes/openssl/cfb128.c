/*
 * Copyright 2008-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "aes.h"
#include "modes.h"
#include <string.h>

/*
 * The input and output encrypted as though 128bit cfb mode is being used.
 * The extra state information to record how much of the 128bit block we have
 * used is contained in *num;
 */
void CRYPTO_cfb128_encrypt(const unsigned char *in, unsigned char *out,
                           size_t len, const void *key,
                           unsigned char ivec[16], int *num,
                           int enc, block128_f block)
{
    unsigned int n;
    size_t l = 0;

    n = *num;

    if (enc) {
#if !defined(OPENSSL_SMALL_FOOTPRINT)
        if (16 % sizeof(size_t) == 0) { /* always true actually */
            do {
                while (n && len) {
                    *(out++) = ivec[n] ^= *(in++);
                    --len;
                    n = (n + 1) % 16;
                }
# if defined(STRICT_ALIGNMENT)
                if (((size_t)in | (size_t)out | (size_t)ivec) %
                    sizeof(size_t) != 0)
                    break;
# endif
                while (len >= 16) {
                    (*block) (ivec, ivec, key);
                    for (; n < 16; n += sizeof(size_t)) {
                        *(size_t *)(out + n) =
                            *(size_t *)(ivec + n) ^= *(size_t *)(in + n);
                    }
                    len -= 16;
                    out += 16;
                    in += 16;
                    n = 0;
                }
                if (len) {
                    (*block) (ivec, ivec, key);
                    while (len--) {
                        out[n] = ivec[n] ^= in[n];
                        ++n;
                    }
                }
                *num = n;
                return;
            } while (0);
        }
        /* the rest would be commonly eliminated by x86* compiler */
#endif
        while (l < len) {
            if (n == 0) {
                (*block) (ivec, ivec, key);
            }
            out[l] = ivec[n] ^= in[l];
            ++l;
            n = (n + 1) % 16;
        }
        *num = n;
    } else {
#if !defined(OPENSSL_SMALL_FOOTPRINT)
        if (16 % sizeof(size_t) == 0) { /* always true actually */
            do {
                while (n && len) {
                    unsigned char c;
                    *(out++) = ivec[n] ^ (c = *(in++));
                    ivec[n] = c;
                    --len;
                    n = (n + 1) % 16;
                }
# if defined(STRICT_ALIGNMENT)
                if (((size_t)in | (size_t)out | (size_t)ivec) %
                    sizeof(size_t) != 0)
                    break;
# endif
                while (len >= 16) {
                    (*block) (ivec, ivec, key);
                    for (; n < 16; n += sizeof(size_t)) {
                        size_t t = *(size_t *)(in + n);
                        *(size_t *)(out + n) = *(size_t *)(ivec + n) ^ t;
                        *(size_t *)(ivec + n) = t;
                    }
                    len -= 16;
                    out += 16;
                    in += 16;
                    n = 0;
                }
                if (len) {
                    (*block) (ivec, ivec, key);
                    while (len--) {
                        unsigned char c;
                        out[n] = ivec[n] ^ (c = in[n]);
                        ivec[n] = c;
                        ++n;
                    }
                }
                *num = n;
                return;
            } while (0);
        }
        /* the rest would be commonly eliminated by x86* compiler */
#endif
        while (l < len) {
            unsigned char c;
            if (n == 0) {
                (*block) (ivec, ivec, key);
            }
            out[l] = ivec[n] ^ (c = in[l]);
            ivec[n] = c;
            ++l;
            n = (n + 1) % 16;
        }
        *num = n;
    }
}
