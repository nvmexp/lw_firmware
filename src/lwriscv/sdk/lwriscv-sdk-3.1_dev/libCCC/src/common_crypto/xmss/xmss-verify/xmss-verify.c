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
#include <stdint.h>		// for types
#include <string.h>
#endif

#include "sha256.h"
#include "xmss-verify.h"
#include "xmss-intrinsics.h"

typedef uint32_t b32[8];

#define XMSS_PARAM_W 16U	// 2^4: max Winternitz value
#define XMSS_PARAM_N 32U
#define XMSS_PARAM_LEN 67U
#define XMSS_PARAM_H 20U

// type XMSS_ADDR_TYPE  (not enum to make Coverity happy)
#define OTS 0U
#define L_TREE 1U
#define MERKLE_TREE 2U

/* ADDR has the following fields:
 *
 * .type
 *      any of {OTS, L_TREE, MERKLE_TREE}
 * .contextIdx
 *      depends on .type, but corresponds to the sequential index of this ADDR in a larger tree
 * .level
 *      the level at which this ADDR appears
 * .levelIdx
 *      the index of the element at this level
 * .keyAndMask
 *      has values {0,1,2} or fewer.
 *
 * Output is b32 that serializes the ADDR that will be used in hashing
 */
typedef union {
	struct {
		uint32_t zeroes[3];	// set to 0
		uint32_t type;	// XMSS_ADDR_TYPE
		uint32_t contextIdx;
		uint32_t level;
		uint32_t levelIdx;
		uint32_t keyAndMask;
	} f;
	uint32_t all[8];
} xmss_address;	// must be sizeof(xmss_address)==32

// a = 0
static inline void zero_(b32 a) {
#ifdef XMSS_RISCV_TARGET
	// faster than memset
	a[0] = a[1] = a[2] = a[3] =
		a[4] = a[5] = a[6] = a[7] = 0;
#else
	XMSS_MEMSET(a, 0, XMSS_SIZEOF(b32));	// not RISC-V
#endif
}
#define zero(a) zero_(&(a)[0])

static inline void zero_addr(xmss_address *a) {
#ifdef XMSS_RISCV_TARGET
	// faster than memset
	a->all[0] = a->all[1] = a->all[2] = a->all[3] =
		a->all[4] = a->all[5] = a->all[6] = a->all[7] = 0;
#else
	XMSS_MEMSET(a, 0, XMSS_SIZEOF(*a));	// not RISC-V
#endif
}


// out ^= in
static void addgf2_(const b32 in, b32 out) {
	out[0] ^= in[0];
	out[1] ^= in[1];
	out[2] ^= in[2];
	out[3] ^= in[3];
	out[4] ^= in[4];
	out[5] ^= in[5];
	out[6] ^= in[6];
	out[7] ^= in[7];
}
#define addgf2(in, out) addgf2_(&(in)[0], &(out)[0])


static inline void copy_(const b32 in, b32 out) {
#ifdef XMSS_RISCV_TARGET
	// Faster and there is no built-in
	out[0] = in[0];
	out[1] = in[1];
	out[2] = in[2];
	out[3] = in[3];
	out[4] = in[4];
	out[5] = in[5];
	out[6] = in[6];
	out[7] = in[7];
#else
	XMSS_MEMCPY(out, in, XMSS_SIZEOF(b32));		// not RISC-V
#endif
}
#define copy(a,b) copy_(&(a)[0], &(b)[0])

// viewing a as 32-bit element array, swap elements
static void swap32p_(const void *a, b32 out) {

	const uint32_t *a32 = a;

	out[0] = SWAP32(a32[0]);
	out[1] = SWAP32(a32[1]);
	out[2] = SWAP32(a32[2]);
	out[3] = SWAP32(a32[3]);
	out[4] = SWAP32(a32[4]);
	out[5] = SWAP32(a32[5]);
	out[6] = SWAP32(a32[6]);
	out[7] = SWAP32(a32[7]);
}
#define swap32p(a,out) swap32p_(a, &(out)[0])

