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

/* T23X HW support for one and two key key derivations and HMAC-SHA
 * => "SHA operations with a key"
 */
#include <ccc_lwrm_drf.h>
#include <arse0.h>

#if HAVE_SE_KAC_KDF || HAVE_HW_HMAC_SHA

#include <include/tegra_se.h>
#include <tegra_se_gen.h>
#include <tegra_se_kac_intern.h>

/* These must match for correct operation; uses tegra_se_sha.c
 */
#define HMAC_HW_DST_MEMORY SHA_DST_MEMORY

/* Max intermediate context and result size of SHA-512 is 64 bytes */
#define MAX_HMAC_SHA2_ALIGNED_SIZE (SHA_CONTEXT_STATE_BITS / 8U)

#if HAVE_SE_KAC_KDF
/* HW only supports HMAC_SHA256_1KEY and HMAC_SHA256_1KEY.
 * Caller checks this.
 */
static uint32_t se_kdf_algo_to_pkt_mode(te_crypto_algo_t algo)
{
	uint32_t pkt_mode = SE_MODE_PKT_KDFMODE_HMAC_SHA256_1KEY;

	if (algo == TE_ALG_KEYOP_KDF_2KEY) {
		pkt_mode = SE_MODE_PKT_KDFMODE_HMAC_SHA256_2KEY;
	}

	return pkt_mode;
}
#endif /* HAVE_SE_KAC_KDF */

#if HAVE_HW_HMAC_SHA
/* HW only supports HMAC with SHA-2 algos. Caller checks this.
 */
static uint32_t se_hmac_sha2_algo_to_pkt_mode(te_crypto_algo_t algo)
{
	uint32_t pkt_mode = 0U;

	switch(algo) {
	case TE_ALG_HMAC_SHA224:
		pkt_mode = SE_MODE_PKT_HMACMODE_SHA224;
		break;
	case TE_ALG_HMAC_SHA256:
		pkt_mode = SE_MODE_PKT_HMACMODE_SHA256;
		break;
	case TE_ALG_HMAC_SHA384:
		pkt_mode = SE_MODE_PKT_HMACMODE_SHA384;
		break;
	case TE_ALG_HMAC_SHA512:
		pkt_mode = SE_MODE_PKT_HMACMODE_SHA512;
		break;
	default:
		/* not HMAC_SHA mode */
		break;
	}

	return pkt_mode;
}
#endif /* HAVE_HW_HMAC_SHA */

/* Set values for the following two registers for the key operation:
 *
 * SE0_SHA_CONFIG_0
 * SE0_SHA_CRYPTO_CONFIG_0
 *
 * se_ks is used for resolving the key slots used by the operations.
 *
 * XXX FIXME: Extend to support HMAC with the HW engine
 */
