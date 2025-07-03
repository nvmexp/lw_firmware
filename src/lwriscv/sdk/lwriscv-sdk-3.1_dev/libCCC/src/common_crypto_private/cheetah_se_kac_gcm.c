/*
 * Copyright (c) 2019-2021, LWPU CORPORATION.  All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 *
 */

#include <crypto_system_config.h>

#include <ccc_lwrm_drf.h>
#include <arse0.h>

#include <include/tegra_se.h>
#include <tegra_se_gen.h>
#include <tegra_se_kac_intern.h>

#if CCC_WITH_AES_GCM
#include <tegra_se_kac_gcm.h>
#endif

#if HAVE_SOC_FEATURE_KAC == 0
#error "HAVE_SOC_FEATURE_KAC not set when compiling tegra_se_kac.c"
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if CCC_WITH_AES_GCM
status_t eng_aes_gcm_get_operation_preset(const struct se_engine_aes_context *econtext,
					  uint32_t *op_preset_p)
{
	status_t ret = NO_ERROR;
	uint32_t op_preset = 0U;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(econtext->is_first) && (0U == econtext->gcm.aad_len)) {
		/* (INIT == 1) => Initialize GHASH input to zero vector */
		op_preset = LW_FLD_SET_DRF_DEF(SE0_AES0, OPERATION,
					       INIT, TRUE, op_preset);
	}

	if (BOOL_IS_TRUE(econtext->is_last)) {
		op_preset = LW_FLD_SET_DRF_DEF(SE0_AES0, OPERATION,
					       FINAL, TRUE, op_preset);
	}

	*op_preset_p = op_preset;

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t start_engine_for_gcm_tag(const struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t op_preset = 0U;

	LTRACEF("entry\n");

	ret = eng_aes_gcm_get_operation_preset(econtext, &op_preset);
	CCC_ERROR_CHECK(ret);

	AES_ERR_CHECK(econtext->engine, "AES GCM FINAL START");

	/**
	 * Issue START command in SE_OPERATION.OP
	 */
	ret = tegra_start_se0_operation_locked(econtext->engine, op_preset);
	CCC_ERROR_CHECK(ret);

	ret = se_wait_engine_free(econtext->engine);
	CCC_ERROR_CHECK(ret);

	AES_ERR_CHECK(econtext->engine, "after AES GCM FINAL");
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t engine_aes_gcm_hw_finalize_tag(struct se_engine_aes_context *econtext,
					       uint32_t processed_bytes,
					       uint8_t *aligned_tag_buffer)
{
	status_t ret = NO_ERROR;
	uint32_t out_reg = 0U;
	uint32_t bitlen = econtext->gcm.aad_len;
	PHYS_ADDR_T paddr_dst_addr = 0U;
	const uint32_t aligned_size = DMA_ALIGN_SIZE(SE_AES_BLOCK_LENGTH);

	LTRACEF("entry\n");

	/* Number of processed AAD bytes is in ECONTEXT AAD_LEN */
	CCC_SAFE_MULT_U32(bitlen, econtext->gcm.aad_len, 8U);

	LTRACEF("GCM(lenfin) AAD: %u (bits %u)\n", econtext->gcm.aad_len, bitlen);

	tegra_engine_set_reg(econtext->engine, SE0_AES0_CRYPTO_AAD_LENGTH_1, 0U);
	tegra_engine_set_reg(econtext->engine, SE0_AES0_CRYPTO_AAD_LENGTH_0, bitlen);

	/* Number of processed TEXT bytes is in parameter processed_bytes */
	CCC_SAFE_MULT_U32(bitlen, processed_bytes, 8U);

	LTRACEF("GCM(lenfin) TXT: %u (bits %u)\n", processed_bytes, bitlen);

	tegra_engine_set_reg(econtext->engine, SE0_AES0_CRYPTO_MSG_LENGTH_1, 0U);
	tegra_engine_set_reg(econtext->engine, SE0_AES0_CRYPTO_MSG_LENGTH_0, bitlen);

	paddr_dst_addr = DOMAIN_VADDR_TO_PADDR(econtext->domain, aligned_tag_buffer);
	SE_PHYS_RANGE_CHECK(paddr_dst_addr, aligned_size);

	if (! IS_40_BIT_PHYSADDR(paddr_dst_addr)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	out_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, OUT_ADDR_HI, SZ, SE_AES_BLOCK_LENGTH,
		out_reg);

	out_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, OUT_ADDR_HI, MSB,
		SE_GET_32_UPPER_PHYSADDR_BITS(paddr_dst_addr),
		out_reg);

	tegra_engine_set_reg(econtext->engine, SE0_AES0_OUT_ADDR_0,
			     se_util_lsw_u64((uint64_t)paddr_dst_addr));

	tegra_engine_set_reg(econtext->engine, SE0_AES0_OUT_ADDR_HI_0, out_reg);

	SE_CACHE_ILWALIDATE((VIRT_ADDR_T)aligned_tag_buffer, aligned_size);

#if HAVE_SE_APERTURE
	ret = se_set_aperture(PADDR_NULL, 0U,
			      paddr_dst_addr, aligned_size);
	CCC_ERROR_CHECK(ret);
#endif

	LOG_HEXBUF("GCM counter for TAG before J0=1",
		   (const uint8_t *)&econtext->gcm.counter[0],
		   16U);

	/* The GCM TAG computing requires the GCM J0 (counter with BE value 1)
	 * to callwlate TAG = MSB<128>(GCTR(K, J0, S)
	 */
	econtext->gcm.counter[3] = se_util_swap_word_endian(1U);

	LOG_HEXBUF("GCM counter for TAG after J0=1",
		   (const uint8_t *)&econtext->gcm.counter[0],
		   16U);

	ret = engine_aes_op_mode_normal(econtext,
					econtext->algorithm,
					econtext->is_encrypt,
					false,
					SE_MODE_PKT_AESMODE_GCM_FINAL,
					SE0_AES0_CONFIG_0_DST_MEMORY);
	CCC_ERROR_CHECK(ret);

	ret = start_engine_for_gcm_tag(econtext);
	CCC_ERROR_CHECK(ret);
fail:
	/* Keyslot context completed or in error for this context,
	 * so remove the HW CONTEXT flag to indicate
	 * context keyslot is no longer needed for this op.
	 *
	 * Now the key slot may be erased unless marked
	 * to be preserved by other means.
	 */
	econtext->aes_flags &= ~AES_FLAGS_HW_CONTEXT;
	LTRACEF("GCM keyslot context marked completed\n");

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_gcm_aad_bytes_locked_start(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;
	uint32_t op_preset = 0U;

	LTRACEF("entry\n");

	// GMAC
	ret = aes_encrypt_decrypt_arg_check(ac);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(ac->started)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("AES operation context already started\n"));
	}

	ret = engine_aes_setup_key_iv(ac);
	CCC_ERROR_CHECK(ret);

	/* AAD data does not increment AES-CTR counter */
	ac->update_ctr = false;
	ac->flush_dst_size = 0U;

	ac->input.dst = NULL;
	ac->input.output_size = 0U;

	/* Setup SE engine parameters for AES encrypt operation. */
	ret = setup_aes_op_mode_locked(
		ac->econtext, ac->econtext->algorithm,
		ac->econtext->is_encrypt, ac->econtext->is_first,
		SE0_AES0_CONFIG_0_DST_MEMORY);
	CCC_ERROR_CHECK(ret);

	/* Callwlate the number of blocks written to the engine and write the
	 * SE engine SE_CRYPTO_LAST_BLOCK_0.
	 *
	 * Also sets RESIDUAL_BITS field if (byte_size % 16) != 0U
	 */
	ret = engine_aes_set_block_count(ac->econtext,
					 ac->input.input_size,
					 &ac->num_blocks);
	CCC_ERROR_CHECK(ret);

