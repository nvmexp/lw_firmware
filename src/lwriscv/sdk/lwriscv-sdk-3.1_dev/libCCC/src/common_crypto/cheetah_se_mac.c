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

/* All SE engine HW related code should be in this file.
 * If not => move it here and move other code out of here (TODO).
 */

/* NOTE about key slots.  As a convention we should assume that one
 * out of the 4 rsa keyslots is reserved for secure code (e.g. this
 * driver) and this reg shyould then be set TZ access only ( which is
 * a sticky to reset setting). Otherwise the SE driver can not do any
 * RSA operations with the SE PKA0 unit.
 *
 * As a convention (as dislwssed and suggested by me ;-) I will take
 * the last RSA key slot (slot 3) and will make it TZ only at some
 * point of time before releasing this driver...
 *
 * Same thing for AES key slots => I need at least one keyslot for
 * this code, but I do not know which number I can start to use =>
 * more dislwssions required. Now using AES key slot 2 without
 * agreements of any kind for testing (but hey, it seems to
 * work!).
 *
 * What ever slot is reserved for this it will most likely be
 * the only keyslot this driver can use to write keys. Existing key
 * use will need some authorization mechanism (as dislwssed the
 * dynloader adds simple manifest authorizations) but theese are not
 * yet merged. Lwrrently any client can use any keyslot (which is not
 * really that fine, even though all clients are secure).
 *
 * In case I do the keystore using ciphered keys => then it might be
 * required to allocate one more AES keyslot for this purpose.
 */

/* XXX TODO set "TZ use only" to any keyslot we set in this code before setting it...
 */

#include <crypto_system_config.h>

#include <ccc_lwrm_drf.h>
#include <arse0.h>

#ifndef SE0_AES0_SPARE_0
/* This file only exists in T194 and newer; e.g. this define
 * has been moved to the private version.
 */
#include <arse0_private.h>
#endif

#include <crypto_ops.h>
#include <tegra_se_gen.h>
#include <tegra_se_aes.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_SOC_FEATURE_KAC
#error "HAVE_SOC_FEATURE_KAC set when compiling tegra_se_mac.c"
#endif

#if HAVE_CMAC_AES && (HAVE_SE_AES == 0)
#error "AES based MACs require HAVE_SE_AES"
#endif

#if HAVE_CMAC_AES

#define AES_MAC_ALGO(alg) (TE_ALG_CMAC_AES == (alg))

/**** CMAC-AES operations ****/

static status_t se_aes_mac_start_operation_locked(
					struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;
#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
	uint32_t gen_op_perf_usec = GET_USEC_TIMESTAMP;
#endif

	LTRACEF("entry\n");

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
		show_se_gen_op_entry_perf("AES MAC",
					  get_elapsed_usec(gen_op_perf_usec));
	}
#endif
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
	PHYS_ADDR_T paddr_input_message = 0U;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	if (!AES_MAC_ALGO(econtext->algorithm)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("AES MAC with incorrect algorithm: 0x%x\n",
						       econtext->algorithm));
	}

#if SE_RD_DEBUG
	LTRACEF("[JUKI => SE_CMAC_HASH_BLOCKS: first=%u, last=%u, num_blocks %u (pinput_msg= %p, aes_mac phash=%p)]\n",
		econtext->is_first, econtext->is_last, num_blocks, pinput_message, econtext->aes_mac.phash);

	LOG_HEXBUF("XXX Doing CMAC data=>", pinput_message, num_blocks*16U);
