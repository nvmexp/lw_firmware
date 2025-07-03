/*
 * Copyright (c) 2016-2019, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/**
 * @file   crypto_process_md5.c
 * @author Juki <jvirtanen@lwpu.com>
 * @date   Tue Jan 22 15:37:14 2019
 *
 * @brief  Processing layer for MD5 operations.
 *
 * Do not enable for production code!
 */

#include <crypto_system_config.h>

#if HAVE_MD5

#include <md5/crypto_md5_proc.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/* handle MD5 digest data processing ops (update,dofinal)
 * (trusty kvaddrs)
 */
status_t soft_md5_process_input(
	struct se_data_params *input_params,
	struct soft_md5_context *context)
{
	status_t ret = NO_ERROR;
	uint32_t bytes_left = 0U;

	LTRACEF("entry\n");

	if ((NULL == context) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	bytes_left = input_params->input_size;

	LTRACEF("DIGEST INPUT: src=%p, input_size=%u, hash_addr dst=%p\n",
		input_params->src, input_params->input_size,
		input_params->dst);

	if ((NULL == input_params->src) && (input_params->input_size > 0U)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!BOOL_IS_TRUE(context->ec.is_last)) {
		input_params->output_size = 0U;
	}

	if (BOOL_IS_TRUE(context->ec.is_last)) {
		if (input_params->output_size < context->ec.hash_size) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}

		if (bytes_left > 0U) {
			/* XXX removes const from SRC (should fix the MD5 lib) */
			MD5_Update(&context->ec.md5_ctx,
				   (uint8_t *)input_params->src, bytes_left);
			context->ec.is_first = false;
			context->byte_count += bytes_left;
			bytes_left = 0U;
		}

		MD5_Final(context->ec.tmphash, &context->ec.md5_ctx);

#if SE_RD_DEBUG
		LTRACEF("MD5 result %u bytes\n", context->ec.hash_size);
		LOG_HEXBUF("MD5 result", context->ec.tmphash, 16U);
#endif

		input_params->output_size = 0U;
		if (NULL != input_params->dst) {
			if (context->ec.tmphash != input_params->dst) {
				se_util_mem_move(input_params->dst, context->ec.tmphash,
						 context->ec.hash_size);
			}
			input_params->output_size = context->ec.hash_size;
		}
	} else {
		if (bytes_left > 0U) {
			/* XXX removes const from SRC (should fix the MD5 lib) */
			MD5_Update(&context->ec.md5_ctx,
				   (uint8_t *)input_params->src, bytes_left);
			context->ec.is_first = false;
			context->byte_count += bytes_left;
			bytes_left = 0U;
		}
	}

	DEBUG_ASSERT(bytes_left == 0U);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_MD5 */
