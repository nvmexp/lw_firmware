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

#ifndef INCLUDED_CRYPTO_CIPHER_PROC_H
#define INCLUDED_CRYPTO_CIPHER_PROC_H

#include <crypto_ops.h>

#if HAVE_SE_AES

status_t call_aes_process_handler(struct se_data_params *input,
				  struct se_aes_context *aes_context);

status_t aes_engine_context_static_init_cm(te_crypto_domain_t domain,
					   engine_id_t engine_id, te_crypto_algo_t algo,
					   struct se_engine_aes_context *aes_econtext,
					   struct context_mem_s *cmem);

/* backwards compatibility, NULL cmem
 *
 * Systems upgrading to CMEM must call the above function
 * aes_engine_context_static_init_cm() instead of this.
 */
static inline status_t aes_engine_context_static_init(te_crypto_domain_t domain,
						      engine_id_t engine_id,
						      te_crypto_algo_t algo,
						      struct se_engine_aes_context *aes_econtext)
{
	return aes_engine_context_static_init_cm(domain, engine_id,
						 algo, aes_econtext,
						 NULL);
}

#if CCC_USE_DMA_MEM_BUFFER_ALLOC
void aes_context_dma_buf_release(struct se_aes_context *aes_context);
#endif /* CCC_USE_DMA_MEM_BUFFER_ALLOC */

status_t aes_context_static_init(te_crypto_domain_t domain, engine_id_t engine_id,
				 te_crypto_algo_t algo,
				 struct se_aes_context *aes_context,
				 struct context_mem_s *cmem);

status_t aes_init_econtext(struct se_engine_aes_context *econtext,
			   te_args_init_t *args);

/* internal (shared code) key setup routine.
 */
status_t setup_aes_key(struct se_aes_context *aes_context,
		       te_crypto_key_t *key,
		       const te_args_key_data_t *kargs);


/* internal types and functions */

/* Exported for new AES mode support.
 */
struct aes_istate {
	uint32_t       ai_bytes_left; /**< Bytes to process (decremented) */
	uint32_t       ai_out_len;    /**< Space left in output buffer (decremented) ) */
	const uint8_t *ai_input_src;  /**< Current input buffer address (advanced) */
	uint8_t       *ai_input_dst;  /**< Current output buffer address (advanced) */
	bool	       ai_add_padding; /**< Do data padding on encryption (padding block cipher) */
	uint32_t       ai_strip_pad_bytes; /**< Data unpad on deciphering (padding block cipher) */
	bool	       ai_stream_cipher; /**< Block cipher used as stream cipher */
	uint32_t       ai_unaligned_bytes; /**< Excess bytes after last full AES block */
};
#endif /* HAVE_SE_AES */

#endif /* INCLUDED_CRYPTO_CIPHER_PROC_H */