typedef struct {
	b32 block[2];	// full sha256 input block
	SHA256_CTX c;
} xmss_block_c;


typedef struct xmss_stack {
	b32 Mprime;		/* 1 */
	b32 pk[67];		/* 68 */

	xmss_address ADDR;	/* 69 */
	b32 root1;			/* 70 */
	b32 root2;			/* 71 */

	b32 top_t[2];		/* 73 */

	b32 hash2_key;		/* 74 */
	b32 hash2_bm[2];		/* 76 */

	union {
		b32 chain_w_key;	/* 77 */
		b32 root_from_auth_path_a;
	};

	union {
		b32 chain_w_bm;
		b32 root_from_auth_path_node[2];/* 79 */
	};

	SHA256_CTX ctx_3_seed;	/* 80*32+8*/
	SHA256_CTX chain_w_c2;	/* 81*32 + 8*2 */
	SHA256_CTX hash2_c;		/* 82*32 + 8*3 */

	union {
		xmss_block_c f_block_c;
		xmss_block_c H_block_c;
		xmss_block_c H_msg_block_c;	/* 88*32 + 8*5 = XMSS_SCRATCH_BUFFER_SIZE/32 */
	};
} xmss_stack;

/*
 * Colwerts the n-byte message (which is hash),
 * to an array of len w-words, encoding n bytes and adding the checksum.
 *
 * We hard-wire w=16 here (two w-words per byte of input, len1) and 3-w
 * checksum (len2), for total len=67
 */
static void xmss_expand_msg(const b32 m, uint8_t out[67]) {
	const uint8_t *mb = (const void*)m;
	const size_t len1 = XMSS_PARAM_N * 2U;
	uint32_t csum = 0;
	const size_t L = XMSS_SIZEOF(b32);	// bytes in m
	size_t i;
	for(i = 0; i < L; i++) {
		const size_t j = L - 1U - i;
		const unsigned v = mb[j];
		out[2U * (XMSS_PARAM_N - 1U - i)] = (uint8_t)((v >> 4) & 0xffU);
		out[(2U * (XMSS_PARAM_N - 1U - i)) + 1U] = (uint8_t)(v & 0xfU);
	}

	// now callwlate the checksum
	for (i = 0; i < len1; i++) {
		// Avoid Coverity cert_int30_c_violation:
		// Unsigned integer operation "csum += 15U - (out[i] & 0xf)" may wrap
		csum &= 0xfffU;
		csum += (XMSS_PARAM_W - 1U - (uint32_t)out[i]);
	}
	// csum < (w-1)*len1 < 2^4*2^6 < 2^10 ; this is 3 w-words

	// now serialize 3 w-words of csum
	out[len1] = (uint8_t)((csum >> 8) & 0xfU);
	out[len1 + 1U] = (uint8_t)((csum >> 4) & 0xfU);
	out[len1 + 2U] = (uint8_t)(csum & 0xfU);
}


// type 0
// hash 0 || key || in
static void f_func(xmss_stack *stack, const b32 key, b32 in_out) {
	b32 *t0 = &stack->f_block_c.block[0];	// t[0,1] is full sha256 input block
	b32 *t1 = &stack->f_block_c.block[1];
	SHA256_CTX *c = &stack->f_block_c.c;

	sha256_init(c);

	zero(*t0);
	// t[0][31] = 0;
	copy(key, *t1);
	sha256_update1_le(c, t0);

	sha256_finalize_le(c, in_out, 32, in_out);
}

