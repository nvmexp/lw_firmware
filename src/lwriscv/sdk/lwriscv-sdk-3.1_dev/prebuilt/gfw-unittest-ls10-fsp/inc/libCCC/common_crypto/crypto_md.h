/*
 * Copyright (c) 2017-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */
/**
 * @file   crypto_md.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2017
 *
 * Message digest CLASS API
 */
#ifndef INCLUDED_CRYPTO_MD_H
#define INCLUDED_CRYPTO_MD_H

#include <crypto_ops.h>

/* Some external subsystems
 * include crypto_md.h directly.
 *
 * Include this to avoid compilation issues
 * in such systems.
 */
#include <crypto_md_proc.h>

/**
 * @ingroup class_layer_api
 * @defgroup class_layer_md_handlers Class handlers for message digests
 */
/*@{*/
/**
 * @brief message digest class handler for TE_OP_INIT primitive
 *
 * After initialization the ctx can be used for other message digest
 * primitive operations. Digests do not support TE_OP_SET_KEY
 * operation, digests do not use keys.
 *
 * Creates run_state context for this operation valid
 * until TE_OP_RESET is called for the context either implicitly
 * or due to an error.
 *
 * Init fields supported:
 * - engine_hint: used engine [in/out]
 *
 * This handler is called by:
 * - TE_OP_COMBINED_OPERATION
 * - TE_OP_INIT
 *
 * @param ctx Crypto context from client (set up for message digest)
 * @param args Init operation values for message digests
 *
 * @return NO_ERROR on success error code otherwise. ctx initialized
 * for message digest operations.
 */
status_t se_crypto_md_init(crypto_context_t *ctx, te_args_init_t *args);

/**
 * @brief message digest class handler for TE_OP_UPDATE
 *
 * Calling TE_OP_UPDATE operation is optional.
 *
 * Arbitrary number of update operations supported to feed more data chunks
 * into a previously initialized ctx. Digest value available only after
 * TE_OP_DOFINAL completes successfully.
 *
 * The TE_OP_UPDATE triggers a call to the @ref process_layer_api
 *
 * args fields used:
 * - src : chunk data address
 * - src_size : chunk data size in bytes
 *
 * @param ctx Initialized context
 * @param run_state runtime state defined by TE_OP_INIT
 * @param args address and size of data chunk to process
 *
 * @return NO_ERROR on success error code otherwise.
 */
status_t se_crypto_md_update(const crypto_context_t *ctx, struct run_state_s *run_state,
			     te_args_data_t *args);

/**
 * @brief message digest class handler for TE_OP_DOFINAL
 *
 * Complete the initialized message digest operation.
 *
 * Calling TE_OP_DOFINAL operation is mandatory for message digest
 * result callwlation.
 *
 * The <src,src_size> may be <NULL, 0> in TE_OP_DOFINAL.
 * If no data has been provided in previous TE_OP_UPDATE operations,
 * callwlates a message digest from null input.
 *
 * Depending on the digest algorithm, process layer may buffer the
 * input data (into multiples of digest internal block size bytes) if
 * data size does not comply with HW requirements.
 *
 * args fields used:
 * - src : chunk data address
 * - src_size : chunk data size in bytes
 * - dst : address of buffer to write the resulting digest value
 * - dst_size : dst buffer size (must be at least the digest length)
 *
 * dst_size field will be set to the callwlated digest byte length.
 *
 * This CLASS handler is called by the TE_OP_COMBINED_OPERATION
 *
 * The TE_OP_DOFINAL triggers a call to the @ref process_layer_api
 *
 * @param ctx Initialized context
 * @param run_state runtime state defined by TE_OP_INIT
 * @param args address and size of last data chunk to process
 * @param init_args Init object previously passed to TE_OP_INIT (unused for message digests)
 *
 * @return NO_ERROR on success error code otherwise.
 */
status_t se_crypto_md_dofinal(const crypto_context_t *ctx, struct run_state_s *run_state,
			      te_args_data_t *args, te_args_init_t *init_args);

/**
 * @brief message digest class handler for TE_OP_RESET
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
status_t se_crypto_md_reset(crypto_context_t *ctx);

#if HAVE_SE_ASYNC_SHA
status_t se_crypto_md_async_check_state(const crypto_context_t *ctx,
					const struct run_state_s *run_state,
					te_args_init_t *init_args);
status_t se_crypto_md_async_update(const crypto_context_t *ctx, struct run_state_s *run_state,
				   te_args_data_t *args, bool start_operation);
status_t se_crypto_md_async_dofinal(const crypto_context_t *ctx, struct run_state_s *run_state,
				    te_args_data_t *args, te_args_init_t *init_args,
				    bool start_operation);
#endif
/*@}*/
#endif /* INCLUDED_CRYPTO_MD_H */
