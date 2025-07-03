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

#include <crypto_ops.h>
#include <tegra_se_gen.h>

#if MODULE_TRACE
#define LOCAL_TRACE MODULE_TRACE
#else
#define LOCAL_TRACE 0
#endif

#if HAVE_SOC_FEATURE_KAC
#error "HAVE_SOC_FEATURE_KAC set when compiling tegra_se_pka0.c"
#endif

#if HAVE_SE_RSA

/**** RSA operations ****/

#define RSA_ERR_CHECK(engine, str) CCC_ENGINE_ERR_CHECK(engine, str)

static status_t rsa_write_key_arg_check(uint32_t rsa_keyslot,
					const uint8_t *exponent,
					const uint8_t *modulus,
					uint32_t rsa_expsize_bytes,
					uint32_t rsa_size_bytes)
{
	status_t ret = NO_ERROR;

	if (rsa_keyslot >= SE_RSA_MAX_KEYSLOTS) {
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

	/* Check exponent length to write: SE engine supported private and
	 * public key exponents.
	 */
	if (!BOOL_IS_TRUE(IS_VALID_RSA_EXPSIZE_BITS_SE_ENGINE(rsa_expsize_bytes * 8U))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* Check modulus length to write: SE engine supported lengths.
	 */
	if (!BOOL_IS_TRUE(IS_VALID_RSA_KEYSIZE_BITS_SE_ENGINE(rsa_size_bytes * 8U))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

fail:
	return ret;
}

static void se_rsa_write_keytable(const engine_t *engine,
				  const uint32_t *modexp32_param,
				  uint32_t bytes,
				  bool is_big_endian,
				  uint32_t rsa_keypkt_in)
{
	uint32_t i = 0U;
	uint32_t rsa_keytable_addr = 0U;
	const uint32_t *modexp32 = modexp32_param;

	for (; i < (bytes / BYTES_IN_WORD); i++) {
		uint32_t rsa_keypkt =
			rsa_keypkt_in | (i << SE_RSA_KEY_PKT_WORD_ADDR_SHIFT);

		rsa_keytable_addr = 0U;
		rsa_keytable_addr = LW_FLD_SET_DRF_NUM(SE0, RSA_KEYTABLE_ADDR, PKT,
						       rsa_keypkt, rsa_keytable_addr);

		/* Start RSA key slot write: exponent value */
		tegra_engine_set_reg(engine, SE0_RSA_KEYTABLE_ADDR_0, rsa_keytable_addr);

		/* Write exponent or modulus to keyslot */
		if (NULL != modexp32) {
			uint32_t val32 = 0U;
			if (BOOL_IS_TRUE(is_big_endian)) {
				val32 = se_util_swap_word_endian(*modexp32);
				modexp32--;
			} else {
				val32 = *modexp32;
				modexp32++;
			}
			tegra_engine_set_reg(engine, SE0_RSA_KEYTABLE_DATA_0, val32);
		} else {
			tegra_engine_set_reg(engine, SE0_RSA_KEYTABLE_DATA_0, 0U);
		}
	}
}

static void se_rsa_write_exponent(const engine_t *engine,
				  const uint32_t *exp32,
				  uint32_t rsa_keyslot,
				  uint32_t rsa_expsize_bytes,
				  bool exponent_big_endian)
{
	uint32_t exp_keypkt =
		(SE_RSA_KEY_PKT_INPUT_MODE_REGISTER << SE_RSA_KEY_PKT_INPUT_MODE_SHIFT) |
		(rsa_keyslot << SE_RSA_KEY_PKT_KEY_SLOT_SHIFT) |
		(SE_RSA_KEY_PKT_EXPMOD_SEL_EXPONENT << SE_RSA_KEY_PKT_EXPMOD_SEL_SHIFT);

	se_rsa_write_keytable(engine, exp32, rsa_expsize_bytes,
			      exponent_big_endian, exp_keypkt);
}

static void se_rsa_write_modulus(const engine_t *engine,
				 const uint32_t *mod32,
				 uint32_t rsa_keyslot,
				 uint32_t rsa_size_bytes,
				 bool modulus_big_endian)
{
	uint32_t mod_keypkt =
		(SE_RSA_KEY_PKT_INPUT_MODE_REGISTER << SE_RSA_KEY_PKT_INPUT_MODE_SHIFT) |
		(rsa_keyslot << SE_RSA_KEY_PKT_KEY_SLOT_SHIFT) |
		(SE_RSA_KEY_PKT_EXPMOD_SEL_MODULUS << SE_RSA_KEY_PKT_EXPMOD_SEL_SHIFT);

	se_rsa_write_keytable(engine, mod32, rsa_size_bytes,
			      modulus_big_endian, mod_keypkt);
}

/* Write RSA exponent and key
 *
 * If both modulus and exponent are NULL => clear the key slot.
 */
status_t rsa_write_key_locked(
	const engine_t *engine,
	uint32_t rsa_keyslot,
	uint32_t rsa_expsize_bytes, uint32_t rsa_size_bytes,
	const uint8_t *exponent, const uint8_t *modulus,
	bool exponent_big_endian, bool modulus_big_endian)
{
	status_t ret = NO_ERROR;
	const uint32_t *exp32 = NULL;
	const uint32_t *mod32 = NULL;

	LTRACEF("entry (modulus(is BE): %s [%u bytes], exponent(is BE): %s [%u bytes])\n",
		(BOOL_IS_TRUE(modulus_big_endian) ? "yes" : "no"),
		rsa_size_bytes,
		(BOOL_IS_TRUE(exponent_big_endian) ? "yes" : "no"),
		rsa_expsize_bytes);

	DEBUG_ASSERT_HW_MUTEX;

	ret = rsa_write_key_arg_check(rsa_keyslot, exponent, modulus,
				      rsa_expsize_bytes, rsa_size_bytes);
	CCC_ERROR_CHECK(ret);

	if (((NULL != exponent) && (NULL != modulus))) {
		if (BOOL_IS_TRUE(modulus_big_endian)) {
			mod32 = (const uint32_t *)&modulus[ rsa_size_bytes - BYTES_IN_WORD ];
		} else {
			mod32 = (const uint32_t *)&modulus[ 0U ];
		}
		if (BOOL_IS_TRUE(exponent_big_endian)) {
			exp32 = (const uint32_t *)&exponent[ rsa_expsize_bytes - BYTES_IN_WORD ];
		} else {
			exp32 = (const uint32_t *)&exponent[ 0U ];
		}
	}

	se_rsa_write_exponent(engine, exp32, rsa_keyslot,
			      rsa_expsize_bytes, exponent_big_endian);

	se_rsa_write_modulus(engine, mod32, rsa_keyslot,
			     rsa_size_bytes, modulus_big_endian);

	/* Check err_status register to make sure keyslot write is success */
	RSA_ERR_CHECK(engine, "after key write");

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

status_t tegra_se_clear_rsa_keyslot_locked(const struct engine_s *engine,
					   uint32_t rsa_keyslot,
					   uint32_t rsa_size)
{
	status_t ret = NO_ERROR;
	uint32_t exp_bytesize = rsa_size;
	uint32_t mod_bytesize = rsa_size;

	LTRACEF("entry\n");

	if ((engine->e_class & CCC_ENGINE_CLASS_RSA) == 0U) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					 CCC_ERROR_MESSAGE("rsa key clear with non-rsa engine (%s)\n",
							   engine->e_name));
	}

	if ((rsa_size < (RSA_MIN_MODULUS_SIZE_BITS / 8U)) ||
	    (rsa_size > (SE_PKA0_RSA_MAX_MODULUS_SIZE_BITS / 8U)) ||
	    ((rsa_size % 64U) != 0U)) {
		mod_bytesize = (SE_PKA0_RSA_MAX_MODULUS_SIZE_BITS / 8U);
		exp_bytesize = (SE_PKA0_RSA_MAX_MODULUS_SIZE_BITS / 8U);
	}

	ret = rsa_write_key_locked(engine,
				   rsa_keyslot, exp_bytesize, mod_bytesize,
				   NULL, NULL, CFALSE, CFALSE);
	CCC_ERROR_CHECK(ret);

	RSA_ERR_CHECK(engine, "after key clear");
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t engine_rsa_process_arg_check(
	const struct se_data_params *input_params,
	const struct se_engine_rsa_context *econtext)
{
	status_t ret = NO_ERROR;

	LTRACEF("entry\n");

	if ((NULL == input_params) || (NULL == econtext)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	// XXX code does not support setting RSA output to RSA keyslot
	// XXX e.g. implied by input_params->dst == NULL => should maybe support this...
	//
	if ((NULL == input_params->src) || (NULL == input_params->dst)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (econtext->rsa_keyslot >= SE_RSA_MAX_KEYSLOTS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (!BOOL_IS_TRUE(IS_VALID_RSA_KEYSIZE_BITS_SE_ENGINE(econtext->rsa_size * 8U))) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	/* public key exponent length restricted to 32 bit word here */
	if ((econtext->rsa_pub_expsize * 8U) > RSA_MAX_PUB_EXPONENT_SIZE_BITS) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	if (input_params->output_size < econtext->rsa_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					 LTRACEF("Output buffer too small for RSA output\n"));
	}

fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

static status_t engine_rsa_write_keyslot(struct se_engine_rsa_context *econtext,
					 bool *leave_key_to_keyslot_p,
					 uint32_t *exp_size_p,
					 bool *key_written_p)
{
	status_t ret = NO_ERROR;
	uint32_t exp_size = 0U;
	bool leave_key_to_keyslot = false;
	bool key_written = false;

	LTRACEF("entry\n");

	if ((NULL == econtext) ||
	    (NULL == exp_size_p) ||
	    (NULL == leave_key_to_keyslot_p) ||
	    (NULL == key_written_p)) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS);
	}

	*exp_size_p = 0U;
	*leave_key_to_keyslot_p = false;
	*key_written_p = false;

	RSA_ERR_CHECK(econtext->engine, "RSA KEY WRITE START");

	if ((econtext->rsa_flags & RSA_FLAGS_USE_PRIVATE_KEY) != 0U) {
		if (KEY_TYPE_RSA_PRIVATE != econtext->rsa_keytype) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
						 CCC_ERROR_MESSAGE("RSA operation 0x%x needs a private key\n",
								   econtext->algorithm));
		}

		exp_size = econtext->rsa_size;

		if ((NULL != econtext->rsa_priv_exponent) &&
		    (NULL != econtext->rsa_modulus)) {
			LOG_HEXBUF("RSA modulus:", econtext->rsa_modulus, econtext->rsa_size);
			key_written = true;

			/* Copies private key to keyslot; private key exponent size is
			 * identical with rsa_size
			 */
			ret = rsa_write_key_locked(
				econtext->engine,
				econtext->rsa_keyslot, econtext->rsa_size, econtext->rsa_size,
				econtext->rsa_priv_exponent, econtext->rsa_modulus,
				((econtext->rsa_flags & RSA_FLAGS_BIG_ENDIAN_EXPONENT) != 0U),
				((econtext->rsa_flags & RSA_FLAGS_BIG_ENDIAN_MODULUS) != 0U));
			CCC_ERROR_CHECK(ret);
		} else {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
						 CCC_ERROR_MESSAGE("RSA private key undefined\n"));
		}

		/* If the loaded RSA private key can be left to keyslot after the
		 * operation
		 */
		if ((econtext->rsa_flags & RSA_FLAGS_LEAVE_KEY_TO_KEYSLOT) != 0U) {
			leave_key_to_keyslot = true;
		}
	} else {
		/*  In case we have an RSA private key but no flag RSA_FLAGS_USE_PRIVATE_KEY is set =>
		 *  load the rsa_modulus and rsa_pub_exponent instead of the private exponent.
		 */
		if ((KEY_TYPE_RSA_PUBLIC != econtext->rsa_keytype) &&
		    (KEY_TYPE_RSA_PRIVATE != econtext->rsa_keytype)) {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
						 CCC_ERROR_MESSAGE("RSA operations needs public key => not provided"));
		}

		exp_size = econtext->rsa_pub_expsize;

		if ((NULL != econtext->rsa_pub_exponent) &&
		    (NULL != econtext->rsa_modulus) &&
		    (econtext->rsa_pub_expsize > 0U)) {
			LOG_HEXBUF("RSA pub expon:", econtext->rsa_pub_exponent,
				   econtext->rsa_pub_expsize);
			LOG_HEXBUF("RSA modulus:", econtext->rsa_modulus, econtext->rsa_size);
			key_written = true;

			/* Copies public key to keyslot; public key exponent size is
			 * rsa_pub_expsize bytes (4 bytes).
			 */
			ret = rsa_write_key_locked(
				econtext->engine,
				econtext->rsa_keyslot,
				econtext->rsa_pub_expsize, econtext->rsa_size,
				econtext->rsa_pub_exponent, econtext->rsa_modulus,
				((econtext->rsa_flags & RSA_FLAGS_BIG_ENDIAN_EXPONENT) != 0U),
				((econtext->rsa_flags & RSA_FLAGS_BIG_ENDIAN_MODULUS) != 0U));
			CCC_ERROR_CHECK(ret);
		} else {
			CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
						 CCC_ERROR_MESSAGE("RSA public key components undefined\n"));
		}

		/* Public keys are always left to the keyslot => saves time
		 *
		 * XXX TODO: Does anyone object? Any reason not to leave the pubkey to keyslot
		 * XXX  (this only saves the time required to clear the key)
		 */
		leave_key_to_keyslot = true;
	}

	RSA_ERR_CHECK(econtext->engine, "RSA KEY WRITE DONE");

	*exp_size_p = exp_size;
	*leave_key_to_keyslot_p = leave_key_to_keyslot;
	*key_written_p = key_written;