#endif

	// Added this generic check here for now...
	if (0U == num_blocks) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Hash the input data for blocks zero to NumBLocks.
	 *
	 * Setup SE engine parameters.
	 * Use zero ORIGINAL iv if is_first, otherwise set previous cmac intermediate result to UPDATED iv
	 */
	ret = setup_aes_op_mode_locked(econtext, TE_ALG_CMAC_AES, CTRUE, econtext->is_first,
				       SE0_AES0_CONFIG_0_DST_HASH_REG);
	CCC_ERROR_CHECK(ret);

	paddr_input_message = DOMAIN_VADDR_TO_PADDR(econtext->domain, pinput_message);
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

	AES_ERR_CHECK(econtext->engine, "AES MAC START");

	ret = se_aes_mac_start_operation_locked(econtext);
	CCC_ERROR_CHECK(ret);

	AES_ERR_CHECK(econtext->engine, "AES MAC DONE");

	{
		uint32_t *phash32 = &econtext->aes_mac.phash[0];

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

static status_t engine_cmac_input_param_check(
	const struct se_data_params *input_params)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == input_params) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

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

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

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

	ret = engine_cmac_input_param_check(input_params);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t engine_cmac_aes_update_key_iv(struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/*
	 * SE HW does not directly support CMAC updates (like it
	 * supports SHA updates), but fortunately it can be done by
	 * copying the previous CMAC result to the UPDATED_IVS of the
	 * next operation.
	 */
	if (BOOL_IS_TRUE(econtext->is_first)) {
		/* Initialize key slot OriginalIv[127:0] to as required by OMAC-1
		 * (i.e. NIST CMAC).
		 */
		LTRACEF("CMAC init IV cleared (is_first=%u)!\n", econtext->is_first);
		ret = aes_write_key_iv_locked(
			econtext->engine, econtext->se_ks.ks_slot, 0U,
			SE_AES_KSLOT_SEL_ORIGINAL_IV, NULL);
		CCC_ERROR_CHECK(ret);
	} else {
		LOG_HEXBUF("CMAC intermediate written to UPDATED_IVS",
			   econtext->aes_mac.phash, 16U);

		/* Write the intermediate CMAC result data to updated iv slots */
		ret = aes_write_key_iv_locked(
			econtext->engine, econtext->se_ks.ks_slot, 0U,
			SE_AES_KSLOT_SEL_UPDATED_IV,
			(const uint8_t *)econtext->aes_mac.phash);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t engine_cmac_set_dst(struct se_data_params *input_params,
				    struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL != input_params->dst) {
		if (input_params->dst != (const uint8_t *)&econtext->aes_mac.phash[0]) {
			if (input_params->output_size < SE_AES_BLOCK_LENGTH) {
				CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
						     LTRACEF("DST too small for AES-MAC result\n"));
			}
			se_util_mem_move(input_params->dst, (const uint8_t *)&econtext->aes_mac.phash[0],
					 SE_AES_BLOCK_LENGTH);
		}
		input_params->output_size = SE_AES_BLOCK_LENGTH;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t cmac_process_key_iv_locked(struct se_engine_aes_context *econtext,
					   bool *use_preset_key_p,
					   bool *key_written_p,
					   bool *leave_key_to_keyslot_p)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((econtext->aes_flags & AES_FLAGS_USE_PRESET_KEY) != 0U) {
		/* Commit the pre-existing keyslot */
		*use_preset_key_p = true;
	} else {
		if (NULL == econtext->se_ks.ks_keydata) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
						 LTRACEF("CMAC-AES key undefined\n"));
		}

		/* commit keyslot; forces slot clear even if write fails */
		*key_written_p = true;

		ret = aes_write_key_iv_locked(
			econtext->engine, econtext->se_ks.ks_slot,
			econtext->se_ks.ks_bitsize,
			SE_AES_KSLOT_SEL_KEY, econtext->se_ks.ks_keydata);
		CCC_ERROR_CHECK(ret);
	}

	/* If the loaded key must be left to keyslot after the operation
	 */
	*leave_key_to_keyslot_p = ((econtext->aes_flags & AES_FLAGS_LEAVE_KEY_TO_KEYSLOT) != 0U);

	ret = engine_cmac_aes_update_key_iv(econtext);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static bool cmac_need_to_clear_keyslot(bool use_preset_key, bool key_written,
				       bool leave_key_to_keyslot)
{
	bool rval = false;

	if (!BOOL_IS_TRUE(leave_key_to_keyslot)) {
		rval = (BOOL_IS_TRUE(use_preset_key) ||
			BOOL_IS_TRUE(key_written));
	}
	return rval;
}

/* Note: This is not a self-contained CMAC-AES function. This only does the
 * low level engine dependent parts of it. For full CMAC-AES => use
 * the crypto_process.c layer MAC functions.
 *
 * AES block bytes must fit in output buffer.
 * Caller must deal with shorter output request.
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

	ret = cmac_process_key_iv_locked(econtext, &use_preset_key, &key_written,
					 &leave_key_to_keyslot);
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
		ret = engine_cmac_set_dst(input_params, econtext);
		CCC_ERROR_CHECK(ret);
	}

	econtext->is_first = false;

fail:
	/* If key was committed and not flagged to leave in keyslot => clear slot
	 */
	if (BOOL_IS_TRUE(cmac_need_to_clear_keyslot(use_preset_key, key_written,
						    leave_key_to_keyslot))) {
		status_t fret = tegra_se_clear_aes_keyslot_locked(
			econtext->engine, econtext->se_ks.ks_slot, true);
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
