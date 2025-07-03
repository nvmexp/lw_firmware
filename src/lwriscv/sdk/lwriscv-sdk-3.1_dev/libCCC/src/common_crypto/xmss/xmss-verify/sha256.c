/*
 * Copyright (c) 2020, LWPU Corporation.  All rights reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/*
 * This code is expected to be compiled with -O2 on the command line.
 *
 * Unfortunately, the compiler on CheetAh doesn't support
 * placing -O2 into source code like this:
 *
 * #pragma GCC push_options
 * #pragma GCC optimize ("O2")
 * this code
 * #pragma GCC pop_options
 */

#ifndef XMSS_NO_SYSTEM_INCLUDES
#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#endif

#include "sha256.h"
#include "xmss-intrinsics.h"

#ifdef XMSS_ARM_TARGET
#include <arm_acle.h>
#include <arm_neon.h>
#endif

#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32 - (b))))
#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32 - (b))))

#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA_EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define SHA_EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

typedef uint32_t WORD;

static const WORD k[64] = {
	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define PROCESS_SHA_STATE0 \
	STATE[0] = vsha256hq_u32(STATE[0], STATE[1], T[0]); \
	STATE[1] = vsha256h2q_u32(STATE[1], T[2], T[0]);

#define PROCESS_SHA_STATE1 \
	STATE[0] = vsha256hq_u32(STATE[0], STATE[1], T[1]); \
	STATE[1] = vsha256h2q_u32(STATE[1], T[2], T[1]);

#define ROUND4_0(i) \
	M[0] = vsha256su0q_u32(M[0], M[1]); \
	T[2] = STATE[0]; \
	T[1] = vaddq_u32(M[1], vld1q_u32(k + i)); \
	PROCESS_SHA_STATE0 \
		M[0] = vsha256su1q_u32(M[0], M[2], M[3]);

#define ROUND4_1(i) \
	M[1] = vsha256su0q_u32(M[1], M[2]); \
	T[2] = STATE[0]; \
	T[0] = vaddq_u32(M[2], vld1q_u32(k + i)); \
	PROCESS_SHA_STATE1 \
		M[1] = vsha256su1q_u32(M[1], M[3], M[0]);

#define ROUND4_2(i) \
	M[2] = vsha256su0q_u32(M[2], M[3]); \
	T[2] = STATE[0]; \
	T[1] = vaddq_u32(M[3], vld1q_u32(k + i)); \
	PROCESS_SHA_STATE0 \
		M[2] = vsha256su1q_u32(M[2], M[0], M[1]);

#define ROUND4_3(i) \
	M[3] = vsha256su0q_u32(M[3], M[0]); \
	T[2] = STATE[0]; \
	T[0] = vaddq_u32(M[0], vld1q_u32(k + i)); \
	PROCESS_SHA_STATE1 \
		M[3] = vsha256su1q_u32(M[3], M[1], M[2]);


#define ROUND4_last0(i) \
	T[2] = STATE[0]; \
	T[1] = vaddq_u32(M[1], vld1q_u32(k + i)); \
	PROCESS_SHA_STATE0

#define ROUND4_last1(i) \
	T[2] = STATE[0]; \
	T[0] = vaddq_u32(M[2], vld1q_u32(k + i)); \
	PROCESS_SHA_STATE1

#define ROUND4_last2(i) \
	T[2] = STATE[0]; \
	T[1] = vaddq_u32(M[3], vld1q_u32(k + i)); \
	PROCESS_SHA_STATE0

static inline void sha256_memcpy_4(uint32_t *out, const uint32_t *in, size_t l) {
#ifdef XMSS_RISCV_TARGET
	size_t i;
	for(i = 0; i < l / 4; i += 4) {
		out[i] = ((const uint32_t*)in)[i];
		out[i + 1] = ((const uint32_t*)in)[i + 1];
		out[i + 2] = ((const uint32_t*)in)[i + 2];
		out[i + 3] = ((const uint32_t*)in)[i + 3];
	}
#else
	XMSS_MEMCPY(out, in, l);	// not RISC-V
#endif
}

/*
 * Assume that memset here is worse on any architecture:
 * it needs to know alignment and it needs to carry the value to set
 * (which is zero here).
 */
static inline void sha256_zero_4(uint32_t *out, size_t l) {
	size_t i;
	for(i = 0; i < (l / 4U); i += 4U) {
		out[i] = 0;
		out[i + 1U] = 0;
		out[i + 2U] = 0;
		out[i + 3U] = 0;
	}
}

/*
 * hash 64 bytes into the state
 */
static void sha256_transform(SHA256_CTX *ctx, const void *data_in)
{
#if !defined(XMSS_ARM_TARGET)
	WORD a, b, c, d, e, f, g, h, t1, t2, m[64];
	const uint8_t * const data = data_in;
	size_t i, j;

	for (i = 0; i < 16U; i++) {
		j = i * 4U;
		//m[i] = SWAP32(*(const uint64_t*)(data+j));    // slower on RISCV
		m[i] = ((WORD)data[j] << 24) | ((WORD)data[j + 1U] << 16) | ((WORD)data[j + 2U] << 8) | ((WORD)data[j + 3U]);
	}
	for ( ; i < 64UL; ++i) {
		/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		m[i] = SIG1(m[i - 2U]) + m[i - 7U] + SIG0(m[i - 15U]) + m[i - 16U];
	}

	a = ctx->state[0];
	b = ctx->state[1];
	c = ctx->state[2];
	d = ctx->state[3];
	e = ctx->state[4];
	f = ctx->state[5];
	g = ctx->state[6];
	h = ctx->state[7];

	for (i = 0; i < 64U; ++i) {
		/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		t1 = h + SHA_EP1(e) + CH(e,f,g) + k[i] + m[i];
		/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		t2 = SHA_EP0(a) + MAJ(a,b,c);
		h = g;
		g = f;
		f = e;
		/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		e = d + t1;
		d = c;
		c = b;
		b = a;
		/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		a = t1 + t2;
	}

	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[0] += a;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[1] += b;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[2] += c;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[3] += d;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[4] += e;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[5] += f;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[6] += g;
	/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
	ctx->state[7] += h;
#else
	const uint8_t * const data = data_in;
	uint32x4_t STATE[2], STATE_OLD[2];
	uint32x4_t M[4];	// 64 uint32_t
	uint32x4_t T[3];

	STATE_OLD[0] = STATE[0] = vld1q_u32(&ctx->state[0]);	// load 4x4 at a time
	STATE_OLD[1] = STATE[1] = vld1q_u32(&ctx->state[4]);	// same

	/* load 64-byte block, reversing to little endian in 4-byte fields */
	M[0] = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(vld1q_u32((const uint32_t *)(data +  0)))));
	M[1] = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(vld1q_u32((const uint32_t *)(data + 16)))));
	M[2] = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(vld1q_u32((const uint32_t *)(data + 32)))));
	M[3] = vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(vld1q_u32((const uint32_t *)(data + 48)))));

	T[0] = vaddq_u32(M[0], vld1q_u32(k + 0x00));

	ROUND4_0(4)			// 0,1,2,3
	ROUND4_1(8)			// 4,5,6,7
	ROUND4_2(12)
	ROUND4_3(16)

	ROUND4_0(20)
	ROUND4_1(24)
	ROUND4_2(28)
	ROUND4_3(32)

	ROUND4_0(36)
	ROUND4_1(40)
	ROUND4_2(44)
	ROUND4_3(48)

	ROUND4_last0(52)
	ROUND4_last1(56)
	ROUND4_last2(60)

	T[2] = STATE[0];
	STATE[0] = vsha256hq_u32(STATE[0], STATE[1], T[1]);
	STATE[1] = vsha256h2q_u32(STATE[1], T[2], T[1]);

	STATE[0] = vaddq_u32(STATE[0], STATE_OLD[0]);
	STATE[1] = vaddq_u32(STATE[1], STATE_OLD[1]);

	/* Save state */
	vst1q_u32(&ctx->state[0], STATE[0]);	// store 4x4 at a time
	vst1q_u32(&ctx->state[4], STATE[1]);	// same
