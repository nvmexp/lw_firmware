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

/* Support for Key Wrap and Kwy Unwrap with key access control (KAC) units.
 */
#include <crypto_system_config.h>

#if HAVE_SE_KAC_KEYOP

#include <crypto_cipher_proc.h>
#include <crypto_kwuw_proc.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

/* Value check for the 12 byte zero pad is lwrrently compile time
 * disabled as requested by Bijan.
 *
 * The HW protects all other bits (than these zero pads) by GCM TAG
 * since these bits are not programmed to the HW (they are just
 * ignored).
 *
 * When creating a HW blob the KW operation always writes these bits
 * as zero to the lwrren KW blob. The original intention was to be
 * able to reject an incompatible blob before programming it to the
 * HW. But this is now disabled and before we have another version of
 * the KW blob it really does not matter.
 */
#ifndef HAVE_VERIFY_REDUNDANT_KUW_ZERO_PAD
#define HAVE_VERIFY_REDUNDANT_KUW_ZERO_PAD 0
#endif

/* Wrapped Key output Blob = (Key manifest || 0^96 || Wrapped Key || 0^(256-n) || Tag)
 * This is the only code in CCC that parses the KW blob format.
 *
 * When any of the 96 bit zeroes are non zero the KW blob format is
 * not supported by this CCC version. If the format is later changed
 * it may mean that the manifest is longer or some other incompatible
 * changes are made => at least part of these 96 bits may be treated
 * as version numbers later.
 */
#define KAC_KEY_WRAP_DST_MANIFEST_OFFSET	 0U	/* 4 byte manifest */
#define KAC_KEY_WRAP_DST_MANIFEST_BYTE_LEN	 4U
#if HAVE_VERIFY_REDUNDANT_KUW_ZERO_PAD
#define KAC_KEY_WRAP_DST_ZERO_PAD_OFFSET	 4U
#define KAC_KEY_WRAP_DST_ZERO_PAD_LENGTH	12U	/* 12 byte zero pad follow before the wrapped key */
#endif
#define KAC_KEY_WRAP_DST_WKEY_OFFSET		16U	/* Key is zero padded to 128 bits (16 bytes) */
#define KAC_KEY_WRAP_DST_TAG_OFFSET		48U
#define KAC_KEY_WRAP_DST_TAG_BYTE_LEN		16U

/* Supports the first version of KW blob format with 96 zero pad bits
 * after the 32 bit manifest. Wrapped key follows the zero bits,
 * and then terminated by a 16 byte TAG.
 */
status_t se_kwuw_parse_wrap_blob(struct se_engine_aes_context *econtext,
				 const uint8_t *wb, uint32_t wb_len)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == wb) || (wb_len != KAC_KEY_WRAP_BLOB_LEN)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((NULL == econtext->kwuw.wkey) || (NULL == econtext->kwuw.tag)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

#if HAVE_VERIFY_REDUNDANT_KUW_ZERO_PAD
	/* Verify the zero padding to contain 96 zero bits in the first
	 * KW blob format
	 */
	if (!BOOL_IS_TRUE(se_util_vec_is_zero(&wb[KAC_KEY_WRAP_DST_ZERO_PAD_OFFSET],
					      KAC_KEY_WRAP_DST_ZERO_PAD_LENGTH,
					      &ret))) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("KW blob format not supported\n"));
	}
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_VERIFY_REDUNDANT_KUW_ZERO_PAD */

	/* Copy manifest from the blob to the context (32 bit unsigned value)
	 */
	se_util_mem_move((uint8_t *)&econtext->kwuw.manifest,
			 &wb[KAC_KEY_WRAP_DST_MANIFEST_OFFSET],
			 KAC_KEY_WRAP_DST_MANIFEST_BYTE_LEN);

	/* Copy wrapped key from the blob to the context
	 */
	se_util_mem_move((uint8_t *)&econtext->kwuw.wkey[0],
			 &wb[KAC_KEY_WRAP_DST_WKEY_OFFSET],
			 KAC_KEY_WRAP_DST_WKEY_MAX_BYTE_LEN);

	/* Copy KUW tag from the blob to the context
	 */
	se_util_mem_move((uint8_t *)&econtext->kwuw.tag[0],
			 &wb[KAC_KEY_WRAP_DST_TAG_OFFSET],
			 KAC_KEY_WRAP_DST_TAG_BYTE_LEN);

	LOG_INFO("KUW using blob manifest value: 0x%x\n",
		 econtext->kwuw.manifest);
	LOG_HEXBUF("KUW using wkey\n",
		   (const uint8_t *)&econtext->kwuw.wkey[0],
		   KAC_KEY_WRAP_DST_WKEY_MAX_BYTE_LEN);
	LOG_HEXBUF("KUW using TAG\n",
		   (const uint8_t *)&econtext->kwuw.tag[0],
		   KAC_KEY_WRAP_DST_TAG_BYTE_LEN);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t se_kwuw_process_input(
	struct se_data_params *input_params,
	struct se_aes_context *aes_context)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	HW_MUTEX_TAKE_ENGINE(aes_context->ec.engine);
	ret = CCC_ENGINE_SWITCH_KWUW(aes_context->ec.engine, input_params, &aes_context->ec);
	HW_MUTEX_RELEASE_ENGINE(aes_context->ec.engine);

	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_KAC_KEYOP */
