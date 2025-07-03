/*
 * Copyright (c) 2020, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */
#include <crypto_system_config.h>

#if CCC_WITH_XMSS

#include <crypto_md.h>
#include <xmss/crypto_xmss.h>
#include <xmss/xmss-verify/xmss-verify.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

static status_t xmss_engine_context_static_init(te_crypto_domain_t domain,
						engine_id_t engine_id,
						te_crypto_algo_t algo,
						struct se_engine_xmss_context *xmss_econtext,
						struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == xmss_econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = ccc_select_engine(&xmss_econtext->engine, CCC_ENGINE_CLASS_XMSS, engine_id);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("XMSS engine selection failed: %u\n", ret));

	xmss_econtext->algorithm = algo;
	xmss_econtext->domain = domain;
	xmss_econtext->cmem = cmem;

#if (TEGRA_MEASURE_PERFORMANCE & TEGRA_PERF_CONTEXT)
	xmss_econtext->perf_usec = GET_USEC_TIMESTAMP;
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t xmss_context_static_init(te_crypto_domain_t domain, engine_id_t engine_id,
				  te_crypto_algo_t algo_param,
				  struct se_xmss_context *xmss_context,
				  struct context_mem_s *cmem)
{
	status_t ret = NO_ERROR;
	te_crypto_algo_t hash_algorithm = TE_ALG_NONE;
	uint32_t hlen = 0U;
	te_crypto_algo_t algo = algo_param;

	LTRACEF("entry\n");

