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

#include <crypto_system_config.h>

#include <ccc_lwrm_drf.h>
#include <arse0.h>

#if HAVE_SE_KAC_KEYOP

#if ((CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_TYPE) != CCC_SOC_FAMILY_ID(CCC_SOC_FAMILY_T23X)))
#error "Platform does not support T23X key operations"
#endif

#include <include/tegra_se.h>
#include <tegra_se_gen.h>
#include <tegra_se_kac_intern.h>

/* Special case for mapping TE_ALG_KEYOP_KWUW based on the mode:
 * the algorithm is fixed to TE_ALG_KEYOP_KW (encrypt) or
 * TE_ALG_KEYOP_KUW (decrypt).
 */
static te_crypto_algo_t se_keyop_kwuw_algo_map(te_crypto_algo_t algo_arg,
					       bool is_encrypt)
{
	te_crypto_algo_t algo = algo_arg;

	if (TE_ALG_KEYOP_KWUW == algo) {
		if (BOOL_IS_TRUE(is_encrypt)) {
			algo = TE_ALG_KEYOP_KW;
			LTRACEF("KWUW switched to KW in encrypt\n");
		} else {
			algo = TE_ALG_KEYOP_KUW;
			LTRACEF("KWUW switched to KUW in decrypt\n");
		}
	}
	return algo;
}

/* Set values for the following two registers for the key operation:
 *
 * SE0_AES0_CONFIG_0
 * SE0_AES0_CRYPTO_CONFIG_0
 *
 * XXX extend to MACs later???
 *
 * se_ks is used for resolving the key slots used by the operations.
 */
