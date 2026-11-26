/*
 * Copyright 2009 Colin Percival, 2011 ArtForz, 2011-2014 pooler
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file was originally written by Colin Percival as part of the Tarsnap
 * online backup system.
 */

#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "miner.h"

bool opt_ryzen = false;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#endif
#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#if defined(__APPLE__)
#include <mach/vm_statistics.h>
#ifndef VM_FLAGS_SUPERPAGE_SIZE_2MB
#define VM_FLAGS_SUPERPAGE_SIZE_2MB (2 << 16)
#endif
#endif
#if defined(__linux__)
#ifndef MAP_HUGE_SHIFT
#define MAP_HUGE_SHIFT 26
#endif
#ifndef MAP_HUGE_2MB
#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif
#endif

/* Huge-page size used to hint the kernel for the scrypt scratchpad. */
#define SCRYPT_HUGEPAGE_SIZE (2u * 1024u * 1024u)

static const uint32_t keypad[12] = {
	0x80000000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x00000280
};
static const uint32_t innerpad[11] = {
	0x80000000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x000004a0
};
static const uint32_t outerpad[8] = {
	0x80000000, 0, 0, 0, 0, 0, 0, 0x00000300
};
static const uint32_t finalblk[16] = {
	0x00000001, 0x80000000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x00000620
};

static inline void HMAC_SHA256_80_init(const uint32_t *key,
	uint32_t *tstate, uint32_t *ostate)
{
	uint32_t ihash[8];
	uint32_t pad[16];
	int i;

	/* tstate is assumed to contain the midstate of key */
	memcpy(pad, key + 16, 16);
	memcpy(pad + 4, keypad, 48);
	sha256_transform(tstate, pad, 0);
	memcpy(ihash, tstate, 32);

	sha256_init(ostate);
	for (i = 0; i < 8; i++)
		pad[i] = ihash[i] ^ 0x5c5c5c5c;
	for (; i < 16; i++)
		pad[i] = 0x5c5c5c5c;
	sha256_transform(ostate, pad, 0);

	sha256_init(tstate);
	for (i = 0; i < 8; i++)
		pad[i] = ihash[i] ^ 0x36363636;
	for (; i < 16; i++)
		pad[i] = 0x36363636;
	sha256_transform(tstate, pad, 0);
}

static inline void PBKDF2_SHA256_80_128(const uint32_t *tstate,
	const uint32_t *ostate, const uint32_t *salt, uint32_t *output)
{
	uint32_t istate[8], ostate2[8];
	uint32_t ibuf[16], obuf[16];
	int i, j;

	memcpy(istate, tstate, 32);
	sha256_transform(istate, salt, 0);
	
	memcpy(ibuf, salt + 16, 16);
	memcpy(ibuf + 5, innerpad, 44);
	memcpy(obuf + 8, outerpad, 32);

	for (i = 0; i < 4; i++) {
		memcpy(obuf, istate, 32);
		ibuf[4] = i + 1;
		sha256_transform(obuf, ibuf, 0);

		memcpy(ostate2, ostate, 32);
		sha256_transform(ostate2, obuf, 0);
		for (j = 0; j < 8; j++)
			output[8 * i + j] = swab32(ostate2[j]);
	}
}

static inline void PBKDF2_SHA256_128_32(uint32_t *tstate, uint32_t *ostate,
	const uint32_t *salt, uint32_t *output)
{
	uint32_t buf[16];
	int i;
	
	sha256_transform(tstate, salt, 1);
	sha256_transform(tstate, salt + 16, 1);
	sha256_transform(tstate, finalblk, 0);
	memcpy(buf, tstate, 32);
	memcpy(buf + 8, outerpad, 32);

	sha256_transform(ostate, buf, 0);
	for (i = 0; i < 8; i++)
		output[i] = swab32(ostate[i]);
}


#ifdef HAVE_SHA256_4WAY

static const uint32_t keypad_4way[4 * 12] = {
	0x80000000, 0x80000000, 0x80000000, 0x80000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000280, 0x00000280, 0x00000280, 0x00000280
};
static const uint32_t innerpad_4way[4 * 11] = {
	0x80000000, 0x80000000, 0x80000000, 0x80000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x000004a0, 0x000004a0, 0x000004a0, 0x000004a0
};
static const uint32_t outerpad_4way[4 * 8] = {
	0x80000000, 0x80000000, 0x80000000, 0x80000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000300, 0x00000300, 0x00000300, 0x00000300
};
static const uint32_t _ALIGN(16) finalblk_4way[4 * 16] = {
	0x00000001, 0x00000001, 0x00000001, 0x00000001,
	0x80000000, 0x80000000, 0x80000000, 0x80000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000620, 0x00000620, 0x00000620, 0x00000620
};

static inline void HMAC_SHA256_80_init_4way(const uint32_t *key,
	uint32_t *tstate, uint32_t *ostate)
{
	uint32_t _ALIGN(16) ihash[4 * 8];
	uint32_t _ALIGN(16) pad[4 * 16];
	int i;

	/* tstate is assumed to contain the midstate of key */
	memcpy(pad, key + 4 * 16, 4 * 16);
	memcpy(pad + 4 * 4, keypad_4way, 4 * 48);
	sha256_transform_4way(tstate, pad, 0);
	memcpy(ihash, tstate, 4 * 32);

	sha256_init_4way(ostate);
	for (i = 0; i < 4 * 8; i++)
		pad[i] = ihash[i] ^ 0x5c5c5c5c;
	for (; i < 4 * 16; i++)
		pad[i] = 0x5c5c5c5c;
	sha256_transform_4way(ostate, pad, 0);

	sha256_init_4way(tstate);
	for (i = 0; i < 4 * 8; i++)
		pad[i] = ihash[i] ^ 0x36363636;
	for (; i < 4 * 16; i++)
		pad[i] = 0x36363636;
	sha256_transform_4way(tstate, pad, 0);
}

static inline void PBKDF2_SHA256_80_128_4way(const uint32_t *tstate,
	const uint32_t *ostate, const uint32_t *salt, uint32_t *output)
{
	uint32_t _ALIGN(16) istate[4 * 8];
	uint32_t _ALIGN(16) ostate2[4 * 8];
	uint32_t _ALIGN(16) ibuf[4 * 16];
	uint32_t _ALIGN(16) obuf[4 * 16];
	int i, j;

	memcpy(istate, tstate, 4 * 32);
	sha256_transform_4way(istate, salt, 0);
	
	memcpy(ibuf, salt + 4 * 16, 4 * 16);
	memcpy(ibuf + 4 * 5, innerpad_4way, 4 * 44);
	memcpy(obuf + 4 * 8, outerpad_4way, 4 * 32);

	for (i = 0; i < 4; i++) {
		memcpy(obuf, istate, 4 * 32);
		ibuf[4 * 4 + 0] = i + 1;
		ibuf[4 * 4 + 1] = i + 1;
		ibuf[4 * 4 + 2] = i + 1;
		ibuf[4 * 4 + 3] = i + 1;
		sha256_transform_4way(obuf, ibuf, 0);

		memcpy(ostate2, ostate, 4 * 32);
		sha256_transform_4way(ostate2, obuf, 0);
		for (j = 0; j < 4 * 8; j++)
			output[4 * 8 * i + j] = swab32(ostate2[j]);
	}
}

