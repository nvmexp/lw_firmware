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
 * @file   crypto_md_proc.h
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2017
 *
 * SHA context initializer calls
 */
#ifndef INCLUDED_CRYPTO_MD_PROC_H
#define INCLUDED_CRYPTO_MD_PROC_H

#include <crypto_ops.h>

/**
 * @brief Initialize the sha_engine_context_t object.
 *
 * @param domain kernel/user space data handling
 * @param engine_id Hint for the engine to use
 * @param algo SHA algorithms
 * @param sha_econtext SHA engine context to initialize
 * @param CMEM context memory reference or NULL when not used.
 *
 * @return NO_ERROR on success error code otherwise.
 */
status_t sha_engine_context_static_init_cm(te_crypto_domain_t domain,
					   engine_id_t engine_id,
					   te_crypto_algo_t algo,
					   struct se_engine_sha_context *sha_econtext,
					   struct context_mem_s *cmem);

/* backwards compatibility, NULL cmem
 *
 * Systems upgrading to CMEM must call the above function
 * sha_engine_context_static_init_cm() instead of this.
 *
 * This just calls the above function with NULL cmem argument.
 */
status_t sha_engine_context_static_init(te_crypto_domain_t domain,
					engine_id_t engine_id,
					te_crypto_algo_t algo,
					struct se_engine_sha_context *sha_econtext);

#if CCC_USE_DMA_MEM_BUFFER_ALLOC
/**
 * @brief Release DMA buffer object in struct se_sha_context
 * field "buf" using RELEASE_ALIGNED_DMA_MEMORY
 *
 * sha_context->buf is zeroed and pointer set NULL.
 *
 * @param sha_context SHA context to release the DMA buffer from.
 */
void sha_context_dma_buf_release(struct se_sha_context *sha_context);
#endif /* CCC_USE_DMA_MEM_BUFFER_ALLOC */

/**
 * @brief Initialize the sha_context_t object.
 *
 * @param domain kernel/user space data handling
 * @param engine_id Hint for the engine to use
 * @param algo SHA algorithms
 * @param sha_context SHA context to initialize
 * @param CMEM context memory reference or NULL when not used.
 *
 * @return NO_ERROR on success error code otherwise.
 */
status_t sha_context_static_init(te_crypto_domain_t domain, engine_id_t engine_id,
				 te_crypto_algo_t algo,
				 struct se_sha_context *sha_context,
				 struct context_mem_s *cmem);

#endif /* INCLUDED_CRYPTO_MD_PROC_H */
