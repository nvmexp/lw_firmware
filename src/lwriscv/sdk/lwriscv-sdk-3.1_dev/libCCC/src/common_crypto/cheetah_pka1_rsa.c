/*
 * Copyright (c) 2015-2021, LWPU Corporation. All Rights Reserved.
 *
 * LWPU Corporation and its licensors retain all intellectual property and
 * proprietary rights in and to this software and related documentation.  Any
 * use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from LWPU Corporation
 * is strictly prohibited.
 */

/* Support CheetAh Security Engine Elliptic engine : modular exponentiation
 */
#include <crypto_system_config.h>

#if HAVE_PKA1_RSA

#include <tegra_cdev.h>
#include <tegra_pka1_rsa.h>

#if HAVE_PKA1_LOAD
#include <crypto_asig_proc.h>
#endif

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

static status_t pka1_get_rsa_opmode(const struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t rsa_bitsize = 0U;

	LTRACEF("entry\n");
	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	CCC_SAFE_MULT_U32(rsa_bitsize, econtext->rsa_size, 8U);

	switch (rsa_bitsize) {
	case  512U: econtext->pka1_data->op_mode = PKA1_OP_MODE_RSA512;  break;
	case  768U: econtext->pka1_data->op_mode = PKA1_OP_MODE_RSA768;  break;
	case 1024U: econtext->pka1_data->op_mode = PKA1_OP_MODE_RSA1024; break;
	case 1536U: econtext->pka1_data->op_mode = PKA1_OP_MODE_RSA1536; break;
	case 2048U: econtext->pka1_data->op_mode = PKA1_OP_MODE_RSA2048; break;
	case 3072U: econtext->pka1_data->op_mode = PKA1_OP_MODE_RSA3072; break;
	case 4096U: econtext->pka1_data->op_mode = PKA1_OP_MODE_RSA4096; break;
	default:
		LTRACEF("Unsupported RSA size %u\n", econtext->rsa_size);
		ret = SE_ERROR(ERR_ILWALID_ARGS);
		break;
	}
	CCC_ERROR_CHECK(ret);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_program_rsa_run(const struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((econtext->rsa_size % BYTES_IN_WORD) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("RSA size (%u bytes) not multiple of word byte size\n",
					     econtext->rsa_size));
	}

	// XXX RSA only uses MOD_EXP operation
	// XXX The opcode in econtext->pka1_data->pka1_op is not used/checked ==> FIXME
	ret = pka1_go_start(econtext->engine, econtext->pka1_data->op_mode, econtext->rsa_size,
			    PKA1_ENTRY_MOD_EXP);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Program RSA start failed %d\n", ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_read_rsa_result(struct se_data_params *input_params,
				     const struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t nwords = 0U;

	LTRACEF("entry\n");

	if ((NULL == input_params) || (NULL == econtext) ||
	    (NULL == econtext->pka1_data)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	nwords = input_params->output_size / BYTES_IN_WORD;

	ret = pka1_bank_register_read(econtext->engine, econtext->pka1_data->op_mode,
				      BANK_A, R0, input_params->dst, nwords, false);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to read RSA result register\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_PKA1_LOAD
static status_t pka1_load_rsa_keyslot(
	const uint8_t *exponent, const struct se_engine_rsa_context *econtext,
	uint32_t exp_words, bool modulus_BE, bool exp_BE, bool m_prime_BE,
	bool r2_BE)
{
	status_t ret = NO_ERROR;
	uint32_t rsa_size_words = 0U;

	LTRACEF("entry\n");

	LTRACEF("Loading RSA parameters via SE PKA1 keyslot %u\n", econtext->rsa_keyslot);

	rsa_size_words = econtext->rsa_size / BYTES_IN_WORD;

	LTRACEF("Writing %u word exponent\n", exp_words);
	ret = pka1_set_keyslot_field(econtext->engine, KMAT_RSA_EXPONENT,
				     econtext->rsa_keyslot,
				     exponent,
				     exp_words, exp_BE, rsa_size_words);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write RSA kslot %u exponent (field %u)\n",
					  econtext->rsa_keyslot, KMAT_RSA_EXPONENT));

	LTRACEF("Writing %u word modulus\n", rsa_size_words);
	ret = pka1_set_keyslot_field(econtext->engine, KMAT_RSA_MODULUS,
				     econtext->rsa_keyslot,
				     econtext->rsa_modulus,
				     rsa_size_words, modulus_BE, rsa_size_words);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write RSA kslot %u modulus (field %u)\n",
					  econtext->rsa_keyslot, KMAT_RSA_MODULUS));

	LTRACEF("Writing %u word m_prime\n", rsa_size_words);
	ret = pka1_set_keyslot_field(econtext->engine, KMAT_RSA_M_PRIME,
				     econtext->rsa_keyslot,
				     econtext->pka1_data->m_prime,
				     rsa_size_words, m_prime_BE, rsa_size_words);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write RSA kslot %u M' (field %u)\n",
					  econtext->rsa_keyslot, KMAT_RSA_M_PRIME));
	LTRACEF("Writing %u word r2_sqr\n", rsa_size_words);
	ret = pka1_set_keyslot_field(econtext->engine, KMAT_RSA_R2_SQR,
				     econtext->rsa_keyslot,
				     econtext->pka1_data->r2,
				     rsa_size_words, r2_BE, rsa_size_words);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write RSA kslot %u R2 (field %u)\n",
					  econtext->rsa_keyslot, KMAT_RSA_R2_SQR));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_LOAD */