static inline void PBKDF2_SHA256_128_32_4way(uint32_t *tstate,
	uint32_t *ostate, const uint32_t *salt, uint32_t *output)
{
	uint32_t _ALIGN(16) buf[4 * 16];
	int i;
	
	sha256_transform_4way(tstate, salt, 1);
	sha256_transform_4way(tstate, salt + 4 * 16, 1);
	sha256_transform_4way(tstate, finalblk_4way, 0);
	memcpy(buf, tstate, 4 * 32);
	memcpy(buf + 4 * 8, outerpad_4way, 4 * 32);

	sha256_transform_4way(ostate, buf, 0);
	for (i = 0; i < 4 * 8; i++)
		output[i] = swab32(ostate[i]);
}

#endif /* HAVE_SHA256_4WAY */


#ifdef HAVE_SHA256_8WAY

static const uint32_t _ALIGN(32) finalblk_8way[8 * 16] = {
	0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001, 0x00000001,
	0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000, 0x80000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000620, 0x00000620, 0x00000620, 0x00000620, 0x00000620, 0x00000620, 0x00000620, 0x00000620
};

static inline void HMAC_SHA256_80_init_8way(const uint32_t *key,
	uint32_t *tstate, uint32_t *ostate)
{
	uint32_t _ALIGN(32) ihash[8 * 8];
	uint32_t _ALIGN(32)  pad[8 * 16];
	int i;
	
	/* tstate is assumed to contain the midstate of key */
	memcpy(pad, key + 8 * 16, 8 * 16);
	for (i = 0; i < 8; i++)
		pad[8 * 4 + i] = 0x80000000;
	memset(pad + 8 * 5, 0x00, 8 * 40);
	for (i = 0; i < 8; i++)
		pad[8 * 15 + i] = 0x00000280;
	sha256_transform_8way(tstate, pad, 0);
	memcpy(ihash, tstate, 8 * 32);
	
	sha256_init_8way(ostate);
	for (i = 0; i < 8 * 8; i++)
		pad[i] = ihash[i] ^ 0x5c5c5c5c;
	for (; i < 8 * 16; i++)
		pad[i] = 0x5c5c5c5c;
	sha256_transform_8way(ostate, pad, 0);
	
	sha256_init_8way(tstate);
	for (i = 0; i < 8 * 8; i++)
		pad[i] = ihash[i] ^ 0x36363636;
	for (; i < 8 * 16; i++)
		pad[i] = 0x36363636;
	sha256_transform_8way(tstate, pad, 0);
}

static inline void PBKDF2_SHA256_80_128_8way(const uint32_t *tstate,
	const uint32_t *ostate, const uint32_t *salt, uint32_t *output)
{
	uint32_t _ALIGN(32) istate[8 * 8];
	uint32_t _ALIGN(32) ostate2[8 * 8];
	uint32_t _ALIGN(32) ibuf[8 * 16];
	uint32_t _ALIGN(32) obuf[8 * 16];
	int i, j;
	
	memcpy(istate, tstate, 8 * 32);
	sha256_transform_8way(istate, salt, 0);
	
	memcpy(ibuf, salt + 8 * 16, 8 * 16);
	for (i = 0; i < 8; i++)
		ibuf[8 * 5 + i] = 0x80000000;
	memset(ibuf + 8 * 6, 0x00, 8 * 36);
	for (i = 0; i < 8; i++)
		ibuf[8 * 15 + i] = 0x000004a0;
	
	for (i = 0; i < 8; i++)
		obuf[8 * 8 + i] = 0x80000000;
	memset(obuf + 8 * 9, 0x00, 8 * 24);
	for (i = 0; i < 8; i++)
		obuf[8 * 15 + i] = 0x00000300;
	
	for (i = 0; i < 4; i++) {
		memcpy(obuf, istate, 8 * 32);
		ibuf[8 * 4 + 0] = i + 1;
		ibuf[8 * 4 + 1] = i + 1;
		ibuf[8 * 4 + 2] = i + 1;
		ibuf[8 * 4 + 3] = i + 1;
		ibuf[8 * 4 + 4] = i + 1;
		ibuf[8 * 4 + 5] = i + 1;
		ibuf[8 * 4 + 6] = i + 1;
		ibuf[8 * 4 + 7] = i + 1;
		sha256_transform_8way(obuf, ibuf, 0);
		
		memcpy(ostate2, ostate, 8 * 32);
		sha256_transform_8way(ostate2, obuf, 0);
		for (j = 0; j < 8 * 8; j++)
			output[8 * 8 * i + j] = swab32(ostate2[j]);
	}
}

static inline void PBKDF2_SHA256_128_32_8way(uint32_t *tstate,
	uint32_t *ostate, const uint32_t *salt, uint32_t *output)
{
	uint32_t _ALIGN(32) buf[8 * 16];
	int i;
	
	sha256_transform_8way(tstate, salt, 1);
	sha256_transform_8way(tstate, salt + 8 * 16, 1);
	sha256_transform_8way(tstate, finalblk_8way, 0);
	
	memcpy(buf, tstate, 8 * 32);
	for (i = 0; i < 8; i++)
		buf[8 * 8 + i] = 0x80000000;
	memset(buf + 8 * 9, 0x00, 8 * 24);
	for (i = 0; i < 8; i++)
		buf[8 * 15 + i] = 0x00000300;
	sha256_transform_8way(ostate, buf, 0);
	
	for (i = 0; i < 8 * 8; i++)
		output[i] = swab32(ostate[i]);
}

#endif /* HAVE_SHA256_8WAY */


#if defined(USE_ASM) && defined(__x86_64__)

#define SCRYPT_MAX_WAYS 12
#define HAVE_SCRYPT_3WAY 1
int scrypt_best_throughput();
void scrypt_core(uint32_t *X, uint32_t *V, int N);
void scrypt_core_3way(uint32_t *X, uint32_t *V, int N);
#if defined(USE_AVX2)
#undef SCRYPT_MAX_WAYS
#define SCRYPT_MAX_WAYS 24
#define HAVE_SCRYPT_6WAY 1
void scrypt_core_6way(uint32_t *X, uint32_t *V, int N);
#endif

#elif defined(USE_ASM) && defined(__i386__)

#define SCRYPT_MAX_WAYS 4
#define scrypt_best_throughput() 1
void scrypt_core(uint32_t *X, uint32_t *V, int N);

#elif defined(USE_ASM) && defined(__arm__) && defined(__APCS_32__)

