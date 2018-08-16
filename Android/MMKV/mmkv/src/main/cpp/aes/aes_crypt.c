/*
 * aes_crypt.c
 *
 *  Created on: 2013-7-17
 *      Author: zhouzhijie
 */

#include "aes_crypt.h"
#include <string.h>
#include <stdlib.h>
#include "openssl/aes.h"

#define AES_KEY_LEN 16
#define AES_KEY_BITSET_LEN 128

int aes_cfb_encrypt(const unsigned char* pKey, unsigned int uiKeyLen
        , const unsigned char* pInput, unsigned int uiInputLen
        , unsigned char** ppOutput, unsigned int* pOutputLen)
{
    if(pKey == NULL || uiKeyLen == 0 || pInput == NULL || uiInputLen == 0
       || pOutputLen == NULL || ppOutput == NULL) {
        return -1;
    }

    unsigned char iv[AES_KEY_LEN] = {0};
    memcpy(iv, pKey, (uiKeyLen > AES_KEY_LEN) ? AES_KEY_LEN : uiKeyLen);

    unsigned char keyBuf[AES_KEY_LEN] = {0};
    memcpy(keyBuf, pKey, (uiKeyLen > AES_KEY_LEN) ? AES_KEY_LEN : uiKeyLen);

    AES_KEY aesKey;
    int ret = AES_set_encrypt_key(keyBuf, AES_KEY_BITSET_LEN, &aesKey);
    if(ret != 0) {
        return -2;
    }

    *pOutputLen = uiInputLen;
    *ppOutput = malloc(uiInputLen);
    memset(*ppOutput, 0, uiInputLen);

    int num = 0;
    AES_cfb128_encrypt(pInput, *ppOutput, uiInputLen, &aesKey, iv, &num, AES_ENCRYPT);

    return 0;
}

int aes_cfb_decrypt(const unsigned char* pKey, unsigned int uiKeyLen
        , const unsigned char* pInput, unsigned int uiInputLen
        , unsigned char** ppOutput, unsigned int* pOutputLen)
{
    if(pKey == NULL || uiKeyLen == 0 || pInput == NULL || uiInputLen == 0
       || pOutputLen == NULL || ppOutput == NULL) {
        return -1;
    }

    unsigned char iv[AES_KEY_LEN] = {0};
    memcpy(iv, pKey, (uiKeyLen > AES_KEY_LEN) ? AES_KEY_LEN : uiKeyLen);

    unsigned char keyBuf[AES_KEY_LEN] = {0};
    memcpy(keyBuf, pKey, (uiKeyLen > AES_KEY_LEN) ? AES_KEY_LEN : uiKeyLen);

    AES_KEY aesKey;
    int ret = AES_set_encrypt_key(keyBuf, AES_KEY_BITSET_LEN, &aesKey);
    if(ret != 0) {
        return -2;
    }

    *pOutputLen = uiInputLen;
    *ppOutput = malloc(uiInputLen);
    memset(*ppOutput, 0, uiInputLen);

    int num = 0;
    AES_cfb128_encrypt(pInput, *ppOutput, uiInputLen, &aesKey, iv, &num, AES_DECRYPT);

    return 0;
}