static status_t pka1_write_rsa_key_to_bank_registers(
	const uint8_t *exponent, const struct se_engine_rsa_context *econtext,
	uint32_t exp_words, bool modulus_BE, bool exp_BE, bool m_prime_BE,
	bool r2_BE)
{
	status_t ret = NO_ERROR;
	uint32_t rsa_size_words = 0U;

	LTRACEF("entry\n");

	/* Write key material directly to the per-operation PKA registers-
	 * do not use any keyslots for this
	 */

	rsa_size_words = econtext->rsa_size / BYTES_IN_WORD;

	/* Callee zero pads short fields! */
	LTRACEF("Reg writing %u word exponent\n", exp_words);
	ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
				       BANK_D, R2,
				       exponent,
				       exp_words, exp_BE, rsa_size_words);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write RSA exp PKA bank %u reg %u\n",
					  BANK_D, R2));

	LTRACEF("Reg writing %u word modulus\n", rsa_size_words);
	ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
				       BANK_D, R0,
				       econtext->rsa_modulus,
				       rsa_size_words, modulus_BE, rsa_size_words);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write RSA modulus PKA bank %u reg %u\n",
					  BANK_D, R0));

	LTRACEF("Reg writing %u word m_prime\n", rsa_size_words);
	ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
				       BANK_D, R1,
				       econtext->pka1_data->m_prime,
				       rsa_size_words, m_prime_BE, rsa_size_words);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write RSA m_prime PKA bank %u reg %u\n",
					  BANK_D, R1));

	LTRACEF("Reg writing %u word r2_sqr\n", rsa_size_words);
	ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
				       BANK_D, R3,
				       econtext->pka1_data->r2,
				       rsa_size_words, r2_BE, rsa_size_words);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write RSA r2 PKA bank %u reg %u\n",
					  BANK_D, R3));

	LTRACEF("Loading key directly to PKA bank registers\n");

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_set_rsa_keymat(const struct se_engine_rsa_context *econtext,
				    bool use_keyslot)
{
	status_t ret = NO_ERROR;
	uint32_t rsa_size_words = 0U;
	uint32_t exp_words = 0U;
	const uint8_t *exponent = NULL;
	bool modulus_BE = false;
	bool exp_BE     = false;
	bool m_prime_BE = false;
	bool r2_BE = false;

	LTRACEF("entry\n");
	if ((NULL == econtext) || (NULL == econtext->pka1_data)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	// XXXXXXX TEST THIS with the setup in EDN_0575_PKA_FW_RSA_Lib_v1p1.pdf
	// XXXXXXX It defines different registers compared with EDN-0243
	// XXXXXXX  FIXME!!! (e.g. R_ILW does not need to be saved/set again
	// XXXXXXX  because the result is preserved and the register has changed for M_PRIME
	// XXXXXXX  callwlation!!!

	modulus_BE = ((econtext->rsa_flags & RSA_FLAGS_BIG_ENDIAN_MODULUS) != 0U);

	exp_BE = ((econtext->rsa_flags & RSA_FLAGS_BIG_ENDIAN_EXPONENT) != 0U);

	m_prime_BE = ((econtext->pka1_data->mont_flags & PKA1_MONT_FLAG_M_PRIME_LITTLE_ENDIAN) == 0U);

	r2_BE = ((econtext->pka1_data->mont_flags & PKA1_MONT_FLAG_R2_LITTLE_ENDIAN) == 0U);

	LTRACEF("IS BIG ENDIAN(modulus: %u, exponent: %u, m_prime: %u, r2: %u), use key slot: %u\n",
		modulus_BE, exp_BE, m_prime_BE, r2_BE, use_keyslot);

	rsa_size_words = econtext->rsa_size / BYTES_IN_WORD;

	if (econtext->rsa_keytype == KEY_TYPE_RSA_PRIVATE) {
		if ((econtext->rsa_flags & RSA_FLAGS_USE_PRIVATE_KEY) != 0U) {
			exponent = econtext->rsa_priv_exponent;
			exp_words = rsa_size_words;
		} else {
			exponent = econtext->rsa_pub_exponent;
			/* This may be zero if pub exponent is not known */
			exp_words = econtext->rsa_pub_expsize / BYTES_IN_WORD;
		}
	} else {
		LTRACEF("PUB EXPONENT used\n");
		exponent = econtext->rsa_pub_exponent;
		exp_words = econtext->rsa_pub_expsize / BYTES_IN_WORD;
	}

	if (BOOL_IS_TRUE(use_keyslot)) {
#if HAVE_PKA1_LOAD
		ret = pka1_load_rsa_keyslot(exponent, econtext, exp_words,
					    modulus_BE, exp_BE,
					    m_prime_BE, r2_BE);
		CCC_ERROR_CHECK(ret);
#else
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED,
				     CCC_ERROR_MESSAGE("PKA RSA keyslot loading not supported\n"));
#endif /* HAVE_PKA1_LOAD */
	} else {
		ret = pka1_write_rsa_key_to_bank_registers(exponent, econtext,
							   exp_words,
							   modulus_BE, exp_BE,
							   m_prime_BE, r2_BE);
		CCC_ERROR_CHECK(ret);
	}

	/* XXX TODO FIXME!
	 * XXX  Should set the FullWidth (F0) flag to make the RSA
	 * XXX  exponentiation time-ilwariant. This is not yet done (don't really know which
	 * XXX  register bit to set....
	 *
	 * The blinding support needs to be added for security reasons, to make it harder
	 * to do timing attacks for finding out priv key values.
	 */

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* RSA support funs */

static status_t pka1_setup_rsa_montgomery(const struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;
	bool modulus_BE = false;

	LTRACEF("entry\n");
	if ((NULL == econtext) || (NULL == econtext->pka1_data)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if ((econtext->pka1_data->mont_flags & PKA1_MONT_FLAG_VALUES_OK) != 0U) {
		LTRACEF("RSA key montgomery keys loaded out of band\n");
	} else {
		modulus_BE = ((econtext->rsa_flags & RSA_FLAGS_BIG_ENDIAN_MODULUS) != 0U);

		ret = pka1_montgomery_values_calc(econtext->cmem,
						  econtext->engine,
						  econtext->rsa_size,
						  econtext->rsa_modulus,
						  econtext->pka1_data,
						  modulus_BE);
		CCC_ERROR_CHECK(ret,
				CCC_ERROR_MESSAGE("Callwlating montgomery values failed: %d\n",
						  ret));
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Set the RSA key either to SE keyslot or directly to PKA registers
 * (for the RSA exponentiation operation)
 */
static status_t pka1_rsa_setkey(const struct se_engine_rsa_context *econtext,
				bool use_keyslot)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = pka1_setup_rsa_montgomery(econtext);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("RSA montgomery residue constant callwlation failed: %d\n",
					  ret));

	ret = pka1_set_rsa_keymat(econtext, use_keyslot);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to set RSA key material: %d\n",
					  ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;

}

/* The pka1_data object allocation is managed by the caller */
static status_t pka1_rsa_init(struct se_engine_rsa_context *econtext,
			      enum pka1_engine_ops pka1_op,
			      struct pka1_engine_data *pka1_data)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	if ((NULL == econtext) || (NULL == pka1_data)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	econtext->pka1_data		= pka1_data;
	econtext->pka1_data->pka1_op    = pka1_op;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Clear the econtext->pka1_data object */
static status_t pka1_rsa_release(struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (NULL != econtext->pka1_data) {
		pka1_data_release(econtext->cmem, econtext->pka1_data);

		/* Not a dynamic object lwrrently for RSA.
		 * If this changes need to release the object.
		 */
		econtext->pka1_data = NULL;
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_rsa_exit(struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = pka1_rsa_release(econtext);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_rsa_set_op_flags(const struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t flag_val = 0U;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if HAVE_PKA1_BLINDING
	if (econtext->rsa_keytype == KEY_TYPE_RSA_PRIVATE) {
		if ((econtext->rsa_flags & RSA_FLAGS_USE_PRIVATE_KEY) != 0U) {
			LTRACEF("RSA key blinding enabled\n");
			flag_val |= SE_PKA1_FLAGS_FLAG_F0(SE_ELP_ENABLE);
		}
	}
#endif

	tegra_engine_set_reg(econtext->engine, SE_PKA1_FLAGS_OFFSET, flag_val);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_rsa_op_check_args(
		const struct se_data_params *input_params,
		const struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == input_params) || (NULL == econtext) ||
	    (NULL == econtext->pka1_data)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if 0
	if ((econtext->rsa_flags & RSA_FLAGS_BIG_ENDIAN_DATA) != 0U) {
		/* ignore => callers have swapped the RSA input data
		 * to little endian already. If swapped or padded
		 * input probably is passed in context buffer.
		 * TODO: not very clear => improve.
		 */
	}
#endif

	DUMP_DATA_PARAMS("RSA OP input", 0x1U, input_params);

	if (input_params->input_size != econtext->rsa_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("RSA input size %u must equal RSA size\n",
					     input_params->input_size));
	}

	if (input_params->output_size < input_params->input_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("RSA input size %u shorter than RSA size\n",
					     input_params->output_size));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Setup RSA operation and execute it; then read result */
static status_t pka1_rsa_op(struct se_data_params *input_params,
			    const struct se_engine_rsa_context *econtext)
{
	status_t ret  = NO_ERROR;
	uint32_t nwords = 0U;
	bool data_big_endian = false;

	LTRACEF("entry\n");

	ret = pka1_rsa_op_check_args(input_params, econtext);
	CCC_ERROR_CHECK(ret);

	/* Verified to be rsa size in words above */
	nwords = input_params->input_size / BYTES_IN_WORD;

	ret = pka1_bank_register_write(econtext->engine, econtext->pka1_data->op_mode,
				       BANK_A, R0,
				       input_params->src,
				       nwords, data_big_endian, nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA operation data write failed %d\n",
				ret));

	ret = pka1_rsa_set_op_flags(econtext);
	CCC_ERROR_CHECK(ret);

	ret = pka1_program_rsa_run(econtext);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA operation programming returned %d\n",
				ret));

	ret = pka1_wait_op_done(econtext->engine);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA operation wait returned %d\n",
				ret));

	ret = pka1_read_rsa_result(input_params, econtext);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA result read returned %d\n", ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_rsa_modexp(struct se_engine_rsa_context *econtext,
				struct se_data_params *input_params,
				bool use_keyslot, bool set_key_value)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* TODO: Could support also crypto API providing montgomery values
	 * OOB, but not required at this point => maybe add here later
	 *
	 * Lwrrently subsystems using CCC as library can setup such keys
	 * directly to PKA1 keyslots, but not via the crypto API.
	 */

	/* Set PKA1 key unless using existing preset key already in SE HW keyslot
	 */
	if (BOOL_IS_TRUE(set_key_value)) {
		ret = pka1_rsa_setkey(econtext, use_keyslot);
		CCC_ERROR_CHECK(ret,
				LTRACEF("RSA setkey failed %d\n", ret));
	}

#if HAVE_PKA1_LOAD
	if (BOOL_IS_TRUE(use_keyslot)) {
		ret = pka1_keyslot_load(econtext->engine, econtext->rsa_keyslot);
		CCC_ERROR_CHECK(ret,
				LTRACEF("RSA keyslot load setup failed %d\n",
					ret));
	}
#endif /* HAVE_PKA1_LOAD */

	LTRACEF("RSA input size after setkey %u\n", input_params->input_size);

	ret = pka1_rsa_op(input_params, econtext);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA operation failed %d\n", ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_PKA1_LOAD
/* It would fail the calling modexp operation; hence this returns void now. */
static void engine_pka1_rsa_clean_keyslot(
	const struct se_engine_rsa_context *econtext,
	bool use_keyslot)
{
	if (BOOL_IS_TRUE(use_keyslot)) {
		/* CD#5 update 1 behavior: clear key unless told otherwise */
		if ((econtext->rsa_flags & RSA_FLAGS_LEAVE_KEY_TO_KEYSLOT) == 0U) {
			status_t fret = pka1_clear_rsa_keyslot_locked(econtext->engine,
								      econtext->rsa_keyslot,
								      econtext->rsa_size);
			if (NO_ERROR != fret) {
				LTRACEF("Failed to clear PKA1 RSA keyslot %u\n",
					econtext->rsa_keyslot);
				/* FALLTHROUGH */
			}
		}
	}
}
#endif /* HAVE_PKA1_LOAD */

/* This is the only exported function from the PKA1 RSA system.
 *
 * Everything in this file is called with the mutex locked.
 *
 * x^y mod m
 */
status_t engine_pka1_rsa_modular_exp_locked(
	struct se_data_params *input_params,
	struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;
	bool use_keyslot = false; /* default: Do not use keyslot for PKA1 */
	bool set_key_value = true;
	struct pka1_engine_data pka1_data = { .op_mode = PKA1_OP_MODE_ILWALID };

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LTRACEF("===> RSA keyslot: %u\n", econtext->rsa_keyslot);

	DUMP_DATA_PARAMS("RSA input", 0x1, input_params);

	ret = pka1_rsa_init(econtext, PKA1_OP_MODULAR_EXPONENTIATION, &pka1_data);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA init failed %d\n", ret));

	LTRACEF("RSA input size after init %u\n", input_params->input_size);

	ret = pka1_get_rsa_opmode(econtext);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("RSA opmode set failed: %d\n", ret));

	if ((econtext->rsa_flags & RSA_FLAGS_DYNAMIC_KEYSLOT) != 0U) {
		LTRACEF("PKA1 RSA loading keys via bank registers\n");
		use_keyslot = false;
	}

#if HAVE_PKA1_LOAD
	if ((econtext->rsa_flags & RSA_FLAGS_USE_KEYSLOT) != 0U) {
		LTRACEF("PKA using HW keyslots, writing keymaterial via keyslots\n");
		use_keyslot = true;
	}

	/* CCC can not know if the keyslot contains sensible key values
	 * in the general case => just trust the caller parameters.
	 */
	if ((econtext->rsa_flags & RSA_FLAGS_USE_PRESET_KEY) != 0U) {
		LTRACEF("PKA1 RSA using keyslot key %u\n",
			econtext->rsa_keyslot);
		set_key_value = false;
		use_keyslot = true;
	}
#else
	if (((econtext->rsa_flags & RSA_FLAGS_USE_KEYSLOT) != 0U) ||
	    ((econtext->rsa_flags & RSA_FLAGS_USE_PRESET_KEY) != 0U)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     CCC_ERROR_MESSAGE("PKA RSA HW keyslots not supported\n"));
	}
