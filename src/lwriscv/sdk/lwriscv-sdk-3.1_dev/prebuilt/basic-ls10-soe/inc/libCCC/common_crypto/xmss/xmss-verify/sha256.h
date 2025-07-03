/*
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

#ifndef SHA256_H
#define SHA256_H

#ifndef XMSS_NO_SYSTEM_INCLUDES
#include <stddef.h>
#endif

typedef  struct  {
	/*
	 * On bitlen:
	 *
	 * While we don't exceed 2^32 bit (2^29 byte) length for the input buffer,
	 * size_t is more efficient at least on RISC-V. This particular
	 * structure is needed to make Coverity happy.
	 */
	union {
		size_t bitlen;
		uint32_t bitlen_low;
	};
	uint32_t state[8];
} SHA256_CTX;

void sha256_init(SHA256_CTX *ctx);

void sha256_update(SHA256_CTX *ctx, const void *data, size_t len);
void sha256_finalize(SHA256_CTX *ctx, const void *input, size_t input_size, uint32_t out[8]);

void sha256_update1_le(SHA256_CTX *ctx, const void *data_le);
void sha256_finalize_le(SHA256_CTX *ctx, const void *input_le, size_t input_size, uint32_t out[8]);

void sha256_copy(const SHA256_CTX *ctx_in, SHA256_CTX *ctx_out);

#endif /* SHA256_H */