// type 1
// hash 1 || key || M0 || M1
static void H(xmss_stack *stack, const b32 key, const void *M, uint32_t out[16]) {
	b32 *t0 = &stack->H_block_c.block[0];	// t[0,1] is full sha256 input block
	b32 *t1 = &stack->H_block_c.block[1];
	SHA256_CTX *c = &stack->H_block_c.c;

	sha256_init(c);

	zero(*t0);
	(*t0)[7] = 1U;
	copy(key, *t1);

	sha256_update1_le(c, t0);			// block 1
	sha256_update1_le(c, M);			// block 2

	sha256_finalize_le(c, M, 0, out);	// pass something (M) to make Coverity happy
}

// type 2
// hash 2 || r || root || pad(idx, 32) || M
// Returns M'
static void H_msg(xmss_stack *stack, const b32 r, const b32 root, uint32_t idx, const uint8_t *M, size_t Ml, b32 out) {
	b32 *t0 = &stack->H_msg_block_c.block[0];	// t[0,1] is full sha256 input block
	b32 *t1 = &stack->H_msg_block_c.block[1];

	SHA256_CTX *c = &stack->H_msg_block_c.c;
	size_t l;

	sha256_init(c);

	zero(*t0);
	(*t0)[7] = 2U;
	copy(r, *t1);
	sha256_update1_le(c, t0);	// block 1: 2 || r

	copy(root, *t0);	// LE root
	zero(*t1);
	(*t1)[7] = idx;
	sha256_update1_le(c, t0);	// block 2: root || idx

	l = (Ml >> 6) << 6;	// Ml - (Ml & 63U); to make Coverity happy

	sha256_update(c, M, l);	// block 3...: most of M

	sha256_finalize(c, M + l, Ml & 63U, out);	// ... the tail
}

static void hash2(xmss_stack *stack, const b32 left, const b32 right, const SHA256_CTX *sha256_ctx_3_seed, xmss_address *ADDR, b32 out) {
	b32 *key = &stack->hash2_key;
	b32 *bm0 = &stack->hash2_bm[0];
	b32 *bm1 = &stack->hash2_bm[1];

	//assert(ADDR->type == L_TREE || ADDR->type == MERKLE_TREE);

	SHA256_CTX *c = &stack->hash2_c;

	sha256_copy(sha256_ctx_3_seed, c);					// 3 || seed
	ADDR->f.keyAndMask = 0;	// for key
	sha256_finalize_le(c, ADDR->all, sizeof(*ADDR), &(*key)[0]);	// = prf(SEED, ADDR, key)

	sha256_copy(sha256_ctx_3_seed, c);					// 3 || seed
	ADDR->f.keyAndMask = 1;	// for mask 1
	sha256_finalize_le(c, ADDR->all, sizeof(*ADDR), &(*bm0)[0]);// = prf(SEED, ADDR, bm)

	sha256_copy(sha256_ctx_3_seed, c);					// 3 || seed
	ADDR->f.keyAndMask = 2;	// for mask 2
	sha256_finalize_le(c, ADDR->all, sizeof(*ADDR), &(*bm1)[0]);// = prf(SEED, ADDR, bm)

	addgf2(left, *bm0);
	addgf2(right, *bm1);

	H(stack, &(*key)[0], bm0, &out[0]);
}

// chain to w-1 (maximum value)
static void chain_w(xmss_stack *stack, xmss_address *ADDR, const uint32_t *m, const SHA256_CTX *sha256_ctx_3_seed, unsigned start, b32 out) {
	// current values
	b32 *key = &stack->chain_w_key;
	b32 *bm = &stack->chain_w_bm;
	SHA256_CTX *c2 =  &stack->chain_w_c2;

	//assert(ADDR->type == OTS);

	swap32p(m,out);

	for (; start < (XMSS_PARAM_W - 1U); start++) {

		ADDR->f.levelIdx = start;	// "hash address", 0 ... w-2

		sha256_copy(sha256_ctx_3_seed, c2);				// 3 || seed
		ADDR->f.keyAndMask = 0;	// for key
		sha256_finalize_le(c2, ADDR->all, sizeof(*ADDR), &(*key)[0]);	// = prf(SEED, ADDR, key)

		sha256_copy(sha256_ctx_3_seed, c2);				// 3 || seed
		ADDR->f.keyAndMask = 1;	// for mask
		sha256_finalize_le(c2, ADDR->all, sizeof(*ADDR), &(*bm)[0]);	// = prf(SEED, ADDR, key)

		addgf2(*bm, out);
		f_func(stack, &(*key)[0], out);	// out = new value
	}
}


