/*
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */
#ifndef INCLUDED_CRYPTO_WHIRLPOOL_H
#define INCLUDED_CRYPTO_WHIRLPOOL_H

#if HAVE_SW_WHIRLPOOL

#include <whirlpool.h>

struct soft_whirlpool_context;

typedef status_t (*process_whirlpool_t)(struct se_data_params *input_params,
					struct soft_whirlpool_context *econtext);

typedef struct se_engine_whirlpool_context {
	const struct engine_s *engine;
	struct whirlpool_ctx   wp_ctx;
	bool	 is_first;
	bool	 is_last;
	uint32_t hash_size;	/* hash algorithm result size */
	uint32_t block_size;	/* fixed value per hash_algorithm (algo internal block size) */
	te_crypto_domain_t domain;
	uint32_t perf_usec;
	uint8_t  tmphash[ WHIRLPOOL_BLOCK_SIZE ]; /* hmac-whirlpool intermediate value saved here */
} se_engine_whirlpool_context_t;

/* sw implementation of whirlpool digest */
struct soft_whirlpool_context {
	process_whirlpool_t  process;

	uint32_t byte_count;	/* bytes passed since init */

	se_engine_whirlpool_context_t ec; // Sort of "sw engine" for this one
};

status_t whirlpool_engine_context_static_init(te_crypto_domain_t domain,
					      engine_id_t engine_id,
					      te_crypto_algo_t algo,
					      struct se_engine_whirlpool_context *wp_econtext);

status_t whirlpool_context_static_init(te_crypto_domain_t domain, engine_id_t engine_id,
				       te_crypto_algo_t algo,
				       struct soft_whirlpool_context *wp_context);

status_t soft_whirlpool_process_input(
	struct se_data_params *input_params,
	struct soft_whirlpool_context *context);

#endif /* HAVE_SW_WHIRLPOOL */

#endif /* INCLUDED_CRYPTO_WHIRLPOOL_H */