fail:
	LTRACEF("exit: %d\n", ret);
	return ret;
}

/* Input RSA data must be in correct byte order (little endian) when entering.
 * The code ilwerts the exponent and modulus if they are BE, but not the DATA!
 * (it has no space to do that (it would need to assume that src could be
 *  reversed or allocate some scratch space... Things would be easier if done
 *  like this  => consider changing it... TODO)
 *
 * LE <=> BE byte order colwersions must be handled by the caller
 * SE/PKA0 engine uses little endian always.
 */
status_t engine_rsa_modular_exp_locked(
	struct se_data_params *input_params,
	struct se_engine_rsa_context *econtext)
{
	uint32_t se_config_reg = 0U;
	PHYS_ADDR_T paddr_input_message_addr = PADDR_NULL;
	status_t ret = NO_ERROR;
	bool leave_key_to_keyslot = false;
	bool key_written = false;
	bool use_preset_key = false;

	uint32_t exp_size = 0U;
	uint32_t *result = NULL;
	uint32_t words_in_result = 0U;
	uint32_t inx = 0U;

	LTRACEF("entry\n");

	DEBUG_ASSERT_HW_MUTEX;

	ret = engine_rsa_process_arg_check(input_params, econtext);
	CCC_ERROR_CHECK(ret);

