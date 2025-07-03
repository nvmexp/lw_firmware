/*
 * Copyright (c) 2019-2021, LWPU Corporation. All Rights Reserved.
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

#if HAVE_LWPKA_RSA

#include <tegra_cdev.h>
#include <lwpka/tegra_lwpka_gen.h>
#include <lwpka/tegra_lwpka_rsa.h>

#include <crypto_asig_proc.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

static status_t lwpka_get_rsa_opmode(struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t rsa_bitsize = 0U;

	LTRACEF("entry\n");

	CCC_SAFE_MULT_U32(rsa_bitsize, econtext->rsa_size, 8U);

	switch (rsa_bitsize) {
	case  512U: econtext->pka_data.op_mode = PKA1_OP_MODE_RSA512;  break;
	case  768U: econtext->pka_data.op_mode = PKA1_OP_MODE_RSA768;  break;
	case 1024U: econtext->pka_data.op_mode = PKA1_OP_MODE_RSA1024; break;
	case 1536U: econtext->pka_data.op_mode = PKA1_OP_MODE_RSA1536; break;
	case 2048U: econtext->pka_data.op_mode = PKA1_OP_MODE_RSA2048; break;
	case 3072U: econtext->pka_data.op_mode = PKA1_OP_MODE_RSA3072; break;
	case 4096U: econtext->pka_data.op_mode = PKA1_OP_MODE_RSA4096; break;
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

static inline uint32_t lwpka_rsa_scena(const struct se_engine_rsa_context *econtext)
{
	return (((econtext->rsa_flags & RSA_FLAGS_USE_PRIVATE_KEY) == 0U) ?
		CCC_SC_DISABLE : CCC_SC_ENABLE);
}

static status_t lwpka_program_rsa_run(const struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;
	bool use_kslot = ((econtext->rsa_flags & RSA_FLAGS_USE_KEYSLOT) != 0U);

	LTRACEF("entry\n");

	if ((econtext->rsa_size % BYTES_IN_WORD) != 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
				     LTRACEF("RSA size (%u bytes) not multiple of word byte size\n",
					     econtext->rsa_size));
	}

	// XXX RSA only uses MOD_EXP operation
	// XXX The opcode in econtext->pka_data.op_mode is not used/checked ==> FIXME
	ret = lwpka_go_start_keyslot(econtext->engine, econtext->pka_data.op_mode,
				     LWPKA_PRIMITIVE_MOD_EXP,
				     use_kslot, econtext->rsa_keyslot,
				     lwpka_rsa_scena(econtext));
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Program RSA start failed %d\n", ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_read_rsa_result(struct se_data_params *input_params,
				     const struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;
	uint32_t nwords = 0U;

	LTRACEF("entry\n");

	if ((NULL == input_params) || (NULL == econtext)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	nwords = input_params->output_size / BYTES_IN_WORD;

	ret = lwpka_register_read(econtext->engine, econtext->pka_data.op_mode,
				  LOR_D1, input_params->dst, nwords, false);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to read RSA result register\n"));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

#if HAVE_LWPKA_LOAD
static status_t lwpka_write_rsa_keyslot(
	const uint8_t *exponent, const struct se_engine_rsa_context *econtext,
	uint32_t exp_words, bool modulus_BE, bool exp_BE)
{
	status_t ret = NO_ERROR;
	uint32_t rsa_size_words = 0U;

	LTRACEF("entry\n");

	LTRACEF("Writing RSA parameters to LWPKA keyslot %u\n", econtext->rsa_keyslot);

	rsa_size_words = econtext->rsa_size / BYTES_IN_WORD;

	LTRACEF("Writing %u word exponent\n", exp_words);
	ret = lwpka_set_keyslot_field(econtext->engine, KMAT_RSA_EXPONENT,
				      econtext->rsa_keyslot,
				      exponent,
				      exp_words, exp_BE, rsa_size_words);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write RSA kslot %u exponent (field %u)\n",
					  econtext->rsa_keyslot, KMAT_RSA_EXPONENT));

	LTRACEF("Writing %u word modulus\n", rsa_size_words);
	ret = lwpka_set_keyslot_field(econtext->engine, KMAT_RSA_MODULUS,
				      econtext->rsa_keyslot,
				      econtext->rsa_modulus,
				      rsa_size_words, modulus_BE, rsa_size_words);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write RSA kslot %u modulus (field %u)\n",
					  econtext->rsa_keyslot, KMAT_RSA_MODULUS));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t lwpka_clear_rsa_keyslot_locked(const struct engine_s *engine,
				       uint32_t pka_keyslot,
				       uint32_t rsa_size)
{
	status_t ret = NO_ERROR;
	uint32_t rsa_size_words = rsa_size / BYTES_IN_WORD;
	const uint32_t zero = 0U;

	LTRACEF("entry\n");

	if ((NULL == engine) || (pka_keyslot >= LWPKA_MAX_KEYSLOTS)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LTRACEF("Clearing RSA LWPKA keyslot %u\n", pka_keyslot);

	/* On size error: clear max size RSA operands
	 * i.e. just use value 0 to clear max size.
	 */
	if (rsa_size_words == 0U) {
		rsa_size_words = KMAT_RSA_EXPONENT_WORDS;
	} else {
		if ((rsa_size_words < 16U) || (rsa_size_words > 128U)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
		}
	}

	/* clear exponent field, modulus never matters.
	 */
	ret = lwpka_set_keyslot_field(engine, KMAT_RSA_EXPONENT,
				      pka_keyslot, (const uint8_t *)&zero,
				      1U, CFALSE, rsa_size_words);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to clear RSA kslot %u : RSA EXPONENT\n",
				pka_keyslot));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* It would fail the calling modexp operation; hence this returns void now. */
