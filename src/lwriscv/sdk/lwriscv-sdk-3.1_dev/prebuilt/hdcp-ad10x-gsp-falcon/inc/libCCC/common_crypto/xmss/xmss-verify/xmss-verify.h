/*
 * Copyright (c) 2020, NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

#ifndef LW_XMSS_H
#define LW_XMSS_H 1

typedef enum xmss_error {
	XMSS_ERR_OK = 0,
	XMSS_ERR_BUFFER_TOO_SMALL = 1,
	XMSS_ERR_UNALIGNED_BUFFER = 2,
	XMSS_ERR_BAD_PARAMS = 3,
	XMSS_ERR_BAD_SIG = 4
} xmss_error_t;

#define XMSS_SCRATCH_BUFFER_SIZE 2856	// in bytes; access with 4- and 8-byte integers

xmss_error_t xmss_verify_low_stack(const uint8_t root_seed[64], const uint8_t *sig, size_t sigl, uint64_t scratch[XMSS_SCRATCH_BUFFER_SIZE / sizeof(uint64_t)], size_t scratchl, const uint8_t *M, size_t Ml);

/* "large stack" version of xmss_verify_low_stack */
static inline xmss_error_t xmss_verify(const uint8_t root_seed[64], const uint8_t *sig, size_t sigl, const uint8_t *M, size_t Ml) {
	uint64_t stack[XMSS_SCRATCH_BUFFER_SIZE / sizeof(uint64_t)];
	return xmss_verify_low_stack(root_seed, sig, sigl, stack, sizeof(stack), M, Ml);
}

#endif // LW_XMSS_H

