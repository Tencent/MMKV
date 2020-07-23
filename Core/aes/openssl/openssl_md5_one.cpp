/*
 * Copyright 1995-2016 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the OpenSSL license (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include <stdio.h>
#include <string.h>
#include "openssl_md5.h"

namespace openssl {

unsigned char *MD5(const unsigned char *d, size_t n, unsigned char *md)
{
    MD5_CTX c;
    static unsigned char m[MD5_DIGEST_LENGTH];

    if (md == nullptr)
        md = m;
    if (!MD5_Init(&c))
        return nullptr;
    MD5_Update(&c, d, n);
    MD5_Final(md, &c);
    return md;
}

} // namespace openssl
