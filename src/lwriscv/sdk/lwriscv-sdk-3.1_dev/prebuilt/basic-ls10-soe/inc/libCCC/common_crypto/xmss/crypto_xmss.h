/*
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation
 * is strictly prohibited.
 *
 */

/* CCC adaptation for the SW implementation of XMSS verify.
 */
#ifndef INCLUDED_CRYPTO_XMSS_H
#define INCLUDED_CRYPTO_XMSS_H

#if CCC_WITH_XMSS

#define XMSS_VALUE_N	XMSS_SHA256_HASH_SIZE /* XMSS N is SHA256 digest length used in
					       * TE_ALG_XMSS_SHA2_20_256
					       */
#define XMSS_VALUE_H	20U	/* Tree height */

#define XMSS_SHA2_20_256_SIGLEN	(4U + ((1U + 67U + XMSS_VALUE_H) * XMSS_VALUE_N))

/* XMSS is a SW algorithm using HW SHA engine
 * Only supprts XMSS verify for now.
 */
struct se_engine_xmss_context {
	const struct engine_s *engine;
	struct context_mem_s  *cmem; /* context memory for this call chain or NULL */

	uint32_t               xmss_flags;
	te_key_type_t          keytype;
	te_crypto_algo_t       algorithm;
	te_crypto_domain_t     domain;

	uint32_t	       perf_usec;
#if CCC_WITH_XMSS_MINIMAL_CMEM
	uint8_t		       root_seed[XMSS_ROOT_SEED_MAX_SIZE];
#else
	uint8_t		      *root_seed;
#endif

#if 0
	se_engine_sha_context_t ec_hw; /**< Context for SHA HW engine
					* Using HW SHA is not yet supported as XMSS
					* implementation uses little-endian SW SHA for
					* improving performance of the SW-only version.
					*/
#endif
};

/**
 * @ingroup process_layer_api
 * @brief Context for XMSS operations
 */
/*@{*/
struct se_xmss_context {
	te_crypto_alg_mode_t mode;

	te_crypto_algo_t     md_algorithm; /**< XMSS digest algo
					    * (XXX now fixed to TE_ALG_SHA256)
					    */
	uint32_t	     hlen;	   /**< length of XMSS md algo output */

	struct se_engine_xmss_context ec; /**< Engine context object for XMSS */
};
/*@}*/

status_t xmss_context_static_init(te_crypto_domain_t domain, engine_id_t engine_id,
				  te_crypto_algo_t algo_param,
				  struct se_xmss_context *xmss_context,
				  struct context_mem_s *cmem);

status_t se_xmss_process_verify(const struct se_data_params *input_params,
				const struct se_xmss_context *xmss_context);

status_t se_setup_xmss_key(struct se_xmss_context *xmss_context,  te_crypto_key_t *key,
			   const te_args_key_data_t *kvargs);
#endif /* CCC_WITH_XMSS */
#endif /* INCLUDED_CRYPTO_XMSS_H */