#if SE_RD_DEBUG
	LTRACEF("[num_blocks %u (bytes %u) (src= %p, dst=%p), algo=0x%x]\n",
		ac->num_blocks, ac->input.input_size, ac->input.src,
		ac->input.dst, ac->econtext->algorithm);
	LOG_HEXBUF("AAD data", ac->input.src, ac->input.input_size);
#endif

	/* Set the IN_ADDR/IN_ADDR_HI registers.
	 */
	ret = aes_input_output_regs(ac);
	CCC_ERROR_CHECK(ret);

	/* Set both INIT and FINAL bits in the OPERATION_0 for the AAD
	 * processing.
	 *
	 * INIT initializes the GHASH engine input to zero.
	 * FINAL pads the last 16 byte vector with zeros if not
	 * aligned.
	 *
	 * GHASH result is written to keyslot context for GCM
	 * data processing OR tag finalization.
	 */
	/* First AAD input => set GHASH 0 */
	op_preset = LW_FLD_SET_DRF_DEF(SE0_AES0, OPERATION,
				       INIT, TRUE, op_preset);

	/* Last AAD input => Zero PAD last block to 128 bits
	 * according to CRYPTO_LAST_BLOCK_0.RESIDUAL_BITS if not
	 * zero.
	 */
	op_preset = LW_FLD_SET_DRF_DEF(SE0_AES0, OPERATION,
				       FINAL, TRUE, op_preset);

	AES_ERR_CHECK(ac->econtext->engine, "AES-GCM AAD START");

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
	ac->gen_op_perf_usec = GET_USEC_TIMESTAMP;