static status_t se_sha_keytable_op_config(const engine_t *engine,
					  const struct se_key_slot *se_ks,
					  te_crypto_algo_t alg)
{
	status_t ret = NO_ERROR;
	uint32_t config_reg = 0U;
	uint32_t crypto_config_reg = 0U;
	bool crypto_config_modified = false;
	bool set_keytable_dst = false;
	te_crypto_algo_t algorithm = alg;
	uint32_t pkt_mode = 0U;

	// XXXXXXXXXXXXXX TODO: Check if need big/little endian support

	/* Written keyslot index */
	uint32_t dst_index = ILWALID_KEYSLOT_INDEX;

	/* Key used in the keyop (not modified) */
	uint32_t key_index = ILWALID_KEYSLOT_INDEX;
#if HAVE_SE_KAC_KDF
	uint32_t key2_index = ILWALID_KEYSLOT_INDEX;
#endif

	LTRACEF("entry\n");

	switch (algorithm) {
	case TE_ALG_HMAC_SHA224:
	case TE_ALG_HMAC_SHA256:
	case TE_ALG_HMAC_SHA384:
	case TE_ALG_HMAC_SHA512:
	case TE_ALG_KEYOP_KDF_1KEY:
		key_index = se_ks->ks_slot;
		break;

#if HAVE_SE_KAC_KDF
	case TE_ALG_KEYOP_KDF_2KEY:
		key_index = se_ks->ks_slot;
		key2_index = se_ks->ks_subkey_slot;
		break;
#endif

	default:
		/* not a SHA keytable op, trapped below */
		break;
	}

	/* INS ok with AES0, AES1 and SHA engines
	 *
	 * FIXME: If there is a difference for setting MAC and AES keys to keyslots =>
	 * need to add support for setting MAC keys (for SHA engines).
	 */
	switch (algorithm) {
	case TE_ALG_HMAC_SHA224:
	case TE_ALG_HMAC_SHA256:
	case TE_ALG_HMAC_SHA384:
	case TE_ALG_HMAC_SHA512:
#if HAVE_HW_HMAC_SHA
		pkt_mode = se_hmac_sha2_algo_to_pkt_mode(algorithm);

		config_reg = LW_FLD_SET_DRF_DEF(SE0_SHA, CONFIG, ENC_ALG, HMAC,
						config_reg);
		config_reg = LW_FLD_SET_DRF_DEF(SE0_SHA, CONFIG, DEC_ALG, NOP,
						config_reg);

#if HMAC_HW_DST_MEMORY
		config_reg = LW_FLD_SET_DRF_DEF(SE0_SHA, CONFIG, DST,
						MEMORY, config_reg);
#else
		config_reg = LW_FLD_SET_DRF_DEF(SE0_SHA, CONFIG, DST,
						HASH_REG, config_reg);
#endif /* HMAC_HW_DST_MEMORY */

		config_reg = LW_FLD_SET_DRF_NUM(SE0_SHA, CONFIG, ENC_MODE, pkt_mode,
						config_reg);

		/* SE0_SHA_CRYPTO_CONFIG_0 */
		crypto_config_reg = LW_FLD_SET_DRF_NUM(SE0_SHA, CRYPTO_CONFIG,
						       KEY_INDEX, key_index,
						       crypto_config_reg);

		crypto_config_modified = true;
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif /* HAVE_HW_HMAC_SHA */
		break;

	case TE_ALG_KEYOP_KDF_1KEY:
	case TE_ALG_KEYOP_KDF_2KEY:
#if HAVE_SE_KAC_KDF
		pkt_mode = se_kdf_algo_to_pkt_mode(algorithm);

		config_reg = LW_FLD_SET_DRF_DEF(SE0_SHA, CONFIG, ENC_ALG, KDF,
						config_reg);
		config_reg = LW_FLD_SET_DRF_DEF(SE0_SHA, CONFIG, DEC_ALG, NOP,
						config_reg);
		config_reg = LW_FLD_SET_DRF_DEF(SE0_SHA, CONFIG, DST,
						KEYTABLE, config_reg);

		config_reg = LW_FLD_SET_DRF_NUM(SE0_SHA, CONFIG, ENC_MODE, pkt_mode,
						config_reg);

		/* SE0_SHA_CRYPTO_CONFIG_0 */
		crypto_config_reg = LW_FLD_SET_DRF_NUM(SE0_SHA, CRYPTO_CONFIG,
						       KEY_INDEX, key_index,
						       crypto_config_reg);

		if (TE_ALG_KEYOP_KDF_2KEY == algorithm) {
			crypto_config_reg = LW_FLD_SET_DRF_NUM(SE0_SHA, CRYPTO_CONFIG,
							       KEY2_INDEX, key2_index,
							       crypto_config_reg);
		}

		set_keytable_dst = true;

		/* target key to derive key to */
		dst_index = se_ks->ks_kdf_index;

		crypto_config_modified = true;
#else
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
#endif /* HAVE_SE_KAC_KDF */
		break;

	default:
		ret = SE_ERROR(ERR_NOT_SUPPORTED);
		break;
	}
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(set_keytable_dst)) {
		uint32_t keytable_dst = 0U;

		keytable_dst = LW_DRF_NUM(SE0_SHA, CRYPTO_KEYTABLE_DST,
					  KEY_INDEX, dst_index);

		tegra_engine_set_reg(engine, SE0_SHA_CRYPTO_KEYTABLE_DST_0, keytable_dst);
	}

	tegra_engine_set_reg(engine, SE0_SHA_CONFIG_0, config_reg);

	if (BOOL_IS_TRUE(crypto_config_modified)) {
		tegra_engine_set_reg(engine, SE0_SHA_CRYPTO_CONFIG_0, crypto_config_reg);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t setup_sha_op_keys(const struct se_engine_sha_context *econtext,
				  uint32_t manifest_sw,
				  bool set_operation_key,
				  bool set_operation_subkey)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	(void)set_operation_subkey;

	if (BOOL_IS_TRUE(set_operation_key)) {
		/* Setting KEY1 (e.g. KDK key)
		 *
		 * Set the cipher KEY1 used in the algorithm if
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
		te_crypto_algo_t key1_purpose_mapping_algo = econtext->algorithm;

		if (TE_ALG_KEYOP_KDF_2KEY == econtext->algorithm) {
			key1_purpose_mapping_algo = TE_ALG_KEYOP_KDF_1KEY;
		}

		/* Forcing constant USER for key and subkey (Set this in config file
		 * SE_KAC_KEYSLOT_USER: default is CCC_MANIFEST_USER_PSC
		 * which makes sense only for PSC !!!)
		 */
		ret = se_set_engine_kac_keyslot_locked(econtext->engine,
						       econtext->se_ks.ks_slot,
						       econtext->se_ks.ks_keydata,
						       econtext->se_ks.ks_bitsize,
						       SE_KAC_KEYSLOT_USER,		// XXXXX TODO: IMPROVE
						       SE_KAC_FLAGS_NONE,		// XXXXX TODO: IMPROVE
						       manifest_sw,
						       key1_purpose_mapping_algo);
		CCC_ERROR_CHECK(ret);
	}

#if HAVE_SE_KAC_KDF
	if (BOOL_IS_TRUE(set_operation_subkey)) {
		/* Setting KEY2 (subkey, e.g. KDD key)
		 */
		const uint8_t *subkey = &econtext->se_ks.ks_kdd_key[0];

		if (NULL == subkey) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     LTRACEF("KDD key not set\n"));
		}

		/* Set the cipher subkey (e.g. KDD key for KDF 2-KEY
		 * operation) used in the algorithm when required and
		 * not using a pre-set key.
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
						       econtext->se_ks.ks_subkey_slot,
						       subkey,
						       econtext->se_ks.ks_bitsize,	// XXXXX TODO: IMPROVE => support different KDK and KDD sizes
						       SE_KAC_KEYSLOT_USER,		// XXXXX TODO: IMPROVE
						       SE_KAC_FLAGS_NONE,		// XXXXX TODO: IMPROVE
						       manifest_sw,
						       econtext->algorithm);
		CCC_ERROR_CHECK(ret);
	}
#endif /* HAVE_SE_KAC_KDF */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t sha_keyop_algo_set_keys(const struct se_engine_sha_context *econtext,
					bool set_operation_key_arg,
					uint32_t manifest_sw,
					bool *mem_input_p)
{
	status_t ret = NO_ERROR;
	bool set_operation_key = set_operation_key_arg;
	bool set_operation_subkey = false;

	LTRACEF("entry\n");

	switch (econtext->algorithm) {
	case TE_ALG_HMAC_SHA224:
	case TE_ALG_HMAC_SHA256:
	case TE_ALG_HMAC_SHA384:
	case TE_ALG_HMAC_SHA512:
		set_operation_key =
			((econtext->sha_flags & SHA_FLAGS_USE_PRESET_KEY) == 0U);
		*mem_input_p = true;
		break;

	case TE_ALG_KEYOP_KDF_1KEY:
		break;

	case TE_ALG_KEYOP_KDF_2KEY:
		/* KDD key is set for 2KEY KDF operations
		 * if not using pre-existing key.
		 *
		 * XXX TODO: Support different "use pre-existing states" for KDK and KDD!!!
		 */
		set_operation_subkey =
			((econtext->sha_flags & SHA_FLAGS_USE_PRESET_KEY) == 0U);
		break;

	default:
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret,
			LTRACEF("Invalid algo 0x%x\n", econtext->algorithm));

	ret = setup_sha_op_keys(econtext, manifest_sw,
				set_operation_key, set_operation_subkey);
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_setup_sha_keyop(struct se_data_params *input_params,
				   const struct se_engine_sha_context *econtext,
				   uint32_t manifest_sw,
				   uint8_t *hmac_dst_aligned_memory)
{
	status_t ret = NO_ERROR;
	bool set_operation_key = false;
	bool manifest_ctrl = false;
	bool mem_input = false;
	struct se_key_slot target_key = { .ks_keydata = NULL };

	LTRACEF("entry\n");

#if HAVE_SE_KAC_KDF
	if (IS_ALG_KDF_KEY_DERIVATION(econtext->algorithm)) {
		/* KDK key is set for both 1KEY and 2KEY KDF operations
		 * if not using pre-existing key.
		 */
		set_operation_key =
			((econtext->sha_flags & SHA_FLAGS_USE_PRESET_KEY) == 0U);

		mem_input = true;
		manifest_ctrl = true;

		// KDF target keyslot
		target_key.ks_slot = econtext->se_ks.ks_kdf_index;

		target_key.ks_kac.k_manifest.mf_user    = econtext->kdf_mf_user;
		target_key.ks_kac.k_manifest.mf_purpose = econtext->kdf_mf_purpose;
		target_key.ks_kac.k_manifest.mf_size    = econtext->kdf_mf_keybits;
		target_key.ks_kac.k_manifest.mf_sw	= econtext->kdf_mf_sw;
	}
#endif

	ret = sha_keyop_algo_set_keys(econtext,
				      set_operation_key,
				      manifest_sw,
				      &mem_input);
	CCC_ERROR_CHECK(ret);

	/* Configure the (selected) SE SHA engine to perform the keyop
	 */
	ret = se_sha_keytable_op_config(econtext->engine,
					&econtext->se_ks,
					econtext->algorithm);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(manifest_ctrl)) {
		/* Setup manifest and cntrl registers, but do not write keybuf
		 *
		 * Manifest/ctrl for the DST key derived to keyslot
		 */
		ret = kac_setup_keyslot_data_locked(econtext->engine,
						    &target_key,
						    false);
		CCC_ERROR_CHECK(ret);
	}

	ret = se_set_sha_task_config(econtext);
	CCC_ERROR_CHECK(ret);

	/* This can be true even when input_params->input_size == 0,
	 * it just means that the SHA engine length fields must be programmed.
	 */
	if (BOOL_IS_TRUE(mem_input)) {
		uint8_t *dst_save = input_params->dst;
		uint32_t dst_size_save = input_params->output_size;

		/* hmac_dst_aligned_memory is NULL if output is to HASH_REGS
		 * or keyslots.
		 */
		input_params->dst = hmac_dst_aligned_memory;

#if HMAC_HW_DST_MEMORY
		if (NULL != hmac_dst_aligned_memory) {
			/* Fixed size intermediate buffer in this case */
			input_params->output_size = MAX_HMAC_SHA2_ALIGNED_SIZE;
		}
#endif /* HMAC_HW_DST_MEMORY */

		ret = se_sha_engine_set_msg_sizes(input_params, econtext);
		CCC_ERROR_CHECK(ret);

		ret = se_write_sha_engine_inout_regs(input_params, econtext);

		input_params->dst = dst_save;
		input_params->output_size = dst_size_save;

		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_run_sha_keyop(const engine_t *engine)
{
	status_t ret = NO_ERROR;
	uint32_t se_op_reg_preset = 0U;	// TODO: Remove if not required

	LTRACEF("entry\n");

	// XXX TODO: maybe check it is SHA engine?

	SHA_ERR_CHECK(engine, "KEYOP START");

	{
#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
		uint32_t gen_op_perf_usec = GET_USEC_TIMESTAMP;
#endif

		/**
		 * Issue START command in SE0_SHA_OPERATION.OP.
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

	SHA_ERR_CHECK(engine, "KEYOP DONE");

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t sha_keyop_process_check_args(
	const struct se_data_params *input_params,
	const struct se_engine_sha_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL == input_params) {
		/* Either output buffer or wrapped key provided here
		 */
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("SHA keyops (0x%x) require data param",
						       econtext->algorithm));
	}

	/* SHA keyops only with SHA engine
	 */
	if (CCC_ENGINE_SE0_SHA != econtext->engine->e_id) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Wrapping keyops supported with SHA engine only\n"));
	}

	if (econtext->se_ks.ks_slot >= SE_AES_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("Invalid SE key index %u for SHA keyop\n",
					     econtext->se_ks.ks_slot));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t se_sha_econtext_clear_keyslot_locked(const struct se_engine_sha_context *econtext,
						     uint32_t keyslot)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (keyslot < SE_AES_MAX_KEYSLOTS) {
		ret = se_set_engine_kac_keyslot_locked(econtext->engine, keyslot, NULL, 256U,
						       SE_KAC_KEYSLOT_USER, SE_KAC_FLAGS_NONE, 0U,
						       econtext->algorithm);
		CCC_ERROR_CHECK(ret);
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* hmac_aligned_memory_result is NULL when HMAC HW engine DST is not MEMORY.
 * In that case result in HASH REGISTERs
 */
static status_t sha_engine_keyop_result(struct se_engine_sha_context *econtext,
					uint8_t *hmac_aligned_memory_result)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	//
	// XXXX TODO: Check if HASH_RESULT_0 values are swapped also for HMAC-SHA2 like they are
	// XXXX  word swapped for SHA-2 operations that operate in 64 bit words!!!!
	//
	// TODO => if so set the boolean parameter True
	//
	se_sha_engine_process_read_result_locked(econtext,
						 hmac_aligned_memory_result,
						 false);

	LTRACEF("exit: %d\n", ret);
	return ret;
}


static status_t sha_keyop_process_locked(struct se_data_params *input_params,
					 struct se_engine_sha_context *econtext,
					 uint32_t manifest_sw,
					 uint8_t *hmac_dst_aligned_memory,
					 bool *key_check_p)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	LTRACEF("[KEYOP (0x%x)]\n", econtext->algorithm);

	// XXX FIXME: HMAC_SHA does not need to complete in this task => REWORK
	// to support OPERATIONS THAT KEEP CONTEX IN KEYSLOT
	//
	if (!BOOL_IS_TRUE(econtext->is_first) || !BOOL_IS_TRUE(econtext->is_last)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("KEYOP 0x%x not completed in this task",
					     econtext->algorithm));
	}

	if ((NULL == input_params->src) || (0U == input_params->input_size)) {
		input_params->src = NULL;
		input_params->input_size = 0U;
	}

	LOG_HEXBUF("SHA keyop input", input_params->src, input_params->input_size);

	if (BOOL_IS_TRUE(econtext->is_first)) {
		econtext->byte_count = 0U;
	}

	/* Track number of input bytes processed by the engine */
	econtext->byte_count += input_params->input_size;

	*key_check_p = true;

	ret = se_setup_sha_keyop(input_params, econtext, manifest_sw,
				 hmac_dst_aligned_memory);
	CCC_ERROR_CHECK(ret);

	ret = se_run_sha_keyop(econtext->engine);
	CCC_ERROR_CHECK(ret);

	/* Some keyops (e.g. HMAC-SHA2) require the result to be extracted
	 *
	 * Other kops write to KEYSLOT and result is not accessible to SW.
	 */
	switch (econtext->algorithm) {
	case TE_ALG_HMAC_SHA224:
	case TE_ALG_HMAC_SHA256:
	case TE_ALG_HMAC_SHA384:
	case TE_ALG_HMAC_SHA512:
		ret = sha_engine_keyop_result(econtext,
					      hmac_dst_aligned_memory);
		break;
	default:
		/* none */
		break;
	}
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t sha_keyop_cleanup_ks(const struct se_engine_sha_context *econtext,
				     status_t ret)
{
	status_t nret = ret;
	status_t fret = se_sha_econtext_clear_keyslot_locked(econtext,
							     econtext->se_ks.ks_slot);
	if (NO_ERROR != fret) {
		LTRACEF("Failed to clear engine %u SHA engine keyslot %u\n",
			econtext->engine->e_id, econtext->se_ks.ks_slot);
		if (NO_ERROR == nret) {
			nret = fret;
		}
		/* FALLTHROUGH */
	}

	// XXX TODO: Could clear if
	// ILWALID_KEYSLOT_INDEX != econtext->se_ks.ks_subkey_slot...
	// Now clear on algo match.
	//
	if (TE_ALG_KEYOP_KDF_2KEY == econtext->algorithm) {
		fret = se_sha_econtext_clear_keyslot_locked(econtext,
							    econtext->se_ks.ks_subkey_slot);
		if (NO_ERROR != fret) {
			LTRACEF("Failed to clear engine %u SHA engine keyslot %u\n",
				econtext->engine->e_id, econtext->se_ks.ks_slot);
			if (NO_ERROR == nret) {
				nret = fret;
			}
			/* FALLTHROUGH */
		}
	}

	return nret;
}

