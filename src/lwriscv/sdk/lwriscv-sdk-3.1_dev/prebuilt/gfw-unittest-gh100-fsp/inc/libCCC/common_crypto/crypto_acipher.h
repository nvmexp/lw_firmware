/*
 * Copyright (c) 2016-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */
#ifndef INCLUDED_CRYPTO_ACIPHER_H
#define INCLUDED_CRYPTO_ACIPHER_H

#include <crypto_ops.h>

/**
 * @ingroup class_layer_api
 * @defgroup class_layer_acipher_handlers Class handlers for asymmetric ciphers
 */
/*@{*/
status_t se_crypto_acipher_init(crypto_context_t *ctx, te_args_init_t *args);

status_t se_crypto_acipher_dofinal(const crypto_context_t *ctx, struct run_state_s *run_state,
				   te_args_data_t *args, te_args_init_t *init_args);

status_t se_crypto_acipher_reset(crypto_context_t *ctx);

status_t se_crypto_acipher_set_key(crypto_context_t *ctx, te_crypto_key_t *key,
				   const te_args_key_data_t *key_args);
/*@}*/
#endif /* INCLUDED_CRYPTO_ACIPHER_H */