void scrypt_core(uint32_t *X, uint32_t *V, int N);
#if defined(__ARM_NEON__)
#undef HAVE_SHA256_4WAY
#define SCRYPT_MAX_WAYS 3
#define HAVE_SCRYPT_3WAY 1
#define scrypt_best_throughput() 3
void scrypt_core_3way(uint32_t *X, uint32_t *V, int N);
#endif

#elif defined(__aarch64__)

#undef HAVE_SHA256_4WAY
#define SCRYPT_MAX_WAYS 3
#define HAVE_SCRYPT_3WAY 1
#define scrypt_best_throughput() 3
void scrypt_core(uint32_t *X, uint32_t *V, int N);
void scrypt_core_3way(uint32_t *X, uint32_t *V, int N);

#else

static inline void xor_salsa8(uint32_t B[16], const uint32_t Bx[16])
{
	uint32_t x00,x01,x02,x03,x04,x05,x06,x07,x08,x09,x10,x11,x12,x13,x14,x15;
	int i;

	x00 = (B[ 0] ^= Bx[ 0]);
	x01 = (B[ 1] ^= Bx[ 1]);
	x02 = (B[ 2] ^= Bx[ 2]);
	x03 = (B[ 3] ^= Bx[ 3]);
	x04 = (B[ 4] ^= Bx[ 4]);
	x05 = (B[ 5] ^= Bx[ 5]);
	x06 = (B[ 6] ^= Bx[ 6]);
	x07 = (B[ 7] ^= Bx[ 7]);
	x08 = (B[ 8] ^= Bx[ 8]);
	x09 = (B[ 9] ^= Bx[ 9]);
	x10 = (B[10] ^= Bx[10]);
	x11 = (B[11] ^= Bx[11]);
	x12 = (B[12] ^= Bx[12]);
	x13 = (B[13] ^= Bx[13]);
	x14 = (B[14] ^= Bx[14]);
	x15 = (B[15] ^= Bx[15]);
	for (i = 0; i < 8; i += 2) {
#define R(a, b) (((a) << (b)) | ((a) >> (32 - (b))))
		/* Operate on columns. */
		x04 ^= R(x00+x12, 7);	x09 ^= R(x05+x01, 7);
		x14 ^= R(x10+x06, 7);	x03 ^= R(x15+x11, 7);
		
		x08 ^= R(x04+x00, 9);	x13 ^= R(x09+x05, 9);
		x02 ^= R(x14+x10, 9);	x07 ^= R(x03+x15, 9);
		
		x12 ^= R(x08+x04,13);	x01 ^= R(x13+x09,13);
		x06 ^= R(x02+x14,13);	x11 ^= R(x07+x03,13);
		
		x00 ^= R(x12+x08,18);	x05 ^= R(x01+x13,18);
		x10 ^= R(x06+x02,18);	x15 ^= R(x11+x07,18);
		
		/* Operate on rows. */
		x01 ^= R(x00+x03, 7);	x06 ^= R(x05+x04, 7);
		x11 ^= R(x10+x09, 7);	x12 ^= R(x15+x14, 7);
		
		x02 ^= R(x01+x00, 9);	x07 ^= R(x06+x05, 9);
		x08 ^= R(x11+x10, 9);	x13 ^= R(x12+x15, 9);
		
		x03 ^= R(x02+x01,13);	x04 ^= R(x07+x06,13);
		x09 ^= R(x08+x11,13);	x14 ^= R(x13+x12,13);
		
		x00 ^= R(x03+x02,18);	x05 ^= R(x04+x07,18);
		x10 ^= R(x09+x08,18);	x15 ^= R(x14+x13,18);
#undef R
	}
	B[ 0] += x00;
	B[ 1] += x01;
	B[ 2] += x02;
	B[ 3] += x03;
	B[ 4] += x04;
	B[ 5] += x05;
	B[ 6] += x06;
	B[ 7] += x07;
	B[ 8] += x08;
	B[ 9] += x09;
	B[10] += x10;
	B[11] += x11;
	B[12] += x12;
	B[13] += x13;
	B[14] += x14;
	B[15] += x15;
}

static inline void scrypt_core(uint32_t *X, uint32_t *V, int N)
{
	int i;

	for (i = 0; i < N; i++) {
		memcpy(&V[i * 32], X, 128);
		xor_salsa8(&X[0], &X[16]);
		xor_salsa8(&X[16], &X[0]);
	}
	for (i = 0; i < N; i++) {
		uint32_t j = 32 * (X[16] & (N - 1));
		for (uint8_t k = 0; k < 32; k++)
			X[k] ^= V[j + k];
		xor_salsa8(&X[0], &X[16]);
		xor_salsa8(&X[16], &X[0]);
	}
}

#endif

#ifndef SCRYPT_MAX_WAYS
#define SCRYPT_MAX_WAYS 1
#define scrypt_best_throughput() 1
#endif

/*
 * Bytes of V (scrypt ROM) touched by one scrypt_core* call.
 * x86-64 assembly:
 *   1-way: N * 128
 *   3-way: N * 3 * 128   (AVX; stride 3*128)
 *   6-way: N * 6 * 128   (AVX2; stride 6*128)
 * 12-way and 24-way hashing reuse that ROM, so the scratchpad stays 3 or 6
 * lanes rather than SCRYPT_MAX_WAYS (12 / 24). Treating every throughput >= 3
 * as 6 lanes doubled RAM on AVX-only CPUs.
 *
 * g_scrypt_lanes_override is the autotune choice (1, 3, or 6). --ryzen wins.
 */
static int g_scrypt_lanes_override;

int scrypt_rom_lane_count(void)
{
#if defined(USE_ASM) && defined(__x86_64__)
	int tp;

	if (opt_ryzen)
		return 3;
	if (g_scrypt_lanes_override > 0)
		return g_scrypt_lanes_override;
	tp = scrypt_best_throughput();
	if (tp >= 6)
		return 6;
	if (tp >= 3)
		return 3;
	return 1;
#else
	if (g_scrypt_lanes_override > 0)
		return g_scrypt_lanes_override;
	return SCRYPT_MAX_WAYS > 0 ? SCRYPT_MAX_WAYS : 1;
#endif
}

int scrypt_rom_lane_count_for(int force_throughput)
{
	if (force_throughput == 1 || force_throughput == 3 || force_throughput == 6)
		return force_throughput;
	return scrypt_rom_lane_count();
}

void scrypt_set_lane_override(int lanes)
{
	if (lanes == 1 || lanes == 3 || lanes == 6)
		g_scrypt_lanes_override = lanes;
}

size_t scrypt_scratchpad_bytes(int N)
{
	return (size_t)N * (size_t)scrypt_rom_lane_count() * 128 + 63;
}

size_t scrypt_scratchpad_bytes_for(int N, int force_throughput)
{
	return (size_t)N * (size_t)scrypt_rom_lane_count_for(force_throughput) * 128 + 63;
}

