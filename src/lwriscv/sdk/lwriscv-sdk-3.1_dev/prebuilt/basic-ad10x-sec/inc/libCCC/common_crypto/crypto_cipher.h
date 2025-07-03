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

/**
 * @file   crypto_cipher.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2016
 *
 * @brief  Handlers and definitions for symmetric cipher operations
 *
 * Shared between AES cipher operations this defines the CCC API layer
 * init/setkey/update/dofinal handler interface functions for
 * symmetric ciphers.
 *
 * The internal object type aes_istate is exposed by this for sharing
 * it between the code supporting legacy AES modes and newly added AES
 * modes that are implemented in separate files (e.g. AES-CTS and
 * AES-GCM) but partly share the generalized common legacy code.
 */
#ifndef INCLUDED_CRYPTO_CIPHER_H
#define INCLUDED_CRYPTO_CIPHER_H

#include <crypto_ops.h>

/**
 * @ingroup class_layer_api
 * @defgroup class_layer_cipher_handlers Class handlers for symmetric ciphers
 */
/*@{*/
/**
 * @brief cipher class handler for TE_OP_INIT primitive
 *
 * After initialization the ctx can be used for cipher class
 * primitive operations.
 *
 * Creates run_state context for this operation valid
 * until TE_OP_RESET is called for the context either implicitly
 * or due to an error.
 *
 * Init fields supported:
 * - engine_hint: used engine [in/out]
 * - aes : field, @ref cipher_init_args
 *
 * This handler is called by:
 * - TE_OP_COMBINED_OPERATION
 * - TE_OP_INIT
 *
 * @param ctx Crypto context from client (set up for symmetric cipher init)
 * @param args Init operation values for symmetric ciphers
 *
 * @return NO_ERROR on success error code otherwise. ctx initialized
 * for symmetric cipher operations.
 */
status_t se_crypto_cipher_init(crypto_context_t *ctx, te_args_init_t *args);

/**
 * @brief cipher class handler for TE_OP_UPDATE
 *
 * Calling TE_OP_UPDATE operation is optional.
 *
 * Arbitrary number of update operations supported to feed more data chunks
 * into a previously initialized ctx. Cipher is completed only after a mandatory
 * TE_OP_DOFINAL completes successfully.
 *
 * The TE_OP_UPDATE triggers a call to the @ref process_layer_api
 *
 * Depending on the cipher algorithm and mode, process layer may
 * buffer the input data (into multiples of AES block sizes) if data
 * size does not comply with HW requirements.
 *
 * args fields used:
 * - src : chunk data address
 * - src_size : chunk data size in bytes
 * - dst : output buffer address
 * - dst_size : size of output buffer
 *
 * @param ctx Initialized context
 * @param run_state runtime state defined by TE_OP_INIT
 * @param args address and size of data chunk to process, if
 * HW processes data the engine writes it to args->dst.
 *
 * @return NO_ERROR on success error code otherwise.
 */
status_t se_crypto_cipher_update(const crypto_context_t *ctx, struct run_state_s *run_state,
				 te_args_data_t *args);

/**
 * @brief cipher class handler for TE_OP_DOFINAL
 *
 * Calling TE_OP_DOFINAL operation is optional.
 *
 * Completes the cipher operation after processing final bytes of input
 * with the engine and write result to args->dst.
 *
 * The TE_OP_DOFINAL triggers a call to the @ref process_layer_api
 *
 * args fields used:
 * - src : chunk data address
 * - src_size : chunk data size in bytes
 * - dst : output buffer address
 * - dst_size : size of output buffer
 *
 * @param ctx Initialized context
 * @param run_state runtime state defined by TE_OP_INIT
 * @param args address and size of data chunk to process, if
 * @param init_args Init object previously passed to TE_OP_INIT.
 * Final IV value returned in init_args->aes.iv
 *
 * @return NO_ERROR on success error code otherwise.
 */
status_t se_crypto_cipher_dofinal(const crypto_context_t *ctx, struct run_state_s *run_state,
				  te_args_data_t *args, te_args_init_t *init_args);

/**
 * @brief symmetric cipher class handler for TE_OP_RESET
 *
 * Reset and wipe objects used and release resources reserved
 * for this crypto operation.
 *
 * TE_OP_RESET is called automatically on error and also
 * via the ctx reset.
 *
 * This CLASS handler is called by:
 * - TE_OP_COMBINED_OPERATION
 * - TE_OP_RESET
 * - error condition (automatic, in default configurations)
 * - crypto_kernel_context_reset()
 * - crypto_context_reset()
 *
 * @param ctx Initialized context
 *
 * @return NO_ERROR on success error code otherwise.
 */
status_t se_crypto_cipher_reset(crypto_context_t *ctx);

/**
 * @brief symmetric cipher class handler for TE_OP_SET_KEY
 *
 * Define which key is used for the symmetric cipher operation context.
 *
 * For AES engines one of the SE HW keyslots (AES keyslots 0..15)
 * must be used.
 *
 * This CLASS handler is called by:
 * - TE_OP_COMBINED_OPERATION
 * - TE_OP_SET_KEY
 *
 * @param ctx Initialized context
 * @param key CCC core copy of processed key definitions, based on key_args
 * @param key_args subsystem provided immutable key definitions
 *
 * @return NO_ERROR on success error code otherwise.
 */
status_t se_crypto_cipher_set_key(crypto_context_t *ctx, te_crypto_key_t *key,
				  const te_args_key_data_t *key_args);

#if HAVE_SE_ASYNC_AES
status_t se_crypto_cipher_async_check_state(const crypto_context_t *ctx,
					    const struct run_state_s *run_state,
					    te_args_init_t *init_args);
status_t se_crypto_cipher_async_update(const crypto_context_t *ctx, struct run_state_s *run_state,
				       te_args_data_t *args, bool start_operation);
status_t se_crypto_cipher_async_dofinal(const crypto_context_t *ctx, struct run_state_s *run_state,
					te_args_data_t *args, te_args_init_t *init_args,
					bool start_operation);
#endif
/*@}*/

#endif /* INCLUDED_CRYPTO_CIPHER_H */
