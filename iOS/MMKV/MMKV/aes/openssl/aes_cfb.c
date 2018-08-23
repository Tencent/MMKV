/*
 * Copyright 2002-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "aes.h"
#include "modes.h"

/*
 * The input and output encrypted as though 128bit cfb mode is being used.
 * The extra state information to record how much of the 128bit block we have
 * used is contained in *num;
 */

void AES_cfb128_encrypt(const unsigned char *in, unsigned char *out,
                        size_t length, const AES_KEY *key,
                        unsigned char *ivec, int *num, const int enc)
{

    CRYPTO_cfb128_encrypt(in, out, length, key, ivec, num, enc,
                          (block128_f) AES_encrypt);
}