#endif /* HAVE_PKA1_LOAD */

	ret = pka1_rsa_modexp(econtext, input_params,
			      use_keyslot, set_key_value);
	CCC_ERROR_CHECK(ret);

fail:
#if HAVE_PKA1_LOAD
	engine_pka1_rsa_clean_keyslot(econtext, use_keyslot);
	/* Final key cleanup does not trigger operation failure here. */
#endif /* HAVE_PKA1_LOAD */

	(void)pka1_rsa_exit(econtext);

	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_PKA1_LOAD
status_t pka1_clear_rsa_keyslot_locked(const struct engine_s *engine,
				       uint32_t pka_keyslot,
				       uint32_t rsa_size)
{
	status_t ret = NO_ERROR;
	status_t tret = NO_ERROR;
	uint32_t rsa_size_words = rsa_size / BYTES_IN_WORD;
	const uint32_t zero = 0U;
	uint32_t field = KMAT_RSA_EXPONENT;

	LTRACEF("entry\n");

	if ((NULL == engine) || (pka_keyslot >= PKA1_MAX_KEYSLOTS)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LTRACEF("Clearing RSA PKA1 keyslot %u\n", pka_keyslot);

	/* On size error: clear max size RSA operands
	 * i.e. just use value 0 to clear max size.
	 */
	if ((rsa_size_words < 16U) || (rsa_size_words > 128U)) {
		rsa_size_words = 128U;
	}

	/* Try to clear all fields in spite of errors
	 */
	for (field = KMAT_RSA_EXPONENT; field <= KMAT_RSA_R2_SQR; field++) {
		tret = pka1_set_keyslot_field(engine, field,
					      pka_keyslot, (const uint8_t *)&zero,
					      1U, CFALSE, rsa_size_words);
		if (NO_ERROR != tret) {
			LTRACEF("Failed to clear RSA kslot %u field %u\n",
				pka_keyslot, field);
			if (NO_ERROR == ret) {
				ret = tret;
				/* FALLTHROUGH */
			}
		}
	}
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t pka1_rsa_write_key_locked_check_args(const engine_t *engine,
						     uint32_t rsa_keyslot,
						     uint32_t rsa_expsize_bytes,
						     uint32_t rsa_size_bytes,
						     const uint8_t *exponent,
						     const uint8_t *modulus)
{
	status_t ret = NO_ERROR;

	if ((NULL == engine) || (rsa_keyslot >= SE_RSA_MAX_KEYSLOTS)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* If one of these is NULL, both must be NULL (=> clear keyslot)
	 * If only one is NULL => error.
	 */
	if ((NULL == exponent) || (NULL == modulus)) {
		if (!((NULL == exponent) && (NULL == modulus))) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}
	}

	if (!BOOL_IS_TRUE(IS_SUPPORTED_RSA_EXPONENT_BYTE_LENGTH(rsa_expsize_bytes))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!BOOL_IS_TRUE(IS_SUPPORTED_RSA_MODULUS_BYTE_LENGTH(rsa_size_bytes))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

fail:
	return ret;
}

/* call with PKA1 mutex held */
static status_t pka1_rsa_set_key_locked(struct se_engine_rsa_context *rsa_econtext,
					struct pka1_engine_data *pka1_data)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = pka1_rsa_init(rsa_econtext, PKA1_OP_MODULAR_EXPONENTIATION,
			    pka1_data);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA init failed %d\n", ret));

	ret = pka1_get_rsa_opmode(rsa_econtext);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("RSA opmode set failed: %d\n", ret));

	/* Callwlate (or use provided) montgomery residues and write
	 * the key && friends to keyslot
	 */
	ret = pka1_rsa_setkey(rsa_econtext, true);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to set PKA1 keyslot %u\n",
				rsa_econtext->rsa_keyslot));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* TODO: the rsa_expsize_bytes value now defines whether this is a
 * public or private key operation. Maybe should instead add a key
 * type parameter to make this explicit.
 *
 * if exponent size is rsa size => private key
 * if exponent size is 4U => public key operation
 * else error.
 */