	if ((econtext->rsa_flags & RSA_FLAGS_USE_PRESET_KEY) == 0U) {

		ret = engine_rsa_write_keyslot(econtext,
					       &leave_key_to_keyslot,
					       &exp_size,
					       &key_written);
		CCC_ERROR_CHECK(ret);
	} else {
		/* Even when using a pre-loaded key => need to know the exponent size
		 * XXXX TODO !!! ===> Need test cases for pre-loaded keys, both PUB && PRIV exponents.
		 * XXXX and AES preloaded keys as well (not lwrrenlty tested at all)
		 * XXXX On pre-loaded keys the caller must know that the key sizes are correct.
		 * XXXX  There is no way to check what kind of key has been pre-loaded.
		 */
		if ((econtext->rsa_flags & RSA_FLAGS_USE_PRIVATE_KEY) != 0U) {
			exp_size = econtext->rsa_size;
		} else {
			exp_size = econtext->rsa_pub_expsize;
		}

		if ((econtext->rsa_flags & RSA_FLAGS_LEAVE_KEY_TO_KEYSLOT) != 0U) {
			leave_key_to_keyslot = true;
		}

		use_preset_key = true;
	}

	if (0U == exp_size) {
		CCC_ERROR_WITH_ECODE(ERR_ILWALID_ARGS,
					 LTRACEF("RSA exponent size invalid\n"));
	}

