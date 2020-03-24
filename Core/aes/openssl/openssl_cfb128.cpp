/*
 * Copyright 2008-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "openssl_aes.h"
#include <string.h>

namespace openssl {

/*
 * The input and output encrypted as though 128bit cfb mode is being used.
 * The extra state information to record how much of the 128bit block we have
 * used is contained in *num;
 */
void AES_cfb128_encrypt(const unsigned char *in, unsigned char *out,
                        size_t len, const AES_KEY *key,
                        unsigned char ivec[16], int *num)
{
    auto n = *num;

    while (n && len) {
        *(out++) = ivec[n] ^= *(in++);
        --len;
        n = (n + 1) % 16;
    }
    while (len >= 16) {
        AES_encrypt(ivec, ivec, key);
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
        AES_encrypt(ivec, ivec, key);
        while (len--) {
            out[n] = ivec[n] ^= in[n];
            ++n;
        }
    }

    *num = n;
}

/*
* The input and output encrypted as though 128bit cfb mode is being used.
* The extra state information to record how much of the 128bit block we have
* used is contained in *num;
*/
void AES_cfb128_decrypt(const unsigned char *in, unsigned char *out,
                        size_t len, const AES_KEY *key,
                        unsigned char ivec[16], int *num)
{
    auto n = *num;

    while (n && len) {
        unsigned char c = *(in++);
        *(out++) = ivec[n] ^ c;
        ivec[n] = c;
        --len;
        n = (n + 1) % 16;
    }
    while (len >= 16) {
        AES_encrypt(ivec, ivec, key);
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
        AES_encrypt(ivec, ivec, key);
        while (len--) {
            unsigned char c = in[n];
            out[n] = ivec[n] ^ c;
            ivec[n] = c;
            ++n;
        }
    }

    *num = n;
}

} // namespace openssl