// KEYOPS are key manipulation operations which always completes in a single HW TASK (KDF)
// OR
// they are SHA engine operations with a KEYSLOT which maintain context inside keyslot
// (i.e. CONTEXT can not be exported, like the T23X HMAC-SHA with HW support).
//
// TODO: kac_engine_write_keyslot_locked() also handles INS => should unify.
//
status_t engine_sha_keyop_process_locked(
	struct se_data_params *input_params,
	struct se_engine_sha_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t manifest_sw = 0U;
	bool key_check = false;
	uint8_t *hmac_dst_aligned_memory = NULL;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	ret = sha_keyop_process_check_args(input_params, econtext);
	CCC_ERROR_CHECK(ret);

#if HMAC_HW_DST_MEMORY
	switch (econtext->algorithm) {
	case TE_ALG_HMAC_SHA224:
	case TE_ALG_HMAC_SHA256:
	case TE_ALG_HMAC_SHA384:
	case TE_ALG_HMAC_SHA512:
		hmac_dst_aligned_memory = CMTAG_MEM_GET_BUFFER(econtext->cmem,
							       CMTAG_ALIGNED_DMA_BUFFER,
							       CCC_DMA_MEM_ALIGN,
							       MAX_HMAC_SHA2_ALIGNED_SIZE);
		if (NULL == hmac_dst_aligned_memory) {
			ret = SE_ERROR(ERR_NO_MEMORY);
		}
		break;
	default:
		/* Other algos write to keyslots, not memory */
		break;
	}
	CCC_ERROR_CHECK(ret);
#endif

	ret = sha_keyop_process_locked(input_params, econtext,
				       manifest_sw, hmac_dst_aligned_memory,
				       &key_check);
	CCC_ERROR_CHECK(ret);

fail:
#if HMAC_HW_DST_MEMORY
	if (NULL != hmac_dst_aligned_memory) {
		se_util_mem_set(hmac_dst_aligned_memory, 0U,
				MAX_HMAC_SHA2_ALIGNED_SIZE);

		CMTAG_MEM_RELEASE(econtext->cmem,
				  CMTAG_ALIGNED_DMA_BUFFER,
				  hmac_dst_aligned_memory);
	}
#endif

	if (BOOL_IS_TRUE(key_check) &&
	    ((econtext->sha_flags & SHA_FLAGS_LEAVE_KEY_TO_KEYSLOT) == 0U)) {
		ret = sha_keyop_cleanup_ks(econtext, ret);
		/* FALLTHROUGH */
	}

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_SE_KAC_KDF || HAVE_HW_HMAC_SHA */
