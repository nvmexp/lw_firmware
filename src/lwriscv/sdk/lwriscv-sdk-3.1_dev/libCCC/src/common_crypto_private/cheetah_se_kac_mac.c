/*
 * Copyright (c) 2015-2021, LWPU CORPORATION.  All Rights Reserved.
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

#if HAVE_SOC_FEATURE_KAC == 0
#error "HAVE_SOC_FEATURE_KAC not set when compiling tegra_se_kac.c"
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_CMAC_AES

#if HAVE_SE_AES == 0
#error "AES based MACs require HAVE_SE_AES"
#endif

/**** CMAC-AES operations ****/

/* Note: For T23X the CMAC_RESULT register is the only way to provide
 * data back to the engine. It does not use any internal keyslot
 * context for CMAC.
 *
 * This must be zeroed on CMAC init and set to the intermediated value
 * when continued.
 */
static status_t se_write_cmac_result_regs(const engine_t *engine,
					  const uint32_t *cvalue)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;

	LTRACEF("entry\n");

	for (inx = 0U; inx < 4U; inx++) {
		tegra_engine_set_reg(engine,
				     SE0_AES0_CMAC_RESULT + (BYTES_IN_WORD * inx),
				     ((NULL == cvalue) ? 0U : cvalue[inx]));
	}

	FI_BARRIER(u32, inx);

	if (4U != inx) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Do AES encrypt in CMAC mode. CMAC is otherwise dealt with by the caller.
 */
static status_t se_aes_cmac_hash_blocks_locked(
	struct se_engine_aes_context *econtext,
	const uint8_t *pinput_message,
	uint32_t num_blocks)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;
	uint32_t se_config_reg = 0U;
	PHYS_ADDR_T paddr_input_message = PADDR_NULL;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	if (!AES_MAC_ALGO(econtext->algorithm)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("AES MAC with incorrect algorithm: 0x%x\n",
						       econtext->algorithm));
	}

#if SE_RD_DEBUG
	LTRACEF("[SE_CMAC_HASH_BLOCKS: first=%u, last=%u, num_blocks %u (pinput_msg= %p, phash=%p)]\n",
		econtext->is_first, econtext->is_last, num_blocks,
		pinput_message, econtext->aes_mac.phash);

	LOG_HEXBUF("XXX Doing CMAC data=>", pinput_message, num_blocks*16U);
#endif

	// Added this generic check here for now...
	if (0U == num_blocks) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Hash the input data for blocks zero to NumBLocks.
	 *
	 * Setup SE engine parameters.  Use zero ORIGINAL iv if
	 * is_first, otherwise set previous cmac intermediate result
	 * to UPDATED iv
	 */
	ret = setup_aes_op_mode_locked(econtext, TE_ALG_CMAC_AES, CTRUE, econtext->is_first,
				       SE0_AES0_CONFIG_0_DST_HASH_REG);
	CCC_ERROR_CHECK(ret);

	if (NULL == pinput_message) {
		/* Workaround for coverity issue on cert-C check */
		paddr_input_message = DOMAIN_VADDR_TO_PADDR(econtext->domain, (const uint8_t *)NULL);
	} else {
		paddr_input_message = DOMAIN_VADDR_TO_PADDR(econtext->domain, &pinput_message[0]);
	}
	SE_PHYS_RANGE_CHECK(paddr_input_message, num_blocks * SE_AES_BLOCK_LENGTH);

	SE_CACHE_FLUSH((VIRT_ADDR_T)pinput_message, num_blocks * SE_AES_BLOCK_LENGTH);

	/* Program input message address (low 32 bits) */
	tegra_engine_set_reg(econtext->engine, SE0_AES0_IN_ADDR_0,
			     se_util_lsw_u64((uint64_t)paddr_input_message));

	/* Program input message buffer byte size (24 bits) into
	 * SE0_AES0_IN_ADDR_HI_0_SZ and 8-bit MSB of 40b
	 * dma addr into MSB field.
	 */
	se_config_reg = 0U;
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, IN_ADDR_HI, SZ,
					   num_blocks * SE_AES_BLOCK_LENGTH,
					   se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, IN_ADDR_HI, MSB,
					   SE_GET_32_UPPER_PHYSADDR_BITS(paddr_input_message),
					   se_config_reg);
	tegra_engine_set_reg(econtext->engine, SE0_AES0_IN_ADDR_HI_0, se_config_reg);

	/* num_blocks - 1 = SE_CRYPTO_LAST_BLOCK */
	tegra_engine_set_reg(econtext->engine, SE0_AES0_CRYPTO_LAST_BLOCK_0, num_blocks - 1U);