/*
 * Compute the OTS public key from the signature and expanded message.
 *
 * This simply completes the path that would be needed
 * to go from the private key to public key.
 *
 * Hash: len*(w-1)*(3n+3n+3n).
 */
static void xmss_pub_from_ots(xmss_stack *stack, xmss_address *ADDR, const SHA256_CTX *ctx_3_seed, const uint32_t ots[67 * 8], const uint8_t m[67], b32 pk_out[67]) {
	unsigned i;

	ADDR->f.type = OTS;

	for (i = 0; i < XMSS_PARAM_LEN; i++) {
		ADDR->f.level = i;
		chain_w(stack, ADDR, ots + (i * 8U), ctx_3_seed, (unsigned)m[i], &pk_out[i][0]);
	}
}

/*
 * Return b32 representing the hash of the public OTS key.
 * This is done via hashing a tree in which OTS public key is the entire set
 * of leafs.
 *
 * Because len is not a power of 2, we need to employ some trick to accomodate this.
 * This is done as follows. When the last element at the current level is in
 * an odd position, it's lifted up to the previous level and used in place of a hash.
 *
 * Returns result as pk[0]
 */
static void ltree_hash_ots(xmss_stack *stack, b32 pk[67], xmss_address *ADDR, const SHA256_CTX *ctx_3_seed) {
	uint32_t l = XMSS_PARAM_LEN;	// number of nodes at the current level
	uint32_t i;

	ADDR->f.type = L_TREE;

	ADDR->f.level = 0;	// leaf level is level 0
	while (l > 1U) {
		for (i = 0; i < (l / 2U); i++) {
			ADDR->f.levelIdx = i;
			hash2(stack, &pk[2U * i][0], &pk[(2U * i) + 1U][0], ctx_3_seed, ADDR, &pk[i][0]);
		}
		// "lift up" an orphan node one level up
		// (to use it instead of the hash that would normally cover this node)
		if ((l & 1U) == 1U) {
			copy(pk[l - 1U], pk[l / 2U]);
			l++;// to make sure that the next one is l = ceil(l/2)
		}

		l /= 2U;
		ADDR->f.level &= 0xffU;	// for Coverity
		ADDR->f.level++;
	}
	// result is in pk[0]
}

static void root_from_auth_path(xmss_stack *stack, xmss_address * const ADDR, const b32 lhash, const uint32_t authPath[XMSS_PARAM_H * 8], const SHA256_CTX *ctx_3_seed, b32 root_out) {
	unsigned k;
	b32 *node = &stack->root_from_auth_path_node[0];
	b32 *a = &stack->root_from_auth_path_a;
	const unsigned idx = ADDR->f.levelIdx;

	ADDR->f.type = MERKLE_TREE;

	copy(lhash, *node);

	for (k = 0; k < XMSS_PARAM_H; k++) {

		ADDR->f.level = k;	// 0-based
		ADDR->f.levelIdx >>= 1;	// /=2
		swap32p(authPath + (k * 8U), &(*a)[0]);
		if(((idx >> k) & 1U) == 0U) {
			hash2(stack, &node[k & 1U][0], &(*a)[0], ctx_3_seed, ADDR, &node[(k & 1U) ^ 1U][0]);
		} else {
			hash2(stack, &(*a)[0], &node[k & 1U][0], ctx_3_seed, ADDR, &node[(k & 1U) ^ 1U][0]);
		}
	}

	copy(node[(k & 1U)], root_out);
}


