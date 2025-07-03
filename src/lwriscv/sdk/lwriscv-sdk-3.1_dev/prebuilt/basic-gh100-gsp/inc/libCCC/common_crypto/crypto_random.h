/*
 * Copyright (c) 2017-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

#ifndef INCLUDED_CRYPTO_RANDOM_H
#define INCLUDED_CRYPTO_RANDOM_H

#include <crypto_ops.h>

/**
 * @ingroup class_layer_api
 * @defgroup class_layer_rng_handlers Class handlers for random number generators
 */
/*@{*/
status_t se_crypto_random_init(crypto_context_t *ctx, te_args_init_t *args);
status_t se_crypto_random_dofinal(const crypto_context_t *ctx, struct run_state_s *run_state,
				  te_args_data_t *args, te_args_init_t *init_args);
status_t se_crypto_random_reset(crypto_context_t *ctx);
/*@}*/

#endif /* INCLUDED_CRYPTO_RANDOM_H */