	/* RSA exponent size in words */
	tegra_engine_set_reg(econtext->engine, SE0_RSA_EXP_SIZE_0,
			     exp_size / BYTES_IN_WORD);

	/* RSA key size in SE HW: (modulus_bitsize / 512) - 1 */
	tegra_engine_set_reg(econtext->engine, SE0_RSA_KEY_SIZE_0,
			     ((econtext->rsa_size * 8U) / 512U) - 1U);

	/**
	 * 1. Set SE to RSA modular exponentiation operation
	 */
	se_config_reg = 0U;

	/* Write RSA result to RSA_OUTPUT registers only */
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0, RSA_CONFIG, DST, RSA_REG, se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0, RSA_CONFIG, DEC_ALG, NOP, se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0, RSA_CONFIG, ENC_ALG, RSA, se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0, RSA_CONFIG, DEC_MODE, DEFAULT, se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_DEF(SE0, RSA_CONFIG, ENC_MODE, DEFAULT, se_config_reg);

	/* Write to SE_RSA_CONFIG register */
	tegra_engine_set_reg(econtext->engine, SE0_RSA_CONFIG_0, se_config_reg);

#if SE_RD_DEBUG
	LTRACEF("RSA input data (%u bytes)\n", econtext->rsa_size);
	LOG_HEXBUF("RSA input data", input_params->src, econtext->rsa_size);
#endif

