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

#ifndef INCLUDED_CRYPTO_MAC_MD5_H
#define INCLUDED_CRYPTO_MAC_MD5_H

#if HAVE_HMAC_MD5

status_t se_crypto_hmac_md5_init(const crypto_context_t *ctx,
				 struct context_mem_s *cmem,
				 te_args_init_t *args,
				 struct se_mac_state_s *s,
				 te_crypto_algo_t digest_algorithm);

status_t hmac_handle_ipad_intern_md5(struct se_mac_state_s *s);

status_t hmac_md5_opad_calc(struct se_mac_state_s *s);

status_t call_md5_hmac_process_context(struct se_data_params *input, struct soft_md5_context *m5ctx);

status_t mac_hmac_md5_dofinal(const crypto_context_t *ctx,
			      struct run_state_s *run_state,
			      te_args_data_t *args);

status_t hmac_md5_set_key(struct se_mac_state_s *s,
			  te_crypto_key_t *key,
			  const te_args_key_data_t *kargs,
			  struct soft_md5_context *md5_context,
			  te_crypto_domain_t   cdomain);

#endif /* HAVE_HMAC_MD5 */
#endif /* INCLUDED_CRYPTO_MAC_MD5_H */