static status_t se_aes_keytable_op_config(const engine_t *engine,
					  const struct se_key_slot *se_ks,
					  te_crypto_algo_t alg,
					  bool is_encrypt)
{
	status_t ret = NO_ERROR;
	uint32_t config_reg = 0U;
	uint32_t crypto_config_reg = 0U;
	bool crypto_config_modified = false;
	bool set_keytable_dst = false;
	te_crypto_algo_t algorithm = alg;

	// Written keyslot index (not with KW)
	uint32_t dst_index = ILWALID_KEYSLOT_INDEX;

	/* Key used in the keyop (not modified) */
	uint32_t key_index = ILWALID_KEYSLOT_INDEX;

	LTRACEF("entry\n");

	if (IS_ALG_KEY_WRAPPING(algorithm)) {
		/* Cipher key for the wrapping operation or target for the genkey
		 */
		key_index = se_ks->ks_slot;

		/* Map KWUW to KW or KUW based on operation mode
		 *
		 * TE_ALG_KEYOP_KWUW is not handled below because of
		 * this.
		 */
		algorithm = se_keyop_kwuw_algo_map(algorithm, is_encrypt);
	}

	/* INS ok with AES0, AES1 and SHA engines
	 *
	 * FIXME: If there is a difference for setting MAC and AES keys to keyslots =>
	 * need to add support for setting MAC keys (for SHA engines).
	 */
	switch (algorithm) {
	case TE_ALG_KEYOP_KW: /* Wrap keyslot to memory */

		/* SE0_AES0_CONFIG_0 */
		config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG, NOP,
						config_reg);
		config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG, AES_ENC,
						config_reg);
		config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DST, MEMORY,
						config_reg);
		config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_MODE, KW,
						config_reg);

		/* SE0_AES0_CRYPTO_CONFIG_0 */
		crypto_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CRYPTO_CONFIG,
						       KEY_INDEX, key_index,
						       crypto_config_reg);

		crypto_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CRYPTO_CONFIG,
						       KEY2_INDEX,
						       se_ks->ks_wrap_index,
						       crypto_config_reg);

		crypto_config_modified = true;
		break;


	case TE_ALG_KEYOP_KUW: /* Unwrap keyslot from memory to keyslot */

		/* SE0_AES0_CONFIG_0 */
		config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG, AES_DEC,
						config_reg);
		config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG, NOP,
						config_reg);
		config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DST, KEYTABLE,
						config_reg);
		config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_MODE, KW,
						config_reg);

		/* SE0_AES0_CRYPTO_CONFIG_0 */
		crypto_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, CRYPTO_CONFIG,
						       KEY_INDEX, key_index,
						       crypto_config_reg);
		set_keytable_dst = true;

		/* target key to unwrap to */
		dst_index = se_ks->ks_wrap_index;

		crypto_config_modified = true;
		break;

	case TE_ALG_KEYOP_GENKEY:
		/* Generate a key with RNG0 to keyslot */

		/* SE0_AES0_CONFIG_0 */
		config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG, NOP,
						config_reg);
		config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG, RNG,
						config_reg);
		config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DST, KEYTABLE,
						config_reg);
		set_keytable_dst = true;

		/* target key to GENKEY to */
		dst_index = se_ks->ks_slot; // XXXX ks_slot is written. Maybe use e.g. ks_wrap_index for written reg?
		break;

	case TE_ALG_KEYOP_INS:
		/* Insert a key to keyslot */

		/* SE0_AES0_CONFIG_0 */
		config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, DEC_ALG, NOP,
						config_reg);
		config_reg = LW_FLD_SET_DRF_DEF(SE0_AES0, CONFIG, ENC_ALG, INS,
						config_reg);
		set_keytable_dst = true;

		/* target key to INS to */
		dst_index = se_ks->ks_slot; // XXXX ks_slot is written. Maybe use e.g. ks_wrap_index for written reg?
		break;

	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(set_keytable_dst)) {
		uint32_t keytable_dst = 0U;

		keytable_dst = LW_DRF_NUM(SE0_AES0, CRYPTO_KEYTABLE_DST,
					  KEY_INDEX, dst_index);

		tegra_engine_set_reg(engine, SE0_AES0_CRYPTO_KEYTABLE_DST_0, keytable_dst);
	}

	tegra_engine_set_reg(engine, SE0_AES0_CONFIG_0, config_reg);

	if (BOOL_IS_TRUE(crypto_config_modified)) {
		tegra_engine_set_reg(engine, SE0_AES0_CRYPTO_CONFIG_0, crypto_config_reg);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

// XXX TODO: Shared with HW GCM once implemented (move)
static status_t se_write_gcm_tag(const engine_t *engine,
				 const uint8_t *cvalue)
{
	status_t ret = NO_ERROR;
	uint32_t inx = 0U;
	uint32_t kd = 0u;

	LTRACEF("entry\n");

	for (inx = 0U; inx < SE_AES_BLOCK_LENGTH; inx += BYTES_IN_WORD) {
		if (NULL != cvalue) {
			kd = se_util_buf_to_u32(&cvalue[inx], CFALSE);
		}

		tegra_engine_set_reg(engine,
				     SE0_AES0_CRYPTO_TAG_0 + inx,
				     kd);
	}

	FI_BARRIER(u32, inx);

	if (SE_AES_BLOCK_LENGTH != inx) {
		CCC_ERROR_WITH_ECODE(ERR_BAD_STATE);
	}

fail:
	LTRACEF("exit: %u\n", ret);
	return ret;
}

static status_t kuw_write_to_keytable(const engine_t *engine, uint32_t kuw_manifest,
				      const uint8_t *kuw_wkey, uint32_t target_kslot)
{
	status_t ret = NO_ERROR;

	/* Max length of kuw_wkey data */
	const uint32_t key_words = 256U / BITS_IN_WORD;

	LTRACEF("entry\n");

	if (NULL == kuw_wkey) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Write the keytable manifest register */
	tegra_engine_set_reg(engine, SE0_AES0_CRYPTO_KEYTABLE_KEYMANIFEST_0, kuw_manifest);

	/* Write the wrapped key (including the zero padding if any) to the KEYBUF
	 */
	ret = se_write_keybuf(engine, target_kslot, kuw_wkey, key_words);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_aes_set_keyop_input(struct se_data_params *input_params,
				       const struct se_engine_aes_context *econtext,
				       PHYS_ADDR_T *src_paddr_p)
{
	status_t ret = NO_ERROR;
	uint32_t memory_input_size = 0U;
	uint32_t num_blocks = 0U;
	uint32_t se_config_reg = 0U;
	PHYS_ADDR_T paddr_input = PADDR_NULL;

	LTRACEF("entry\n");

	*src_paddr_p = PADDR_NULL;

	if (NULL != input_params->src) {
		/* Setup memory input for keyop
		 */
		memory_input_size = input_params->input_size;

		paddr_input = DOMAIN_VADDR_TO_PADDR(econtext->domain, input_params->src);
		SE_PHYS_RANGE_CHECK(paddr_input, memory_input_size);
		SE_CACHE_FLUSH((VIRT_ADDR_T)input_params->src, memory_input_size);

		if (!IS_40_BIT_PHYSADDR(paddr_input)) {
			CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
					     LTRACEF("In address out of SE 40 bit range\n"));
		}
	}

	/* KW/KUW adds one "implicit block" which contains bits
	 * (key_manifest || 0^96) as AAD prefix.
	 *
	 * Any memory input AAD is appended after that.
	 *
	 * So for KW/KUW the input_size is never zero, the byte count includes
	 * always one block, the implicit 128 bits.
	 */
	{
		uint32_t work_bsize = 0U;
		CCC_SAFE_ADD_U32(work_bsize, memory_input_size, SE_AES_BLOCK_LENGTH);

		ret = engine_aes_set_block_count(econtext, work_bsize, &num_blocks);
		CCC_ERROR_CHECK(ret);
	}

	(void)num_blocks;

	/* Key wrap algos have the additional manifest block included
	 * in the last_block value =>
	 *  0 means <manifest_block> (0 AAD bytes)
	 *  1 means <manifest_block> + <1..16 AAD bytes>
	 *  2 means <manifest_block> + <17..31 AAD bytes>
	 *  ...
	 */
	LTRACEF("CCC KW/KUW num_blocks returned %u\n", num_blocks);

	*src_paddr_p = paddr_input;

	/* Program input message address (low 32 bits) */
	tegra_engine_set_reg(econtext->engine, SE0_AES0_IN_ADDR_0,
			     se_util_lsw_u64((uint64_t)paddr_input));

	/* Program input message buffer (memory input part) byte size (24 bits) into
	 * SE0_AES0_IN_ADDR_HI_0_SZ and 8-bit MSB of 40b
	 * dma addr into MSB field.
	 */
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, IN_ADDR_HI, SZ,
					   memory_input_size, se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, IN_ADDR_HI, MSB,
					   SE_GET_32_UPPER_PHYSADDR_BITS(paddr_input),
					   se_config_reg);

	/* Set IN_ADDR_HI (size and paddr) */
	tegra_engine_set_reg(econtext->engine, SE0_AES0_IN_ADDR_HI_0, se_config_reg);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_aes_set_keyop_output(struct se_data_params *input_params,
					const struct se_engine_aes_context *econtext,
					uint32_t minimum_output_size,
					PHYS_ADDR_T *dst_paddr_p)
{
	status_t ret = NO_ERROR;
	uint32_t output_size = input_params->output_size;
	uint32_t se_config_reg = 0U;
	PHYS_ADDR_T paddr_output = PADDR_NULL;

	*dst_paddr_p = PADDR_NULL;

	/* If output committed, large enough output buffer must be provided.
	 */
	if ((NULL == input_params->dst) || (input_params->output_size < minimum_output_size)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Keyop[0x%x] output buffer invalid for the request\n",
					     econtext->algorithm));
	}

	// XXX potentially cache unaligned values used!!!

	/* Setup memory output for keyop
	 */
	paddr_output = DOMAIN_VADDR_TO_PADDR(econtext->domain, input_params->dst);
	SE_PHYS_RANGE_CHECK(paddr_output, output_size);
	SE_CACHE_FLUSH((VIRT_ADDR_T)input_params->dst, output_size);

	if (!IS_40_BIT_PHYSADDR(paddr_output)) {
		CCC_ERROR_WITH_ECODE(ERR_NOT_VALID,
				     LTRACEF("Out address out of SE 40 bit range\n"));
	}

	*dst_paddr_p = paddr_output;

	/* Program output message address (low 32 bits) */
	tegra_engine_set_reg(econtext->engine, SE0_AES0_OUT_ADDR_0,
			     se_util_lsw_u64((uint64_t)paddr_output));

	/* Program output message buffer byte size (24 bits) into
	 * SE0_AES0_OUT_ADDR_HI_0_SZ and 8-bit MSB of 40b
	 * dma addr into MSB field.
	 */
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, OUT_ADDR_HI, SZ,
					   output_size, se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_AES0, OUT_ADDR_HI, MSB,
					   SE_GET_32_UPPER_PHYSADDR_BITS(paddr_output),
					   se_config_reg);

	/* Set OUT_ADDR_HI (size and paddr) */
	tegra_engine_set_reg(econtext->engine, SE0_AES0_OUT_ADDR_HI_0, se_config_reg);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_keyop_process_check_args(
	const struct se_data_params *input_params,
	const struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (IS_ALG_KEY_WRAPPING(econtext->algorithm)) {
		if (NULL == input_params) {
			/* Either output buffer or wrapped key provided here
			 */
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     CCC_ERROR_MESSAGE("Key wrapping [0x%x] require data param",
							       econtext->algorithm));
		}

#if SE_RD_DEBUG
		if (!BOOL_IS_TRUE(econtext->is_encrypt)) {
			DUMP_DATA_PARAMS("Key unwrapping data =>", 0x3U, input_params);
		}
#endif

		/* Key wrappings with AES engines
		 */
		if (econtext->se_ks.ks_slot >= SE_AES_MAX_KEYSLOTS) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("Invalid SE key index %u for keyop\n",
						     econtext->se_ks.ks_slot));
		}

		if (econtext->se_ks.ks_wrap_index >= SE_AES_MAX_KEYSLOTS) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("Invalid SE key index %u to wrap\n",
						     econtext->se_ks.ks_wrap_index));
		}
	} else if (IS_ALG_GENKEY(econtext->algorithm)) {
		/* GENKEY only with AES0 engine
		 */
		if (CCC_ENGINE_SE0_AES0 != econtext->engine->e_id) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("Keygen supported with AES0 engine only\n"));
		}
	} else {
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     LTRACEF("Unsupported keyop algorithm 0x%x\n",
					     econtext->algorithm));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_run_aes_keyop(const engine_t *engine,
				 uint32_t se_op_reg_preset)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	// XXX TODO: maybe check it is AES* engine?

	AES_ERR_CHECK(engine, "KEYOP START");

	{
#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
		uint32_t gen_op_perf_usec = GET_USEC_TIMESTAMP;
#endif

		/**
		 * Issue START command in SE0_AES0_OPERATION.OP.
		 */
		ret = tegra_start_se0_operation_locked(engine, se_op_reg_preset);
		CCC_ERROR_CHECK(ret);

		/* Block while SE processes this chunk, then release mutex. */
		ret = se_wait_engine_free(engine);
		CCC_ERROR_CHECK(ret);

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
		if (0U != gen_op_perf_usec) {
			show_se_gen_op_entry_perf("KEYOP", get_elapsed_usec(gen_op_perf_usec));
		}
#endif
	}

	AES_ERR_CHECK(engine, "KEYOP DONE");

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t setup_aes_keyops_ks(struct se_engine_aes_context *econtext,
				    uint32_t manifest_sw,
				    bool set_operation_key,
				    bool write_tag,
				    bool manifest_ctrl)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(set_operation_key)) {
		/* Set the cipher KEY1 used in the algorithm if
		 * required and not using a pre-set key.
		 *
		 * If done => this must be first so it does not
		 * overwrite the later settings.
		 *
		 * This call must use the original algo, not the
		 * mapped. Because the econtext->algorithm is mapped
		 * to key purpose and key purpose must be KWUW for
		 * keys used for both KW and KUW.
		 */
		ret = se_set_engine_kac_keyslot_locked(econtext->engine,
						       econtext->se_ks.ks_slot,
						       econtext->se_ks.ks_keydata,
						       econtext->se_ks.ks_bitsize,
						       SE_KAC_KEYSLOT_USER,
						       SE_KAC_FLAGS_NONE,
						       manifest_sw,
						       econtext->algorithm);
		CCC_ERROR_CHECK(ret);
	}

	if (IS_ALG_KEY_WRAPPING(econtext->algorithm)) {
		ret = se_write_linear_counter(econtext->engine, &econtext->kwuw.counter[0]);
		CCC_ERROR_CHECK(ret);
	}

	if (BOOL_IS_TRUE(write_tag)) {
		/* Unwrap does AES-GCM decipher; needs a valid TAG
		 */
		ret = se_write_gcm_tag(econtext->engine, &econtext->kwuw.tag[0]);
		CCC_ERROR_CHECK(ret);
	}

	/* Configure the selected SE AES engine to perform the keyop
	 */
	ret = se_aes_keytable_op_config(econtext->engine,
					&econtext->se_ks,
					econtext->algorithm,
					econtext->is_encrypt);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(manifest_ctrl)) {
		/* Setup manifest and cntrl registers, but do not write keybuf */
		ret = kac_setup_keyslot_data_locked(econtext->engine,
						    &econtext->se_ks,
						    false);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t setup_aes_keyop_mem(struct se_data_params *input_params,
				    const struct se_engine_aes_context *econtext,
				    bool mem_input,
				    uint32_t memory_output_minimum_size)
{
	status_t ret = NO_ERROR;
	PHYS_ADDR_T paddr_src_addr = PADDR_NULL;
	PHYS_ADDR_T paddr_dst_addr = PADDR_NULL;

	LTRACEF("entry\n");

	if (BOOL_IS_TRUE(mem_input)) {
		ret = se_aes_set_keyop_input(input_params, econtext, &paddr_src_addr);
		CCC_ERROR_CHECK(ret);
	}

	if (memory_output_minimum_size > 0U) {
		ret = se_aes_set_keyop_output(input_params, econtext, memory_output_minimum_size,
					      &paddr_dst_addr);
		CCC_ERROR_CHECK(ret);
	} else {
		/* Set OUT_ADDR_HI (size and paddr) for no output */
		tegra_engine_set_reg(econtext->engine, SE0_AES0_OUT_ADDR_0, 0U);
		tegra_engine_set_reg(econtext->engine, SE0_AES0_OUT_ADDR_HI_0, 0U);

		input_params->dst = NULL;
		input_params->output_size = 0U;
	}

#if HAVE_SE_APERTURE
	ret = se_set_aperture(paddr_src_addr, input_params->input_size,
			      paddr_dst_addr, input_params->output_size);
	CCC_ERROR_CHECK(ret);
#endif

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_setup_aes_keyop(struct se_data_params *input_params,
				   struct se_engine_aes_context *econtext,
				   uint32_t manifest_sw)
{
	status_t ret = NO_ERROR;
	uint32_t memory_output_minimum_size = 0U;

	bool manifest_ctrl = false;
	bool set_operation_key = false;
	bool mem_input = false;
	bool write_tag = false;
	bool write_keytable = false;

	LTRACEF("entry\n");

	switch (se_keyop_kwuw_algo_map(econtext->algorithm, econtext->is_encrypt)) {
	case TE_ALG_KEYOP_KW:
		set_operation_key =
			((econtext->aes_flags & AES_FLAGS_USE_PRESET_KEY) == 0U);

		manifest_ctrl = true;
		mem_input = true;
		memory_output_minimum_size = SE_KEY_WRAP_MEMORY_BYTE_SIZE;
		break;

	case TE_ALG_KEYOP_KUW:
		LOG_HEXBUF("KUW COUNTER", (const uint8_t *)&econtext->kwuw.counter[0], 16);

		set_operation_key =
			((econtext->aes_flags & AES_FLAGS_USE_PRESET_KEY) == 0U);

		mem_input = true;
		write_tag = true;
		write_keytable = true;
		/* IAS (5.1.3.5 KW/KUW states that one byte will be written to output
		 * as KUW status:
		 * OK   => 0x00000000
		 * FAIL => 0xBDBDBDBD
		 */
		memory_output_minimum_size = 1U;
		break;

	case TE_ALG_KEYOP_GENKEY:
		manifest_ctrl = true;
		break;

	case TE_ALG_KEYOP_INS:
		set_operation_key = true;
		manifest_ctrl = true;
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);

	ret = setup_aes_keyops_ks(econtext, manifest_sw, set_operation_key,
				  write_tag, manifest_ctrl);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(write_keytable)) {
		LOG_INFO("KOP KUW using blob manifest value: 0x%x\n",
			 econtext->kwuw.manifest);
		LOG_HEXBUF("KOP KUW using wkey\n",
			   &econtext->kwuw.wkey[0], 32U);

		ret = kuw_write_to_keytable(econtext->engine,
					    econtext->kwuw.manifest,
					    &econtext->kwuw.wkey[0],
					    econtext->se_ks.ks_wrap_index);
		CCC_ERROR_CHECK(ret);
	}

	ret = setup_aes_keyop_mem(input_params, econtext, mem_input, memory_output_minimum_size);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* If oret is set on entry => that is returned.
 *  Otherwise if error on keyslot clear, that is returned.
 *
 * Clean keyslot only if clear_keyslot is set.
 *
 * Clear input_params->dst if kuw_op_status_set is true, as it was set to
 * internal buffer in that case by caller.
 */
static status_t aes_keyop_process_cleanup(struct se_data_params *input_params,
					  const struct se_engine_aes_context *econtext,
					  bool clear_keyslot,
					  bool kuw_op_status_set,
					  status_t oret)
{
	status_t ret = oret;

	if (BOOL_IS_TRUE(kuw_op_status_set)) {
		input_params->dst = NULL;
		input_params->output_size = 0U;
	}

	if (BOOL_IS_TRUE(clear_keyslot)) {
		status_t fret = tegra_se_clear_aes_keyslot_locked(econtext->engine,
								  econtext->se_ks.ks_slot,
								  true);
		if (NO_ERROR != fret) {
			LTRACEF("Failed to clear engine %u AES keyslot %u\n",
				econtext->engine->e_id, econtext->se_ks.ks_slot);
			if (NO_ERROR == ret) {
				ret = fret;
			}
			/* FALLTHROUGH */
		}
	}

	return ret;
}

#define KUW_STATUS_BYTE_SUCCESS_VALUE	0x00000000U
#define KUW_STATUS_BYTE_ERROR_VALUE	0xBDBDBDBDU

/* Check the HW stored status value from KUW op in memory */
static status_t kuw_result_state_check(const struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t op_status = se_util_buf_to_u32(econtext->kwuw.wbuf, false);

	LTRACEF("entry\n");

	switch (op_status) {
	case KUW_STATUS_BYTE_SUCCESS_VALUE:
		break;
	case KUW_STATUS_BYTE_ERROR_VALUE:
		/* error status value: TAG/LOCK/KAC error */
		ret = SE_ERROR(ERR_NOT_VALID);
		break;
	default:
		/* unspecified status value */
		ret = SE_ERROR(ERR_BAD_STATE);
		break;
	}
	CCC_ERROR_CHECK(ret,
			LTRACEF("Key unwrap status failed [0x%x], ret %d\n",
				op_status, ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_keyop_kwuw_prepare(struct se_data_params *input_params,
				       struct se_engine_aes_context *econtext,
				       bool *kuw_op_status_set_p)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	/* Interpretation of the LAST_BLOCK field changed in
	 * HW: The value includes the implicit zero padded
	 * manifest block and thus value 0 (which implies 1
	 * block) now only passes the implicit manifest block
	 * and no extra AAD data to the KW/KUW operations.
	 *
	 * This means that when client does not pass AAD values
	 * CCC no longer needs to push the "non_key_manifest" zero block
	 * to the HW operation.
	 */
	if ((NULL == input_params->src) || (0U == input_params->input_size)) {
		input_params->src = NULL;
		input_params->input_size = 0U;
	}

	if (!BOOL_IS_TRUE(econtext->is_encrypt)) {
		/* An arbitrary value to init wbuf
		 * with (e.g. "FAIL" in ASCII)
		 */
		se_util_u32_to_buf(0x4641494LW, econtext->kwuw.wbuf, false);

		/* Deciphering KW blob (KUW, KWUW) => status is written
		 * to memory by HW engine as a 32 bit value.
		 */
		input_params->dst = econtext->kwuw.wbuf;
		input_params->output_size = sizeof_u32(uint32_t);

		*kuw_op_status_set_p = true; /**< encrypting not set => KUW operation */
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t aes_keyop_process_locked(struct se_data_params *input_params,
					 struct se_engine_aes_context *econtext,
					 uint32_t manifest_sw)
{
	/* FAIL by default. Used as a local value, avoid misra grunts making static.
	 */
	status_t ret = NO_ERROR;
	bool kuw_op_status_set = false;
	bool clear_keyslot = false; /**< True => clear keyslot unless
				     * prevented by caller
				     */

	LTRACEF("entry\n");

	LTRACEF("[KEYOP (0x%x)]\n", econtext->algorithm);

	if (NULL == econtext->kwuw.wbuf) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("KUW work buffer NULL\n"));
	}

	if (IS_ALG_KEY_WRAPPING(econtext->algorithm)) {
		ret = aes_keyop_kwuw_prepare(input_params, econtext, &kuw_op_status_set);
		CCC_ERROR_CHECK(ret);
	}

	LOG_HEXBUF("AAD kwuw value", input_params->src, input_params->input_size);

	/* After this point clear the keyslot unless caller wants to keep it */
	clear_keyslot = ((econtext->aes_flags & AES_FLAGS_LEAVE_KEY_TO_KEYSLOT) == 0U);

	ret = se_setup_aes_keyop(input_params, econtext, manifest_sw);
	CCC_ERROR_CHECK(ret);

	/* Per SE IAS the INIT and FINAL field should be filled with 0 for pass the
	 * task configuration legitimacy check.
	 */
	ret = se_run_aes_keyop(econtext->engine, 0U);
	CCC_ERROR_CHECK(ret);

	if ((NULL != input_params->dst) && (0U != input_params->output_size)) {
		/* This ilwalidation MUST BE LEFT IN PLACE since the CPU may
		 * prefetch the data while the SE AES DMA writes the phys mem
		 * values causing inconsistencies.
		 *
		 * Without this ilwalidate the prefetched cache content may
		 * cause the result to be incorrect when the CPU reads the
		 * cache content and the DMA wrrites to phys mem if caches
		 * are not snooped.
		 */
		SE_CACHE_ILWALIDATE((VIRT_ADDR_T)input_params->dst, input_params->output_size);
	}

	if (BOOL_IS_TRUE(kuw_op_status_set)) {
		ret = kuw_result_state_check(econtext);
		CCC_ERROR_CHECK(ret);
	}

fail:
	/* Does not clear ret if set on entry */
	ret = aes_keyop_process_cleanup(input_params, econtext,
					clear_keyslot, kuw_op_status_set,
					ret);
	/* FALLTHROUGH */

	LTRACEF("exit: %d\n", ret);
	return ret;
}

// KEYOPS are key manipulation operations which always completes in a single HW TASK
//
// Keyops use AES context because it is handy, even though this is not necessarily
// an AES operation (e.g. INS).
//
// XXX maybe add a keyop econtext for this... OTOH, KW/KUW use AES-GCM which requirs AES context.
//
// TODO: kac_engine_write_keyslot_locked() also handles INS => should unify.
//
status_t engine_aes_keyop_process_locked(
	struct se_data_params *input_params,
	struct se_engine_aes_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t manifest_sw = 0U;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	ret = aes_keyop_process_check_args(input_params, econtext);
	CCC_ERROR_CHECK(ret);

	if (!BOOL_IS_TRUE(econtext->is_first) || !BOOL_IS_TRUE(econtext->is_last)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("KEYOP 0x%x not completed in this task",
					     econtext->algorithm));
	}

	ret = aes_keyop_process_locked(input_params, econtext, manifest_sw);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_KAC_KEYOP */