	paddr_input_message_addr = DOMAIN_VADDR_TO_PADDR(econtext->domain, input_params->src);
	SE_PHYS_RANGE_CHECK(paddr_input_message_addr, econtext->rsa_size);

	SE_CACHE_FLUSH((VIRT_ADDR_T)input_params->src, econtext->rsa_size);

	/* Program RSA task input message. */
	tegra_engine_set_reg(econtext->engine, SE0_RSA_IN_ADDR_0, (uint32_t)paddr_input_message_addr);

	/* Program input message buffer size into SE0_RSA_IN_ADDR_HI_0_SZ and
	 * 8-bit MSB of 40b dma addr into MSB field.
	 *
	 * The RSA output is written to RSA_OUTPUT registers (only).
	 */
	se_config_reg = 0U;
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_RSA, IN_ADDR_HI, SZ,
					   econtext->rsa_size, se_config_reg);
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0_RSA, IN_ADDR_HI, MSB,
					   SE_GET_32_UPPER_PHYSADDR_BITS(paddr_input_message_addr),
					   se_config_reg);
	tegra_engine_set_reg(econtext->engine, SE0_RSA_IN_ADDR_HI_0, se_config_reg);

	/**
	 * Set RSA key slot number to RSA_TASK_CONFIG register
	 */
	se_config_reg = 0U;
	se_config_reg = LW_FLD_SET_DRF_NUM(SE0, RSA_TASK_CONFIG, KEY_SLOT,
					   econtext->rsa_keyslot, se_config_reg);
	tegra_engine_set_reg(econtext->engine, SE0_RSA_TASK_CONFIG_0, se_config_reg);

	RSA_ERR_CHECK(econtext->engine, "RSA START");

	{
#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
		uint32_t gen_op_perf_usec = GET_USEC_TIMESTAMP;
#endif

		ret = tegra_start_se0_operation_locked(econtext->engine, 0U);
		CCC_ERROR_CHECK(ret);

		ret = se_wait_engine_free(econtext->engine);
		CCC_ERROR_CHECK(ret);

#if MEASURE_ENGINE_PERF(MEASURE_PERF_ENGINE_SE_OPERATION)
		if (0U != gen_op_perf_usec) {
			show_se_gen_op_entry_perf("RSA", get_elapsed_usec(gen_op_perf_usec));
		}
#endif
	}

	RSA_ERR_CHECK(econtext->engine, "RSA DONE");

	result = NULL;
	words_in_result = econtext->rsa_size / BYTES_IN_WORD;

	if (NULL != input_params->dst) {
		result = (uint32_t *)input_params->dst;
		input_params->output_size = econtext->rsa_size;
	}

	for (inx = 0U; inx < words_in_result; inx++) {
		uint32_t val = tegra_engine_get_reg(econtext->engine, SE0_RSA_OUTPUT_0 + (BYTES_IN_WORD*inx));
#if CLEAR_HW_RESULT
		tegra_engine_set_reg(econtext->engine, SE0_RSA_OUTPUT_0 + (BYTES_IN_WORD*inx), 0U);
#endif
		if (NULL != result) {
			result[ inx ] = val;
		}
	}

#if SE_RD_DEBUG
	LOG_HEXBUF("RSA RESULT (dst) after op", input_params->dst, econtext->rsa_size);
#endif

fail:
	if ((BOOL_IS_TRUE(use_preset_key) || BOOL_IS_TRUE(key_written)) &&
	    !BOOL_IS_TRUE(leave_key_to_keyslot)) {
		status_t fret = tegra_se_clear_rsa_keyslot_locked(econtext->engine,
								  econtext->rsa_keyslot,
								  econtext->rsa_size);
		if (NO_ERROR != fret) {
			LTRACEF("Failed to clear RSA engine %u keyslot %u\n",
				econtext->engine->e_id,
				econtext->rsa_keyslot);
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
void engine_rsa_close_locked(void)
{
	// Nothing lwrrently
}
#endif
#endif /* HAVE_SE_RSA */