	if (NULL == xmss_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch(algo) {
	case TE_ALG_XMSS_SHA2_20_256:
		hash_algorithm = TE_ALG_SHA256;
		hlen = 32U;
		break;

	default:
		CCC_ERROR_MESSAGE("XMSS algorithm 0x%x unsupported\n", algo);
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	xmss_context->md_algorithm = hash_algorithm;
	xmss_context->hlen = hlen;

	ret = xmss_engine_context_static_init(domain, engine_id, algo,
					      &xmss_context->ec, cmem);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("XMSS engine context setup failed: %u\n",
					  ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Handle XMSS verify request.
 */
status_t se_xmss_process_verify(const struct se_data_params *input_params,
				const struct se_xmss_context *xmss_context)
{
	status_t ret = NO_ERROR;
	uint32_t work_buf_blen = 0U;
	uint64_t *work_buf = NULL;

	LTRACEF("entry\n");

	if ((NULL == xmss_context) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if CCC_WITH_XMSS_MINIMAL_CMEM == 0
	if (NULL == xmss_context->ec.root_seed) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("XMSS seed root not set\n"));
	}
#endif

	LTRACEF("XMSS: src=%p, input_size=%u, signature %p, sig size %u\n",
		input_params->src, input_params->input_size,
		input_params->src_signature, input_params->src_signature_size);

	/* Get an 64 bit aligned work buffer, length in bytes
	 */
	work_buf_blen = XMSS_SCRATCH_BUFFER_SIZE;
	work_buf = CMTAG_MEM_GET_TYPED_BUFFER(xmss_context->ec.cmem,
					      CMTAG_ALIGNED_BUFFER,
					      sizeof_u32(uint64_t),
					      uint64_t *,
					      work_buf_blen);
	if (NULL == work_buf) {
		CCC_ERROR_WITH_ECODE(ERR_NO_MEMORY);
	}

	{
		xmss_error_t xret = XMSS_ERR_BAD_PARAMS;

		/* XMSS verify with a scratch buffer */
		xret = xmss_verify_low_stack(xmss_context->ec.root_seed,
					     input_params->src_signature,
					     (size_t)input_params->src_signature_size,
					     work_buf,
					     (size_t)work_buf_blen,
					     input_params->src,
					     (size_t)input_params->input_size);
		switch (xret) {
		case XMSS_ERR_OK:
			ret = NO_ERROR;
			break;
		case XMSS_ERR_BAD_PARAMS:
			ret = SE_ERROR(ERR_ILWALID_ARGS);
			break;
		case XMSS_ERR_BAD_SIG:
			ret = SE_ERROR(ERR_SIGNATURE_ILWALID);
			break;
		default:
			ret = SE_ERROR(ERR_BAD_STATE);
			break;
		}
	}

fail:
	if (NULL != work_buf) {
		CMTAG_MEM_RELEASE(xmss_context->ec.cmem,
				  CMTAG_ALIGNED_BUFFER,
				  work_buf);
	}
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if CCC_WITH_XMSS_MINIMAL_CMEM == 0
static status_t xmss_key_to_context(struct se_xmss_context *xmss_context, te_crypto_key_t *key)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (key->k_key_type) {
	case KEY_TYPE_XMSS_PUBLIC:
		xmss_context->ec.root_seed = &key->k_xmss_public.root_seed[0];
		break;

	case KEY_TYPE_XMSS_PRIVATE:
		ret = SE_ERROR(ERR_NOT_IMPLEMENTED);
		break;

	default:
		LTRACEF("xmss key type invalid: %u\n", key->k_key_type);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	xmss_context->ec.keytype = key->k_key_type;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_XMSS_MINIMAL_CMEM == 0 */

status_t se_setup_xmss_key(struct se_xmss_context *xmss_context,
			   te_crypto_key_t *key,
			   const te_args_key_data_t *kvargs)
{
	status_t ret = NO_ERROR;

	te_key_type_t ktype = kvargs->k_key_type;
	uint32_t ksize      = kvargs->k_byte_size;
	uint32_t kflags     = kvargs->k_flags;
	uint8_t *root_seed  = NULL;

	LTRACEF("entry\n");

	(void)ksize;
	(void)key;

	switch (ktype) {
	case KEY_TYPE_XMSS_PUBLIC:
#if CCC_WITH_XMSS_MINIMAL_CMEM

		xmss_context->ec.keytype = ktype;

		/* When there is no key object the root_seed for XMSS
		 * is placed to engine context buffer.
		 */
		root_seed = &xmss_context->ec.root_seed[0];
#else
		key->k_byte_size = ksize;
		key->k_key_type  = ktype;
		key->k_flags     = kflags;

		/* When there is a key object, store it there
		 */
		root_seed = &key->k_xmss_public.root_seed[0];
#endif /* CCC_WITH_XMSS_MINIMAL_CMEM */

		/* Colwert the RFC key format into root_seed format
		 * supported by the xmss verify function.
		 *
		 * As long as we only support one XMSS digest (SHA256)
		 * the root_seed length is fixed. If this is not
		 * true someday, the lengths can be found by the OID.
		 */
		if ((kflags & KEY_FLAG_RFC_FORMAT) != 0U) {
			/* XXX Skip the pubkey OID check for now in
			 * kvargs->k_xmss_public.rfc8391.pubkey[0]
			 *
			 * FIXME: Add OID check, match OID with the algorithm.
			 */
			se_util_mem_move(root_seed,
					 &kvargs->k_xmss_public.rfc8391.pubkey[XMSS_PUBKEY_OID_LEN],
					 XMSS_ROOT_SEED_MAX_SIZE);
		} else {
			se_util_mem_move(root_seed,
					 &kvargs->k_xmss_public.root_seed[0],
					 XMSS_ROOT_SEED_MAX_SIZE);
		}

		LOG_HEXBUF("root_seed", root_seed, XMSS_ROOT_SEED_MAX_SIZE);
		break;

	case KEY_TYPE_XMSS_PRIVATE:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;

	default:
		LTRACEF("xmss key type invalid: %u\n", ktype);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

#if CCC_WITH_XMSS_MINIMAL_CMEM == 0
	/* For minimal CMEM setup the root_seed was placed to context
	 * above so no need to do anything here.
	 */
	ret = xmss_key_to_context(xmss_context, key);
	CCC_ERROR_CHECK(ret);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_XMSS */