/*
 * Scratchpad allocator.
 *
 * Verium's ROM is 128 MB per lane (N = 2^20). Random lookups in the second
 * scrypt pass thrash a 4 KB TLB, so 2 MB pages are a large fraction of
 * hashrate. Hash regression tests leave preparation off and keep the old
 * lazy commit; main() turns it on before tuning and mining.
 *
 * Windows MEM_LARGE_PAGES requires the size to be a multiple of
 * GetLargePageMinimum(). Rounding only to 4 KB made that call fail even
 * when "Lock pages in memory" was granted.
 */
enum {
	SCRYPT_KIND_MALLOC = 1,
	SCRYPT_KIND_VIRTUAL = 2,
	SCRYPT_KIND_MMAP = 3
};

enum {
	SCRYPT_PAGES_4K = 0,
	SCRYPT_PAGES_HUGE = 1,
	SCRYPT_PAGES_THP = 2
};

#define SCRYPT_MEM_SLOTS (MAX_CPUS + 8)

struct scrypt_memslot {
	unsigned char *ptr;
	size_t bytes;
	int kind;
	int used;
};

static struct scrypt_memslot g_scrypt_slots[SCRYPT_MEM_SLOTS];
static pthread_mutex_t g_scrypt_mem_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_scrypt_large_pages;
static int g_scrypt_prepare_pages;
#if defined(WIN32)
static unsigned long g_scrypt_page_err;
#endif

int scrypt_large_pages_active(void)
{
	return g_scrypt_large_pages;
}

void scrypt_prepare_huge_pages(int enable)
{
	g_scrypt_prepare_pages = enable ? 1 : 0;
}

static int scrypt_track(unsigned char *p, size_t bytes, int kind)
{
	int i;

	if (!p)
		return 0;
	pthread_mutex_lock(&g_scrypt_mem_lock);
	for (i = 0; i < SCRYPT_MEM_SLOTS; i++) {
		if (!g_scrypt_slots[i].used) {
			g_scrypt_slots[i].used = 1;
			g_scrypt_slots[i].ptr = p;
			g_scrypt_slots[i].bytes = bytes;
			g_scrypt_slots[i].kind = kind;
			pthread_mutex_unlock(&g_scrypt_mem_lock);
			return 1;
		}
	}
	pthread_mutex_unlock(&g_scrypt_mem_lock);
	return 0;
}

static void scrypt_discard(unsigned char *p, size_t bytes, int kind)
{
	if (!p)
		return;
	if (kind == SCRYPT_KIND_MMAP) {
#if defined(__linux__) || defined(__APPLE__)
		munmap(p, bytes);
#endif
	} else if (kind == SCRYPT_KIND_VIRTUAL) {
#if defined(WIN32)
		VirtualFree(p, 0, MEM_RELEASE);
#endif
	} else {
		free(p);
	}
}

static void scrypt_note_pages(int mode)
{
	static int logged;
	int do_log = 0;

	if (mode == SCRYPT_PAGES_HUGE || mode == SCRYPT_PAGES_THP)
		g_scrypt_large_pages = 1;
	pthread_mutex_lock(&g_scrypt_mem_lock);
	if (!logged) {
		logged = 1;
		do_log = 1;
	}
	pthread_mutex_unlock(&g_scrypt_mem_lock);
	if (!do_log)
		return;
	if (mode == SCRYPT_PAGES_HUGE)
		applog(LOG_NOTICE, "Scrypt scratchpad: using 2 MB pages");
	else if (mode == SCRYPT_PAGES_THP)
		applog(LOG_NOTICE, "Scrypt scratchpad: using transparent 2 MB pages");
#if defined(WIN32)
	else if (g_scrypt_page_err)
		applog(LOG_NOTICE,
			"Scrypt scratchpad: using 4 KB pages (large-page error %lu — grant \"Lock pages in memory\")",
			g_scrypt_page_err);
#endif
	else
		applog(LOG_NOTICE,
			"Scrypt scratchpad: using 4 KB pages (2 MB pages raise hashrate)");
}

static void scrypt_try_memlock(void *p, size_t bytes)
{
	static int warned;

	if (!g_scrypt_prepare_pages || !p || !bytes)
		return;
#if defined(__linux__) || defined(__APPLE__)
	if (mlock(p, bytes) == 0)
		return;
	if (!warned) {
		warned = 1;
		applog(LOG_INFO,
			"Scrypt scratchpad not locked in RAM (memlock limit). Pages can be swapped");
	}
#elif defined(WIN32)
	{
		HANDLE proc = GetCurrentProcess();
		SIZE_T minws = 0, maxws = 0;

		if (GetProcessWorkingSetSize(proc, &minws, &maxws)) {
			SIZE_T add = (SIZE_T)bytes;
			if (minws <= (SIZE_T)-1 - add && maxws <= (SIZE_T)-1 - add)
				SetProcessWorkingSetSize(proc, minws + add, maxws + add);
		}
		if (VirtualLock(p, (SIZE_T)bytes))
			return;
		if (!warned) {
			warned = 1;
			applog(LOG_INFO,
				"Scrypt scratchpad not locked in RAM (working set). Pages can be paged out");
		}
	}
#else
	(void)warned;
#endif
}

#if defined(__linux__)
static void scrypt_touch(void *p, size_t bytes, size_t stride)
{
	volatile unsigned char *b = (volatile unsigned char *)p;
	size_t off;

	if (!stride)
		stride = 4096;
	for (off = 0; off < bytes; off += stride)
		b[off] = 0;
}

static int scrypt_linux_vma_huge(const void *ptr)
{
	FILE *f;
	char line[320];
	char prefix[80];
	int in = 0;
	int n;
	unsigned long size_kb = 0, huge_kb = 0, kps = 0;

	f = fopen("/proc/self/smaps", "r");
	if (!f)
		return 0;
	n = snprintf(prefix, sizeof(prefix), "%" PRIxPTR "-", (uintptr_t)ptr);
	if (n <= 0 || (size_t)n >= sizeof(prefix)) {
		fclose(f);
		return 0;
	}
	while (fgets(line, sizeof(line), f)) {
		if (!in) {
			if (strncmp(line, prefix, (size_t)n) == 0)
				in = 1;
			continue;
		}
		if (line[0] != ' ' && line[0] != '\t' && strchr(line, '-'))
			break;
		if (strncmp(line, "Size:", 5) == 0) {
			if (sscanf(line, "Size: %lu", &size_kb) != 1)
				size_kb = 0;
		} else if (strncmp(line, "KernelPageSize:", 15) == 0) {
			if (sscanf(line, "KernelPageSize: %lu", &kps) != 1)
				kps = 0;
		} else if (strncmp(line, "AnonHugePages:", 14) == 0) {
			if (sscanf(line, "AnonHugePages: %lu", &huge_kb) != 1)
				huge_kb = 0;
		}
	}
	fclose(f);
	if (kps >= 2048)
		return 1;
	return size_kb > 0 && huge_kb * 2 >= size_kb;
}

