/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All Rights Reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

#ifndef INCLUDED_CCC_XMSS_DEFS_H
#define INCLUDED_CCC_XMSS_DEFS_H

#ifndef XMSS_USE_INLINE_MAPPING_FUNCTIONS
#define XMSS_USE_INLINE_MAPPING_FUNCTIONS 1
#endif

/* Use inline misra/cert-c clean mapping functions into CCC support functions
 * until a faster alternative like builtin functions exists and
 * is approved by safety board.
 *
 * Since XMSS code uses size_t and signed integers for some reason,
 * the condtional statements are used due to conversion rules in compilers
 * and cert-c checkers.
 */
#if XMSS_USE_INLINE_MAPPING_FUNCTIONS

static inline void xmss_memset(uint8_t *s, int32_t c, size_t n_a)
{
	uint64_t n = (uint64_t)n_a;
	uint32_t val = 0U;

	if (n > 0xFFFFFFFFUL) {
		n &= 0xFFFFFFFFUL;
	}

	if (c == -1) {
		val = 0xFFU;
	} else {
		val = (uint32_t)c;
	}

	se_util_mem_set(s, BYTE_VALUE(val), (uint32_t)n);
}

/* This returns void since xmss-verify does not use the returned pointer of
 * the posix memcpy()
 */
static inline void xmss_memcpy(uint8_t *dest, const uint8_t *src, size_t n_a)
{
	uint64_t n = (uint64_t)n_a;

	if (n > 0xFFFFFFFFUL) {
		n &= 0xFFFFFFFFUL;
	}

	se_util_mem_move(dest, src, (uint32_t)n);
	return;
}

static inline int32_t xmss_memcmp(const uint8_t *s1, const uint8_t *s2, size_t n_a)
{
	uint64_t n = (uint64_t)n_a;
	int32_t rv = 1;

	if (n > 0xFFFFFFFFUL) {
		n &= 0xFFFFFFFFUL;
	}

	if (0U == se_util_mem_cmp(s1,s2, (uint32_t)n)) {
		rv = 0;
	}
	return rv;
}

#define XMSS_MEMSET(d,v,b) xmss_memset((uint8_t *)(d), v, b)
#define XMSS_MEMCPY(d,s,b) xmss_memcpy((uint8_t *)(d), (const uint8_t *)(s), b)
#define XMSS_MEMCMP(s1,s2,b) xmss_memcmp((uint8_t *)(s1), (const uint8_t *)(s2), b)
#define SWAP32(x) se_util_swap_word_endian(x)

#endif /* XMSS_USE_INLINE_MAPPING_FUNCTIONS */

#endif /* INCLUDED_CCC_XMSS_DEFS_H */