/*
 * 'sig' structure:
 *  4    idx
 * 32    r
 * 32*67 ots
 * 32*h  path
 * ----------
 * 2692 bytes
 *
 * h must be <= 20
 *
 */
xmss_error_t xmss_verify_low_stack(const uint8_t root_seed[64], const uint8_t *sig, size_t sigl, uint64_t scratch[XMSS_SCRATCH_BUFFER_SIZE / sizeof(uint64_t)], size_t scratchl, const uint8_t *M, size_t Ml) {
	unsigned idx;
	b32 r;
	void const * const p32 = sig + 4U + 32U;
	const uint32_t * const ots =  p32;
	const uint32_t * const path = ots + (XMSS_PARAM_LEN * 8U);
	uint8_t msgHash[XMSS_PARAM_LEN] = {0};	// to make the Coverity happy
	xmss_stack *stack = (xmss_stack *)(void*)scratch;

	b32 *Mprime = &stack->Mprime;
	b32 *pk = stack->pk;

	xmss_address *ADDR = &stack->ADDR;
	b32 *root1 = &stack->root1;
	b32 *root2 = &stack->root2;

	if( scratchl < sizeof(xmss_stack)) {	/* we assume XMSS_SCRATCH_BUFFER_SIZE = sizeof(xmss_stack) */
		return XMSS_ERR_BUFFER_TOO_SMALL;
	}

	if((((size_t)(sig - 0) | (size_t)(scratch - 0)) & 3U) != 0U ) {
		return XMSS_ERR_UNALIGNED_BUFFER;
	}

	/* We consume some stack space. This may need to be moved to .bss */

	if( sigl != (4U + ((1U + 67U + XMSS_PARAM_H) * 32U))) {
		return XMSS_ERR_BAD_PARAMS;
	}

	idx = SWAP32(*(const uint32_t*)(const void *)sig);

	if(idx > (1UL << XMSS_PARAM_H)) {
		return XMSS_ERR_BAD_PARAMS;
	}

	zero_addr(ADDR);

	/*
	 * From this point nothing can fail due to resource constraints or bad parameters.
	 * The only outcome from this point is "signature is valid" or "forged".
	 */

	swap32p(sig + 4, r);		// an LE random from the sig
	swap32p(root_seed, *root1);		// an LE root from the sig

	H_msg(stack, &r[0], &(*root1)[0], idx, M, Ml, &(*Mprime)[0]);	// M' = H_msg(r||root||pad(idx,32)||M)

	xmss_expand_msg(&(*Mprime)[0], msgHash);	// expanded M'; that's what was signed

	SHA256_CTX *ctx_3_seed = &stack->ctx_3_seed;

	{
		b32 *t0 = &stack->top_t[0];	// t[0,1] is full sha256 input block
		b32 *t1 = &stack->top_t[1];

		sha256_init(ctx_3_seed);
		zero(*t0);
		(*t0)[7] = 3U;
		swap32p(root_seed + 32, *t1);	// a seed from the sig
		sha256_update1_le(ctx_3_seed, t0);		// 3 || seed
	}

	ADDR->f.contextIdx = idx;	// OTS or L-Tree address: index of the OTS key in the tree
	ADDR->f.levelIdx = 0;
	xmss_pub_from_ots(stack, &stack->ADDR, ctx_3_seed, ots, msgHash, pk);	// recover PK
	ltree_hash_ots(stack, pk, ADDR, ctx_3_seed);		// return lhash in pk[0]

	ADDR->f.contextIdx = 0;
	ADDR->f.levelIdx = idx;	// Merkle tree address: index of the current node in the tree
	root_from_auth_path(stack, &stack->ADDR, &(*pk)[0], path, ctx_3_seed, &(*root2)[0]);

	return (XMSS_MEMCMP(root1, root2, 32) == 0) ? XMSS_ERR_OK : XMSS_ERR_BAD_SIG;
}