static unsigned char *scrypt_linux_hugepage(size_t bytes)
{
	void *p = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB | MAP_HUGE_2MB, -1, 0);

	if (p == MAP_FAILED)
		return NULL;
#ifdef MADV_DONTDUMP
	madvise(p, bytes, MADV_DONTDUMP);
#endif
	return (unsigned char *)p;
}

/* Over-map, trim to a 2 MB alignment, then ask for transparent huge pages. */
static unsigned char *scrypt_linux_thp(size_t bytes, int *huge)
{
	size_t map_bytes = bytes + SCRYPT_HUGEPAGE_SIZE;
	void *raw;
	uintptr_t base, aligned;
	size_t lead, tail;

	*huge = 0;
	raw = mmap(NULL, map_bytes, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (raw == MAP_FAILED)
		return NULL;
	base = (uintptr_t)raw;
	aligned = (base + SCRYPT_HUGEPAGE_SIZE - 1) & ~(uintptr_t)(SCRYPT_HUGEPAGE_SIZE - 1);
	lead = (size_t)(aligned - base);
	tail = map_bytes - lead - bytes;
	if (lead)
		munmap(raw, lead);
	if (tail)
		munmap((void *)(aligned + bytes), tail);
#ifdef MADV_HUGEPAGE
	madvise((void *)aligned, bytes, MADV_HUGEPAGE);
#endif
#ifdef MADV_DONTDUMP
	madvise((void *)aligned, bytes, MADV_DONTDUMP);
#endif
	if (g_scrypt_prepare_pages) {
		int collapsed = 0;

		scrypt_touch((void *)aligned, bytes, SCRYPT_HUGEPAGE_SIZE);
		if (!scrypt_linux_vma_huge((void *)aligned)) {
			scrypt_touch((void *)aligned, bytes, 4096);
#ifdef MADV_COLLAPSE
			if (madvise((void *)aligned, bytes, MADV_COLLAPSE) == 0)
				collapsed = 1;
#endif
		}
		if (collapsed || scrypt_linux_vma_huge((void *)aligned))
			*huge = 1;
	}
	return (unsigned char *)aligned;
}
#endif /* __linux__ */

static unsigned char *scrypt_alloc_platform(size_t need, size_t *out_bytes, int *kind, int *mode)
{
	*kind = SCRYPT_KIND_MALLOC;
	*mode = SCRYPT_PAGES_4K;
	*out_bytes = need;

#if defined(WIN32)
	{
		SIZE_T page = GetLargePageMinimum();
		if (page >= 4096 && (page & (page - 1)) == 0) {
			size_t bytes = (need + (size_t)page - 1) & ~((size_t)page - 1);
			void *p = VirtualAlloc(NULL, bytes,
				MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES, PAGE_READWRITE);
			if (p) {
				*kind = SCRYPT_KIND_VIRTUAL;
				*mode = SCRYPT_PAGES_HUGE;
				*out_bytes = bytes;
				return (unsigned char *)p;
			}
			g_scrypt_page_err = (unsigned long)GetLastError();
		}
		{
			size_t bytes = (need + 4095u) & ~(size_t)4095u;
			void *p = VirtualAlloc(NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
			if (p) {
				*kind = SCRYPT_KIND_VIRTUAL;
				*out_bytes = bytes;
				return (unsigned char *)p;
			}
		}
	}
#endif

#if defined(__linux__)
	{
		size_t bytes = (need + SCRYPT_HUGEPAGE_SIZE - 1)
			& ~(size_t)(SCRYPT_HUGEPAGE_SIZE - 1);
		unsigned char *p = NULL;
		int huge = 0;

		if (g_scrypt_prepare_pages) {
			p = scrypt_linux_hugepage(bytes);
			if (p) {
				*kind = SCRYPT_KIND_MMAP;
				*mode = SCRYPT_PAGES_HUGE;
				*out_bytes = bytes;
				return p;
			}
			p = scrypt_linux_thp(bytes, &huge);
			if (p) {
				*kind = SCRYPT_KIND_MMAP;
				*mode = huge ? SCRYPT_PAGES_THP : SCRYPT_PAGES_4K;
				*out_bytes = bytes;
				return p;
			}
		}
		p = (unsigned char *)aligned_alloc(SCRYPT_HUGEPAGE_SIZE, bytes);
		if (p) {
#ifdef MADV_HUGEPAGE
			madvise(p, bytes, MADV_HUGEPAGE);
#endif
#ifdef MADV_DONTDUMP
			madvise(p, bytes, MADV_DONTDUMP);
#endif
			*kind = SCRYPT_KIND_MALLOC;
			*out_bytes = bytes;
			return p;
		}
	}
#endif

#if defined(__APPLE__)
	{
		size_t bytes = (need + SCRYPT_HUGEPAGE_SIZE - 1)
			& ~(size_t)(SCRYPT_HUGEPAGE_SIZE - 1);
		void *p = MAP_FAILED;

		if (g_scrypt_prepare_pages) {
			p = mmap(NULL, bytes, PROT_READ | PROT_WRITE,
				MAP_PRIVATE | MAP_ANON, VM_FLAGS_SUPERPAGE_SIZE_2MB, 0);
			if (p != MAP_FAILED) {
				*kind = SCRYPT_KIND_MMAP;
				*mode = SCRYPT_PAGES_HUGE;
				*out_bytes = bytes;
				return (unsigned char *)p;
			}
		}
		bytes = (need + 4095u) & ~(size_t)4095u;
		p = NULL;
		if (posix_memalign(&p, 4096, bytes) == 0 && p) {
			*kind = SCRYPT_KIND_MALLOC;
			*out_bytes = bytes;
			return (unsigned char *)p;
		}
	}
#endif

	return (unsigned char *)malloc(need);
}

unsigned char *scrypt_buffer_alloc(int N)
{
	return scrypt_buffer_alloc_for(N, -1);
}

unsigned char *scrypt_buffer_alloc_for(int N, int force_throughput)
{
	size_t need = scrypt_scratchpad_bytes_for(N, force_throughput);
	size_t bytes = need;
	int kind = SCRYPT_KIND_MALLOC;
	int mode = SCRYPT_PAGES_4K;
	unsigned char *p = scrypt_alloc_platform(need, &bytes, &kind, &mode);

	if (!p)
		return NULL;
	if (scrypt_track(p, bytes, kind)) {
		scrypt_note_pages(mode);
		if (mode != SCRYPT_PAGES_HUGE)
			scrypt_try_memlock(p, bytes);
		return p;
	}
	scrypt_discard(p, bytes, kind);
	p = (unsigned char *)malloc(need);
	if (!p)
		return NULL;
	if (!scrypt_track(p, need, SCRYPT_KIND_MALLOC))
		return p;
	scrypt_note_pages(SCRYPT_PAGES_4K);
	return p;
}

void scrypt_buffer_free(unsigned char *buf, int N)
{
	int i;
	int kind = 0;
	size_t bytes = 0;

	(void)N;
	if (!buf)
		return;

	pthread_mutex_lock(&g_scrypt_mem_lock);
	for (i = 0; i < SCRYPT_MEM_SLOTS; i++) {
		if (g_scrypt_slots[i].used && g_scrypt_slots[i].ptr == buf) {
			kind = g_scrypt_slots[i].kind;
			bytes = g_scrypt_slots[i].bytes;
			g_scrypt_slots[i].used = 0;
			g_scrypt_slots[i].ptr = NULL;
			g_scrypt_slots[i].bytes = 0;
			g_scrypt_slots[i].kind = 0;
			break;
		}
	}
	pthread_mutex_unlock(&g_scrypt_mem_lock);

	if (kind) {
		scrypt_discard(buf, bytes, kind);
		return;
	}
#if defined(WIN32)
	if (!VirtualFree(buf, 0, MEM_RELEASE))
		free(buf);
#else
	free(buf);
#endif
}

static void scrypt_1024_1_1_256(const uint32_t *input, uint32_t *output,
	uint32_t *midstate, unsigned char *scratchpad, int N)
{
	uint32_t tstate[8], ostate[8];
	uint32_t X[32];
	uint32_t *V;
	
	V = (uint32_t *)(((uintptr_t)(scratchpad) + 63) & ~ (uintptr_t)(63));

	memcpy(tstate, midstate, 32);
	HMAC_SHA256_80_init(input, tstate, ostate);
	PBKDF2_SHA256_80_128(tstate, ostate, input, X);

	scrypt_core(X, V, N);

	PBKDF2_SHA256_128_32(tstate, ostate, X, output);
}

#ifdef HAVE_SHA256_4WAY
static void scrypt_1024_1_1_256_4way(const uint32_t *input,
	uint32_t *output, uint32_t *midstate, unsigned char *scratchpad, int N)
{
	uint32_t _ALIGN(128) tstate[4 * 8];
	uint32_t _ALIGN(128) ostate[4 * 8];
	uint32_t _ALIGN(128) W[4 * 32];
	uint32_t _ALIGN(128) X[4 * 32];
	uint32_t *V;
	int i, k;
	
	V = (uint32_t *)(((uintptr_t)(scratchpad) + 63) & ~ (uintptr_t)(63));

	for (i = 0; i < 20; i++)
		for (k = 0; k < 4; k++)
			W[4 * i + k] = input[k * 20 + i];
	for (i = 0; i < 8; i++)
		for (k = 0; k < 4; k++)
			tstate[4 * i + k] = midstate[i];
	HMAC_SHA256_80_init_4way(W, tstate, ostate);
	PBKDF2_SHA256_80_128_4way(tstate, ostate, W, W);
	for (i = 0; i < 32; i++)
		for (k = 0; k < 4; k++)
			X[k * 32 + i] = W[4 * i + k];
	scrypt_core(X + 0 * 32, V, N);
	scrypt_core(X + 1 * 32, V, N);
	scrypt_core(X + 2 * 32, V, N);
	scrypt_core(X + 3 * 32, V, N);
	for (i = 0; i < 32; i++)
		for (k = 0; k < 4; k++)
			W[4 * i + k] = X[k * 32 + i];
	PBKDF2_SHA256_128_32_4way(tstate, ostate, W, W);
	for (i = 0; i < 8; i++)
		for (k = 0; k < 4; k++)
			output[k * 8 + i] = W[4 * i + k];
}
#endif /* HAVE_SHA256_4WAY */

#ifdef HAVE_SCRYPT_3WAY

static void scrypt_1024_1_1_256_3way(const uint32_t *input,
	uint32_t *output, uint32_t *midstate, unsigned char *scratchpad, int N)
{
	uint32_t _ALIGN(64) tstate[3 * 8], ostate[3 * 8];
	uint32_t _ALIGN(64) X[3 * 32];
	uint32_t *V;
	
	V = (uint32_t *)(((uintptr_t)(scratchpad) + 63) & ~ (uintptr_t)(63));

	memcpy(tstate +  0, midstate, 32);
	memcpy(tstate +  8, midstate, 32);
	memcpy(tstate + 16, midstate, 32);
	HMAC_SHA256_80_init(input +  0, tstate +  0, ostate +  0);
	HMAC_SHA256_80_init(input + 20, tstate +  8, ostate +  8);
	HMAC_SHA256_80_init(input + 40, tstate + 16, ostate + 16);
	PBKDF2_SHA256_80_128(tstate +  0, ostate +  0, input +  0, X +  0);
	PBKDF2_SHA256_80_128(tstate +  8, ostate +  8, input + 20, X + 32);
	PBKDF2_SHA256_80_128(tstate + 16, ostate + 16, input + 40, X + 64);

	scrypt_core_3way(X, V, N);

	PBKDF2_SHA256_128_32(tstate +  0, ostate +  0, X +  0, output +  0);
	PBKDF2_SHA256_128_32(tstate +  8, ostate +  8, X + 32, output +  8);
	PBKDF2_SHA256_128_32(tstate + 16, ostate + 16, X + 64, output + 16);
}

#ifdef HAVE_SHA256_4WAY
static void scrypt_1024_1_1_256_12way(const uint32_t *input,
	uint32_t *output, uint32_t *midstate, unsigned char *scratchpad, int N)
{
	uint32_t _ALIGN(128) tstate[12 * 8];
	uint32_t _ALIGN(128) ostate[12 * 8];
	uint32_t _ALIGN(128) W[12 * 32];
	uint32_t _ALIGN(128) X[12 * 32];
	uint32_t *V;
	int i, j, k;
	
	V = (uint32_t *)(((uintptr_t)(scratchpad) + 63) & ~ (uintptr_t)(63));

	for (j = 0; j < 3; j++)
		for (i = 0; i < 20; i++)
			for (k = 0; k < 4; k++)
				W[128 * j + 4 * i + k] = input[80 * j + k * 20 + i];
	for (j = 0; j < 3; j++)
		for (i = 0; i < 8; i++)
			for (k = 0; k < 4; k++)
				tstate[32 * j + 4 * i + k] = midstate[i];
	HMAC_SHA256_80_init_4way(W +   0, tstate +  0, ostate +  0);
	HMAC_SHA256_80_init_4way(W + 128, tstate + 32, ostate + 32);
	HMAC_SHA256_80_init_4way(W + 256, tstate + 64, ostate + 64);
	PBKDF2_SHA256_80_128_4way(tstate +  0, ostate +  0, W +   0, W +   0);
	PBKDF2_SHA256_80_128_4way(tstate + 32, ostate + 32, W + 128, W + 128);
	PBKDF2_SHA256_80_128_4way(tstate + 64, ostate + 64, W + 256, W + 256);
	for (j = 0; j < 3; j++)
		for (i = 0; i < 32; i++)
			for (k = 0; k < 4; k++)
				X[128 * j + k * 32 + i] = W[128 * j + 4 * i + k];
	scrypt_core_3way(X + 0 * 96, V, N);
	scrypt_core_3way(X + 1 * 96, V, N);
	scrypt_core_3way(X + 2 * 96, V, N);
	scrypt_core_3way(X + 3 * 96, V, N);
	for (j = 0; j < 3; j++)
		for (i = 0; i < 32; i++)
			for (k = 0; k < 4; k++)
				W[128 * j + 4 * i + k] = X[128 * j + k * 32 + i];
	PBKDF2_SHA256_128_32_4way(tstate +  0, ostate +  0, W +   0, W +   0);
	PBKDF2_SHA256_128_32_4way(tstate + 32, ostate + 32, W + 128, W + 128);
	PBKDF2_SHA256_128_32_4way(tstate + 64, ostate + 64, W + 256, W + 256);
	for (j = 0; j < 3; j++)
		for (i = 0; i < 8; i++)
			for (k = 0; k < 4; k++)
				output[32 * j + k * 8 + i] = W[128 * j + 4 * i + k];
}
#endif /* HAVE_SHA256_4WAY */

#endif /* HAVE_SCRYPT_3WAY */

#ifdef HAVE_SCRYPT_6WAY
static void scrypt_1024_1_1_256_24way(const uint32_t *input,
	uint32_t *output, uint32_t *midstate, unsigned char *scratchpad, int N)
{
	uint32_t _ALIGN(128) tstate[24 * 8];
	uint32_t _ALIGN(128) ostate[24 * 8];
	uint32_t _ALIGN(128) W[24 * 32];
	uint32_t _ALIGN(128) X[24 * 32];
	uint32_t *V;
	int i, j, k;
	
	V = (uint32_t *)(((uintptr_t)(scratchpad) + 63) & ~ (uintptr_t)(63));
	
	for (j = 0; j < 3; j++) 
		for (i = 0; i < 20; i++)
			for (k = 0; k < 8; k++)
				W[8 * 32 * j + 8 * i + k] = input[8 * 20 * j + k * 20 + i];
	for (j = 0; j < 3; j++)
		for (i = 0; i < 8; i++)
			for (k = 0; k < 8; k++)
				tstate[8 * 8 * j + 8 * i + k] = midstate[i];
	HMAC_SHA256_80_init_8way(W +   0, tstate +   0, ostate +   0);
	HMAC_SHA256_80_init_8way(W + 256, tstate +  64, ostate +  64);
	HMAC_SHA256_80_init_8way(W + 512, tstate + 128, ostate + 128);
	PBKDF2_SHA256_80_128_8way(tstate +   0, ostate +   0, W +   0, W +   0);
	PBKDF2_SHA256_80_128_8way(tstate +  64, ostate +  64, W + 256, W + 256);
	PBKDF2_SHA256_80_128_8way(tstate + 128, ostate + 128, W + 512, W + 512);
	for (j = 0; j < 3; j++)
		for (i = 0; i < 32; i++)
			for (k = 0; k < 8; k++)
				X[8 * 32 * j + k * 32 + i] = W[8 * 32 * j + 8 * i + k];
	scrypt_core_6way(X + 0 * 32, V, N);
	scrypt_core_6way(X + 6 * 32, V, N);
	scrypt_core_6way(X + 12 * 32, V, N);
	scrypt_core_6way(X + 18 * 32, V, N);
	for (j = 0; j < 3; j++)
		for (i = 0; i < 32; i++)
			for (k = 0; k < 8; k++)
				W[8 * 32 * j + 8 * i + k] = X[8 * 32 * j + k * 32 + i];
	PBKDF2_SHA256_128_32_8way(tstate +   0, ostate +   0, W +   0, W +   0);
	PBKDF2_SHA256_128_32_8way(tstate +  64, ostate +  64, W + 256, W + 256);
	PBKDF2_SHA256_128_32_8way(tstate + 128, ostate + 128, W + 512, W + 512);
	for (j = 0; j < 3; j++)
		for (i = 0; i < 8; i++)
			for (k = 0; k < 8; k++)
				output[8 * 8 * j + k * 8 + i] = W[8 * 32 * j + 8 * i + k];
}
#endif /* HAVE_SCRYPT_6WAY */

#if defined(HAVE_SCRYPT_6WAY) && defined(HAVE_SCRYPT_3WAY) && defined(HAVE_SHA256_4WAY)
static volatile uint32_t g_scrypt_tune_sink;

static double scrypt_seconds(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + (double)tv.tv_usec * 1e-6;
}

/* One production batch: 24 hashes on the AVX2 path, 12 on the AVX path. */
static double scrypt_bench_lane(int lanes, unsigned char *scratch, int N, uint32_t nonce)
{
	uint32_t input[24 * 20];
	uint32_t output[24 * 8];
	uint32_t midstate[8];
	double t0, t1;
	int hashes = (lanes >= 6) ? 24 : 12;

	memset(input, 0, sizeof(input));
	input[19] = nonce;
	sha256_init(midstate);
	sha256_transform(midstate, input, 0);
	t0 = scrypt_seconds();
	if (lanes >= 6)
		scrypt_1024_1_1_256_24way(input, output, midstate, scratch, N);
	else
		scrypt_1024_1_1_256_12way(input, output, midstate, scratch, N);
	t1 = scrypt_seconds();
	g_scrypt_tune_sink = output[7];
	if (t1 <= t0)
		return 0.0;
	return (double)hashes / (t1 - t0);
}
#endif

int scrypt_lanes_tunable(void)
{
#if defined(HAVE_SCRYPT_6WAY) && defined(HAVE_SCRYPT_3WAY) && defined(HAVE_SHA256_4WAY)
	if (opt_ryzen)
		return 0;
	return scrypt_best_throughput() >= 6 && sha256_use_4way();
#else
	return 0;
#endif
}

int scrypt_measure_lanes(int N, double *hps_6way, double *hps_3way)
{
#if defined(HAVE_SCRYPT_6WAY) && defined(HAVE_SCRYPT_3WAY) && defined(HAVE_SHA256_4WAY)
	unsigned char *scratch;
	double a6, b6, a3, b3;

	if (!hps_6way || !hps_3way)
		return 0;
	if (!scrypt_lanes_tunable() || N < 2 || (N & (N - 1)) != 0)
		return 0;
	scratch = scrypt_buffer_alloc_for(N, 6);
	if (!scratch)
		return 0;
	(void)scrypt_bench_lane(6, scratch, N, 1);
	(void)scrypt_bench_lane(3, scratch, N, 2);
	a6 = scrypt_bench_lane(6, scratch, N, 3);
	a3 = scrypt_bench_lane(3, scratch, N, 4);
	b6 = scrypt_bench_lane(6, scratch, N, 5);
	b3 = scrypt_bench_lane(3, scratch, N, 6);
	scrypt_buffer_free(scratch, N);
	if (a6 <= 0.0 || b6 <= 0.0 || a3 <= 0.0 || b3 <= 0.0)
		return 0;
	*hps_6way = (a6 + b6) * 0.5;
	*hps_3way = (a3 + b3) * 0.5;
	return 1;
#else
	(void)N;
	(void)hps_6way;
	(void)hps_3way;
	return 0;
#endif
}

/*
 * Runtime CPU-feature dispatch.
 *
 * A single binary adapts to the host CPU at run time: scrypt_best_throughput()
 * and sha256_use_4way() probe CPUID (x86/x86-64) or compile-time NEON support
 * (ARM) and pick the widest validated core:
 *
 *     SSE2  -> 1-way   (scrypt_1024_1_1_256)
 *     AVX   -> 3-way   (scrypt_core_3way)        [throughput 3 / 12]
 *     AVX2  -> 6-way   (scrypt_core_6way)        [throughput 24]
 *
 * AVX-512 extension point: a future 8-way/16-way core would slot in here,
 * gated behind a new scrypt_best_throughput() return value and a runtime
 * AVX-512 check. It must produce bit-identical output and be locked by the
 * golden vectors in tests/ before being enabled, so it is intentionally not
 * shipped unvalidated.
 */
extern int scanhash_scrypt(int thr_id, struct work *work, uint32_t max_nonce, uint64_t *hashes_done,
	unsigned char *scratchbuf, uint32_t N, int force_throughput)
{
	uint32_t *pdata = work->data;
	uint32_t *ptarget = work->target;
	uint32_t data[SCRYPT_MAX_WAYS * 20], hash[SCRYPT_MAX_WAYS * 8];
	uint32_t midstate[8];
	uint32_t n = pdata[19] - 1;
	const uint32_t Htarg = ptarget[7];
	int throughput = scrypt_best_throughput();
	int i, j;

	if (g_scrypt_lanes_override > 0)
		throughput = g_scrypt_lanes_override;
	if (opt_ryzen)
		throughput = 3;
	if (force_throughput > 0)
		throughput = force_throughput;

#ifdef HAVE_SHA256_4WAY
	if (sha256_use_4way())
		throughput *= 4;
#endif

	for (i = 0; i < throughput; i++)
		for (j = 0; j < 19; j++)
			data[i * 20 + j] = have_stratum
				? be32dec((const unsigned char *)&pdata[j])
				: pdata[j];

	sha256_init(midstate);
	sha256_transform(midstate, data, 0);

	do {
		for (i = 0; i < throughput; i++) {
			uint32_t nonce = ++n;
			unsigned char nonce_le[4];

			le32enc(nonce_le, nonce);
			data[i * 20 + 19] = have_stratum
				? be32dec(nonce_le)
				: nonce;
		}
		
#if defined(HAVE_SHA256_4WAY)
		if (throughput == 4)
			scrypt_1024_1_1_256_4way(data, hash, midstate, scratchbuf, N);
		else
#endif
#if defined(HAVE_SCRYPT_3WAY) && defined(HAVE_SHA256_4WAY)
		if (throughput == 12)
			scrypt_1024_1_1_256_12way(data, hash, midstate, scratchbuf, N);
		else
#endif
#if defined(HAVE_SCRYPT_6WAY)
		if (throughput == 24)
			scrypt_1024_1_1_256_24way(data, hash, midstate, scratchbuf, N);
		else
#endif
#if defined(HAVE_SCRYPT_3WAY)
		if (throughput == 3)
			scrypt_1024_1_1_256_3way(data, hash, midstate, scratchbuf, N);
		else
#endif
		scrypt_1024_1_1_256(data, hash, midstate, scratchbuf, N);
		
		for (i = 0; i < throughput; i++) {
			if (unlikely(hash[i * 8 + 7] <= Htarg && fulltest(hash + i * 8, ptarget))) {
				work_set_target_ratio(work, hash + i * 8);
				*hashes_done = n - pdata[19] + 1;
				pdata[19] = have_stratum ? n - (throughput - 1 - i) : data[i * 20 + 19];
				return 1;
			}
		}
	} while (likely(n < max_nonce && !work_restart[thr_id].restart));

	*hashes_done = n - pdata[19] + 1;
	pdata[19] = n;
	return 0;
}

/* simple cpu test (util.c) */
void scrypthash(void *output, const void *input, uint32_t N)
{
	uint32_t midstate[8];
	char *scratchbuf = scrypt_buffer_alloc(N);

	memset(output, 0, 32);
	if (!scratchbuf)
		return;

	sha256_init(midstate);
	sha256_transform(midstate, input, 0);

	scrypt_1024_1_1_256((uint32_t*)input, (uint32_t*)output, midstate, scratchbuf, N);

	scrypt_buffer_free(scratchbuf, (int)N);
}

#if defined(HAVE_SCRYPT_3WAY)

/*
 * Verify each lane of the 3-way core matches the 1-way core (consensus safety).
 * Returns 0 on pass, number of failures otherwise.
 */
int scrypt_3way_equiv_selftest(void)
{
	const uint32_t N = 1048576;
	unsigned char *scratchbuf = scrypt_buffer_alloc((int)N);
	uint32_t midstate[8];
	uint32_t in3[60], out1[8], out3[8 * 3];
	uint32_t base_nonces[] = { 0, 1, 42, 0xffff };
	int failures = 0;
	size_t v, lane;

	if (!scratchbuf)
		return 1;

	for (v = 0; v < sizeof(base_nonces) / sizeof(base_nonces[0]); v++) {
		uint32_t n0 = base_nonces[v];

		memset(in3, 0, sizeof(in3));
		for (lane = 0; lane < 3; lane++)
			in3[lane * 20 + 19] = n0 + (uint32_t)lane;

		for (lane = 0; lane < 3; lane++) {
			uint32_t in1[20];

			memcpy(in1, &in3[lane * 20], sizeof(in1));
			sha256_init(midstate);
			sha256_transform(midstate, in1, 0);
			scrypt_1024_1_1_256(in1, out1, midstate, scratchbuf, (int)N);
			if (lane == 0) {
				sha256_init(midstate);
				sha256_transform(midstate, in3, 0);
				scrypt_1024_1_1_256_3way(in3, out3, midstate, scratchbuf, (int)N);
			}
			if (memcmp(out1, out3 + lane * 8, 32) != 0)
				failures++;
		}
	}

	scrypt_buffer_free(scratchbuf, (int)N);
	return failures;
}

#else

int scrypt_3way_equiv_selftest(void)
{
	return 0;
}

#endif