#endif
}

static void sha256_transform_le(const SHA256_CTX *ctx, const uint32_t *data_in_le, uint32_t out[8])
{
#if !defined(XMSS_ARM_TARGET)
	WORD a, b, c, d, e, f, g, h, t1, t2, m[64];
	size_t i;

	for(i = 0; i < 16U; i += 4U) {
		m[i] = data_in_le[i];
		m[i + 1U] = data_in_le[i + 1U];
		m[i + 2U] = data_in_le[i + 2U];
		m[i + 3U] = data_in_le[i + 3U];
	}
	for ( ; i < 64U; i++) {
		/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		m[i] = SIG1(m[i - 2U]) + m[i - 7U] + SIG0(m[i - 15U]) + m[i - 16U];
	}

	a = ctx->state[0];
	b = ctx->state[1];
	c = ctx->state[2];
	d = ctx->state[3];
	e = ctx->state[4];
	f = ctx->state[5];
	g = ctx->state[6];
	h = ctx->state[7];

	for (i = 0; i < 64U; ++i) {
		/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		t1 = h + SHA_EP1(e) + CH(e,f,g) + k[i] + m[i];
		/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		t2 = SHA_EP0(a) + MAJ(a,b,c);
		h = g;
		g = f;
		f = e;
		/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		e = d + t1;
		d = c;
		c = b;
		b = a;
		/* coverity[cert_int30_c_violation]; Deviation-MOD32_DEVIATION_ID */
		a = t1 + t2;
	}

	out[0] = ctx->state[0] + a;
	out[1] = ctx->state[1] + b;
	out[2] = ctx->state[2] + c;
	out[3] = ctx->state[3] + d;
	out[4] = ctx->state[4] + e;
	out[5] = ctx->state[5] + f;
	out[6] = ctx->state[6] + g;
	out[7] = ctx->state[7] + h;
#else
	const uint8_t * const data = (const uint8_t *)data_in_le;
	uint32x4_t STATE[2], STATE_OLD[2];
	uint32x4_t M[4];	// 64 uint32_t
	uint32x4_t T[3];

	STATE_OLD[0] = STATE[0] = vld1q_u32(&ctx->state[0]);	// load 4x4 at a time
	STATE_OLD[1] = STATE[1] = vld1q_u32(&ctx->state[4]);	// same

	/* load 64-byte block, as 4 4x4 vectors */
	M[0] = vld1q_u32((const uint32_t *)(data +  0));
	M[1] = vld1q_u32((const uint32_t *)(data + 16));
	M[2] = vld1q_u32((const uint32_t *)(data + 32));
	M[3] = vld1q_u32((const uint32_t *)(data + 48));

	T[0] = vaddq_u32(M[0], vld1q_u32(k + 0x00));

	ROUND4_0(4)			// 0,1,2,3
	ROUND4_1(8)			// 4,5,6,7
	ROUND4_2(12)
	ROUND4_3(16)

	ROUND4_0(20)
	ROUND4_1(24)
	ROUND4_2(28)
	ROUND4_3(32)

	ROUND4_0(36)
	ROUND4_1(40)
	ROUND4_2(44)
	ROUND4_3(48)

	ROUND4_last0(52)
	ROUND4_last1(56)
	ROUND4_last2(60)

	T[2] = STATE[0];
	STATE[0] = vsha256hq_u32(STATE[0], STATE[1], T[1]);
	STATE[1] = vsha256h2q_u32(STATE[1], T[2], T[1]);

	/* Save state */
	vst1q_u32(&out[0], vaddq_u32(STATE[0], STATE_OLD[0]));		// store 4x4 at a time
	vst1q_u32(&out[4], vaddq_u32(STATE[1], STATE_OLD[1]));		// same
#endif
}