#if HAVE_SE_APERTURE
	ret = se_set_aperture(paddr_input_message, (num_blocks * SE_AES_BLOCK_LENGTH),
			      PADDR_NULL, 0U);
	CCC_ERROR_CHECK(ret);
#endif

	AES_ERR_CHECK(econtext->engine, "AES MAC START");

	{
#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
		uint32_t gen_op_perf_usec = GET_USEC_TIMESTAMP;
#endif

		/**
		 * Issue START command in SE0_AES0_OPERATION.OP.
		 */
		ret = tegra_start_se0_operation_locked(econtext->engine, 0U);
		CCC_ERROR_CHECK(ret);

		/* Block while SE processes this chunk, then release mutex. */
		ret = se_wait_engine_free(econtext->engine);
		CCC_ERROR_CHECK(ret);

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
		if (0U != gen_op_perf_usec) {
			show_se_gen_op_entry_perf("AES MAC", get_elapsed_usec(gen_op_perf_usec));
		}
#endif
	}

	AES_ERR_CHECK(econtext->engine, "AES MAC DONE");

	{
		uint32_t *phash32 = econtext->aes_mac.phash;

		for (inx = 0U; inx < (SE_AES_BLOCK_LENGTH / BYTES_IN_WORD); inx++) {
			uint32_t val = tegra_engine_get_reg(econtext->engine,
							    SE0_AES0_CMAC_RESULT_0 + (BYTES_IN_WORD*inx));
#if CLEAR_HW_RESULT
			tegra_engine_set_reg(econtext->engine, SE0_AES0_CMAC_RESULT_0 + (BYTES_IN_WORD*inx), 0U);
#endif
			phash32[inx] = val;
		}
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_arg_check_ectx(const struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(econtext->is_encrypt) || !BOOL_IS_TRUE(econtext->is_hash)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (econtext->se_ks.ks_slot >= SE_AES_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!IS_VALID_AES_KEYSIZE_BITS(econtext->se_ks.ks_bitsize)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!AES_MAC_ALGO(econtext->algorithm)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_arg_check_input(const struct se_data_params *input_params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* NULL input CMAC can't be handled by this code, it is dealt with the
	 * process layer (by the caller). This code must always have input...
	 */
	if ((NULL == input_params->src) || (0U == input_params->input_size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Input passed here is always AES block aligned; the CMAC
	 * algorithm tweaks and any possible padding is handler by the caller.
	 */
	if ((input_params->input_size % SE_AES_BLOCK_LENGTH) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* AES engine can only handle up to ((2^24 / 16) * 16) bytes in
	 * one call (Largest number of 16 byte blocks in (2^24-1) bytes).
	 */
	if (input_params->input_size > MAX_AES_ENGINE_CHUNK_SIZE) {
		CCC_ERROR_WITH_ECODE(ERR_TOO_BIG,
				     LTRACEF("CMAC input chunk size too big: 0x%x\n",
					     input_params->input_size));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t engine_cmac_process_arg_check(
	const struct se_data_params *input_params,
	const struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = cmac_arg_check_ectx(econtext);
	CCC_ERROR_CHECK(ret);

	ret = cmac_arg_check_input(input_params);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_set_cmac_init_regs(
	const struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* T23X CMAC: Set the init vector to SE0_AES0_CMAC_RESULT registers
	 * both first time (zeroed) and for updates (copy the previous reg contents
	 * back).
	 */
	if (BOOL_IS_TRUE(econtext->is_first)) {
		ret = se_write_cmac_result_regs(econtext->engine, NULL);
		CCC_ERROR_CHECK(ret);

		LTRACEF("CMAC init vector cleared (is_first=%u)!\n", econtext->is_first);
	} else {
		ret = se_write_cmac_result_regs(econtext->engine, econtext->aes_mac.phash);
		CCC_ERROR_CHECK(ret);

		LOG_HEXBUF("CMAC intermediate written to CMAC_RESULT",
			   econtext->aes_mac.phash, SE_AES_BLOCK_LENGTH);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_cmac_move_last_block(
	struct se_data_params *input_params,
	const struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL != input_params->dst) {
		if (input_params->dst != (const uint8_t *)econtext->aes_mac.phash) {
			if (input_params->output_size < SE_AES_BLOCK_LENGTH) {
				CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
						     LTRACEF("DST too small for CMAC-AES result\n"));
			}
			se_util_mem_move(input_params->dst,
					 (const uint8_t *)econtext->aes_mac.phash,
					 SE_AES_BLOCK_LENGTH);
		}
		input_params->output_size = SE_AES_BLOCK_LENGTH;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Note: This is not a self-contained CMAC-AES function. This only does the
 * low level engine dependent parts of it. For full CMAC-AES => use
 * the crypto_process.c layer MAC functions.
 */
status_t engine_cmac_process_block_locked(
	struct se_data_params *input_params,
	struct se_engine_aes_context *econtext)
{
	uint32_t num_blocks;
	status_t ret = NO_ERROR;
	bool leave_key_to_keyslot = false;
	bool key_written = false;
	bool use_preset_key = false;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	ret = engine_cmac_process_arg_check(input_params, econtext);
	CCC_ERROR_CHECK(ret);

	num_blocks = input_params->input_size / SE_AES_BLOCK_LENGTH;

	if ((econtext->aes_flags & AES_FLAGS_USE_PRESET_KEY) != 0U) {
		/* Commit the pre-existing keyslot */
		use_preset_key = true;
	} else {
		uint32_t se_kac_flags = SE_KAC_FLAGS_NONE;

		if (NULL == econtext->se_ks.ks_keydata) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("CMAC-AES key undefined\n"));
		}
#if SE_RD_DEBUG
		LOG_HEXBUF("XXX Setting CMAC pkey:", econtext->se_ks.ks_keydata,
			   econtext->se_ks.ks_bitsize / 8U);
#endif

		/* commit keyslot; forces slot clear even if write fails */
		key_written = true;

		// XXX Should get USER field from context!
		ret = se_set_engine_kac_keyslot_locked(econtext->engine,
						       econtext->se_ks.ks_slot,
						       econtext->se_ks.ks_keydata,
						       econtext->se_ks.ks_bitsize,
						       SE_KAC_KEYSLOT_USER,
						       se_kac_flags,
						       0U,
						       econtext->algorithm);
		CCC_ERROR_CHECK(ret, LTRACEF("Failed to set CMAC-AES key\n"));
	}

	/* If the loaded key must be left to keyslot after the operation
	 */
	leave_key_to_keyslot = ((econtext->aes_flags & AES_FLAGS_LEAVE_KEY_TO_KEYSLOT) != 0U);

	/* Init or continue from intermediate
	 */
	ret = se_set_cmac_init_regs(econtext);
	CCC_ERROR_CHECK(ret);

	LTRACEF("CMAC(SE-op START) is_last %u, is_first %u bytes %u:\n",
		econtext->is_last, econtext->is_first, input_params->input_size);
	LOG_HEXBUF("CMAC START", input_params->src, input_params->input_size);

	ret = se_aes_cmac_hash_blocks_locked(econtext, input_params->src, num_blocks);
	CCC_ERROR_CHECK(ret);

#if SE_RD_DEBUG
	LOG_HEXBUF("CMAC(SE-op END) result:", econtext->aes_mac.phash, 16U);
#endif

	if (BOOL_IS_TRUE(econtext->is_last)) {
		ret = se_cmac_move_last_block(input_params, econtext);
		CCC_ERROR_CHECK(ret);
	}

	econtext->is_first = false;

fail:
	/* If key was committed and not flagged to leave in keyslot => clear slot
	 */
	if ((BOOL_IS_TRUE(use_preset_key) || BOOL_IS_TRUE(key_written)) &&
	    !BOOL_IS_TRUE(leave_key_to_keyslot)) {
		//
		// CMAC_RESULT register already cleared when read => no need to clar IV register
		// since CMAC no longer uses it in T23X
		//
		status_t fret = tegra_se_clear_aes_keyslot_locked(
			econtext->engine, econtext->se_ks.ks_slot, false);
		if (NO_ERROR != fret) {
			LTRACEF("Failed to clear engine %u CMAC-AES keyslot %u\n",
				econtext->engine->e_id, econtext->se_ks.ks_slot);
			if (NO_ERROR == ret) {
				ret = fret;
			}
			/* FALLTHROUGH */
		}
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_CLOSE_FUNCTIONS
void engine_cmac_close_locked(void)
{
	DEBUG_ASSERT_HW_MUTEX;

	return;
}
#endif
#endif /* HAVE_CMAC_AES */
