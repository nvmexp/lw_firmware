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
#ifndef INCLUDED_CRYPTO_ASIG_PROC_H
#define INCLUDED_CRYPTO_ASIG_PROC_H

#include <crypto_ops.h>

#if CCC_WITH_RSA

status_t rsa_engine_context_static_init(te_crypto_domain_t domain,
					engine_id_t engine_id,
					te_crypto_algo_t algo,
					struct se_engine_rsa_context *rsa_econtext,
					struct context_mem_s *cmem);

status_t rsa_context_static_init(te_crypto_domain_t domain, engine_id_t engine_id,
				 te_crypto_algo_t algo_param,
				 struct se_rsa_context *rsa_context,
				 struct context_mem_s *cmem);

/* internal (shared code) key setup routine.
 */
status_t se_setup_rsa_key(struct se_rsa_context *rsa_context, te_crypto_key_t *key,
			  const te_args_key_data_t *kvargs);

#endif /* CCC_WITH_RSA */

#if CCC_WITH_ECC

status_t ec_context_static_init(te_crypto_domain_t domain, engine_id_t engine_id,
				te_crypto_algo_t algo,
				te_ec_lwrve_id_t lwrve_id,
				struct se_ec_context *ec_context,
				struct context_mem_s *cmem);

/* internal (shared code) key setup routine.
 */
status_t se_setup_ec_key(struct se_ec_context *ec_context, te_crypto_key_t *key,
			 const te_args_key_data_t *kvargs);

#endif /* CCC_WITH_ECC */
#endif /* INCLUDED_CRYPTO_ASIG_PROC_H */