void sha256_init(SHA256_CTX *ctx)
{
	ctx->bitlen = 0;
	ctx->state[0] = 0x6a09e667;
	ctx->state[1] = 0xbb67ae85;
	ctx->state[2] = 0x3c6ef372;
	ctx->state[3] = 0xa54ff53a;
	ctx->state[4] = 0x510e527f;
	ctx->state[5] = 0x9b05688c;
	ctx->state[6] = 0x1f83d9ab;
	ctx->state[7] = 0x5be0cd19;
}

/*
 * Hash full blocks, in units of 64 bytes
 */
void sha256_update(SHA256_CTX *ctx, const void *data, size_t len)
{
	unsigned i;

	//assert(len % 64 == 0);

	for (i = 0; i < len; i += 64U) {
		ctx->bitlen &= 0xffffffffU;
		sha256_transform(ctx, ((const uint8_t*)data) + i);
		ctx->bitlen += 512U;
	}
}

/*
 * Hash one full block
 *
 * Input is such that 32-bit words are in LE.
 */
void sha256_update1_le(SHA256_CTX *ctx, const void *data_le)
{
	const uint32_t *p = data_le;
	sha256_transform_le(ctx, p, ctx->state);
	ctx->bitlen = (ctx->bitlen & 0xffffffffU) + 512U;
}


