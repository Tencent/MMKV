#include "openssl_arm_arch.h"
#include "../../MMKVPredef.h"

#if (__ARM_MAX_ARCH__ > 7) && !defined(MMKV_DISABLE_CRYPT)

.text

.align	5
Lrcon:
.long	0x01,0x01,0x01,0x01
.long	0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d	// rotate-n-splat
.long	0x1b,0x1b,0x1b,0x1b

#ifndef __linux__
.globl	_openssl_aes_arm_set_encrypt_key

.align	5
_openssl_aes_arm_set_encrypt_key:
#else // __linux__
.globl	openssl_aes_armv8_set_encrypt_key

.align	5
openssl_aes_armv8_set_encrypt_key:
#endif // __linux__

Lenc_key:
	stp	x29,x30,[sp,#-16]!
	add	x29,sp,#0
	mov	x3,#-1
	cmp	x0,#0
	b.eq	Lenc_key_abort
	cmp	x2,#0
	b.eq	Lenc_key_abort
	mov	x3,#-2
	cmp	w1,#128
	b.lt	Lenc_key_abort
	cmp	w1,#256
	b.gt	Lenc_key_abort
	tst	w1,#0x3f
	b.ne	Lenc_key_abort

	adr	x3,Lrcon
	cmp	w1,#192

	eor	v0.16b,v0.16b,v0.16b
	ld1	{v3.16b},[x0],#16
	mov	w1,#8		// reuse w1
	ld1	{v1.4s,v2.4s},[x3],#32

	b.lt	Loop128
	b.eq	L192
	b	L256

.align	4
Loop128:
	tbl	v6.16b,{v3.16b},v2.16b
	ext	v5.16b,v0.16b,v3.16b,#12
	st1	{v3.4s},[x2],#16
	aese	v6.16b,v0.16b
	subs	w1,w1,#1

	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v6.16b,v6.16b,v1.16b
	eor	v3.16b,v3.16b,v5.16b
	shl	v1.16b,v1.16b,#1
	eor	v3.16b,v3.16b,v6.16b
	b.ne	Loop128

	ld1	{v1.4s},[x3]

	tbl	v6.16b,{v3.16b},v2.16b
	ext	v5.16b,v0.16b,v3.16b,#12
	st1	{v3.4s},[x2],#16
	aese	v6.16b,v0.16b

	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v6.16b,v6.16b,v1.16b
	eor	v3.16b,v3.16b,v5.16b
	shl	v1.16b,v1.16b,#1
	eor	v3.16b,v3.16b,v6.16b

	tbl	v6.16b,{v3.16b},v2.16b
	ext	v5.16b,v0.16b,v3.16b,#12
	st1	{v3.4s},[x2],#16
	aese	v6.16b,v0.16b

	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v6.16b,v6.16b,v1.16b
	eor	v3.16b,v3.16b,v5.16b
	eor	v3.16b,v3.16b,v6.16b
	st1	{v3.4s},[x2]
	add	x2,x2,#0x50

	mov	w12,#10
	b	Ldone

.align	4
L192:
	ld1	{v4.8b},[x0],#8
	movi	v6.16b,#8			// borrow v6.16b
	st1	{v3.4s},[x2],#16
	sub	v2.16b,v2.16b,v6.16b	// adjust the mask

Loop192:
	tbl	v6.16b,{v4.16b},v2.16b
	ext	v5.16b,v0.16b,v3.16b,#12
	st1	{v4.8b},[x2],#8
	aese	v6.16b,v0.16b
	subs	w1,w1,#1

	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v3.16b,v3.16b,v5.16b

	dup	v5.4s,v3.s[3]
	eor	v5.16b,v5.16b,v4.16b
	eor	v6.16b,v6.16b,v1.16b
	ext	v4.16b,v0.16b,v4.16b,#12
	shl	v1.16b,v1.16b,#1
	eor	v4.16b,v4.16b,v5.16b
	eor	v3.16b,v3.16b,v6.16b
	eor	v4.16b,v4.16b,v6.16b
	st1	{v3.4s},[x2],#16
	b.ne	Loop192

	mov	w12,#12
	add	x2,x2,#0x20
	b	Ldone

.align	4
L256:
	ld1	{v4.16b},[x0]
	mov	w1,#7
	mov	w12,#14
	st1	{v3.4s},[x2],#16

Loop256:
	tbl	v6.16b,{v4.16b},v2.16b
	ext	v5.16b,v0.16b,v3.16b,#12
	st1	{v4.4s},[x2],#16
	aese	v6.16b,v0.16b
	subs	w1,w1,#1

	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v3.16b,v3.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v6.16b,v6.16b,v1.16b
	eor	v3.16b,v3.16b,v5.16b
	shl	v1.16b,v1.16b,#1
	eor	v3.16b,v3.16b,v6.16b
	st1	{v3.4s},[x2],#16
	b.eq	Ldone

	dup	v6.4s,v3.s[3]		// just splat
	ext	v5.16b,v0.16b,v4.16b,#12
	aese	v6.16b,v0.16b

	eor	v4.16b,v4.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v4.16b,v4.16b,v5.16b
	ext	v5.16b,v0.16b,v5.16b,#12
	eor	v4.16b,v4.16b,v5.16b

	eor	v4.16b,v4.16b,v6.16b
	b	Loop256

Ldone:
	str	w12,[x2]
	mov	x3,#0

Lenc_key_abort:
	mov	x0,x3			// return value
	ldr	x29,[sp],#16
	ret

#ifndef __linux__
.globl    _openssl_aes_arm_set_decrypt_key

.align    5
_openssl_aes_arm_set_decrypt_key:
#else // __linux__
.globl    openssl_aes_armv8_set_decrypt_key

.align    5
openssl_aes_armv8_set_decrypt_key:
#endif // __linux__
    stp    x29,x30,[sp,#-16]!
    add    x29,sp,#0
    bl    Lenc_key

    cmp    x0,#0
    b.ne    Ldec_key_abort

    sub    x2,x2,#240        // restore original x2
    mov    x4,#-16
    add    x0,x2,x12,lsl#4    // end of key schedule

    ld1    {v0.4s},[x2]
    ld1    {v1.4s},[x0]
    st1    {v0.4s},[x0],x4
    st1    {v1.4s},[x2],#16

Loop_imc:
    ld1    {v0.4s},[x2]
    ld1    {v1.4s},[x0]
    aesimc    v0.16b,v0.16b
    aesimc    v1.16b,v1.16b
    st1    {v0.4s},[x0],x4
    st1    {v1.4s},[x2],#16
    cmp    x0,x2
    b.hi    Loop_imc

    ld1    {v0.4s},[x2]
    aesimc    v0.16b,v0.16b
    st1    {v0.4s},[x0]

    eor    x0,x0,x0        // return value
Ldec_key_abort:
    ldp    x29,x30,[sp],#16
    ret

#ifndef __linux__
.globl	_openssl_aes_arm_encrypt

.align	5
_openssl_aes_arm_encrypt:
#else // __linux__
.globl	openssl_aes_armv8_encrypt

.align	5
openssl_aes_armv8_encrypt:
#endif // __linux__

	ldr	w3,[x2,#240]
	ld1	{v0.4s},[x2],#16
	ld1	{v2.16b},[x0]
	sub	w3,w3,#2
	ld1	{v1.4s},[x2],#16

Loop_enc:
	aese	v2.16b,v0.16b
	aesmc	v2.16b,v2.16b
	ld1	{v0.4s},[x2],#16
	subs	w3,w3,#2
	aese	v2.16b,v1.16b
	aesmc	v2.16b,v2.16b
	ld1	{v1.4s},[x2],#16
	b.gt	Loop_enc

	aese	v2.16b,v0.16b
	aesmc	v2.16b,v2.16b
	ld1	{v0.4s},[x2]
	aese	v2.16b,v1.16b
	eor	v2.16b,v2.16b,v0.16b

	st1	{v2.16b},[x1]
	ret

#ifndef __linux__
.globl    _openssl_aes_arm_decrypt

.align    5
_openssl_aes_arm_decrypt:
#else  // __linux__
.globl    openssl_aes_armv8_decrypt

.align    5
openssl_aes_armv8_decrypt:
#endif  // __linux__

    ldr    w3,[x2,#240]
    ld1    {v0.4s},[x2],#16
    ld1    {v2.16b},[x0]
    sub    w3,w3,#2
    ld1    {v1.4s},[x2],#16

Loop_dec:
    aesd    v2.16b,v0.16b
    aesimc    v2.16b,v2.16b
    ld1    {v0.4s},[x2],#16
    subs    w3,w3,#2
    aesd    v2.16b,v1.16b
    aesimc    v2.16b,v2.16b
    ld1    {v1.4s},[x2],#16
    b.gt    Loop_dec

    aesd    v2.16b,v0.16b
    aesimc    v2.16b,v2.16b
    ld1    {v0.4s},[x2]
    aesd    v2.16b,v1.16b
    eor    v2.16b,v2.16b,v0.16b

    st1    {v2.16b},[x1]
    ret

#endif
