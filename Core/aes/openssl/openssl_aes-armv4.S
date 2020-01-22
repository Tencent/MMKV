/* Copyright 2007-2018 The OpenSSL Project Authors. All Rights Reserved.
*
* Licensed under the OpenSSL license (the "License").  You may not use
* this file except in compliance with the License.  You can obtain a copy
* in the file LICENSE in the source distribution or at
* https://www.openssl.org/source/license.html


* ====================================================================
* Written by Andy Polyakov <appro@openssl.org> for the OpenSSL
* project. The module is, however, dual licensed under OpenSSL and
* CRYPTOGAMS licenses depending on where you obtain it. For further
* details see http://www.openssl.org/~appro/cryptogams/.
* ====================================================================

* AES for ARMv4

* January 2007.
*
* Code uses single 1K S-box and is >2 times faster than code generated
* by gcc-3.4.1. This is thanks to unique feature of ARMv4 ISA, which
* allows to merge logical or arithmetic operation with shift or rotate
* in one instruction and emit combined result every cycle. The module
* is endian-neutral. The performance is ~42 cycles/byte for 128-bit
* key [on single-issue Xscale PXA250 core].

* May 2007.
*
* AES_set_[en|de]crypt_key is added.

* July 2010.
*
* Rescheduling for dual-issue pipeline resulted in 12% improvement on
* Cortex A8 core and ~25 cycles per byte processed with 128-bit key.

* February 2011.
*
* Profiler-assisted and platform-specific optimization resulted in 16%
* improvement on Cortex A8 core and ~21.5 cycles per byte.
*/

#include "openssl_arm_arch.h"

#if (__ARM_MAX_ARCH__ <= 7) && (__ARM_MAX_ARCH__ > 0)

.text
#if defined(__thumb2__) && !defined(__APPLE__)
.syntax	unified
.thumb
#else
.code	32
#undef __thumb2__
#endif