/*
 * Copy state
 */
void sha256_copy(const SHA256_CTX *ctx_in, SHA256_CTX *ctx_out)  {
	*ctx_out = *ctx_in;
}

/*
 * Finalize the hash.
 * Required: input_size < 64. Call sha256_update() first otherwise.
 */
void sha256_finalize(SHA256_CTX *ctx, const void *input, size_t input_size, uint32_t out[8])
{
	uint8_t data[64];
	void *p = data;
	uint32_t t;

	input_size &= 0xffffffffU;
	ctx->bitlen &= 0xffffffffU;

	ctx->bitlen += input_size * 8U;
	XMSS_MEMCPY(p, input, input_size);	// input_size can be arbitrary

	data[input_size] = 0x80;

	if( input_size < 56U) {		// can we fit an 8-byte counter?
		// Pad whatever data is left in the buffer.
		XMSS_MEMSET(data + input_size + 1U, 0, 56U - input_size - 1U);
	} else {	// Go into another block. We are here only for message hashing
		if((input_size + 1U) < 64U ) {
			XMSS_MEMSET(data + input_size + 1U, 0, 64U - input_size - 1U);
		}
		sha256_transform(ctx, data);
		XMSS_MEMSET(data, 0, 56);
	}


	/* Need to avoid a type colwersion error */
	t = ctx->bitlen_low;

	*(uint32_t*)(void*)(data + 56) = 0;
	*(uint32_t*)(void*)(data + 60) = SWAP32(t);

	sha256_transform(ctx, data);

	out[0] = SWAP32(ctx->state[0]);
	out[1] = SWAP32(ctx->state[1]);
	out[2] = SWAP32(ctx->state[2]);
	out[3] = SWAP32(ctx->state[3]);
	out[4] = SWAP32(ctx->state[4]);
	out[5] = SWAP32(ctx->state[5]);
	out[6] = SWAP32(ctx->state[6]);
	out[7] = SWAP32(ctx->state[7]);
}

/*
 * Finalize the hash.
 *
 * The input and output is such that 32-bit words are in LE.
 *
 */
void sha256_finalize_le(SHA256_CTX *ctx, const void *input_le, size_t input_size, uint32_t out[8])
{
	uint32_t data[64];

	// assert(input_size < 64);    // needed to call sha256_update() first otherwise
	// assert(input_size % 4 == 0);

	input_size  &= 0xffffffffU;
	ctx->bitlen &= 0xffffffffU;

	ctx->bitlen += input_size * 8UL;
	sha256_memcpy_4(data, (const uint32_t*)input_le, input_size);

	data[input_size / 4U] = SWAP32(0x80);
	input_size += 4U;

	if( input_size <= 56U) {		// can we fit an 8-byte counter?
		// Pad whatever data is left in the buffer.
		sha256_zero_4(data + (input_size / 4U), 56U - input_size);
	} else {	// Go into another block. We are here only for message hashing
		// not here for our use yet
		sha256_zero_4(data + (input_size / 4U), 64U - input_size);
		sha256_transform_le(ctx, data, ctx->state);
		sha256_zero_4(data, 56);
	}

	data[56 / 4] = 0;
	/* Need to avoid a type colwersion error */
	data[60 / 4] = ctx->bitlen_low;

	sha256_transform_le(ctx, data, (uint32_t*)out);
}
