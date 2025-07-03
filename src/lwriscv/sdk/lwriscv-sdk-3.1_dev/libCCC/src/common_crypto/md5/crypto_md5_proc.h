/*
 * Copyright (c) 2017-2019, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */
#ifndef INCLUDED_CRYPTO_MD5_PROC_H
#define INCLUDED_CRYPTO_MD5_PROC_H

#include <crypto_ops.h>

#if HAVE_MD5

#include <md5/md5.h>

typedef struct se_engine_md5_context {
	const struct engine_s *engine;
	MD5_CTX	 md5_ctx;
	bool	 is_first;
	bool	 is_last;
	uint32_t hash_size;	/* hash algorithm result size */
	uint32_t block_size;	/* fixed value per hash_algorithm (algo internal block size) */
	te_crypto_domain_t domain;
	uint32_t perf_usec;
	uint8_t  tmphash[ 16 ]; /* hmac-md5 intermediate value saved here */
} se_engine_md5_context_t;

/* sw implementation of md5 digest */
struct soft_md5_context {
	uint32_t byte_count;	/* bytes passed since init */

	se_engine_md5_context_t ec;	// Sort of "sw engine" for this one ;-)
};

status_t md5_engine_context_static_init(te_crypto_domain_t domain,
					engine_id_t engine_id,
					te_crypto_algo_t algo,
					struct se_engine_md5_context *md5_econtext);

status_t md5_context_static_init(te_crypto_domain_t domain, engine_id_t engine_id,
				 te_crypto_algo_t algo,
				 struct soft_md5_context *md5_context);

#endif /* HAVE_MD5 */

#endif /* INCLUDED_CRYPTO_MD5_PROC_H */