#endif

	/**
	 * Issue START command in SE_OPERATION.OP
	 */
	ret = tegra_start_se0_operation_locked(ac->econtext->engine, op_preset);
	CCC_ERROR_CHECK(ret);

	ac->started = true;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* AES_FLAGS_AE_AAD_DATA is set when entering this
 *
 * First implementation: AAD data in only one call,
 * not possible to split AAD to multiple calls with this code.
 */
static status_t aes_process_locked_gcm_aad(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;
	struct se_engine_aes_context *econtext = ac->econtext;
	struct se_data_params *input_params = &ac->input;

	LTRACEF("entry\n");

	if ((NULL != input_params->dst) || (0U != input_params->output_size)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

	if ((NULL != input_params->src) && (input_params->input_size > 0U)) {
		ret = aes_gcm_aad_bytes_locked_start(ac);
		CCC_ERROR_CHECK(ret);

		/* Generic AES operation completion */
		ret = aes_process_locked_async_finish(ac);
		CCC_ERROR_CHECK(ret);

		/* GCM context handled AAD byte length */
		econtext->gcm.aad_len = input_params->input_size;
	}

	if (econtext->gcm.aad_len > 0U) {
		/* Not possible to pass more AAD data in.
		 *
		 * econtext->gcm.aad_len is nonzero if any AAD bytes have been
		 * processed at this point.
		 */
		econtext->aes_flags &= ~AES_FLAGS_AE_AAD_DATA;
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_gcm_finalize_tag(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;
	struct se_engine_aes_context *econtext = ac->econtext;

	bool finalize_tag = (((econtext->aes_flags & AES_FLAGS_AE_FINALIZE_TAG) != 0U) &&
			     ((econtext->aes_flags & AES_FLAGS_HW_CONTEXT) != 0U));

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(finalize_tag)) {
		ret = engine_aes_setup_key_iv(ac);
		CCC_ERROR_CHECK(ret);

		ret = engine_aes_gcm_hw_finalize_tag(econtext,
						     ac->input.input_size,
						     ac->input.dst);
		CCC_ERROR_CHECK(ret);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t eng_aes_gcm_process_block_locked(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;
	const struct se_engine_aes_context *econtext = ac->econtext;

	LTRACEF("entry\n");

	// XXXX TODO: Do this in ASYNC_START and let ASYNC_FINISH wait for
	// XXXX  engine completion...???
	//
	if (BOOL_IS_TRUE(econtext->is_last) && (NULL == ac->input.src)) {
		ret = aes_gcm_finalize_tag(ac);
		CCC_ERROR_CHECK(ret);
	} else if ((econtext->aes_flags & AES_FLAGS_AE_AAD_DATA) != 0U) {
		ret = aes_process_locked_gcm_aad(ac);
		CCC_ERROR_CHECK(ret);
	} else {
		ret = aes_process_locked_async_start(ac);
		CCC_ERROR_CHECK(ret);

		/* AES-CTR blocks handled update counter,
		 * no special handling needed.
		 *
		 * If this includes the first block, J0 counter
		 * is programmed to be 00000001 and it gets inc32(J0)
		 * before passing counter to AES-CTR.
		 *
		 * In update cases, the COUNTER must contain the
		 * number of AES block processed so far by AES-CTR, HW
		 * will pre-increment the counter value in all cases.
		 * The number of blocks passed is tracked in the
		 * finish call.
		 */
		ret = aes_process_locked_async_finish(ac);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* CCC_WITH_AES_GCM */
