/*
 * Copyright (c) 2017-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */
#ifndef INCLUDED_CRYPTO_KWUW_H
#define INCLUDED_CRYPTO_KWUW_H

#include <crypto_ops.h>

status_t se_crypto_kwuw_init(crypto_context_t *ctx, te_args_init_t *args);

status_t se_crypto_kwuw_dofinal(const crypto_context_t *ctx, struct run_state_s *run_state,
				  te_args_data_t *args, te_args_init_t *init_args);

status_t se_crypto_kwuw_reset(crypto_context_t *ctx);

status_t se_crypto_kwuw_set_key(crypto_context_t *ctx, te_crypto_key_t *key,
				const te_args_key_data_t *key_args);

#endif /* INCLUDED_CRYPTO_KWUW_H */
