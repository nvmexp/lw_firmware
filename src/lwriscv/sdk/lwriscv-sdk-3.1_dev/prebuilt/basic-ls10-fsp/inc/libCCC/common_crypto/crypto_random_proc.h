/*
 * Copyright (c) 2017-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_CRYPTO_RANDOM_PROC_H
#define INCLUDED_CRYPTO_RANDOM_PROC_H

#include <crypto_ops.h>

status_t rng_engine_context_static_init(te_crypto_domain_t domain,
					engine_id_t engine_id,
					te_crypto_algo_t algo,
					struct se_engine_rng_context *rng_econtext);

status_t rng_context_static_init(te_crypto_domain_t domain,
				 engine_id_t engine_id,
				 te_crypto_algo_t algo,
				 struct se_rng_context *rng_context);

#endif /* INCLUDED_CRYPTO_RANDOM_PROC_H */