.align	5
AES_Te:
.word	0xc66363a5, 0xf87c7c84, 0xee777799, 0xf67b7b8d
.word	0xfff2f20d, 0xd66b6bbd, 0xde6f6fb1, 0x91c5c554
.word	0x60303050, 0x02010103, 0xce6767a9, 0x562b2b7d
.word	0xe7fefe19, 0xb5d7d762, 0x4dababe6, 0xec76769a
.word	0x8fcaca45, 0x1f82829d, 0x89c9c940, 0xfa7d7d87
.word	0xeffafa15, 0xb25959eb, 0x8e4747c9, 0xfbf0f00b
.word	0x41adadec, 0xb3d4d467, 0x5fa2a2fd, 0x45afafea
.word	0x239c9cbf, 0x53a4a4f7, 0xe4727296, 0x9bc0c05b
.word	0x75b7b7c2, 0xe1fdfd1c, 0x3d9393ae, 0x4c26266a
.word	0x6c36365a, 0x7e3f3f41, 0xf5f7f702, 0x83cccc4f
.word	0x6834345c, 0x51a5a5f4, 0xd1e5e534, 0xf9f1f108
.word	0xe2717193, 0xabd8d873, 0x62313153, 0x2a15153f
.word	0x0804040c, 0x95c7c752, 0x46232365, 0x9dc3c35e
.word	0x30181828, 0x379696a1, 0x0a05050f, 0x2f9a9ab5
.word	0x0e070709, 0x24121236, 0x1b80809b, 0xdfe2e23d
.word	0xcdebeb26, 0x4e272769, 0x7fb2b2cd, 0xea75759f
.word	0x1209091b, 0x1d83839e, 0x582c2c74, 0x341a1a2e
.word	0x361b1b2d, 0xdc6e6eb2, 0xb45a5aee, 0x5ba0a0fb
.word	0xa45252f6, 0x763b3b4d, 0xb7d6d661, 0x7db3b3ce
.word	0x5229297b, 0xdde3e33e, 0x5e2f2f71, 0x13848497
.word	0xa65353f5, 0xb9d1d168, 0x00000000, 0xc1eded2c
.word	0x40202060, 0xe3fcfc1f, 0x79b1b1c8, 0xb65b5bed
.word	0xd46a6abe, 0x8dcbcb46, 0x67bebed9, 0x7239394b
.word	0x944a4ade, 0x984c4cd4, 0xb05858e8, 0x85cfcf4a
.word	0xbbd0d06b, 0xc5efef2a, 0x4faaaae5, 0xedfbfb16
.word	0x864343c5, 0x9a4d4dd7, 0x66333355, 0x11858594
.word	0x8a4545cf, 0xe9f9f910, 0x04020206, 0xfe7f7f81
.word	0xa05050f0, 0x783c3c44, 0x259f9fba, 0x4ba8a8e3
.word	0xa25151f3, 0x5da3a3fe, 0x804040c0, 0x058f8f8a
.word	0x3f9292ad, 0x219d9dbc, 0x70383848, 0xf1f5f504
.word	0x63bcbcdf, 0x77b6b6c1, 0xafdada75, 0x42212163
.word	0x20101030, 0xe5ffff1a, 0xfdf3f30e, 0xbfd2d26d
.word	0x81cdcd4c, 0x180c0c14, 0x26131335, 0xc3ecec2f
.word	0xbe5f5fe1, 0x359797a2, 0x884444cc, 0x2e171739
.word	0x93c4c457, 0x55a7a7f2, 0xfc7e7e82, 0x7a3d3d47
.word	0xc86464ac, 0xba5d5de7, 0x3219192b, 0xe6737395
.word	0xc06060a0, 0x19818198, 0x9e4f4fd1, 0xa3dcdc7f
.word	0x44222266, 0x542a2a7e, 0x3b9090ab, 0x0b888883
.word	0x8c4646ca, 0xc7eeee29, 0x6bb8b8d3, 0x2814143c
.word	0xa7dede79, 0xbc5e5ee2, 0x160b0b1d, 0xaddbdb76
.word	0xdbe0e03b, 0x64323256, 0x743a3a4e, 0x140a0a1e
.word	0x924949db, 0x0c06060a, 0x4824246c, 0xb85c5ce4
.word	0x9fc2c25d, 0xbdd3d36e, 0x43acacef, 0xc46262a6
.word	0x399191a8, 0x319595a4, 0xd3e4e437, 0xf279798b
.word	0xd5e7e732, 0x8bc8c843, 0x6e373759, 0xda6d6db7
.word	0x018d8d8c, 0xb1d5d564, 0x9c4e4ed2, 0x49a9a9e0
.word	0xd86c6cb4, 0xac5656fa, 0xf3f4f407, 0xcfeaea25
.word	0xca6565af, 0xf47a7a8e, 0x47aeaee9, 0x10080818
.word	0x6fbabad5, 0xf0787888, 0x4a25256f, 0x5c2e2e72
.word	0x381c1c24, 0x57a6a6f1, 0x73b4b4c7, 0x97c6c651
.word	0xcbe8e823, 0xa1dddd7c, 0xe874749c, 0x3e1f1f21
.word	0x964b4bdd, 0x61bdbddc, 0x0d8b8b86, 0x0f8a8a85
.word	0xe0707090, 0x7c3e3e42, 0x71b5b5c4, 0xcc6666aa
.word	0x904848d8, 0x06030305, 0xf7f6f601, 0x1c0e0e12
.word	0xc26161a3, 0x6a35355f, 0xae5757f9, 0x69b9b9d0
.word	0x17868691, 0x99c1c158, 0x3a1d1d27, 0x279e9eb9
.word	0xd9e1e138, 0xebf8f813, 0x2b9898b3, 0x22111133
.word	0xd26969bb, 0xa9d9d970, 0x078e8e89, 0x339494a7
.word	0x2d9b9bb6, 0x3c1e1e22, 0x15878792, 0xc9e9e920
.word	0x87cece49, 0xaa5555ff, 0x50282878, 0xa5dfdf7a
.word	0x038c8c8f, 0x59a1a1f8, 0x09898980, 0x1a0d0d17
.word	0x65bfbfda, 0xd7e6e631, 0x844242c6, 0xd06868b8
.word	0x824141c3, 0x299999b0, 0x5a2d2d77, 0x1e0f0f11
.word	0x7bb0b0cb, 0xa85454fc, 0x6dbbbbd6, 0x2c16163a
// Te4[256]
.byte	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5
.byte	0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76
.byte	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0
.byte	0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0
.byte	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc
.byte	0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15
.byte	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a
.byte	0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75
.byte	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0
.byte	0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84
.byte	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b
.byte	0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf
.byte	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85
.byte	0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8
.byte	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5
.byte	0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2
.byte	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17
.byte	0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73
.byte	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88
.byte	0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb
.byte	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c
.byte	0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79
.byte	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9
.byte	0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08
.byte	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6
.byte	0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a
.byte	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e
.byte	0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e
.byte	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94
.byte	0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf
.byte	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68
.byte	0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
// rcon[]
.word	0x01000000, 0x02000000, 0x04000000, 0x08000000
.word	0x10000000, 0x20000000, 0x40000000, 0x80000000
.word	0x1B000000, 0x36000000, 0, 0, 0, 0, 0, 0