static void engine_lwpka_rsa_clean_keyslot(
	const struct se_engine_rsa_context *econtext, bool use_keyslot)
{
	if (BOOL_IS_TRUE(use_keyslot)) {
		/* CD#5 update 1 behavior: clear key unless told otherwise */
		if ((econtext->rsa_flags & RSA_FLAGS_LEAVE_KEY_TO_KEYSLOT) == 0U) {
			status_t fret = lwpka_clear_rsa_keyslot_locked(econtext->engine,
								       econtext->rsa_keyslot,
								       econtext->rsa_size);
			if (NO_ERROR != fret) {
				LTRACEF("Failed to clear LWPKA RSA keyslot %u\n",
					econtext->rsa_keyslot);
				/* FALLTHROUGH */
			}
		}
	}
}
#endif /* HAVE_LWPKA_LOAD */

static status_t lwpka_write_rsa_key_to_lor(
	const uint8_t *exponent, const struct se_engine_rsa_context *econtext,
	uint32_t exp_words, bool modulus_BE, bool exp_BE)
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
	ret = lwpka_register_write(econtext->engine, econtext->pka_data.op_mode,
				   LOR_K0,
				   exponent,
				   exp_words, exp_BE, rsa_size_words);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write RSA exp PKA reg %u\n",
					  LOR_K0));

	LTRACEF("Reg writing %u word modulus\n", rsa_size_words);
	ret = lwpka_register_write(econtext->engine, econtext->pka_data.op_mode,
				   LOR_C0,
				   econtext->rsa_modulus,
				   rsa_size_words, modulus_BE, rsa_size_words);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to write RSA modulus PKA reg %u\n",
					  LOR_C0));

	LTRACEF("Loading key directly to LWPKA registers\n");

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_set_rsa_keymat(const struct se_engine_rsa_context *econtext,
				     bool use_keyslot)
{
	status_t ret = NO_ERROR;
	uint32_t rsa_size_words = 0U;
	uint32_t exp_words = 0U;
	const uint8_t *exponent = NULL;
	bool modulus_BE = false;
	bool exp_BE     = false;

	LTRACEF("entry\n");
	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	modulus_BE = ((econtext->rsa_flags & RSA_FLAGS_BIG_ENDIAN_MODULUS) != 0U);

	exp_BE = ((econtext->rsa_flags & RSA_FLAGS_BIG_ENDIAN_EXPONENT) != 0U);

	LTRACEF("IS BIG ENDIAN(modulus: %u, exponent: %u), use key slot: %u\n",
		modulus_BE, exp_BE, use_keyslot);

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
#if HAVE_LWPKA_LOAD
		ret = lwpka_write_rsa_keyslot(exponent, econtext, exp_words,
					      modulus_BE, exp_BE);
		CCC_ERROR_CHECK(ret);
#else
		CCC_ERROR_WITH_ECODE(ERR_NOT_SUPPORTED);
#endif /* HAVE_LWPKA_LOAD */
	}

	if (!BOOL_IS_TRUE(use_keyslot)) {
		ret = lwpka_write_rsa_key_to_lor(exponent, econtext, exp_words,
						 modulus_BE, exp_BE);
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

/* Set the RSA key either to SE keyslot or directly to PKA registers
 * (for the RSA exponentiation operation)
 */
static status_t lwpka_rsa_setkey(const struct se_engine_rsa_context *econtext, bool use_keyslot)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = lwpka_set_rsa_keymat(econtext, use_keyslot);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("Failed to set RSA key material: %d\n",
					  ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;

}

static status_t lwpka_rsa_init(struct se_engine_rsa_context *econtext,
			       enum lwpka_engine_ops lwpka_op)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");
	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	econtext->pka_data.op_mode  = PKA1_OP_MODE_ILWALID;
	econtext->pka_data.lwpka_op = lwpka_op;

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Clear the econtext->pka_data object */
static status_t lwpka_rsa_release(struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	lwpka_data_release(&econtext->pka_data);
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_rsa_exit(struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if (NULL == econtext) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	ret = lwpka_rsa_release(econtext);
	CCC_ERROR_CHECK(ret);

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_rsa_op_check_args(
		const struct se_data_params *input_params,
		const struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == input_params) || (NULL == econtext)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

#if 0
	if ((econtext->rsa_flags & RSA_FLAGS_BIG_ENDIAN_DATA) != 0U) {
		/* ignore => callers have swapped the RSA input data
		 * to little endian already. If swapped or padded
		 * input probably passed in context buffer.
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
static status_t lwpka_rsa_op(struct se_data_params *input_params,
			     const struct se_engine_rsa_context *econtext)
{
	status_t ret  = NO_ERROR;
	uint32_t nwords = 0U;
	bool data_big_endian = false;

	LTRACEF("entry\n");

	ret = lwpka_rsa_op_check_args(input_params, econtext);
	CCC_ERROR_CHECK(ret);

	/* Verified to be rsa size in words above */
	nwords = input_params->input_size / BYTES_IN_WORD;

	ret = lwpka_register_write(econtext->engine, econtext->pka_data.op_mode,
				   LOR_S0,
				   input_params->src,
				   nwords, data_big_endian, nwords);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA operation data write failed %d\n",
				ret));

	ret = lwpka_program_rsa_run(econtext);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA operation programming returned %d\n",
				ret));

	ret = lwpka_wait_op_done(econtext->engine);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA operation wait returned %d\n",
				ret));

	ret = lwpka_read_rsa_result(input_params, econtext);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA result read returned %d\n", ret));
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_rsa_modexp(const struct se_engine_rsa_context *econtext,
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
	 * directly to LWPKA keyslots, but not via the crypto API.
	 */

	/* Set LWPKA key unless using existing preset key already in SE HW keyslot
	 */
	if (BOOL_IS_TRUE(set_key_value)) {
		ret = lwpka_rsa_setkey(econtext, use_keyslot);
		CCC_ERROR_CHECK(ret,
				LTRACEF("RSA setkey failed %d\n", ret));
	}

	LTRACEF("RSA input size after setkey %u\n", input_params->input_size);

	ret = lwpka_rsa_op(input_params, econtext);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA operation failed %d\n", ret));

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* This is the only exported function from the LWPKA RSA system.
 *
 * Everything in this file is called with the mutex locked.
 *
 * x^y mod m
 */
status_t engine_lwpka_rsa_modular_exp_locked(
	struct se_data_params *input_params,
	struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;
	bool use_keyslot = false; /* default: Do not use keyslot for LWPKA */
	bool set_key_value = true;

	LTRACEF("entry\n");

	if ((NULL == econtext) || (NULL == input_params)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	LTRACEF("===> RSA keyslot: %u\n", econtext->rsa_keyslot);

	DUMP_DATA_PARAMS("RSA input", 0x1, input_params);

	ret = lwpka_rsa_init(econtext, LWPKA_OP_MODULAR_EXPONENTIATION);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA init failed %d\n", ret));

	LTRACEF("RSA input size after init %u\n", input_params->input_size);

	ret = lwpka_get_rsa_opmode(econtext);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("RSA opmode set failed: %d\n", ret));

	if ((econtext->rsa_flags & RSA_FLAGS_DYNAMIC_KEYSLOT) != 0U) {
		LTRACEF("LWPKA RSA loading keys via LOR registers\n");
		use_keyslot = false;
	}

	if ((econtext->rsa_flags & RSA_FLAGS_USE_KEYSLOT) != 0U) {
		LTRACEF("PKA using HW keyslots, writing keymaterial via keyslots\n");
		use_keyslot = true;
	}

	/* CCC can not know if the keyslot contains sensible key values
	 * in the general case => just trust the caller parameters.
	 */
	if ((econtext->rsa_flags & RSA_FLAGS_USE_PRESET_KEY) != 0U) {
		LTRACEF("LWPKA RSA using keyslot key %u\n",
			econtext->rsa_keyslot);
		set_key_value = false;
		use_keyslot = true;
	}

	ret = lwpka_rsa_modexp(econtext, input_params,
			      use_keyslot, set_key_value);
	CCC_ERROR_CHECK(ret);

fail:
#if HAVE_LWPKA_LOAD
	engine_lwpka_rsa_clean_keyslot(econtext, use_keyslot);
	/* Final keyslot cleanup does not trigger operation failure here. */
#endif

	(void)lwpka_rsa_exit(econtext);

	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t lwpka_rsa_write_key_locked_check_args(const engine_t *engine,
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

/* call with LWPKA mutex held */
static status_t lwpka_rsa_set_key_locked(struct se_engine_rsa_context *rsa_econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	ret = lwpka_rsa_init(rsa_econtext, LWPKA_OP_MODULAR_EXPONENTIATION);
	CCC_ERROR_CHECK(ret,
			LTRACEF("RSA init failed %d\n", ret));

	ret = lwpka_get_rsa_opmode(rsa_econtext);
	CCC_ERROR_CHECK(ret,
			CCC_ERROR_MESSAGE("RSA opmode set failed: %d\n", ret));

	/* Callwlate (or use provided) montgomery residues and write
	 * the key && friends to keyslot
	 */
	ret = lwpka_rsa_setkey(rsa_econtext, true);
	CCC_ERROR_CHECK(ret,
			LTRACEF("Failed to set LWPKA keyslot %u\n",
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
status_t lwpka_rsa_write_key_locked(const engine_t *engine,
				    uint32_t rsa_keyslot,
				    uint32_t rsa_expsize_bytes,
				    uint32_t rsa_size_bytes,
				    const uint8_t *exponent,
				    const uint8_t *modulus,
				    bool exponent_big_endian,
				    bool modulus_big_endian)
{
	status_t ret = NO_ERROR;
	struct se_engine_rsa_context rsa_econtext = { .engine = NULL };

	/* LWPKA does not need more memory at runtime
	 * for rsa key write since it does not get Montgomery values
	 * from SW. So pass a NULL reference in CMEM.
	 */
	struct context_mem_s *cmem = NULL;

	LTRACEF("entry\n");

	ret = lwpka_rsa_write_key_locked_check_args(engine,
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

	ret = lwpka_rsa_set_key_locked(&rsa_econtext);
	CCC_ERROR_CHECK(ret);

fail:
	(void)lwpka_rsa_exit(&rsa_econtext);

	se_util_mem_set((uint8_t *)&rsa_econtext, 0U, sizeof_u32(rsa_econtext));

	LTRACEF("exit: %d\n", ret);
	return ret;
}
#endif /* HAVE_LWPKA_RSA */