status_t pka1_rsa_write_key_locked(const struct engine_s *engine,
				   uint32_t rsa_keyslot,
				   uint32_t rsa_expsize_bytes,
				   uint32_t rsa_size_bytes,
				   const uint8_t *exponent,
				   const uint8_t *modulus,
				   bool exponent_big_endian,
				   bool modulus_big_endian,
				   const rsa_montgomery_values_t *mg)
{
	status_t ret = NO_ERROR;
	struct se_engine_rsa_context rsa_econtext = { .engine = NULL };
	struct pka1_engine_data pka1_data = { .op_mode = PKA1_OP_MODE_ILWALID };
	struct context_mem_s *cmem = NULL;

#if HAVE_CONTEXT_MEMORY && FORCE_CONTEXT_MEMORY
	/* The RSA key saving needs additional memory for Montgomery
	 * value colwersions unless the values are provided by caller.
	 *
	 * When using context memory so that memory pool operations
	 * are blocked with these options the only place we can place
	 * these objects is to static buffers, since adding this
	 * object to stack is not really possible...
	 *
	 * In most subsystem the stack is way too small to contain
	 * large objects like this.
	 *
	 * Using a static buffer would make this makes the function
	 * non-thread safe, but this function is called with the HW
	 * PKA1 mutex held, so that is not a relevant concern here.
	 *
	 * Colwert the static buffer into a CCC CMEM descriptor
	 * controlled memory space and set it into the rsa engine
	 * context for the call chain use.
	 *
	 * The buffer does not contain any secrets, there is no need
	 * to clean it before exit.
	 */
#define PKA1_RSA_MONT_CALC_MEM_SIZE PAGE_SIZE

	static CMEM_MEM_ALIGNED(CMEM_DESCRIPTOR_SIZE) uint8_t static_buf[PKA1_RSA_MONT_CALC_MEM_SIZE];
	struct context_mem_s cmem_obj = { 0 };
#endif /* HAVE_CONTEXT_MEMORY && FORCE_CONTEXT_MEMORY */

	LTRACEF("entry\n");

#if HAVE_CONTEXT_MEMORY && FORCE_CONTEXT_MEMORY
	/* Colwert the static buffer to a CMEM region controlled
	 * GENERIC context memory.
	 */
	cmem = &cmem_obj;

	ret = cmem_from_buffer(cmem, CMEM_TYPE_GENERIC,
			       &static_buf[0], sizeof_u32(static_buf));
	CCC_ERROR_CHECK(ret);
#endif /* HAVE_CONTEXT_MEMORY && FORCE_CONTEXT_MEMORY */

	ret = pka1_rsa_write_key_locked_check_args(engine,
						   rsa_keyslot,
						   rsa_expsize_bytes,
						   rsa_size_bytes,
						   exponent,
						   modulus);
	CCC_ERROR_CHECK(ret);

	LTRACEF("entry (modulus(is BE: %u), exponent(is BE: %u)\n",
		modulus_big_endian, exponent_big_endian);

	ret = rsa_engine_context_static_init(TE_CRYPTO_DOMAIN_KERNEL,
					     engine->e_id,
					     TE_ALG_RSASSA_PKCS1_PSS_MGF1_SHA1,
					     &rsa_econtext,
					     cmem);
	CCC_ERROR_CHECK(ret);

	if (BOOL_IS_TRUE(exponent_big_endian)) {
		rsa_econtext.rsa_flags |= RSA_FLAGS_BIG_ENDIAN_EXPONENT;
	}

	if (BOOL_IS_TRUE(modulus_big_endian)) {
		rsa_econtext.rsa_flags |= RSA_FLAGS_BIG_ENDIAN_MODULUS;
	}

	rsa_econtext.rsa_keyslot     = rsa_keyslot;
	rsa_econtext.rsa_size        = rsa_size_bytes;
	rsa_econtext.rsa_pub_expsize = rsa_expsize_bytes;
	rsa_econtext.rsa_modulus     = modulus;

	if (rsa_expsize_bytes == rsa_size_bytes) {
		/* pub exponent not set */
		rsa_econtext.rsa_pub_expsize = 0U;
		rsa_econtext.rsa_priv_exponent = exponent;
		rsa_econtext.rsa_keytype = KEY_TYPE_RSA_PRIVATE;
		rsa_econtext.rsa_flags |= RSA_FLAGS_USE_PRIVATE_KEY;
	} else {
		if (rsa_expsize_bytes != 4U) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					     CCC_ERROR_MESSAGE("Unsupported RSA public key size: %u\n",
							       rsa_expsize_bytes));
		}
		rsa_econtext.rsa_pub_expsize = 4U;
		rsa_econtext.rsa_pub_exponent = exponent;
		rsa_econtext.rsa_keytype = KEY_TYPE_RSA_PUBLIC;
	}

	if (NULL != mg) {
		ret = tegra_pka1_set_fixed_montgomery_values(&pka1_data,
							     mg->m_prime, mg->r2,
							     modulus_big_endian);
		CCC_ERROR_CHECK(ret,
				LTRACEF("RSA fixed montgomery values failed %d\n",
					ret));

		/* These originate from "out-of-band" */
		pka1_data.mont_flags |= PKA1_MONT_FLAG_OUT_OF_BAND;
	}

	/* If Montgomery values need to be callwlated in this case the
	 * callee needs to get runtime memory somewhere; for long RSA
	 * keys this is about 1.5 KB). Since this call chain does not
	 * contain any memory contexts it can not be passed from here.
	 */
	ret = pka1_rsa_set_key_locked(&rsa_econtext, &pka1_data);
	CCC_ERROR_CHECK(ret);

fail:
	(void)pka1_rsa_exit(&rsa_econtext);

	se_util_mem_set((uint8_t *)&rsa_econtext, 0U, sizeof_u32(rsa_econtext));

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_PKA1_LOAD */

#endif /* HAVE_PKA1_RSA */
