/*
 * Copyright (c) 2016-2020, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

#include <crypto_system_config.h>

#if HAVE_MD5

#include <md5/crypto_md5_proc.h>
#include <crypto_process_call.h>

#if HAVE_USER_MODE
#include <crypto_ta_api.h>
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_USER_MODE
#include <crypto_process_call.h>

status_t um_sw_process_md5(struct se_data_params *input,
			   struct soft_md5_context *pcontext)
{
	status_t ret = ERR_BAD_STATE;

	FI_BARRIER(status, ret);

	LTRACEF("entry\n");

	if (NULL == pcontext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (pcontext->ec.domain) {
	case TE_CRYPTO_DOMAIN_TA:
		ret = ta_soft_md5_process_input(input, pcontext);
		break;
	default:
		ret = CORE_PROCESS_SW_MD5(input, pcontext);
		break;
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_USER_MODE */

status_t md5_engine_context_static_init(te_crypto_domain_t domain,
					engine_id_t engine_id,
					te_crypto_algo_t algo,
					struct se_engine_md5_context *md5_econtext)
{
	status_t ret = NO_ERROR;

	(void)algo;

	ret = tegra_select_engine(&md5_econtext->engine, CCC_ENGINE_CLASS_MD5, engine_id);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("MD5 software selection failed: %u\n", ret));

	md5_econtext->is_first = true;
	md5_econtext->is_last  = false;
	md5_econtext->hash_size =  16U;
	md5_econtext->block_size = 64U;
	md5_econtext->domain = domain;

	MD5_Init(&md5_econtext->md5_ctx);

#if TEGRA_MEASURE_PERFORMANCE
	md5_econtext->perf_usec = GET_USEC_TIMESTAMP;
#endif

fail:
	return ret;
}

/* Does not memset zero => do it yourself */
status_t md5_context_static_init(te_crypto_domain_t domain,
				 engine_id_t engine_id, te_crypto_algo_t algo,
				 struct soft_md5_context *md5_context)
{
	status_t ret = NO_ERROR;

	if ((algo != TE_ALG_MD5) || (NULL == md5_context)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = md5_engine_context_static_init(domain, engine_id, algo, &md5_context->ec);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("MD5 engine context setup failed: %u\n", ret));

	md5_context->byte_count = 0U;
fail:
	return ret;
}
#endif /* HAVE_MD5 */
