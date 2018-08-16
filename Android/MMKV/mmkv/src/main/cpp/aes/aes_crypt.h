/*
 * aes_crypt.h
 *
 *  Created on: 2013-7-17
 *      Author: zhouzhijie
 */

#ifndef AES_CRYPT_H_
#define AES_CRYPT_H_

#ifdef __cplusplus
extern "C" {
#endif

int aes_cfb_encrypt(const unsigned char* pKey, unsigned int uiKeyLen
		, const unsigned char* pInput, unsigned int uiInputLen
		, unsigned char** ppOutput, unsigned int* pOutputLen);

int aes_cfb_decrypt(const unsigned char* pKey, unsigned int uiKeyLen
		, const unsigned char* pInput, unsigned int uiInputLen
		, unsigned char** ppOutput, unsigned int* pOutputLen);

#ifdef __cplusplus
}
#endif

#endif /* AES_CRYPT_H_ */
