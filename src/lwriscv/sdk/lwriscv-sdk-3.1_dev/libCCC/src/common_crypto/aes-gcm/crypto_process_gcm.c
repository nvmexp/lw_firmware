/*
 * Copyright (c) 2018-2020, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

/**
 * @author Juki <jvirtanen@lwpu.com>
 * @date   2020
 *
 * Support for authenticating encryption (AE) mode AES-GCM,
 * for T23X AES engine with HW keyslot context.
 */
#include <crypto_system_config.h>

#if CCC_WITH_AES_GCM

#include <crypto_cipher.h>
#include <crypto_cipher_proc.h>
#include <aes-gcm/crypto_ops_gcm.h>

#if HAVE_USER_MODE
#include <crypto_ta_api.h>
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/* XXXX TODO: EXPORTED FUNCTION => DOCUMENT IN HEADER */
status_t ae_gcm_finalize_tag(struct se_aes_context *aes_ctx)
{
	status_t ret = NO_ERROR;
	struct se_engine_aes_context *econtext = &aes_ctx->ec;
	struct se_data_params input;

	LTRACEF("entry\n");

	if (!BOOL_IS_TRUE(econtext->is_last)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	/* Very special parameters:
	 *
	 * AES engine input with NULL src is not allowed in any other case and
	 * input_size is the total bytes processed by the cipher/decipher
	 * engine.
	 *
	 * This is the last engine operation to FINALIZE AES-GCM processing
	 * writing the TAG to input.dst (DMA aligned buffer aes_ctx->buf).
	 */
	input.src = NULL;
	input.input_size = aes_ctx->dst_total_written;
	input.dst = aes_ctx->buf;
	input.output_size = SE_AES_BLOCK_LENGTH;

	econtext->aes_flags |= AES_FLAGS_AE_FINALIZE_TAG;

	HW_MUTEX_TAKE_ENGINE(econtext->engine);
	ret = CCC_ENGINE_SWITCH_AES(econtext->engine, &input, econtext);
	HW_MUTEX_RELEASE_ENGINE(econtext->engine);
	CCC_ERROR_CHECK(ret);

	se_util_mem_move(&econtext->gcm.tag[0], &aes_ctx->buf[0], SE_AES_BLOCK_LENGTH);
	se_util_mem_set(&aes_ctx->buf[0], 0U, SE_AES_BLOCK_LENGTH);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* GCM handler */
static status_t gcm_handle_input(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;
	struct se_engine_aes_context *econtext = &aes_context->ec;

	LTRACEF("entry\n");

	if ((aes_context->ec.aes_flags & AES_FLAGS_AE_AAD_DATA) != 0U) {
		/* Short circuit AAD data handling to engine code
		 * since there is no buffering done in process layer
		 * contexts for it.
		 */
		HW_MUTEX_TAKE_ENGINE(econtext->engine);
		ret = CCC_ENGINE_SWITCH_AES(econtext->engine, input_params, econtext);
		HW_MUTEX_RELEASE_ENGINE(econtext->engine);

		CCC_ERROR_CHECK(ret);
	} else {
		ret = se_aes_process_input(input_params, aes_context);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t gcm_handle_aad_from_init(const struct se_data_params *input_params,
					 struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;
	struct se_data_params input;

	LTRACEF("entry\n");

	input.input_size = aes_context->ec.gcm.aad_len;
	input.src        = aes_context->gcm.aad;
	input.output_size = 0U;
	input.dst = NULL;

	/* Trap case with two ways passing AAD buffers to same call.
	 * Not supported now...
	 */
	if ((NULL == input_params->dst) ||
	    (0U == input_params->output_size)) {
		if ((NULL != input_params->src) ||
		    (0U != input_params->input_size)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("AES-GCM two AAD input methods used\n"));
		}
	}

	/* Process INIT AAD (MOD 16 data processed,
	 *  non-aligned data is buffered for later)
	 */
	ret = gcm_handle_input(&input, aes_context);
	CCC_ERROR_CHECK(ret,
			LTRACEF("AES-GCM initial AAD failure: %d\n", ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t gcm_handle_aad(struct se_data_params *input_params,
			       struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Handle the AAD data passed in by the INIT primitive.
	 *
	 * This has the is_first set so no enc/dec has been
	 * performed yet for this context.
	 */
	if (BOOL_IS_TRUE(aes_context->ec.is_first) &&
	    (NULL != aes_context->gcm.aad) &&
	    (0U < aes_context->ec.gcm.aad_len)) {

		ret = gcm_handle_aad_from_init(input_params, aes_context);
		CCC_ERROR_CHECK(ret);

	} else if ((NULL == input_params->dst) &&
		   (0U == input_params->output_size)) {

		/* Handle the AAD data if passed in the input_params; this is indicated
		 * by setting <DST,OUTPUT_SIZE> values <NULL,0U>
		 *
		 * This call does not handle TEXT DATA input for GCM (with AES-CTR)
		 */
		ret = gcm_handle_input(input_params, aes_context);
		CCC_ERROR_CHECK(ret,
				LTRACEF("AES-GCM data param AAD failure: %d\n", ret));
	} else {
		/* No AAD data passed in */
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* GCM needs the intermediate handler to deal with the possible AAD data
 * passed in by the client with the INIT primitive.
 */
status_t se_aes_gcm_process_input(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((aes_context->ec.aes_flags & AES_FLAGS_AE_AAD_DATA) != 0U) {
		ret = gcm_handle_aad(input_params, aes_context);
		CCC_ERROR_CHECK(ret);

		if ((aes_context->ec.gcm.aad_len > 0U) ||
		    (NULL != input_params->dst) ||
		    (0U != input_params->output_size)) {

			/* Not possible to pass more AAD data in after this.
			 *
			 * econtext->gcm.aad_len is nonzero if any AAD bytes have been
			 * processed at this point.
			 */
			aes_context->ec.aes_flags &= ~AES_FLAGS_AE_AAD_DATA;
		}
	}

	if ((aes_context->ec.aes_flags & AES_FLAGS_AE_AAD_DATA) == 0U) {

		// XXX If not is_first then NULL input is ERROR here!!! => TRAP

		if ((NULL != input_params->dst) ||
		    (input_params->output_size != 0U)) {
			ret = gcm_handle_input(input_params, aes_context);
			CCC_ERROR_CHECK(ret);
		}
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_SE_ASYNC_AES
static status_t se_gcm_process_async_input(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context)
{
	// NOT IMPLEMENTED YET => add async support functions
	return SE_ERROR(ERR_NOT_IMPLEMENTED);
}
#endif

/* XXXX TODO: EXPORTED FUNCTION => DOCUMENT IN HEADER
 * XXXX Should be in crypto_gcm_proc.c?
 */
status_t cinit_gcm(te_crypto_domain_t domain,
		   struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)domain;

	if (NULL == aes_context) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Prefer not to set this arg const
	 * even though now it could be.
	 */
	FI_BARRIER(u32, aes_context->used);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_AES_GCM */
