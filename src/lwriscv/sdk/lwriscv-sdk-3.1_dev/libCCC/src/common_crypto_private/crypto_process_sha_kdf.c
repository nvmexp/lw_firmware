/*
 * Copyright (c) 2018-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/* Support for Key Wrap and Key Unwrap with key access control (KAC) units.
 */
#include <crypto_system_config.h>

#if HAVE_SE_KAC_KDF

#include <crypto_ops.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

status_t se_sha_kdf_process_input(struct se_data_params *input_params,
				  struct se_sha_context *sha_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	HW_MUTEX_TAKE_ENGINE(sha_context->ec.engine);
	ret = CCC_ENGINE_SWITCH_KDF(sha_context->ec.engine, input_params, &sha_context->ec);
	HW_MUTEX_RELEASE_ENGINE(sha_context->ec.engine);

	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_KAC_KDF */
