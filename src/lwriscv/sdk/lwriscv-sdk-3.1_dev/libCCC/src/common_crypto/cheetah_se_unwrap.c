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
#error "HAVE_SOC_FEATURE_KAC set when compiling tegra_se_unwrap.c"
#endif

#if HAVE_SE_UNWRAP

#if HAVE_SE_AES == 0
#error "Keyslot unwrap requires HAVE_SE_AES"
#endif

/*
 * Key unwrap for T18X/T19X systems.
 */
static status_t engine_aes_op_mode_t19x_unwrap(
	struct se_engine_aes_context *econtext,
	te_crypto_algo_t algorithm,
	bool upper_quad)
{
	status_t ret = NO_ERROR;
	bool use_orig_iv = false;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	switch (algorithm) {
	case TE_ALG_AES_CBC:
	case TE_ALG_AES_CBC_NOPAD:
		use_orig_iv = !upper_quad;
		break;

	case TE_ALG_AES_ECB_NOPAD:
		break;

#if HAVE_AES_CTR
	case TE_ALG_AES_CTR:
		break;
#endif

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = setup_aes_op_mode_locked(econtext, algorithm, CFALSE, use_orig_iv,
				       SE0_AES0_CONFIG_0_DST_KEYTABLE);
	CCC_ERROR_CHECK(ret);

	ret = aes_keytable_dst_reg_setup(econtext, upper_quad);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

/* T18X/T19X key unwrap to keyslot.
 *
 * T19X additionally supports per-key setting which allows a key to be
 * used only for this unwrapping operation so that the unwrap target
 * is a KEYSLOT.
 *
 * CCC does not deal with key properties (at least lwrrently),
 * and any AES keyslot can be used for unwrapping a key.
 */
static status_t aes_unwrap_arg_check(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	// dst can be NULL with unwrap (not used)
	if ((NULL == ac) || (NULL == ac->econtext) ||
	    (NULL ==  ac->input.src) ||
	    (0U == ac->input.input_size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_unwrap_start_check_algorithm(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	switch (ac->econtext->algorithm) {
#if HAVE_AES_CTR
	case TE_ALG_AES_CTR:
		ac->update_ctr = true;

		if ((ac->econtext->aes_flags & AES_FLAGS_CTR_MODE_SWAP_ENDIAN) != 0U) {
			ac->spare_reg_0 = tegra_engine_get_reg(ac->econtext->engine, SE0_AES0_SPARE_0);
			tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_SPARE_0, (ac->spare_reg_0 | 0x1U));
		}
		break;
#endif /* HAVE_AES_CTR */

	case TE_ALG_AES_ECB_NOPAD:
	case TE_ALG_AES_CBC_NOPAD:
#if HAVE_AES_OFB
	case TE_ALG_AES_OFB:
#endif
		break;

		/* Padding modes not support for unwrapping.
		 *If needed, please add a requirement
		 */
	case TE_ALG_AES_ECB:
	case TE_ALG_AES_CBC:
		/* FALLTHROUGH */
	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_unwrap_iv_setup(struct se_engine_aes_context *econtext,
				    bool upper_quad)
{
	status_t ret = NO_ERROR;
	se_aes_keyslot_id_sel_t iv_sel = SE_AES_KSLOT_SEL_ORIGINAL_IV;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(upper_quad)) {
		iv_sel = SE_AES_KSLOT_SEL_UPDATED_IV;
	}

	LOG_HEXBUF("Unwrap IV set", (const uint8_t *)&econtext->iv_stash[0], 16U);

	ret = aes_write_key_iv_locked(
		econtext->engine,
		econtext->se_ks.ks_slot, 0U, iv_sel,
		(const uint8_t *)&econtext->iv_stash[0]);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* upper_quad is only true for the second half of the unwrap operation
 * done only when unwrapping longer than 128 bit keys.
 */
static status_t aes_unwrap_locked_start(async_aes_ctx_t *ac, bool upper_quad)
{
	uint32_t se_config_reg = 0U;
	status_t ret = NO_ERROR;
	PHYS_ADDR_T paddr_src_addr = 0U;

	LTRACEF("entry\n");

	ret = aes_unwrap_arg_check(ac);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(ac->started)) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE,
				     CCC_ERROR_MESSAGE("AES operation context already started\n"));
	}

	ac->update_ctr = false;

	/* Nothing written to memory with unwrap to keyslot
	 */
	ac->flush_dst_size = 0U;

	ret = aes_unwrap_start_check_algorithm(ac);
	CCC_ERROR_CHECK(ret);

	/* Setup SE engine parameters for AES key unwrap. */
	ret = engine_aes_op_mode_t19x_unwrap(ac->econtext, ac->econtext->algorithm, upper_quad);
	CCC_ERROR_CHECK(ret);

	if (TE_AES_MODE_USES_IV(ac->econtext->algorithm)) {
		ret = aes_unwrap_iv_setup(ac->econtext, upper_quad);
		CCC_ERROR_CHECK(ret);
	}

	/* Callwlate the number of blocks written to the engine and write the
	 * SE engine SE_CRYPTO_LAST_BLOCK_0
	 */
	ret = engine_aes_set_block_count(ac->econtext, ac->input.input_size, &ac->num_blocks);
	CCC_ERROR_CHECK(ret);

#if SE_RD_DEBUG
	LTRACEF("[num_blocks %u (bytes %u) (src= %p, dst=%p), algo=0x%x]\n",
		ac->num_blocks, ac->input.input_size, ac->input.src,
		ac->input.dst, ac->econtext->algorithm);
#endif

	paddr_src_addr = DOMAIN_VADDR_TO_PADDR(ac->econtext->domain, ac->input.src);
	SE_PHYS_RANGE_CHECK(paddr_src_addr, ac->input.input_size);

	SE_CACHE_FLUSH((VIRT_ADDR_T)ac->input.src, DMA_ALIGN_SIZE(ac->input.input_size));

	/* Program input message address. */
	tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_IN_ADDR_0, (uint32_t)paddr_src_addr);

	/* Program input message buffer size into SE0_AES0_IN_ADDR_HI_0_SZ and
	 * 8-bit MSB of 40b dma addr into MSB field
	 */
	se_config_reg = 0U;
	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, IN_ADDR_HI, SZ, ac->input.input_size,
		se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, IN_ADDR_HI, MSB,
		SE_GET_32_UPPER_PHYSADDR_BITS(paddr_src_addr),
		se_config_reg);
	tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_IN_ADDR_HI_0, se_config_reg);

	/* Program output address; 0 with zero bytes */
	tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_OUT_ADDR_0, 0U);

	se_config_reg = 0U;
	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, OUT_ADDR_HI, SZ, 0U,
		se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_NUM(
		SE0_AES0, OUT_ADDR_HI, MSB,
		0U,
		se_config_reg);
	tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_OUT_ADDR_HI_0, se_config_reg);

	AES_ERR_CHECK(ac->econtext->engine, "AES UNWRAP START");

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
	ac->gen_op_perf_usec = GET_USEC_TIMESTAMP;