/* void openssl_aes_arm_encrypt(const unsigned char *in, unsigned char *out,
* 		 const AES_KEY *key) {
*/
#ifndef __ANDROID__
.globl	_openssl_aes_arm_encrypt
#ifdef __thumb2__
.thumb_func	_openssl_aes_arm_encrypt
#endif
.align	5
_openssl_aes_arm_encrypt:
#else
.globl	openssl_aes_arm_encrypt
#ifdef __thumb2__
.thumb_func	openssl_aes_arm_encrypt
#endif
.align	5
openssl_aes_arm_encrypt:
#endif
#ifndef	__thumb2__
	sub	r3,pc,#8		// _AES_encrypt
#else
	adr	r3,.
#endif
	stmdb	sp!,{r1,r4-r12,lr}
#if defined(__thumb2__) || defined(__APPLE__)
	adr	r10,AES_Te
#elif defined(__ANDROID__)
	sub	r10,r3,#openssl_aes_arm_encrypt-AES_Te	// Te
#else
	sub	r10,r3,#_openssl_aes_arm_encrypt-AES_Te	// Te
#endif
	mov	r12,r0		// inp
	mov	r11,r2
#if __ARM_ARCH__<7
	ldrb	r0,[r12,#3]	// load input data in endian-neutral
	ldrb	r4,[r12,#2]	// manner...
	ldrb	r5,[r12,#1]
	ldrb	r6,[r12,#0]
	orr	r0,r0,r4,lsl#8
	ldrb	r1,[r12,#7]
	orr	r0,r0,r5,lsl#16
	ldrb	r4,[r12,#6]
	orr	r0,r0,r6,lsl#24
	ldrb	r5,[r12,#5]
	ldrb	r6,[r12,#4]
	orr	r1,r1,r4,lsl#8
	ldrb	r2,[r12,#11]
	orr	r1,r1,r5,lsl#16
	ldrb	r4,[r12,#10]
	orr	r1,r1,r6,lsl#24
	ldrb	r5,[r12,#9]
	ldrb	r6,[r12,#8]
	orr	r2,r2,r4,lsl#8
	ldrb	r3,[r12,#15]
	orr	r2,r2,r5,lsl#16
	ldrb	r4,[r12,#14]
	orr	r2,r2,r6,lsl#24
	ldrb	r5,[r12,#13]
	ldrb	r6,[r12,#12]
	orr	r3,r3,r4,lsl#8
	orr	r3,r3,r5,lsl#16
	orr	r3,r3,r6,lsl#24
#else
	ldr	r0,[r12,#0]
	ldr	r1,[r12,#4]
	ldr	r2,[r12,#8]
	ldr	r3,[r12,#12]
#ifdef __ARMEL__
	rev	r0,r0
	rev	r1,r1
	rev	r2,r2
	rev	r3,r3
#endif
#endif
	bl	_armv4_AES_encrypt

	ldr	r12,[sp],#4		// pop out
#if __ARM_ARCH__>=7
#ifdef __ARMEL__
	rev	r0,r0
	rev	r1,r1
	rev	r2,r2
	rev	r3,r3
#endif
	str	r0,[r12,#0]
	str	r1,[r12,#4]
	str	r2,[r12,#8]
	str	r3,[r12,#12]
#else
	mov	r4,r0,lsr#24		// write output in endian-neutral
	mov	r5,r0,lsr#16		// manner...
	mov	r6,r0,lsr#8
	strb	r4,[r12,#0]
	strb	r5,[r12,#1]
	mov	r4,r1,lsr#24
	strb	r6,[r12,#2]
	mov	r5,r1,lsr#16
	strb	r0,[r12,#3]
	mov	r6,r1,lsr#8
	strb	r4,[r12,#4]
	strb	r5,[r12,#5]
	mov	r4,r2,lsr#24
	strb	r6,[r12,#6]
	mov	r5,r2,lsr#16
	strb	r1,[r12,#7]
	mov	r6,r2,lsr#8
	strb	r4,[r12,#8]
	strb	r5,[r12,#9]
	mov	r4,r3,lsr#24
	strb	r6,[r12,#10]
	mov	r5,r3,lsr#16
	strb	r2,[r12,#11]
	mov	r6,r3,lsr#8
	strb	r4,[r12,#12]
	strb	r5,[r12,#13]
	strb	r6,[r12,#14]
	strb	r3,[r12,#15]
#endif
#if __ARM_ARCH__>=5
	ldmia	sp!,{r4,r5,r6,r7,r8,r9,r10,r11,r12,pc}
#else
	ldmia	sp!,{r4,r5,r6,r7,r8,r9,r10,r11,r12,lr}
	tst	lr,#1
	moveq	pc,lr			// be binary compatible with V4, yet
.word	0xe12fff1e			// interoperable with Thumb ISA:-)
#endif


#ifdef __thumb2__
.thumb_func	_armv4_AES_encrypt
#endif
.align	2
_armv4_AES_encrypt:
	str	lr,[sp,#-4]!		// push lr
	ldmia	r11!,{r4,r5,r6,r7}
	eor	r0,r0,r4
	ldr	r12,[r11,#240-16]
	eor	r1,r1,r5
	eor	r2,r2,r6
	eor	r3,r3,r7
	sub	r12,r12,#1
	mov	lr,#255

	and	r7,lr,r0
	and	r8,lr,r0,lsr#8
	and	r9,lr,r0,lsr#16
	mov	r0,r0,lsr#24
Lenc_loop:
	ldr	r4,[r10,r7,lsl#2]	// Te3[s0>>0]
	and	r7,lr,r1,lsr#16	// i0
	ldr	r5,[r10,r8,lsl#2]	// Te2[s0>>8]
	and	r8,lr,r1
	ldr	r6,[r10,r9,lsl#2]	// Te1[s0>>16]
	and	r9,lr,r1,lsr#8
	ldr	r0,[r10,r0,lsl#2]	// Te0[s0>>24]
	mov	r1,r1,lsr#24

	ldr	r7,[r10,r7,lsl#2]	// Te1[s1>>16]
	ldr	r8,[r10,r8,lsl#2]	// Te3[s1>>0]
	ldr	r9,[r10,r9,lsl#2]	// Te2[s1>>8]
	eor	r0,r0,r7,ror#8
	ldr	r1,[r10,r1,lsl#2]	// Te0[s1>>24]
	and	r7,lr,r2,lsr#8	// i0
	eor	r5,r5,r8,ror#8
	and	r8,lr,r2,lsr#16	// i1
	eor	r6,r6,r9,ror#8
	and	r9,lr,r2
	ldr	r7,[r10,r7,lsl#2]	// Te2[s2>>8]
	eor	r1,r1,r4,ror#24
	ldr	r8,[r10,r8,lsl#2]	// Te1[s2>>16]
	mov	r2,r2,lsr#24

	ldr	r9,[r10,r9,lsl#2]	// Te3[s2>>0]
	eor	r0,r0,r7,ror#16
	ldr	r2,[r10,r2,lsl#2]	// Te0[s2>>24]
	and	r7,lr,r3		// i0
	eor	r1,r1,r8,ror#8
	and	r8,lr,r3,lsr#8	// i1
	eor	r6,r6,r9,ror#16
	and	r9,lr,r3,lsr#16	// i2
	ldr	r7,[r10,r7,lsl#2]	// Te3[s3>>0]
	eor	r2,r2,r5,ror#16
	ldr	r8,[r10,r8,lsl#2]	// Te2[s3>>8]
	mov	r3,r3,lsr#24

	ldr	r9,[r10,r9,lsl#2]	// Te1[s3>>16]
	eor	r0,r0,r7,ror#24
	ldr	r7,[r11],#16
	eor	r1,r1,r8,ror#16
	ldr	r3,[r10,r3,lsl#2]	// Te0[s3>>24]
	eor	r2,r2,r9,ror#8
	ldr	r4,[r11,#-12]
	eor	r3,r3,r6,ror#8

	ldr	r5,[r11,#-8]
	eor	r0,r0,r7
	ldr	r6,[r11,#-4]
	and	r7,lr,r0
	eor	r1,r1,r4
	and	r8,lr,r0,lsr#8
	eor	r2,r2,r5
	and	r9,lr,r0,lsr#16
	eor	r3,r3,r6
	mov	r0,r0,lsr#24

	subs	r12,r12,#1
	bne	Lenc_loop

	add	r10,r10,#2

	ldrb	r4,[r10,r7,lsl#2]	// Te4[s0>>0]
	and	r7,lr,r1,lsr#16	// i0
	ldrb	r5,[r10,r8,lsl#2]	// Te4[s0>>8]
	and	r8,lr,r1
	ldrb	r6,[r10,r9,lsl#2]	// Te4[s0>>16]
	and	r9,lr,r1,lsr#8
	ldrb	r0,[r10,r0,lsl#2]	// Te4[s0>>24]
	mov	r1,r1,lsr#24

	ldrb	r7,[r10,r7,lsl#2]	// Te4[s1>>16]
	ldrb	r8,[r10,r8,lsl#2]	// Te4[s1>>0]
	ldrb	r9,[r10,r9,lsl#2]	// Te4[s1>>8]
	eor	r0,r7,r0,lsl#8
	ldrb	r1,[r10,r1,lsl#2]	// Te4[s1>>24]
	and	r7,lr,r2,lsr#8	// i0
	eor	r5,r8,r5,lsl#8
	and	r8,lr,r2,lsr#16	// i1
	eor	r6,r9,r6,lsl#8
	and	r9,lr,r2
	ldrb	r7,[r10,r7,lsl#2]	// Te4[s2>>8]
	eor	r1,r4,r1,lsl#24
	ldrb	r8,[r10,r8,lsl#2]	// Te4[s2>>16]
	mov	r2,r2,lsr#24

	ldrb	r9,[r10,r9,lsl#2]	// Te4[s2>>0]
	eor	r0,r7,r0,lsl#8
	ldrb	r2,[r10,r2,lsl#2]	// Te4[s2>>24]
	and	r7,lr,r3		// i0
	eor	r1,r1,r8,lsl#16
	and	r8,lr,r3,lsr#8	// i1
	eor	r6,r9,r6,lsl#8
	and	r9,lr,r3,lsr#16	// i2
	ldrb	r7,[r10,r7,lsl#2]	// Te4[s3>>0]
	eor	r2,r5,r2,lsl#24
	ldrb	r8,[r10,r8,lsl#2]	// Te4[s3>>8]
	mov	r3,r3,lsr#24

	ldrb	r9,[r10,r9,lsl#2]	// Te4[s3>>16]
	eor	r0,r7,r0,lsl#8
	ldr	r7,[r11,#0]
	ldrb	r3,[r10,r3,lsl#2]	// Te4[s3>>24]
	eor	r1,r1,r8,lsl#8
	ldr	r4,[r11,#4]
	eor	r2,r2,r9,lsl#16
	ldr	r5,[r11,#8]
	eor	r3,r6,r3,lsl#24
	ldr	r6,[r11,#12]

	eor	r0,r0,r7
	eor	r1,r1,r4
	eor	r2,r2,r5
	eor	r3,r3,r6

	sub	r10,r10,#2
	ldr	pc,[sp],#4		// pop and return


#ifndef __ANDROID__
.globl	_openssl_aes_arm_set_encrypt_key
#ifdef __thumb2__
.thumb_func	_openssl_aes_arm_set_encrypt_key
#endif
.align	5
_openssl_aes_arm_set_encrypt_key:
#else
.globl	openssl_aes_arm_set_encrypt_key
#ifdef __thumb2__
.thumb_func	openssl_aes_arm_set_encrypt_key
#endif
.align	5
openssl_aes_arm_set_encrypt_key:
#endif
_armv4_AES_set_encrypt_key:
#ifndef	__thumb2__
	sub	r3,pc,#8		// _AES_set_encrypt_key
#else
	adr	r3,.
#endif
	teq	r0,#0
#ifdef	__thumb2__
	itt	eq			// Thumb2 thing, sanity check in ARM
#endif
	moveq	r0,#-1
	beq	Labrt
	teq	r2,#0
#ifdef	__thumb2__
	itt	eq			// Thumb2 thing, sanity check in ARM
#endif
	moveq	r0,#-1
	beq	Labrt

	teq	r1,#128
	beq	Lok
	teq	r1,#192
	beq	Lok
	teq	r1,#256
#ifdef	__thumb2__
	itt	ne			// Thumb2 thing, sanity check in ARM
#endif
	movne	r0,#-1
	bne	Labrt

Lok:	stmdb	sp!,{r4,r5,r6,r7,r8,r9,r10,r11,r12,lr}
	mov	r12,r0		// inp
	mov	lr,r1			// bits
	mov	r11,r2			// key

#if defined(__thumb2__) || defined(__APPLE__)
	adr	r10,AES_Te+1024				// Te4
#else
	sub	r10,r3,#_armv4_AES_set_encrypt_key-AES_Te-1024	// Te4
#endif

#if __ARM_ARCH__<7
	ldrb	r0,[r12,#3]	// load input data in endian-neutral
	ldrb	r4,[r12,#2]	// manner...
	ldrb	r5,[r12,#1]
	ldrb	r6,[r12,#0]
	orr	r0,r0,r4,lsl#8
	ldrb	r1,[r12,#7]
	orr	r0,r0,r5,lsl#16
	ldrb	r4,[r12,#6]
	orr	r0,r0,r6,lsl#24
	ldrb	r5,[r12,#5]
	ldrb	r6,[r12,#4]
	orr	r1,r1,r4,lsl#8
	ldrb	r2,[r12,#11]
	orr	r1,r1,r5,lsl#16
	ldrb	r4,[r12,#10]
	orr	r1,r1,r6,lsl#24
	ldrb	r5,[r12,#9]
	ldrb	r6,[r12,#8]
	orr	r2,r2,r4,lsl#8
	ldrb	r3,[r12,#15]
	orr	r2,r2,r5,lsl#16
	ldrb	r4,[r12,#14]
	orr	r2,r2,r6,lsl#24
	ldrb	r5,[r12,#13]
	ldrb	r6,[r12,#12]
	orr	r3,r3,r4,lsl#8
	str	r0,[r11],#16
	orr	r3,r3,r5,lsl#16
	str	r1,[r11,#-12]
	orr	r3,r3,r6,lsl#24
	str	r2,[r11,#-8]
	str	r3,[r11,#-4]
#else
	ldr	r0,[r12,#0]
	ldr	r1,[r12,#4]
	ldr	r2,[r12,#8]
	ldr	r3,[r12,#12]
#ifdef __ARMEL__
	rev	r0,r0
	rev	r1,r1
	rev	r2,r2
	rev	r3,r3
#endif
	str	r0,[r11],#16
	str	r1,[r11,#-12]
	str	r2,[r11,#-8]
	str	r3,[r11,#-4]
#endif

	teq	lr,#128
	bne	Lnot128
	mov	r12,#10
	str	r12,[r11,#240-16]
	add	r6,r10,#256			// rcon
	mov	lr,#255

L128_loop:
	and	r5,lr,r3,lsr#24
	and	r7,lr,r3,lsr#16
	ldrb	r5,[r10,r5]
	and	r8,lr,r3,lsr#8
	ldrb	r7,[r10,r7]
	and	r9,lr,r3
	ldrb	r8,[r10,r8]
	orr	r5,r5,r7,lsl#24
	ldrb	r9,[r10,r9]
	orr	r5,r5,r8,lsl#16
	ldr	r4,[r6],#4			// rcon[i++]
	orr	r5,r5,r9,lsl#8
	eor	r5,r5,r4
	eor	r0,r0,r5			// rk[4]=rk[0]^...
	eor	r1,r1,r0			// rk[5]=rk[1]^rk[4]
	str	r0,[r11],#16
	eor	r2,r2,r1			// rk[6]=rk[2]^rk[5]
	str	r1,[r11,#-12]
	eor	r3,r3,r2			// rk[7]=rk[3]^rk[6]
	str	r2,[r11,#-8]
	subs	r12,r12,#1
	str	r3,[r11,#-4]
	bne	L128_loop
	sub	r2,r11,#176
	b	Ldone

Lnot128:
#if __ARM_ARCH__<7
	ldrb	r8,[r12,#19]
	ldrb	r4,[r12,#18]
	ldrb	r5,[r12,#17]
	ldrb	r6,[r12,#16]
	orr	r8,r8,r4,lsl#8
	ldrb	r9,[r12,#23]
	orr	r8,r8,r5,lsl#16
	ldrb	r4,[r12,#22]
	orr	r8,r8,r6,lsl#24
	ldrb	r5,[r12,#21]
	ldrb	r6,[r12,#20]
	orr	r9,r9,r4,lsl#8
	orr	r9,r9,r5,lsl#16
	str	r8,[r11],#8
	orr	r9,r9,r6,lsl#24
	str	r9,[r11,#-4]
#else
	ldr	r8,[r12,#16]
	ldr	r9,[r12,#20]
#ifdef __ARMEL__
	rev	r8,r8
	rev	r9,r9
#endif
	str	r8,[r11],#8
	str	r9,[r11,#-4]
#endif

	teq	lr,#192
	bne	Lnot192
	mov	r12,#12
	str	r12,[r11,#240-24]
	add	r6,r10,#256			// rcon
	mov	lr,#255
	mov	r12,#8

L192_loop:
	and	r5,lr,r9,lsr#24
	and	r7,lr,r9,lsr#16
	ldrb	r5,[r10,r5]
	and	r8,lr,r9,lsr#8
	ldrb	r7,[r10,r7]
	and	r9,lr,r9
	ldrb	r8,[r10,r8]
	orr	r5,r5,r7,lsl#24
	ldrb	r9,[r10,r9]
	orr	r5,r5,r8,lsl#16
	ldr	r4,[r6],#4			// rcon[i++]
	orr	r5,r5,r9,lsl#8
	eor	r9,r5,r4
	eor	r0,r0,r9			// rk[6]=rk[0]^...
	eor	r1,r1,r0			// rk[7]=rk[1]^rk[6]
	str	r0,[r11],#24
	eor	r2,r2,r1			// rk[8]=rk[2]^rk[7]
	str	r1,[r11,#-20]
	eor	r3,r3,r2			// rk[9]=rk[3]^rk[8]
	str	r2,[r11,#-16]
	subs	r12,r12,#1
	str	r3,[r11,#-12]
#ifdef	__thumb2__
	itt	eq				// Thumb2 thing, sanity check in ARM
#endif
	subeq	r2,r11,#216
	beq	Ldone

	ldr	r7,[r11,#-32]
	ldr	r8,[r11,#-28]
	eor	r7,r7,r3			// rk[10]=rk[4]^rk[9]
	eor	r9,r8,r7			// rk[11]=rk[5]^rk[10]
	str	r7,[r11,#-8]
	str	r9,[r11,#-4]
	b	L192_loop

Lnot192:
#if __ARM_ARCH__<7
	ldrb	r8,[r12,#27]
	ldrb	r4,[r12,#26]
	ldrb	r5,[r12,#25]
	ldrb	r6,[r12,#24]
	orr	r8,r8,r4,lsl#8
	ldrb	r9,[r12,#31]
	orr	r8,r8,r5,lsl#16
	ldrb	r4,[r12,#30]
	orr	r8,r8,r6,lsl#24
	ldrb	r5,[r12,#29]
	ldrb	r6,[r12,#28]
	orr	r9,r9,r4,lsl#8
	orr	r9,r9,r5,lsl#16
	str	r8,[r11],#8
	orr	r9,r9,r6,lsl#24
	str	r9,[r11,#-4]
#else
	ldr	r8,[r12,#24]
	ldr	r9,[r12,#28]
#ifdef __ARMEL__
	rev	r8,r8
	rev	r9,r9
#endif
	str	r8,[r11],#8
	str	r9,[r11,#-4]
#endif

	mov	r12,#14
	str	r12,[r11,#240-32]
	add	r6,r10,#256			// rcon
	mov	lr,#255
	mov	r12,#7

L256_loop:
	and	r5,lr,r9,lsr#24
	and	r7,lr,r9,lsr#16
	ldrb	r5,[r10,r5]
	and	r8,lr,r9,lsr#8
	ldrb	r7,[r10,r7]
	and	r9,lr,r9
	ldrb	r8,[r10,r8]
	orr	r5,r5,r7,lsl#24
	ldrb	r9,[r10,r9]
	orr	r5,r5,r8,lsl#16
	ldr	r4,[r6],#4			// rcon[i++]
	orr	r5,r5,r9,lsl#8
	eor	r9,r5,r4
	eor	r0,r0,r9			// rk[8]=rk[0]^...
	eor	r1,r1,r0			// rk[9]=rk[1]^rk[8]
	str	r0,[r11],#32
	eor	r2,r2,r1			// rk[10]=rk[2]^rk[9]
	str	r1,[r11,#-28]
	eor	r3,r3,r2			// rk[11]=rk[3]^rk[10]
	str	r2,[r11,#-24]
	subs	r12,r12,#1
	str	r3,[r11,#-20]
#ifdef	__thumb2__
	itt	eq				// Thumb2 thing, sanity check in ARM
#endif
	subeq	r2,r11,#256
	beq	Ldone

	and	r5,lr,r3
	and	r7,lr,r3,lsr#8
	ldrb	r5,[r10,r5]
	and	r8,lr,r3,lsr#16
	ldrb	r7,[r10,r7]
	and	r9,lr,r3,lsr#24
	ldrb	r8,[r10,r8]
	orr	r5,r5,r7,lsl#8
	ldrb	r9,[r10,r9]
	orr	r5,r5,r8,lsl#16
	ldr	r4,[r11,#-48]
	orr	r5,r5,r9,lsl#24

	ldr	r7,[r11,#-44]
	ldr	r8,[r11,#-40]
	eor	r4,r4,r5			// rk[12]=rk[4]^...
	ldr	r9,[r11,#-36]
	eor	r7,r7,r4			// rk[13]=rk[5]^rk[12]
	str	r4,[r11,#-16]
	eor	r8,r8,r7			// rk[14]=rk[6]^rk[13]
	str	r7,[r11,#-12]
	eor	r9,r9,r8			// rk[15]=rk[7]^rk[14]
	str	r8,[r11,#-8]
	str	r9,[r11,#-4]
	b	L256_loop

.align	2
Ldone:	mov	r0,#0
	ldmia	sp!,{r4,r5,r6,r7,r8,r9,r10,r11,r12,lr}
Labrt:
#if __ARM_ARCH__>=5
	bx	lr				// .word	0xe12fff1e
#else
	tst	lr,#1
	moveq	pc,lr			// be binary compatible with V4, yet
.word	0xe12fff1e			// interoperable with Thumb ISA:-)
#endif

#endif