#endif

	/**
	 * Issue START command in SE_OPERATION.OP
	 */
	ret = tegra_start_se0_operation_locked(ac->econtext->engine, 0U);
	CCC_ERROR_CHECK(ret);

	ac->started = true;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_unwrap_clear_keyslot_locked(async_aes_ctx_t *ac,
						status_t prev_ret)
{
	status_t ret = prev_ret;

	LTRACEF("entry\n");

	if ((BOOL_IS_TRUE(ac->use_preset_key) || BOOL_IS_TRUE(ac->key_written)) &&
	    !BOOL_IS_TRUE(ac->leave_key_to_keyslot)) {
		status_t fret = tegra_se_clear_aes_keyslot_locked(
			ac->econtext->engine, ac->econtext->se_ks.ks_slot, true);
		if (NO_ERROR != fret) {
			LTRACEF("Failed to clear engine %u AES keyslot %u\n",
				ac->econtext->engine->e_id, ac->econtext->se_ks.ks_slot);
			if (NO_ERROR == ret) {
				ret = fret;
			}
			/* FALLTHROUGH */
		}
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_unwrap_locked_finish(async_aes_ctx_t *ac)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = se_wait_engine_free(ac->econtext->engine);
	CCC_ERROR_CHECK(ret);

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
	if (0U != ac->gen_op_perf_usec) {
		show_se_gen_op_entry_perf("AES", get_elapsed_usec(ac->gen_op_perf_usec));
	}
#endif

	AES_ERR_CHECK(ac->econtext->engine, "AES DONE");

	if (BOOL_IS_TRUE(ac->update_ctr)) {
		ret = se_update_linear_counter(ac->econtext, ac->num_blocks);
		CCC_ERROR_CHECK(ret);
	}

	if (TE_AES_MODE_USES_IV(ac->econtext->algorithm)) {
		ret = aes_read_iv_locked(
			ac->econtext->engine,
			ac->econtext->se_ks.ks_slot,
			SE_AES_KSLOT_SEL_UPDATED_IV,
			&ac->econtext->iv_stash[0]);
		CCC_ERROR_CHECK(ret);

		LOG_HEXBUF("Unwrap IV saved", (const uint8_t *)&ac->econtext->iv_stash[0], 16U);
	}

fail:
	ac->started = false;

	if (BOOL_IS_TRUE(ac->update_ctr)) {
		/* Restore old content of the SPARE register, if saved in start */
		if ((ac->econtext->aes_flags & AES_FLAGS_CTR_MODE_SWAP_ENDIAN) != 0U) {
			tegra_engine_set_reg(ac->econtext->engine, SE0_AES0_SPARE_0, ac->spare_reg_0);
		}
	}

	/* If key was committed and not flagged to leave in keyslot => clear slot
	 */
	ret = aes_unwrap_clear_keyslot_locked(ac, ret);
	/* FALLTHROUGH */

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_unwrap_handle_key_locked(async_aes_ctx_t *ac,
					     struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((econtext->aes_flags & AES_FLAGS_USE_PRESET_KEY) != 0U) {
		/* Commit the pre-existing keyslot */
		ac->use_preset_key = true;

		LTRACEF("Using AES unwrap key in kslot %u, bit size %u",
			econtext->se_ks.ks_slot,
			econtext->se_ks.ks_bitsize);
	} else {
		if (NULL == econtext->se_ks.ks_keydata) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("AES unwrap key undefined\n"));
		}

		LTRACEF("Setting AES unwrap key to kslot %u, bit size %u",
			econtext->se_ks.ks_slot,
			econtext->se_ks.ks_bitsize);

		/* commit keyslot; forces slot clear even if write fails */
		ac->key_written = true;

		ret = aes_write_key_iv_locked(
			econtext->engine, econtext->se_ks.ks_slot,
			econtext->se_ks.ks_bitsize,
			SE_AES_KSLOT_SEL_KEY, econtext->se_ks.ks_keydata);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Map input length to either 128 bit or 256 bit key size unwrapped
 * from the input_params->src
 *
 * No padding algorithms are supported (no requirement), so the
 * 128 bit key is unwrapped from 16 byte input and 256 bit keys
 * are unwrapped from 32 byte input.
 *
 * 192 bit keys are not directly supported (no requirement).
 * But a 256 bit key can be used like 192 bit key and
 * a client can just pad a 192 bit key to 32 bytes and
 * cipher it; when unwrapped the keyslot can be used for AES-192
 * in T18X/T19X.
 *
 * The above will not work in T23X, since the key manifest contains
 * key size attributes and the key can only be used according
 * to these attributes.
 */
static status_t aes_unwrap_map_input_to_keysize(struct se_data_params *input_params,
						bool *long_key_p)
{
	status_t ret = ERR_ILWALID_ARGS;

	LTRACEF("entry\n");

	if (NULL == input_params) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*long_key_p = false;

	switch (input_params->input_size) {
	case SE_AES_BLOCK_LENGTH:
		ret = NO_ERROR;
		break;

	case (SE_AES_BLOCK_LENGTH * 2U):
		*long_key_p = true;
		ret = NO_ERROR;
		break;

	default:
		/* No action required */
		break;
	}
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t engine_aes_unwrap_locked(
	struct se_data_params *input_params,
	struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;
	bool large_key = false;
	bool leave_key = false;
	async_aes_ctx_t ac = { .magic = 0U };

	LTRACEF("entry\n");

	ret = aes_unwrap_map_input_to_keysize(input_params, &large_key);
	CCC_ERROR_CHECK(ret);

	ret = aes_unwrap_handle_key_locked(&ac, econtext);
	CCC_ERROR_CHECK(ret);

	leave_key = ((econtext->aes_flags & AES_FLAGS_LEAVE_KEY_TO_KEYSLOT) != 0U);
	ac.leave_key_to_keyslot = (leave_key || large_key);

	CCC_OBJECT_ASSIGN(ac.input, *input_params);
	ac.econtext = econtext;

	/* Unwrap lower quad value first, it is 128 bits.
	 */
	ac.input.input_size = SE_AES_BLOCK_LENGTH;

	ret = aes_unwrap_locked_start(&ac, false);
	CCC_ERROR_CHECK(ret);

	ret = aes_unwrap_locked_finish(&ac);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(large_key)) {
		/* Write the upper quad of the target keyslot
		 */
		ac.input.src = &ac.input.src[SE_AES_BLOCK_LENGTH];
		ac.input.input_size = SE_AES_BLOCK_LENGTH;

		ret = aes_unwrap_locked_start(&ac, true);
		CCC_ERROR_CHECK(ret);

		ac.leave_key_to_keyslot = leave_key;

		ret = aes_unwrap_locked_finish(&ac);
		CCC_ERROR_CHECK(ret);
	}

fail:
	se_util_mem_set((uint8_t *)&ac, 0U, sizeof_u32(ac));

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_UNWRAP */
